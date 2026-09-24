// main OFX plugin, grabs dominant colors and composites the strip (cpu or gpu).
#include "LSPPalettePlugin.h"
#include "LSPPaletteDescribe.h"
#include "LSPPaletteConstants.h"
#include "LSPPaletteLog.h"
#include "LSPPaletteUtil.h"
#include "LSPPaletteExtract.h"
#include "LSPPaletteComposite.h"
#include "LSPPaletteProcessor.h"
#include "LSPPaletteRenderCache.h"
#include "LSPPaletteRender.h"
#include "LSPPaletteGpuParams.h"
#include "LSPPaletteRuntimeEnv.h"
#include "LSPPaletteAnalysis.h"
#include "LSPPaletteImageAccess.h"
#include "ColorManagement.h"
#include "ofxsCore.h"

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
#include "LSPPaletteMetalStage.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

bool paramLeafIs(const std::string& pName, const char* leaf) {
    const std::size_t n = std::strlen(leaf);
    if (n == 0 || pName.size() < n)
        return false;
    if (pName.size() == n)
        return pName == leaf;
    if (pName.size() < n + 1u)
        return false;
    const char s = pName[pName.size() - n - 1u];
    if (s != '/' && s != '.')
        return false;
    return pName.compare(pName.size() - n, n, leaf) == 0;
}

int clampChoice(OFX::ChoiceParam* p, double t, int defIdx) {
    if (!p)
        return defIdx;
    const int nOpt = p->getNOptions();
    if (nOpt <= 0)
        return defIdx;
    int v = defIdx;
    p->getValueAtTime(t, v);
    if (v < 0)
        v = 0;
    if (v >= nOpt)
        v = nOpt - 1;
    return v;
}

int clampInt(OFX::IntParam* p, double t, int lo, int hi, int defV) {
    if (!p)
        return defV;
    int v = defV;
    p->getValueAtTime(t, v);
    return std::clamp(v, lo, hi);
}

double clampDouble(OFX::DoubleParam* p, double t, double lo, double hi, double defV) {
    if (!p)
        return defV;
    double v = defV;
    p->getValueAtTime(t, v);
    return std::clamp(v, lo, hi);
}

bool isPaletteExtractParam(const std::string& paramName) {
    return paramLeafIs(paramName, "paletteInputPrimaries") || paramLeafIs(paramName, "paletteInputTransfer")
        || paramLeafIs(paramName, "palettePatchCount") || paramLeafIs(paramName, "paletteSortOrder");
}

bool isPalettePresentationParam(const std::string& paramName) {
    return paramLeafIs(paramName, "paletteLayout") || paramLeafIs(paramName, "paletteFullFrame")
        || paramLeafIs(paramName, "palettePatchSize") || paramLeafIs(paramName, "paletteGap")
        || paramLeafIs(paramName, "paletteCorner")         || paramLeafIs(paramName, "paletteBackgroundLightness");
}

} // namespace

class LSPPalettePlugin : public OFX::ImageEffect {
public:
    explicit LSPPalettePlugin(OfxImageEffectHandle handle);
    ~LSPPalettePlugin() override;

    void render(const OFX::RenderArguments& args) override;

    void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) override;
    void changedClip(const OFX::InstanceChangedArgs& args, const std::string& clipName) override;

private:
    OFX::Clip* dstClip_ = nullptr;
    OFX::Clip* srcClip_ = nullptr;
    OFX::ChoiceParam* paletteInputPrimaries_ = nullptr;
    OFX::ChoiceParam* paletteInputTransfer_ = nullptr;
    OFX::IntParam* palettePatchCount_ = nullptr;
    OFX::ChoiceParam* paletteSortOrder_ = nullptr;
    OFX::ChoiceParam* paletteLayout_ = nullptr;
    OFX::BooleanParam* paletteFullFrame_ = nullptr;
    OFX::DoubleParam* palettePatchSize_ = nullptr;
    OFX::DoubleParam* paletteGap_ = nullptr;
    OFX::DoubleParam* paletteCorner_ = nullptr;
    OFX::DoubleParam* paletteBackgroundLightness_ = nullptr;

    LSPPaletteRenderCache renderCache_;
    LSPPaletteRenderProcessor renderProcessor_;
    std::vector<float> analysisStaging_;
};

