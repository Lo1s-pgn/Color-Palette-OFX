#include "LSPPaletteRenderProcessor.h"

#include "LSPPaletteRuntimeEnv.h"

#include <cstring>
#include <vector>

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
#include "LSPPaletteMetal.h"
#endif

#if defined(LSP_PALETTE_HAS_CUDA)
#include <cuda_runtime.h>
extern "C" bool launchLSPPaletteKernel(
    const float* src, float* dst, int width, int height, const LSPPaletteGpuParams* params, void* cudaStream);
#endif

#if defined(LSP_PALETTE_HAS_OPENCL)
#include <CL/cl.h>
#include "LSPPaletteCLSource.h"
#endif

bool LSPPaletteRenderProcessor::renderWithLayout(const float* src,
    float* dst,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    bool preferCuda) {
#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
    if (renderMetal(src, dst, width, height, srcRowBytes, dstRowBytes)) {
        LSPPaletteRuntimeEnv::logGpuBackend("metal_internal");
        return true;
    }
#endif
#if defined(LSP_PALETTE_HAS_CUDA)
    const bool forceOcl = LSPPaletteRuntimeEnv::openclForceEnabled();
    if (!forceOcl && preferCuda && renderCUDA(src, dst, width, height, srcRowBytes, dstRowBytes)) {
        LSPPaletteRuntimeEnv::logGpuBackend("cuda_internal");
        return true;
    }
#endif
#if defined(LSP_PALETTE_HAS_OPENCL)
    if (!LSPPaletteRuntimeEnv::openclDisableEnabled() && renderOpenCL(src, dst, width, height, srcRowBytes, dstRowBytes)) {
        LSPPaletteRuntimeEnv::logGpuBackend("opencl");
        return true;
    }
#endif
    (void)src;
    (void)dst;
    (void)width;
    (void)height;
    (void)srcRowBytes;
    (void)dstRowBytes;
    (void)preferCuda;
    return false;
}

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
bool LSPPaletteRenderProcessor::renderMetal(
    const float* src, float* dst, int width, int height, size_t srcRowBytes, size_t dstRowBytes) {
    return LSPPaletteMetal::render(src, dst, width, height, srcRowBytes, dstRowBytes, params_);
}

bool LSPPaletteRenderProcessor::renderMetalHostBuffers(const void* srcMetal,
    void* dstMetal,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    void* metalCommandQueue) {
    return LSPPaletteMetal::renderHost(
        srcMetal, dstMetal, width, height, srcRowBytes, dstRowBytes, params_.originX, params_.originY, params_, metalCommandQueue);
}
#endif

#if defined(LSP_PALETTE_HAS_CUDA)
bool LSPPaletteRenderProcessor::renderCUDA(
    const float* src, float* dst, int width, int height, size_t srcRowBytes, size_t dstRowBytes) {
    const size_t packed = static_cast<size_t>(width) * 4u * sizeof(float);
    const size_t bytes = packed * static_cast<size_t>(height);
    float* dSrc = nullptr;
    float* dDst = nullptr;
    if (cudaMalloc(&dSrc, bytes) != cudaSuccess || cudaMalloc(&dDst, bytes) != cudaSuccess) {
        if (dSrc)
            cudaFree(dSrc);
        return false;
    }
    if (srcRowBytes == packed) {
        cudaMemcpy(dSrc, src, bytes, cudaMemcpyHostToDevice);
    } else {
        for (int y = 0; y < height; ++y)
            cudaMemcpy(dSrc + static_cast<size_t>(y) * width * 4, reinterpret_cast<const char*>(src) + y * srcRowBytes, packed,
                cudaMemcpyHostToDevice);
    }
    LSPPaletteGpuParams kernelParams = params_;
    const int packedRowFloats = width * 4;
    kernelParams.srcRowFloats = packedRowFloats;
    kernelParams.dstRowFloats = packedRowFloats;
    kernelParams.srcReadOriginX = params_.srcBoundsX1;
    kernelParams.srcReadOriginY = params_.srcBoundsY1;
    const bool ok = launchLSPPaletteKernel(dSrc, dDst, width, height, &kernelParams, nullptr);
    if (ok) {
        if (dstRowBytes == packed)
            cudaMemcpy(dst, dDst, bytes, cudaMemcpyDeviceToHost);
        else {
            for (int y = 0; y < height; ++y)
                cudaMemcpy(reinterpret_cast<char*>(dst) + y * dstRowBytes, dDst + static_cast<size_t>(y) * width * 4, packed,
                    cudaMemcpyDeviceToHost);
        }
    }
    cudaFree(dSrc);
    cudaFree(dDst);
    return ok;
}

