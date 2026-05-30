#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <dlfcn.h>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

#include "LSPPaletteMetal.h"

#include "LSPPaletteAnalysis.h"

namespace {

struct MetalCtx {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLComputePipelineState> pipeline = nil;
    id<MTLComputePipelineState> downsamplePipeline = nil;
    id<MTLComputePipelineState> gatherPipeline = nil;
    std::mutex initMutex;
    bool initialized = false;
};

MetalCtx& ctx() {
    static MetalCtx c;
    return c;
}

std::string moduleDirectory() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&ctx), &info) == 0 || info.dli_fname == nullptr)
        return std::string();
    return std::filesystem::path(info.dli_fname).parent_path().string();
}

std::string metallibPath() {
    const std::filesystem::path macosDir(moduleDirectory());
    if (macosDir.empty())
        return std::string();
    return (macosDir.parent_path() / "Resources" / "LSPPalette.metallib").string();
}

bool initializePipeline(id<MTLDevice> device, id<MTLCommandQueue> queue) {
    auto& c = ctx();
    if (c.initialized && c.pipeline != nil)
        return true;
    if (device == nil || queue == nil)
        return false;
    c.device = device;
    c.queue = queue;
    const std::string libPathStr = metallibPath();
    if (libPathStr.empty())
        return false;
    NSString* libPath = [NSString stringWithUTF8String:libPathStr.c_str()];
    NSError* err = nil;
    id<MTLLibrary> lib = [device newLibraryWithURL:[NSURL fileURLWithPath:libPath] error:&err];
    if (lib == nil)
        return false;
    id<MTLFunction> fn = [lib newFunctionWithName:@"LSPPaletteCompositeKernel"];
    if (fn == nil)
        return false;
    c.pipeline = [device newComputePipelineStateWithFunction:fn error:&err];
    id<MTLFunction> dsFn = [lib newFunctionWithName:@"LSPPaletteDownsampleKernel"];
    if (dsFn != nil)
        c.downsamplePipeline = [device newComputePipelineStateWithFunction:dsFn error:&err];
    id<MTLFunction> gatherFn = [lib newFunctionWithName:@"LSPPaletteGatherSamplesKernel"];
    if (gatherFn != nil)
        c.gatherPipeline = [device newComputePipelineStateWithFunction:gatherFn error:&err];
    c.initialized = (c.pipeline != nil && c.downsamplePipeline != nil && c.gatherPipeline != nil);
    return c.initialized;
}

bool dispatchKernel(id<MTLCommandBuffer> cmd,
    id<MTLBuffer> srcBuf,
    NSUInteger srcOffset,
    id<MTLBuffer> dstBuf,
    NSUInteger dstOffset,
    const LSPPaletteGpuParams& params) {
    auto& c = ctx();
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    if (enc == nil)
        return false;
    [enc setComputePipelineState:c.pipeline];
    [enc setBuffer:srcBuf offset:srcOffset atIndex:0];
    [enc setBuffer:dstBuf offset:dstOffset atIndex:1];
    [enc setBytes:&params length:sizeof(LSPPaletteGpuParams) atIndex:2];
    const NSUInteger tw = c.pipeline.threadExecutionWidth > 0 ? c.pipeline.threadExecutionWidth : 16;
    const NSUInteger maxT = c.pipeline.maxTotalThreadsPerThreadgroup;
    NSUInteger ty = maxT / tw;
    if (ty < 1)
        ty = 1;
    if (ty > 16)
        ty = 16;
    const MTLSize tpg = MTLSizeMake(tw, ty, 1);
    const MTLSize grid = MTLSizeMake(static_cast<NSUInteger>(params.width), static_cast<NSUInteger>(params.height), 1);
    [enc dispatchThreads:grid threadsPerThreadgroup:tpg];
    [enc endEncoding];
    return true;
}

} // namespace

