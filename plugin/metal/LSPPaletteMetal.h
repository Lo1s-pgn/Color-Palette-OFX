#pragma once

#include "ofxCore.h"

#include <cstddef>
#include <vector>

#include "LSPPaletteGpuExtractShared.h"

#include "LSPPaletteGpuDownsampleParams.h"
#include "LSPPaletteGpuParams.h"

namespace LSPPaletteMetal {

// resolve handed us metal memory not a cpu pointer
bool hostPointerIsMTLBuffer(const void* ptr);

bool gatherOkLabSamplesFromSlab(const float* slabRgba,
    int slabWidth,
    int slabHeight,
    int slabRowFloats,
    int gridW,
    int gridH,
    const LSPPaletteGpuExtractGatherParams& params,
    void* metalCommandQueue,
    std::vector<LSPPaletteGpuOkLabSample>& outSamples);

bool downsampleHostToCpu(const void* srcMetalBuffer,
    void* metalCommandQueue,
    const OfxRectI& srcBounds,
    size_t srcRowBytes,
    int dstW,
    int dstH,
    float* outPackedRgba,
    std::size_t outFloatCapacity);

bool blitCopyHostBuffers(const void* srcMetalBuffer,
    void* dstMetalBuffer,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    int originX,
    int originY,
    void* metalCommandQueue);

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
