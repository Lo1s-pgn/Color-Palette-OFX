#pragma once

#include "ofxCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace LSPPaletteMetalStage {

// tiny downsample for source fingerprint hash
bool computeHostMetalFingerprint(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    uint64_t& outFingerprint);

// shrink for color pick, max 300px side
bool stageHostMetalForExtract(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    int maxAnalysisSide,
    std::vector<float>& outPackedRgba,
    int& outW,
    int& outH);

} // namespace LSPPaletteMetalStage