bool LSPPaletteRenderProcessor::renderCUDAHostBuffers(const float* srcDevice,
    float* dstDevice,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    void* cudaStream) {
    (void)srcRowBytes;
    (void)dstRowBytes;
    return launchLSPPaletteKernel(srcDevice, dstDevice, width, height, &params_, cudaStream);
}
#endif

#if defined(LSP_PALETTE_HAS_OPENCL)
void LSPPaletteRenderProcessor::releaseOpenCL() {
    if (clSrc_)
        clReleaseMemObject(static_cast<cl_mem>(clSrc_));
    if (clDst_)
        clReleaseMemObject(static_cast<cl_mem>(clDst_));
    if (clParamsBuf_)
        clReleaseMemObject(static_cast<cl_mem>(clParamsBuf_));
    if (clKernel_)
        clReleaseKernel(static_cast<cl_kernel>(clKernel_));
    if (clProgram_)
        clReleaseProgram(static_cast<cl_program>(clProgram_));
    if (clQueue_)
        clReleaseCommandQueue(static_cast<cl_command_queue>(clQueue_));
    if (clContext_)
        clReleaseContext(static_cast<cl_context>(clContext_));
    clSrc_ = clDst_ = clParamsBuf_ = clKernel_ = clProgram_ = clQueue_ = clContext_ = nullptr;
    clBufBytes_ = 0;
}

bool LSPPaletteRenderProcessor::initializeOpenCL() {
    if (clInitFailed_)
        return false;
    if (clKernel_ != nullptr)
        return true;
    cl_uint numPlatforms = 0;
    if (clGetPlatformIDs(0, nullptr, &numPlatforms) != CL_SUCCESS || numPlatforms == 0) {
        clInitFailed_ = true;
        return false;
    }
    std::vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs(numPlatforms, platforms.data(), nullptr);
    cl_device_id device = nullptr;
    cl_platform_id platform = nullptr;
    for (cl_platform_id pid : platforms) {
        cl_uint nd = 0;
        if (clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, 1, &device, &nd) == CL_SUCCESS && nd > 0) {
            platform = pid;
            break;
        }
    }
    if (device == nullptr) {
        clInitFailed_ = true;
        return false;
    }
    cl_int err = CL_SUCCESS;
    clContext_ = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS)
        return false;
    clQueue_ = clCreateCommandQueue(static_cast<cl_context>(clContext_), device, 0, &err);
    if (err != CL_SUCCESS) {
        releaseOpenCL();
        clInitFailed_ = true;
        return false;
    }
    const char* src = kLSPPaletteCLSource;
    const size_t len = kLSPPaletteCLSourceSize;
    clProgram_ = clCreateProgramWithSource(static_cast<cl_context>(clContext_), 1, &src, &len, &err);
    if (err != CL_SUCCESS || clBuildProgram(static_cast<cl_program>(clProgram_), 1, &device, nullptr, nullptr, nullptr) != CL_SUCCESS) {
        releaseOpenCL();
        clInitFailed_ = true;
        return false;
    }
    clKernel_ = clCreateKernel(static_cast<cl_program>(clProgram_), "LSPPaletteKernel", &err);
    if (err != CL_SUCCESS) {
        releaseOpenCL();
        clInitFailed_ = true;
        return false;
    }
    return true;
}

