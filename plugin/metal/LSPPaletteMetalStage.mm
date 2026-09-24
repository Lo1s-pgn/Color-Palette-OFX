#include "LSPPaletteMetalStage.h"
#include "LSPPaletteMetal.h"

#include "LSPPaletteAnalysis.h"
#include "LSPPaletteRenderCache.h"

namespace LSPPaletteMetalStage {

bool computeHostMetalFingerprint(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    uint64_t& outFingerprint) {
    outFingerprint = 0;
    const int fw = bounds.x2 - bounds.x1;
    const int fh = bounds.y2 - bounds.y1;
    if (fw < 1 || fh < 1)
        return false;

    std::vector<float> fpBuf(static_cast<std::size_t>(LSPPaletteAnalysis::kFingerprintGrid)
            * static_cast<std::size_t>(LSPPaletteAnalysis::kFingerprintGrid) * 4u);
    if (!LSPPaletteMetal::downsampleHostToCpu(metalBuffer,
            metalCommandQueue,
            bounds,
            srcRowBytes,
            LSPPaletteAnalysis::kFingerprintGrid,
            LSPPaletteAnalysis::kFingerprintGrid,
            fpBuf.data(),
            fpBuf.size())) {
        return false;
    }
    const OfxRectI tight = LSPPaletteAnalysis::makeTightBounds(
        LSPPaletteAnalysis::kFingerprintGrid, LSPPaletteAnalysis::kFingerprintGrid);
    const int rowBytes = LSPPaletteAnalysis::kFingerprintGrid * 4 * static_cast<int>(sizeof(float));
    outFingerprint = LSPPaletteRenderCache::computeSourceFingerprintFromSlab(fpBuf.data(), rowBytes, tight);
    return outFingerprint != 0;
}

bool stageHostMetalForExtract(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    int maxAnalysisSide,
    std::vector<float>& outPackedRgba,
    int& outW,
    int& outH) {
    const int fw = bounds.x2 - bounds.x1;
    const int fh = bounds.y2 - bounds.y1;
    if (fw < 1 || fh < 1)
        return false;
    LSPPaletteAnalysis::computeDownscaledSize(fw, fh, maxAnalysisSide, outW, outH);
    const std::size_t nFloats = static_cast<std::size_t>(outW) * static_cast<std::size_t>(outH) * 4u;
    outPackedRgba.resize(nFloats);
    if (!LSPPaletteMetal::downsampleHostToCpu(
            metalBuffer, metalCommandQueue, bounds, srcRowBytes, outW, outH, outPackedRgba.data(), nFloats)) {
        outPackedRgba.clear();
        outW = 0;
        outH = 0;
        return false;
    }
    return true;
}

} // namespace LSPPaletteMetalStage
