#import "LSPPaletteGridBlur.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

namespace LSPPaletteGridBlur {

bool tryMpsGaussianBlur(const float* src, float* dst, int nw, int nh, float sigmaPixels) {
    if (!src || !dst || nw < 2 || nh < 2)
        return false;
    float sigma = std::max(sigmaPixels, 0.05f);
    sigma = std::min(sigma, 0.45f * static_cast<float>(std::min(nw, nh)));

    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device)
            return false;

        static id<MTLCommandQueue> gQueue = nil;
        static std::mutex gQueueInit;
        {
            std::lock_guard<std::mutex> lk(gQueueInit);
            if (!gQueue)
                gQueue = [device newCommandQueue];
        }
        if (!gQueue)
            return false;

        const NSUInteger W = static_cast<NSUInteger>(nw);
        const NSUInteger H = static_cast<NSUInteger>(nh);
        const NSUInteger bytesPerRow = W * 16u;
        const NSUInteger byteCount = bytesPerRow * H;

        MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                                                        width:W
                                                                                       height:H
                                                                                    mipmapped:NO];
        td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        td.storageMode = MTLStorageModePrivate;
        id<MTLTexture> texSrc = [device newTextureWithDescriptor:td];
        id<MTLTexture> texDst = [device newTextureWithDescriptor:td];
        if (!texSrc || !texDst) {
            if (texSrc)
                [texSrc release];
            if (texDst)
                [texDst release];
            return false;
        }

        id<MTLBuffer> upload = [device newBufferWithBytes:src length:byteCount options:MTLResourceStorageModeShared];
        if (!upload) {
            [texSrc release];
            [texDst release];
            return false;
        }

        id<MTLCommandBuffer> cmdBuf = [gQueue commandBuffer];
        if (!cmdBuf) {
            [upload release];
            [texSrc release];
            [texDst release];
            return false;
        }

        id<MTLBlitCommandEncoder> blitIn = [cmdBuf blitCommandEncoder];
        if (!blitIn) {
            [upload release];
            [texSrc release];
            [texDst release];
            return false;
        }
        MTLSize fullSize = MTLSizeMake(W, H, 1);
        [blitIn copyFromBuffer:upload sourceOffset:0 sourceBytesPerRow:bytesPerRow sourceBytesPerImage:byteCount sourceSize:fullSize
            toTexture:texSrc destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blitIn endEncoding];
        [upload release];

        MPSImageGaussianBlur* blur = [[MPSImageGaussianBlur alloc] initWithDevice:device sigma:sigma];
        if (!blur) {
            [texSrc release];
            [texDst release];
            return false;
        }
        [blur encodeToCommandBuffer:cmdBuf sourceTexture:texSrc destinationTexture:texDst];
        [blur release];

        id<MTLBuffer> readBuf = [device newBufferWithLength:byteCount options:MTLResourceStorageModeShared];
        if (!readBuf) {
            [texSrc release];
            [texDst release];
            return false;
        }

        id<MTLBlitCommandEncoder> blitOut = [cmdBuf blitCommandEncoder];
        if (!blitOut) {
            [readBuf release];
            [texSrc release];
            [texDst release];
            return false;
        }
        [blitOut copyFromTexture:texDst sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:fullSize
            toBuffer:readBuf destinationOffset:0 destinationBytesPerRow:bytesPerRow destinationBytesPerImage:byteCount];
        [blitOut endEncoding];

        [texSrc release];
        [texDst release];

        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
        if (cmdBuf.error)
            return false;

        const void* mapped = [readBuf contents];
        if (!mapped) {
            [readBuf release];
            return false;
        }
        std::memcpy(dst, mapped, static_cast<std::size_t>(byteCount));
        [readBuf release];
        return true;
    }
}

} // namespace LSPPaletteGridBlur
