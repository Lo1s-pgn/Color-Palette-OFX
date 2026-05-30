// OFX plug-in instance: dominant-color palette overlay (CPU extract and composite).
#include "LSPPalettePlugin.h"
#include "LSPPaletteDescribe.h"
#include "LSPPaletteConstants.h"
#include "LSPPaletteLog.h"
#include "LSPPaletteUtil.h"
#include "LSPPaletteExtract.h"
#include "LSPPaletteComposite.h"
#include "LSPPaletteProcessor.h"
#include "ColorManagement.h"
#include "ofxsCore.h"

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
}

void LSPPalettePlugin::render(const OFX::RenderArguments& args) {
    LSP_PALETTE_TRACE(std::string("trace: render enter t=") + std::to_string(args.time));
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
    es.detailSuppression = 0.5f;

    std::vector<LSPPaletteExtract::Swatch> palette;
    LSP_PALETTE_TRACE("trace: extract begin");
    if (!LSPPaletteExtract::extractDominantColors(src.get(), sb, es, palette) || palette.empty()) {
        palette.clear();
        LSP_PALETTE_LOG_ERROR("palette_extract_failed");
    }
    LSP_PALETTE_TRACE(std::string("trace: extract done n=") + std::to_string(palette.size()));

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

    LSPPaletteComposite::OverlayLayout overlayLay;
    LSPPaletteComposite::CompositeFrame frameLay;
    const bool havePlan = !palette.empty()
        && LSPPaletteComposite::buildCompositePlan(db, sb, palette, tfId, primId, pres, overlayLay, frameLay);

    LSPPaletteProcessor proc(*this);
    proc.setDstImg(dst.get());
    proc.setSrcImg(src.get());
    proc.setRenderWindow(args.renderWindow);
    proc.setGPURenderArgs(args);
    proc.setDrawOverlay(!palette.empty());
    proc.setCompositorState(overlayLay, frameLay, havePlan);
    LSP_PALETTE_TRACE("trace: proc.process begin");
    proc.process();
    LSP_PALETTE_TRACE("trace: proc.process done");
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
    desc.setSupportsMetalRender(false);
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
