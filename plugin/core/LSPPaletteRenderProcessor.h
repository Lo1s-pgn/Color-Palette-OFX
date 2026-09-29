#pragma once

#include <cstddef>
#include <mutex>

#include "LSPPaletteGpuParams.h"

class LSPPaletteRenderProcessor {
public:
    void setGpuParams(const LSPPaletteGpuParams& params) { params_ = params; }

    bool renderWithLayout(const float* src,
        float* dst,
        int width,
        int height,
        size_t srcRowBytes,
        size_t dstRowBytes,
        bool preferCuda);

#if defined(__APPLE__)
    bool renderMetal(const float* src, float* dst, int width, int height, size_t srcRowBytes, size_t dstRowBytes);
    bool renderMetalHostBuffers(const void* srcMetal,
        void* dstMetal,
        int width,
        int height,
        size_t srcRowBytes,
        size_t dstRowBytes,
        void* metalCommandQueue);
#endif

#if defined(LSP_PALETTE_HAS_CUDA)
    bool renderCUDA(const float* src, float* dst, int width, int height, size_t srcRowBytes, size_t dstRowBytes);
    bool renderCUDAHostBuffers(
        const float* srcDevice, float* dstDevice, int width, int height, size_t srcRowBytes, size_t dstRowBytes, void* cudaStream);
#endif

#if defined(LSP_PALETTE_HAS_OPENCL)
    bool renderOpenCL(const float* src, float* dst, int width, int height, size_t srcRowBytes, size_t dstRowBytes);
#endif

private:
    LSPPaletteGpuParams params_{};

#if defined(LSP_PALETTE_HAS_OPENCL)
    bool initializeOpenCL();
    void releaseOpenCL();
    bool ensureOpenCLBuffers(size_t bytes);

    std::mutex openclMutex_;
    bool clInitFailed_ = false;
    bool clAvailKnown_ = false;
    bool clAvail_ = false;
    void* clContext_ = nullptr;
    void* clQueue_ = nullptr;
    void* clProgram_ = nullptr;
    void* clKernel_ = nullptr;
    void* clSrc_ = nullptr;
    void* clDst_ = nullptr;
    void* clParamsBuf_ = nullptr;
    size_t clBufBytes_ = 0;
#endif
};