bool LSPPaletteRenderProcessor::ensureOpenCLBuffers(size_t bytes) {
    if (clBufBytes_ >= bytes && clSrc_ != nullptr && clDst_ != nullptr && clParamsBuf_ != nullptr)
        return true;
    if (clSrc_)
        clReleaseMemObject(static_cast<cl_mem>(clSrc_));
    if (clDst_)
        clReleaseMemObject(static_cast<cl_mem>(clDst_));
    if (clParamsBuf_)
        clReleaseMemObject(static_cast<cl_mem>(clParamsBuf_));
    clSrc_ = clDst_ = clParamsBuf_ = nullptr;
    clBufBytes_ = 0;
    if (!initializeOpenCL())
        return false;
    cl_int err = CL_SUCCESS;
    clSrc_ = clCreateBuffer(static_cast<cl_context>(clContext_), CL_MEM_READ_WRITE, bytes, nullptr, &err);
    clDst_ = clCreateBuffer(static_cast<cl_context>(clContext_), CL_MEM_READ_WRITE, bytes, nullptr, &err);
    clParamsBuf_ =
        clCreateBuffer(static_cast<cl_context>(clContext_), CL_MEM_READ_ONLY, sizeof(LSPPaletteGpuParams), nullptr, &err);
    if (err != CL_SUCCESS || clSrc_ == nullptr || clDst_ == nullptr || clParamsBuf_ == nullptr)
        return false;
    clBufBytes_ = bytes;
    return true;
}

bool LSPPaletteRenderProcessor::renderOpenCL(
    const float* src, float* dst, int width, int height, size_t srcRowBytes, size_t dstRowBytes) {
    std::lock_guard<std::mutex> lock(openclMutex_);
    if (!initializeOpenCL())
        return false;
    const size_t packed = static_cast<size_t>(width) * 4u * sizeof(float);
    const size_t bytes = packed * static_cast<size_t>(height);
    if (!ensureOpenCLBuffers(bytes))
        return false;
    cl_command_queue q = static_cast<cl_command_queue>(clQueue_);
    cl_mem clSrc = static_cast<cl_mem>(clSrc_);
    cl_mem clDst = static_cast<cl_mem>(clDst_);
    cl_mem clPar = static_cast<cl_mem>(clParamsBuf_);
    if (srcRowBytes != packed)
        return false;
    if (clEnqueueWriteBuffer(q, clSrc, CL_TRUE, 0, bytes, src, 0, nullptr, nullptr) != CL_SUCCESS)
        return false;
    if (clSetKernelArg(static_cast<cl_kernel>(clKernel_), 0, sizeof(cl_mem), &clSrc) != CL_SUCCESS)
        return false;
    if (clSetKernelArg(static_cast<cl_kernel>(clKernel_), 1, sizeof(cl_mem), &clDst) != CL_SUCCESS)
        return false;
    if (clEnqueueWriteBuffer(q, clPar, CL_TRUE, 0, sizeof(LSPPaletteGpuParams), &params_, 0, nullptr, nullptr) != CL_SUCCESS)
        return false;
    if (clSetKernelArg(static_cast<cl_kernel>(clKernel_), 2, sizeof(cl_mem), &clPar) != CL_SUCCESS)
        return false;
    const size_t global[2] = {static_cast<size_t>(width), static_cast<size_t>(height)};
    if (clEnqueueNDRangeKernel(q, static_cast<cl_kernel>(clKernel_), 2, nullptr, global, nullptr, 0, nullptr, nullptr) != CL_SUCCESS)
        return false;
    if (dstRowBytes != packed)
        return false;
    if (clEnqueueReadBuffer(q, clDst, CL_TRUE, 0, bytes, dst, 0, nullptr, nullptr) != CL_SUCCESS)
        return false;
    return true;
}
#endif