LSPPalettePlugin::LSPPalettePlugin(OfxImageEffectHandle handle)
    : OFX::ImageEffect(handle) {
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
    paletteInputPrimaries_ = fetchChoiceParam("paletteInputPrimaries");
    paletteInputTransfer_ = fetchChoiceParam("paletteInputTransfer");
    palettePatchCount_ = fetchIntParam("palettePatchCount");
    paletteSortOrder_ = fetchChoiceParam("paletteSortOrder");
    paletteLayout_ = fetchChoiceParam("paletteLayout");
    paletteFullFrame_ = fetchBooleanParam("paletteFullFrame");
    palettePatchSize_ = fetchDoubleParam("palettePatchSize");
    paletteGap_ = fetchDoubleParam("paletteGap");
    paletteCorner_ = fetchDoubleParam("paletteCorner");
    paletteBackgroundLightness_ = fetchDoubleParam("paletteBackgroundLightness");

    if (OFX::StringParam* credits = fetchStringParam("supportCreditsLabel"))
        credits->setEnabled(false);

    {
        const std::string versionStr = PLUGIN_VERSION_STR;
        OFX::ImageEffectHostDescription* host = OFX::getImageEffectHostDescription();
        const std::string hostName = host ? host->hostName : "unknown";
        const std::string hostLabel = host ? host->hostLabel : "";
        std::string hostVersion;
        if (host) {
            if (!host->versionLabel.empty())
                hostVersion = host->versionLabel;
            else if (host->versionMajor != 0 || host->versionMinor != 0 || host->versionMicro != 0)
                hostVersion =
                    std::to_string(host->versionMajor) + "." + std::to_string(host->versionMinor) + "."
                    + std::to_string(host->versionMicro);
        }
        const std::string buildInfo = __DATE__;
        const std::string bundlePath = LSPPaletteLog::getPluginBundleRootPath();
        LSP_PALETTE_LOG_SESSION_START(kPluginName, versionStr, hostName, hostLabel, hostVersion, buildInfo, bundlePath, "");
    }
}

LSPPalettePlugin::~LSPPalettePlugin() = default;

void LSPPalettePlugin::changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) {
    (void)args;
    if (paramLeafIs(paramName, "paletteSortOrder"))
        renderCache_.invalidatePlan();
    else if (isPaletteExtractParam(paramName))
        renderCache_.invalidateExtract();
    else if (isPalettePresentationParam(paramName))
        renderCache_.invalidatePlan();
    if (paramLeafIs(paramName, "supportWebsite")) {
        lspPaletteOpenUrl(kPaletteRepoUrl);
        return;
    }
    if (paramLeafIs(paramName, "supportReportIssue")) {
        lspPaletteOpenUrl(kPaletteIssuesUrl);
        return;
    }
    if (paramLeafIs(paramName, "supportOpenLog")) {
        lspPaletteOpenLogExternally();
        return;
    }
}

void LSPPalettePlugin::changedClip(const OFX::InstanceChangedArgs& args, const std::string& clipName) {
    (void)args;
    (void)clipName;
    renderCache_.invalidateAll();
}

