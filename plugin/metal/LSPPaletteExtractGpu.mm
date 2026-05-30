#import <Foundation/Foundation.h>

#include "LSPPaletteExtract.h"
#include "LSPPaletteExtractInternal.h"
#include "LSPPaletteAnalysis.h"
#include "LSPPaletteMetal.h"
#include "ColorManagement.h"

namespace {

void fillPrimariesToSrgb(WorkshopColor::ColorPrimariesId primaries, float out9[9]) {
    const WorkshopColor::Mat3f toXyz = WorkshopColor::rgbToXyzMatrix(primaries);
    const WorkshopColor::Mat3f toSrgb = WorkshopColor::xyzToRgbMatrix(WorkshopColor::ColorPrimariesId::Rec709);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k)
                sum += toSrgb.m[row][k] * toXyz.m[k][col];
            out9[row * 3 + col] = sum;
        }
    }
}

} // namespace

namespace LSPPaletteExtract {

bool extractDominantColorsFromAnalysisSlabGpu(const float* slab,
    int rowBytes,
    const OfxRectI& sampleBounds,
    void* metalCommandQueue,
    const Settings& settings,
    std::vector<Swatch>& outPalette) {
    outPalette.clear();
    if (!slab || rowBytes < 16 || metalCommandQueue == nullptr)
        return false;

    const int fw = sampleBounds.x2 - sampleBounds.x1;
    const int fh = sampleBounds.y2 - sampleBounds.y1;
    if (fw < 2 || fh < 2)
        return false;

    int nw = fw;
    int nh = fh;
    LSPPaletteAnalysis::computeDownscaledSize(fw, fh, LSPPaletteAnalysis::kMaxExtractSide, nw, nh);
    const int gridCells = nw * nh;
    if (gridCells < 1)
        return false;

    LSPPaletteGpuExtractGatherParams gp{};
    gp.slabWidth = fw;
    gp.slabHeight = fh;
    gp.slabRowFloats = fw * 4;
    gp.gridW = nw;
    gp.gridH = nh;
    gp.sampleCap = gridCells;
    gp.transferChoice = WorkshopColor::inputTransferFunctionChoiceIndex(settings.transfer);
    gp.nearBlackLinear = 0.004f;
    fillPrimariesToSrgb(settings.primaries, gp.primariesToSrgb);

    std::vector<LSPPaletteGpuOkLabSample> gpuSamples;
    if (!LSPPaletteMetal::gatherOkLabSamplesFromSlab(
            slab, fw, fh, gp.slabRowFloats, nw, nh, gp, metalCommandQueue, gpuSamples)) {
        return false;
    }

    std::vector<LSPPaletteExtractInternal::OkLabSample> packed;
    packed.reserve(gpuSamples.size());
    for (const LSPPaletteGpuOkLabSample& s : gpuSamples) {
        if (!s.valid)
            continue;
        LSPPaletteExtractInternal::OkLabSample o;
        o.L = s.L;
        o.a = s.a;
        o.b = s.b;
        o.w = static_cast<double>(s.w);
        packed.push_back(o);
    }
    if (packed.empty())
        return false;

    return LSPPaletteExtractInternal::finishPalette(packed, settings, outPalette);
}

} // namespace LSPPaletteExtract
