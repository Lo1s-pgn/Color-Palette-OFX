#pragma once

#include "ColorManagement.h"
#include "ofxsImageEffect.h"

#include <vector>

namespace LSPPaletteExtract {

struct Settings {
    WorkshopColor::ColorPrimariesId primaries = WorkshopColor::ColorPrimariesId::Rec709;
    WorkshopColor::TransferFunctionId transfer = WorkshopColor::TransferFunctionId::Gamma24;
    int patchCount = 8;
    int sortOrder = 0;
};

struct Swatch {
    WorkshopColor::Vec3f linearRgb{};
    float weight = 0.0f;
};

bool extractDominantColors(
    OFX::Image* src, const OfxRectI& sampleBounds, const Settings& settings, std::vector<Swatch>& outPalette);

/** Row-major RGBA float slab; rowBytes is host image row pitch (may exceed width*16). */
bool extractDominantColorsFromSlab(const float* slab,
    int rowBytes,
    const OfxRectI& sampleBounds,
    const Settings& settings,
    std::vector<Swatch>& outPalette);

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
/** Tight analysis RGBA slab (e.g. GPU-downscaled); median-cut remains on CPU. */
bool extractDominantColorsFromAnalysisSlabGpu(const float* slab,
    int rowBytes,
    const OfxRectI& sampleBounds,
    void* metalCommandQueue,
    const Settings& settings,
    std::vector<Swatch>& outPalette);
#endif

} // namespace LSPPaletteExtract
