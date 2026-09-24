#pragma once

#include "ofxCore.h"

#include <cstddef>
#include <vector>

namespace LSPPaletteAnalysis {

constexpr int kMaxExtractSide = 300;
constexpr int kFingerprintGrid = 16;

void computeDownscaledSize(int fullW, int fullH, int maxSide, int& outW, int& outH);

OfxRectI makeTightBounds(int outW, int outH);

bool downscaleRgbaSlab(const float* src,
    int srcRowBytes,
    const OfxRectI& srcBounds,
    int maxSide,
    std::vector<float>& outPacked,
    int& outW,
    int& outH);

} // namespace LSPPaletteAnalysis
