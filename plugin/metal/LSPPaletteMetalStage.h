#pragma once

#include "ofxCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace LSPPaletteMetalStage {

/** GPU 16×16 downsample + hash (no full-frame readback). */
bool computeHostMetalFingerprint(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    uint64_t& outFingerprint);

/** GPU downscale to analysis resolution (max 300 px) for median-cut extract. */
bool stageHostMetalForExtract(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    int maxAnalysisSide,
    std::vector<float>& outPackedRgba,
    int& outW,
    int& outH);

bool copyHostMetalImageToCpu(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    std::vector<float>& outPackedRgba);

bool copyCpuToHostMetalImage(void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t dstRowBytes,
    const float* packedRgba,
    std::size_t packedFloatCount);

} // namespace LSPPaletteMetalStage
