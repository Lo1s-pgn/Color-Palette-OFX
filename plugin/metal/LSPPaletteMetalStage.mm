#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstring>

#include "LSPPaletteMetalStage.h"

#include "LSPPaletteAnalysis.h"
#include "LSPPaletteMetal.h"
#include "LSPPaletteRenderCache.h"

namespace {

bool copyBufferRegionToCpu(id<MTLDevice> device,
    id<MTLCommandQueue> hostQueue,
    id<MTLBuffer> srcBuffer,
    size_t srcOffsetBytes,
    size_t srcRowBytes,
    int width,
    int height,
    std::vector<float>& outPacked) {
    if (device == nil || srcBuffer == nil || width <= 0 || height <= 0)
        return false;
    const size_t packedRowBytes = static_cast<size_t>(width) * 4u * sizeof(float);
    const size_t packedBytes = packedRowBytes * static_cast<size_t>(height);
    const size_t requiredSrcBytes = srcRowBytes * static_cast<size_t>(height);
    if (srcBuffer.length < srcOffsetBytes + requiredSrcBytes)
        return false;

    outPacked.resize(packedBytes / sizeof(float));
    float* dst = outPacked.data();

    const bool hostVisible = (srcBuffer.storageMode == MTLStorageModeShared
        || srcBuffer.storageMode == MTLStorageModeManaged) && srcBuffer.contents != nullptr;

    id<MTLBuffer> staging = [device newBufferWithLength:packedBytes options:MTLResourceStorageModeShared];
    if (staging == nil)
        return false;

    if (hostVisible && srcRowBytes == packedRowBytes) {
        std::memcpy(dst, static_cast<const char*>(srcBuffer.contents) + srcOffsetBytes, packedBytes);
        return true;
    }

    if (hostVisible) {
        const char* srcBase = static_cast<const char*>(srcBuffer.contents) + srcOffsetBytes;
        char* dstBase = static_cast<char*>(staging.contents);
        for (int y = 0; y < height; ++y) {
            std::memcpy(dstBase + static_cast<size_t>(y) * packedRowBytes, srcBase + static_cast<size_t>(y) * srcRowBytes, packedRowBytes);
        }
        std::memcpy(dst, staging.contents, packedBytes);
        return true;
    }

    id<MTLCommandQueue> queue = hostQueue != nil ? hostQueue : [device newCommandQueue];
    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    if (blit == nil)
        return false;

    for (int y = 0; y < height; ++y) {
        const NSUInteger srcOff = static_cast<NSUInteger>(srcOffsetBytes + static_cast<size_t>(y) * srcRowBytes);
        const NSUInteger dstOff = static_cast<NSUInteger>(static_cast<size_t>(y) * packedRowBytes);
        [blit copyFromBuffer:srcBuffer
                  sourceOffset:srcOff
                      toBuffer:staging
             destinationOffset:dstOff
                          size:static_cast<NSUInteger>(packedRowBytes)];
    }
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    if (cmd.status != MTLCommandBufferStatusCompleted)
        return false;

    std::memcpy(dst, staging.contents, packedBytes);
    return true;
}

bool copyBufferRegionFromCpu(id<MTLDevice> device,
    id<MTLCommandQueue> hostQueue,
    id<MTLBuffer> dstBuffer,
    size_t dstOffsetBytes,
    size_t dstRowBytes,
    int width,
    int height,
    const float* packedSrc) {
    if (device == nil || dstBuffer == nil || packedSrc == nullptr || width <= 0 || height <= 0)
        return false;
    const size_t packedRowBytes = static_cast<size_t>(width) * 4u * sizeof(float);
    const size_t packedBytes = packedRowBytes * static_cast<size_t>(height);
    const size_t requiredDstBytes = dstRowBytes * static_cast<size_t>(height);
    if (dstBuffer.length < dstOffsetBytes + requiredDstBytes)
        return false;

    id<MTLBuffer> staging = [device newBufferWithLength:packedBytes options:MTLResourceStorageModeShared];
    if (staging == nil)
        return false;
    std::memcpy(staging.contents, packedSrc, packedBytes);

    const bool hostVisible = (dstBuffer.storageMode == MTLStorageModeShared
        || dstBuffer.storageMode == MTLStorageModeManaged) && dstBuffer.contents != nullptr;

    if (hostVisible && dstRowBytes == packedRowBytes) {
        std::memcpy(static_cast<char*>(dstBuffer.contents) + dstOffsetBytes, staging.contents, packedBytes);
        return true;
    }

    if (hostVisible) {
        char* dstBase = static_cast<char*>(dstBuffer.contents) + dstOffsetBytes;
        const char* srcBase = static_cast<const char*>(staging.contents);
        for (int y = 0; y < height; ++y) {
            std::memcpy(dstBase + static_cast<size_t>(y) * dstRowBytes, srcBase + static_cast<size_t>(y) * packedRowBytes, packedRowBytes);
        }
        return true;
    }

    id<MTLCommandQueue> queue = hostQueue != nil ? hostQueue : [device newCommandQueue];
    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    if (blit == nil)
        return false;
    for (int y = 0; y < height; ++y) {
        const NSUInteger srcOff = static_cast<NSUInteger>(static_cast<size_t>(y) * packedRowBytes);
        const NSUInteger dstOff = static_cast<NSUInteger>(dstOffsetBytes + static_cast<size_t>(y) * dstRowBytes);
        [blit copyFromBuffer:staging
                  sourceOffset:srcOff
                      toBuffer:dstBuffer
             destinationOffset:dstOff
                          size:static_cast<NSUInteger>(packedRowBytes)];
    }
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    return cmd.status == MTLCommandBufferStatusCompleted;
}

} // namespace

