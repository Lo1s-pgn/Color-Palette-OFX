#pragma once

#include "ofxCore.h"

#include <cstddef>
#include <vector>

namespace LSPPaletteAnalysis {

constexpr int kMaxExtractSide = 300;
constexpr int kFingerprintGrid = 16;

void computeDownscaledSize(int fullW, int fullH, int maxSide, int& outW, int& outH);

/** Tight RGBA float slab bounds (0,0,outW,outH). */
OfxRectI makeTightBounds(int outW, int outH);

/** Nearest-phase downscale from a row-major float RGBA slab (full image bounds). */
bool downscaleRgbaSlab(const float* src,
    int srcRowBytes,
    const OfxRectI& srcBounds,
    int maxSide,
    std::vector<float>& outPacked,
    int& outW,
    int& outH);

} // namespace LSPPaletteAnalysis