void LSPPalettePlugin::render(const OFX::RenderArguments& args) {
    LSPPaletteRuntimeEnv::logStageLine(std::string("trace: render enter t=") + std::to_string(args.time));
    if (args.isEnabledMetalRender)
        LSPPaletteRuntimeEnv::logStage("render: OFX Metal buffers");
    else if (args.isEnabledCudaRender)
        LSPPaletteRuntimeEnv::logStage("render: OFX CUDA buffers");
    std::unique_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
    std::unique_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
    if (!dst.get() || !src.get() || !dst->getPixelData() || !src->getPixelData())
        OFX::throwSuiteStatusException(kOfxStatFailed);
    if (dst->getPixelDepth() != OFX::eBitDepthFloat || src->getPixelDepth() != OFX::eBitDepthFloat)
        OFX::throwSuiteStatusException(kOfxStatFailed);
    if (dst->getPixelComponents() != OFX::ePixelComponentRGBA || src->getPixelComponents() != OFX::ePixelComponentRGBA)
        OFX::throwSuiteStatusException(kOfxStatFailed);

    const OfxRectI db = dst->getBounds();
    const OfxRectI sb = src->getBounds();
    const int width = db.x2 - db.x1;
    const int height = db.y2 - db.y1;
    if (width <= 0 || height <= 0)
        return;

    const int primIdx = clampChoice(paletteInputPrimaries_, args.time,
        WorkshopColor::inputPrimariesChoiceIndex(WorkshopColor::ColorPrimariesId::Rec709));
    const int tfIdx = clampChoice(paletteInputTransfer_, args.time,
        WorkshopColor::inputTransferFunctionChoiceIndex(WorkshopColor::TransferFunctionId::Gamma24));

    const int patchCount = clampInt(palettePatchCount_, args.time, 3, 24, 8);
    const int sortOrder = clampChoice(paletteSortOrder_, args.time, 0);
    const int layout = clampChoice(paletteLayout_, args.time, LSPPaletteComposite::kLayoutBottom);

    bool fullFrame = false;
    if (paletteFullFrame_)
        paletteFullFrame_->getValueAtTime(args.time, fullFrame);

    const double stripFrac = clampDouble(palettePatchSize_, args.time, 0.05, 0.55, 0.14);
    const double gapR = clampDouble(paletteGap_, args.time, 0.0, 1.0, 0.22);
    const double cornerR = clampDouble(paletteCorner_, args.time, 0.0, 0.49, 0.10);
    const double bgLight = clampDouble(paletteBackgroundLightness_, args.time, 0.0, 1.0, 0.0);

    LSPPaletteExtract::Settings es;
    es.primaries = WorkshopColor::inputPrimariesIdFromChoiceIndex(primIdx);
    es.transfer = WorkshopColor::inputTransferFunctionIdFromChoiceIndex(tfIdx);
    es.patchCount = patchCount;
    es.sortOrder = sortOrder;

    LSPPaletteRenderCache::ExtractKey eKey;
    eKey.time = args.time;
    eKey.patchCount = patchCount;
    eKey.sortOrder = sortOrder;
    eKey.primaries = es.primaries;
    eKey.transfer = es.transfer;
    eKey.srcW = sb.x2 - sb.x1;
    eKey.srcH = sb.y2 - sb.y1;
    eKey.sourceFingerprint = 0;

    std::vector<LSPPaletteExtract::Swatch> palette;
    bool haveExtract = false;

    if (renderCache_.hasCachedPaletteForParams(eKey) || renderCache_.hasCachedClusterForParams(eKey)) {
        uint64_t quickFp = 0;
        bool quickFpOk = false;
#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
        if (args.isEnabledMetalRender && args.pMetalCmdQ != nullptr) {
            const int srcRb = src->getRowBytes();
            const size_t srcRowBytes = srcRb < 0 ? static_cast<size_t>(-srcRb) : static_cast<size_t>(srcRb);
            quickFpOk = LSPPaletteMetalStage::computeHostMetalFingerprint(
                src->getPixelData(), args.pMetalCmdQ, sb, srcRowBytes, quickFp);
        }
#endif
        if (!quickFpOk && !args.isEnabledMetalRender && !args.isEnabledCudaRender) {
            const float* slab = LSPPaletteImageAccess::cpuReadableSlab(src.get());
            if (slab) {
                int fpW = 0;
                int fpH = 0;
                if (LSPPaletteAnalysis::downscaleRgbaSlab(slab,
                        src->getRowBytes(),
                        sb,
                        LSPPaletteAnalysis::kFingerprintGrid,
                        analysisStaging_,
                        fpW,
                        fpH)) {
                    const OfxRectI fpBounds = LSPPaletteAnalysis::makeTightBounds(fpW, fpH);
                    quickFp = LSPPaletteRenderCache::computeSourceFingerprintFromSlab(
                        analysisStaging_.data(), fpW * 4 * static_cast<int>(sizeof(float)), fpBounds);
                    quickFpOk = quickFp != 0;
                }
            }
        }
        if (quickFpOk) {
            eKey.sourceFingerprint = quickFp;
            haveExtract = renderCache_.tryGetCachedExtract(eKey, palette);
            if (haveExtract)
                LSPPaletteRuntimeEnv::logStage("extract_cache_fast");
            else if (renderCache_.tryResortCachedCluster(eKey, es, palette)) {
                haveExtract = true;
                LSPPaletteRuntimeEnv::logStage("extract_cache_resort");
            }
        }
    }

    if (!haveExtract)
        renderCache_.copyStalePalette(palette);

    if (!haveExtract) {
        const float* extractSlab = nullptr;
        int extractRowBytes = 0;
        OfxRectI extractBounds = sb;

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
        if (args.isEnabledMetalRender && args.pMetalCmdQ != nullptr) {
            const int srcRb = src->getRowBytes();
            const size_t srcRowBytes = srcRb < 0 ? static_cast<size_t>(-srcRb) : static_cast<size_t>(srcRb);
            int analysisW = 0;
            int analysisH = 0;
            if (LSPPaletteMetalStage::stageHostMetalForExtract(src->getPixelData(),
                    args.pMetalCmdQ,
                    sb,
                    srcRowBytes,
                    LSPPaletteAnalysis::kMaxExtractSide,
                    analysisStaging_,
                    analysisW,
                    analysisH)) {
                extractSlab = analysisStaging_.data();
                extractBounds = LSPPaletteAnalysis::makeTightBounds(analysisW, analysisH);
                extractRowBytes = analysisW * 4 * static_cast<int>(sizeof(float));
            } else {
                LSP_PALETTE_LOG_ERROR("metal_stage_extract_failed");
            }
        }
#endif
        if (extractSlab != nullptr)
            eKey.sourceFingerprint = LSPPaletteRenderCache::computeSourceFingerprintFromSlab(
                extractSlab, extractRowBytes, extractBounds);
        else if (!args.isEnabledMetalRender && !args.isEnabledCudaRender) {
            const float* slab = LSPPaletteImageAccess::cpuReadableSlab(src.get());
            if (slab) {
                int analysisW = 0;
                int analysisH = 0;
                if (LSPPaletteAnalysis::downscaleRgbaSlab(slab,
                        src->getRowBytes(),
                        sb,
                        LSPPaletteAnalysis::kMaxExtractSide,
                        analysisStaging_,
                        analysisW,
                        analysisH)) {
                    extractSlab = analysisStaging_.data();
                    extractBounds = LSPPaletteAnalysis::makeTightBounds(analysisW, analysisH);
                    extractRowBytes = analysisW * 4 * static_cast<int>(sizeof(float));
                    eKey.sourceFingerprint = LSPPaletteRenderCache::computeSourceFingerprintFromSlab(
                        extractSlab, extractRowBytes, extractBounds);
                } else {
                    eKey.sourceFingerprint = LSPPaletteRenderCache::computeSourceFingerprint(src.get(), sb);
                    extractSlab = slab;
                    extractBounds = sb;
                    extractRowBytes = src->getRowBytes();
                }
            }
        }

        if (!renderCache_.tryGetCachedExtract(eKey, palette)) {
            if (eKey.sourceFingerprint != 0 && renderCache_.tryResortCachedCluster(eKey, es, palette)) {
                LSPPaletteRuntimeEnv::logStage("extract_cache_resort");
            } else {
                renderCache_.bumpMiss();
                LSPPaletteRuntimeEnv::logStage("trace: extract begin");
                std::vector<LSPPaletteExtract::Swatch> clusterPalette;
                bool extracted = false;
#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
                if (extractSlab != nullptr && args.isEnabledMetalRender && args.pMetalCmdQ != nullptr)
                    extracted = LSPPaletteExtract::extractDominantColorsFromAnalysisSlabGpu(extractSlab,
                        extractRowBytes, extractBounds, args.pMetalCmdQ, es, palette, &clusterPalette);
                else
#endif
                if (extractSlab != nullptr)
                    extracted = LSPPaletteExtract::extractDominantColorsFromSlab(
                        extractSlab, extractRowBytes, extractBounds, es, palette, &clusterPalette);
                else if (!args.isEnabledMetalRender)
                    extracted = LSPPaletteExtract::extractDominantColors(src.get(), sb, es, palette, &clusterPalette);
                if (extracted && args.isEnabledMetalRender)
                    LSPPaletteRuntimeEnv::logStage("extract_gpu_gather");
                if (!extracted || palette.empty()) {
                    palette.clear();
                    LSP_PALETTE_LOG_ERROR("palette_extract_failed");
                } else {
                    if (!clusterPalette.empty())
                        renderCache_.storeCluster(eKey, clusterPalette);
                    renderCache_.storeExtract(eKey, palette);
                }
                LSPPaletteRuntimeEnv::logStageLine(
                    std::string("trace: extract done n=") + std::to_string(palette.size()));
            }
        }
    }

    LSPPaletteComposite::Presentation pres;
    pres.layout = layout;
    pres.fitPaletteToFrame = fullFrame;
    pres.imageFillCoverCrop = fullFrame;
    pres.stripFrac = stripFrac;
    pres.gapFrac = gapR;
    pres.cornerFrac = cornerR;
    pres.bgLightness = bgLight;

    WorkshopColor::TransferFunctionId tfId = WorkshopColor::inputTransferFunctionIdFromChoiceIndex(tfIdx);
    WorkshopColor::ColorPrimariesId primId = WorkshopColor::inputPrimariesIdFromChoiceIndex(primIdx);

    LSPPaletteRenderCache::PresentationKey pKey;
    pKey.layout = layout;
    pKey.fitPaletteToFrame = fullFrame;
    pKey.imageFillCoverCrop = fullFrame;
    pKey.stripFrac = stripFrac;
    pKey.gapFrac = gapR;
    pKey.cornerFrac = cornerR;
    pKey.bgLightness = bgLight;
    pKey.dstW = width;
    pKey.dstH = height;

    LSPPaletteComposite::OverlayLayout overlayLay;
    LSPPaletteComposite::CompositeFrame frameLay;
    bool havePlan = false;
    if (!renderCache_.tryGetCachedPlan(pKey, eKey, overlayLay, frameLay, havePlan)) {
        renderCache_.bumpMiss();
        havePlan = !palette.empty()
            && LSPPaletteComposite::buildCompositePlan(db, sb, palette, tfId, primId, pres, overlayLay, frameLay);
        renderCache_.storePlan(pKey, eKey, overlayLay, frameLay, havePlan);
    }

    LSPPaletteRuntimeEnv::logCacheStats(renderCache_.cacheHits_, renderCache_.cacheMisses_);

    LSPPaletteGpuParams gpuParams{};
    if (havePlan) {
        const int srcRb = src->getRowBytes();
        const int dstRb = dst->getRowBytes();
        const size_t srcRowBytes = srcRb < 0 ? static_cast<size_t>(-srcRb) : static_cast<size_t>(srcRb);
        const size_t dstRowBytes = dstRb < 0 ? static_cast<size_t>(-dstRb) : static_cast<size_t>(dstRb);
        LSPPaletteGpuParamsUtil::packFromLayout(db, sb, overlayLay, frameLay, width, height, srcRowBytes, dstRowBytes, gpuParams);
        renderProcessor_.setGpuParams(gpuParams);
#if defined(LSP_PALETTE_HAS_CUDA)
        if (LSPPaletteRuntimeEnv::preferHostCuda() && args.isEnabledCudaRender && args.pCudaStream != nullptr) {
            const float* srcDev = static_cast<const float*>(src->getPixelData());
            float* dstDev = static_cast<float*>(dst->getPixelData());
            if (srcDev != nullptr && dstDev != nullptr
                && renderProcessor_.renderCUDAHostBuffers(srcDev, dstDev, width, height, srcRowBytes, dstRowBytes, args.pCudaStream)) {
                LSPPaletteRuntimeEnv::logGpuBackend("cuda_host");
                return;
            }
        }
#endif
    }

    LSPPaletteProcessor proc(*this);
    proc.setDstImg(dst.get());
    proc.setSrcImg(src.get());
    proc.setRenderWindow(args.renderWindow);
    proc.setGPURenderArgs(args);
    proc.setDrawOverlay(!palette.empty());
    proc.setCompositorState(overlayLay, frameLay, havePlan);
    proc.setGpuParams(gpuParams);
    proc.setRenderProcessor(&renderProcessor_);
    LSPPaletteRuntimeEnv::logStage("trace: proc.process begin");
    proc.process();
    LSPPaletteRuntimeEnv::logStage("trace: proc.process done");
}

LSPPalettePluginFactory::LSPPalettePluginFactory()
    : OFX::PluginFactoryHelper<LSPPalettePluginFactory>(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor) {}

void LSPPalettePluginFactory::describe(OFX::ImageEffectDescriptor& desc) {
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setVersion(PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH, 0, std::string(PLUGIN_VERSION_STR));
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedContext(OFX::eContextGeneral);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
#if defined(__APPLE__)
    applyPaletteHostRenderSupport(desc, false, true);
#elif defined(OFX_SUPPORTS_CUDARENDER)
    applyPaletteHostRenderSupport(desc, true, false);
#else
    applyPaletteHostRenderSupport(desc, false, false);
#endif
}

void LSPPalettePluginFactory::describeInContext(OFX::ImageEffectDescriptor& desc, OFX::ContextEnum ctx) {
    describePaletteInContext(desc, ctx);
}

OFX::ImageEffect* LSPPalettePluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*ctx*/) {
    return new LSPPalettePlugin(handle);
}

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray& factoryArray) {
    static LSPPalettePluginFactory g_factory;
    factoryArray.push_back(&g_factory);
}