namespace LSPPaletteMetalStage {

bool copyHostMetalImageToCpu(const void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t srcRowBytes,
    std::vector<float>& outPackedRgba) {
    if (metalBuffer == nullptr)
        return false;
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    if (width <= 0 || height <= 0)
        return false;

    id<MTLBuffer> srcBuffer = (id<MTLBuffer>)metalBuffer;
    if (srcBuffer == nil)
        return false;

    id<MTLDevice> device = nil;
    if (metalCommandQueue != nullptr) {
        id<MTLCommandQueue> q = (id<MTLCommandQueue>)metalCommandQueue;
        device = q.device;
    }
    if (device == nil)
        device = srcBuffer.device;
    if (device == nil)
        device = MTLCreateSystemDefaultDevice();
    if (device == nil)
        return false;

    const size_t packedRowBytes = static_cast<size_t>(width) * 4u * sizeof(float);
    if (srcRowBytes == 0)
        srcRowBytes = packedRowBytes;
    if (srcRowBytes < packedRowBytes)
        return false;

    const size_t pixelBytes = 4u * sizeof(float);
    const size_t originOffset =
        static_cast<size_t>(bounds.y1) * srcRowBytes + static_cast<size_t>(bounds.x1) * pixelBytes;
    const size_t requiredSrcBytes = srcRowBytes * static_cast<size_t>(height);

    id<MTLCommandQueue> hostQ = nil;
    if (metalCommandQueue != nullptr)
        hostQ = (id<MTLCommandQueue>)metalCommandQueue;

    if (srcBuffer.length >= originOffset + requiredSrcBytes
        && copyBufferRegionToCpu(device, hostQ, srcBuffer, originOffset, srcRowBytes, width, height, outPackedRgba))
        return true;

    if (srcBuffer.length >= requiredSrcBytes
        && copyBufferRegionToCpu(device, hostQ, srcBuffer, 0, srcRowBytes, width, height, outPackedRgba))
        return true;

    return false;
}

bool copyCpuToHostMetalImage(void* metalBuffer,
    void* metalCommandQueue,
    const OfxRectI& bounds,
    size_t dstRowBytes,
    const float* packedRgba,
    std::size_t packedFloatCount) {
    if (metalBuffer == nullptr || packedRgba == nullptr)
        return false;
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    if (width <= 0 || height <= 0)
        return false;
    const std::size_t needFloats = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    if (packedFloatCount < needFloats)
        return false;

    id<MTLBuffer> dstBuffer = (id<MTLBuffer>)metalBuffer;
    if (dstBuffer == nil)
        return false;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> hostQ = nil;
    if (metalCommandQueue != nullptr) {
        hostQ = (id<MTLCommandQueue>)metalCommandQueue;
        device = hostQ.device;
    }
    if (device == nil)
        device = dstBuffer.device;
    if (device == nil)
        device = MTLCreateSystemDefaultDevice();
    if (device == nil)
        return false;

    const size_t packedRowBytes = static_cast<size_t>(width) * 4u * sizeof(float);
    if (dstRowBytes == 0)
        dstRowBytes = packedRowBytes;
    if (dstRowBytes < packedRowBytes)
        return false;

    const size_t pixelBytes = 4u * sizeof(float);
    const size_t originOffset =
        static_cast<size_t>(bounds.y1) * dstRowBytes + static_cast<size_t>(bounds.x1) * pixelBytes;

    if (dstBuffer.length >= originOffset + dstRowBytes * static_cast<size_t>(height)
        && copyBufferRegionFromCpu(device, hostQ, dstBuffer, originOffset, dstRowBytes, width, height, packedRgba))
        return true;

    if (dstBuffer.length >= dstRowBytes * static_cast<size_t>(height)
        && copyBufferRegionFromCpu(device, hostQ, dstBuffer, 0, dstRowBytes, width, height, packedRgba))
        return true;

    return false;
}

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