namespace LSPPaletteMetal {

bool render(const float* src,
    float* dst,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    const LSPPaletteGpuParams& params) {
    if (src == nullptr || dst == nullptr || width <= 0 || height <= 0)
        return false;
    auto& c = ctx();
    std::lock_guard<std::mutex> lock(c.initMutex);
    if (!c.initialized) {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (dev == nil)
            return false;
        id<MTLCommandQueue> q = [dev newCommandQueue];
        if (!initializePipeline(dev, q))
            return false;
    }
    const size_t packedRow = static_cast<size_t>(width) * 4u * sizeof(float);
    const size_t bytes = packedRow * static_cast<size_t>(height);
    id<MTLBuffer> srcBuf = [c.device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    id<MTLBuffer> dstBuf = [c.device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    if (srcBuf == nil || dstBuf == nil)
        return false;
    if (srcRowBytes == packedRow) {
        std::memcpy(srcBuf.contents, src, bytes);
    } else {
        float* dstS = static_cast<float*>(srcBuf.contents);
        const char* sb = reinterpret_cast<const char*>(src);
        for (int y = 0; y < height; ++y)
            std::memcpy(dstS + static_cast<size_t>(y) * width * 4, sb + y * srcRowBytes, packedRow);
    }
    LSPPaletteGpuParams kernelParams = params;
    const int packedRowFloats = width * 4;
    kernelParams.srcRowFloats = packedRowFloats;
    kernelParams.dstRowFloats = packedRowFloats;
    kernelParams.srcReadOriginX = params.srcBoundsX1;
    kernelParams.srcReadOriginY = params.srcBoundsY1;

    id<MTLCommandBuffer> cmd = [c.queue commandBuffer];
    if (!dispatchKernel(cmd, srcBuf, 0, dstBuf, 0, kernelParams))
        return false;
    [cmd commit];
    [cmd waitUntilCompleted];
    if (cmd.status != MTLCommandBufferStatusCompleted)
        return false;
    if (dstRowBytes == packedRow) {
        std::memcpy(dst, dstBuf.contents, bytes);
    } else {
        const char* sb = static_cast<const char*>(dstBuf.contents);
        char* db = reinterpret_cast<char*>(dst);
        for (int y = 0; y < height; ++y)
            std::memcpy(db + y * dstRowBytes, sb + y * packedRow, packedRow);
    }
    return true;
}

bool renderHost(const void* srcMetalBuffer,
    void* dstMetalBuffer,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    int originX,
    int originY,
    const LSPPaletteGpuParams& params,
    void* metalCommandQueue) {
    if (srcMetalBuffer == nullptr || dstMetalBuffer == nullptr || metalCommandQueue == nullptr || width <= 0 || height <= 0)
        return false;

    id<MTLCommandQueue> hostQ = (id<MTLCommandQueue>)metalCommandQueue;
    if (hostQ == nil)
        return false;
    auto& c = ctx();
    std::lock_guard<std::mutex> lock(c.initMutex);
    if (!initializePipeline(hostQ.device, hostQ))
        return false;

    id<MTLBuffer> srcBuffer = (id<MTLBuffer>)srcMetalBuffer;
    id<MTLBuffer> dstBuffer = (id<MTLBuffer>)dstMetalBuffer;
    if (srcBuffer == nil || dstBuffer == nil)
        return false;

    const size_t packedRowBytes = static_cast<size_t>(width) * 4u * sizeof(float);
    if (srcRowBytes == 0)
        srcRowBytes = packedRowBytes;
    if (dstRowBytes == 0)
        dstRowBytes = packedRowBytes;
    if (srcRowBytes < packedRowBytes || dstRowBytes < packedRowBytes)
        return false;
    if ((srcRowBytes % sizeof(float)) != 0 || (dstRowBytes % sizeof(float)) != 0)
        return false;

    const size_t requiredSrcBytes = srcRowBytes * static_cast<size_t>(height);
    const size_t requiredDstBytes = dstRowBytes * static_cast<size_t>(height);
    const size_t pixelBytes = 4u * sizeof(float);
    const bool hasOriginOffset = (originX > 0 || originY > 0);
    size_t srcOffsetBytes = 0;
    size_t dstOffsetBytes = 0;

    const size_t srcCandidateOffset =
        static_cast<size_t>(originY) * srcRowBytes + static_cast<size_t>(originX) * pixelBytes;
    const size_t dstCandidateOffset =
        static_cast<size_t>(originY) * dstRowBytes + static_cast<size_t>(originX) * pixelBytes;
    const bool canUseZero = (srcBuffer.length >= requiredSrcBytes && dstBuffer.length >= requiredDstBytes);
    const bool canUseOffset = hasOriginOffset && srcBuffer.length >= srcCandidateOffset + requiredSrcBytes
        && dstBuffer.length >= dstCandidateOffset + requiredDstBytes;

    if (canUseOffset) {
        srcOffsetBytes = srcCandidateOffset;
        dstOffsetBytes = dstCandidateOffset;
    } else if (!canUseZero) {
        return false;
    }

    id<MTLBuffer> kernelSrc = srcBuffer;
    id<MTLBuffer> kernelDst = dstBuffer;
    NSUInteger kernelSrcOff = static_cast<NSUInteger>(srcOffsetBytes);
    NSUInteger kernelDstOff = static_cast<NSUInteger>(dstOffsetBytes);

    if (srcBuffer == dstBuffer) {
        const size_t srcBegin = srcOffsetBytes;
        const size_t srcEnd = srcOffsetBytes + requiredSrcBytes;
        const size_t dstBegin = dstOffsetBytes;
        const size_t dstEnd = dstOffsetBytes + requiredDstBytes;
        const bool overlap = (srcBegin < dstEnd) && (dstBegin < srcEnd);
        if (overlap && (srcOffsetBytes != dstOffsetBytes || srcRowBytes != dstRowBytes)) {
            id<MTLBuffer> temp = [c.device newBufferWithLength:requiredSrcBytes options:MTLResourceStorageModeShared];
            if (temp == nil)
                return false;
            id<MTLCommandBuffer> preCmd = [hostQ commandBuffer];
            id<MTLBlitCommandEncoder> blit = [preCmd blitCommandEncoder];
            if (blit == nil)
                return false;
            [blit copyFromBuffer:srcBuffer
                      sourceOffset:static_cast<NSUInteger>(srcOffsetBytes)
                          toBuffer:temp
                 destinationOffset:0
                              size:requiredSrcBytes];
            [blit endEncoding];
            [preCmd commit];
            [preCmd waitUntilCompleted];
            if (preCmd.status != MTLCommandBufferStatusCompleted)
                return false;
            kernelSrc = temp;
            kernelSrcOff = 0;
        }
    }

    LSPPaletteGpuParams kernelParams = params;
    kernelParams.srcReadOriginX = originX;
    kernelParams.srcReadOriginY = originY;

    id<MTLCommandBuffer> cmd = [hostQ commandBuffer];
    if (cmd == nil)
        return false;
    if (!dispatchKernel(cmd, kernelSrc, kernelSrcOff, kernelDst, kernelDstOff, kernelParams))
        return false;
    [cmd commit];
    [cmd waitUntilCompleted];
    return cmd.status == MTLCommandBufferStatusCompleted;
}

bool gatherOkLabSamplesFromSlab(const float* slabRgba,
    int slabWidth,
    int slabHeight,
    int slabRowFloats,
    int gridW,
    int gridH,
    const LSPPaletteGpuExtractGatherParams& params,
    void* metalCommandQueue,
    std::vector<LSPPaletteGpuOkLabSample>& outSamples) {
    if (slabRgba == nullptr || slabWidth <= 0 || slabHeight <= 0 || gridW <= 0 || gridH <= 0)
        return false;
    id<MTLCommandQueue> hostQ = (id<MTLCommandQueue>)metalCommandQueue;
    if (hostQ == nil)
        return false;
    auto& c = ctx();
    std::lock_guard<std::mutex> lock(c.initMutex);
    if (!initializePipeline(hostQ.device, hostQ))
        return false;

    const int gridCells = gridW * gridH;
    const size_t slabBytes = static_cast<size_t>(slabRowFloats) * sizeof(float) * static_cast<size_t>(slabHeight);
    const size_t outBytes = static_cast<size_t>(gridCells) * sizeof(LSPPaletteGpuOkLabSample);

    id<MTLBuffer> srcBuf = [c.device newBufferWithBytes:slabRgba length:slabBytes options:MTLResourceStorageModeShared];
    id<MTLBuffer> outBuf = [c.device newBufferWithLength:outBytes options:MTLResourceStorageModeShared];
    if (srcBuf == nil || outBuf == nil)
        return false;

    id<MTLCommandBuffer> cmd = [hostQ commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    if (enc == nil)
        return false;
    [enc setComputePipelineState:c.gatherPipeline];
    [enc setBuffer:srcBuf offset:0 atIndex:0];
    [enc setBuffer:outBuf offset:0 atIndex:1];
    [enc setBytes:&params length:sizeof(LSPPaletteGpuExtractGatherParams) atIndex:2];
    const NSUInteger tw = c.gatherPipeline.threadExecutionWidth > 0 ? c.gatherPipeline.threadExecutionWidth : 16;
    const NSUInteger maxT = c.gatherPipeline.maxTotalThreadsPerThreadgroup;
    NSUInteger ty = maxT / tw;
    if (ty < 1)
        ty = 1;
    if (ty > 16)
        ty = 16;
    const MTLSize tpg = MTLSizeMake(tw, ty, 1);
    const MTLSize grid = MTLSizeMake(static_cast<NSUInteger>(gridW), static_cast<NSUInteger>(gridH), 1);
    [enc dispatchThreads:grid threadsPerThreadgroup:tpg];
    [enc endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    if (cmd.status != MTLCommandBufferStatusCompleted)
        return false;

    outSamples.resize(static_cast<size_t>(gridCells));
    std::memcpy(outSamples.data(), outBuf.contents, outBytes);
    return true;
}

bool downsampleHostToCpu(const void* srcMetalBuffer,
    void* metalCommandQueue,
    const OfxRectI& srcBounds,
    size_t srcRowBytes,
    int dstW,
    int dstH,
    float* outPackedRgba,
    std::size_t outFloatCapacity) {
    if (srcMetalBuffer == nullptr || outPackedRgba == nullptr || dstW <= 0 || dstH <= 0)
        return false;
    const std::size_t needFloats = static_cast<std::size_t>(dstW) * static_cast<std::size_t>(dstH) * 4u;
    if (outFloatCapacity < needFloats)
        return false;

    id<MTLBuffer> srcBuffer = (id<MTLBuffer>)srcMetalBuffer;
    if (srcBuffer == nil)
        return false;

    id<MTLCommandQueue> hostQ = (id<MTLCommandQueue>)metalCommandQueue;
    if (hostQ == nil)
        return false;

    auto& c = ctx();
    std::lock_guard<std::mutex> lock(c.initMutex);
    if (!initializePipeline(hostQ.device, hostQ))
        return false;

    const int srcW = srcBounds.x2 - srcBounds.x1;
    const int srcH = srcBounds.y2 - srcBounds.y1;
    if (srcW <= 0 || srcH <= 0)
        return false;

    const size_t packedRowBytes = static_cast<size_t>(dstW) * 4u * sizeof(float);
    const size_t packedBytes = packedRowBytes * static_cast<size_t>(dstH);
    if (srcRowBytes == 0)
        srcRowBytes = static_cast<size_t>(srcW) * 4u * sizeof(float);

    const size_t pixelBytes = 4u * sizeof(float);
    const size_t requiredSrcBytes = srcRowBytes * static_cast<size_t>(srcH);
    size_t srcOffsetBytes =
        static_cast<size_t>(srcBounds.y1) * srcRowBytes + static_cast<size_t>(srcBounds.x1) * pixelBytes;
    if (srcBuffer.length < srcOffsetBytes + requiredSrcBytes)
        srcOffsetBytes = 0;

    id<MTLBuffer> dstBuf = [c.device newBufferWithLength:packedBytes options:MTLResourceStorageModeShared];
    if (dstBuf == nil)
        return false;

    LSPPaletteGpuDownsampleParams dp{};
    if (srcOffsetBytes == 0) {
        dp.srcReadOriginX = srcBounds.x1;
        dp.srcReadOriginY = srcBounds.y1;
    } else {
        dp.srcReadOriginX = 0;
        dp.srcReadOriginY = 0;
    }
    dp.srcRowFloats = static_cast<int>(srcRowBytes / sizeof(float));
    dp.srcW = srcW;
    dp.srcH = srcH;
    dp.dstW = dstW;
    dp.dstH = dstH;

    id<MTLCommandBuffer> cmd = [hostQ commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    if (enc == nil)
        return false;
    [enc setComputePipelineState:c.downsamplePipeline];
    [enc setBuffer:srcBuffer offset:static_cast<NSUInteger>(srcOffsetBytes) atIndex:0];
    [enc setBuffer:dstBuf offset:0 atIndex:1];
    [enc setBytes:&dp length:sizeof(LSPPaletteGpuDownsampleParams) atIndex:2];
    const NSUInteger tw = c.downsamplePipeline.threadExecutionWidth > 0 ? c.downsamplePipeline.threadExecutionWidth : 16;
    const NSUInteger maxT = c.downsamplePipeline.maxTotalThreadsPerThreadgroup;
    NSUInteger ty = maxT / tw;
    if (ty < 1)
        ty = 1;
    if (ty > 16)
        ty = 16;
    const MTLSize tpg = MTLSizeMake(tw, ty, 1);
    const MTLSize grid = MTLSizeMake(static_cast<NSUInteger>(dstW), static_cast<NSUInteger>(dstH), 1);
    [enc dispatchThreads:grid threadsPerThreadgroup:tpg];
    [enc endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    if (cmd.status != MTLCommandBufferStatusCompleted)
        return false;

    std::memcpy(outPackedRgba, dstBuf.contents, packedBytes);
    return true;
}

} // namespace LSPPaletteMetal
