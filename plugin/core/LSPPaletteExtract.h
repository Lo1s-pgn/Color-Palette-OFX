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
    /** Blend toward Gaussian low-pass on the analysis grid (plug-in uses 0.5f; 0.f disables blur path). */
    float detailSuppression = 0.5f;
};

struct Swatch {
    WorkshopColor::Vec3f linearRgb{};
    float weight = 0.0f;
};

bool extractDominantColors(
    OFX::Image* src, const OfxRectI& sampleBounds, const Settings& settings, std::vector<Swatch>& outPalette);

} // namespace LSPPaletteExtract
