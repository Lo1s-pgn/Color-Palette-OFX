#pragma once

#include "ofxCore.h"

#include <cstddef>
#include <vector>

#include "LSPPaletteGpuExtractShared.h"

#include "LSPPaletteGpuDownsampleParams.h"
#include "LSPPaletteGpuParams.h"

namespace LSPPaletteMetal {

/** GPU OKLAB sample gather from a tight RGBA analysis slab (median-cut stays on CPU). */
bool gatherOkLabSamplesFromSlab(const float* slabRgba,
    int slabWidth,
    int slabHeight,
    int slabRowFloats,
    int gridW,
    int gridH,
    const LSPPaletteGpuExtractGatherParams& params,
    void* metalCommandQueue,
    std::vector<LSPPaletteGpuOkLabSample>& outSamples);

/** GPU bilinear downscale from host MTLBuffer into a tight CPU RGBA buffer. */
bool downsampleHostToCpu(const void* srcMetalBuffer,
    void* metalCommandQueue,
    const OfxRectI& srcBounds,
    size_t srcRowBytes,
    int dstW,
    int dstH,
    float* outPackedRgba,
    std::size_t outFloatCapacity);

bool render(const float* src,
    float* dst,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    const LSPPaletteGpuParams& params);

bool renderHost(const void* srcMetalBuffer,
    void* dstMetalBuffer,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    int originX,
    int originY,
    const LSPPaletteGpuParams& params,
    void* metalCommandQueue);

} // namespace LSPPaletteMetal
