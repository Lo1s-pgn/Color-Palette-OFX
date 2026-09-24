#include <cuda_runtime.h>

#include "LSPPaletteGpuParamsShared.h"

namespace {

__device__ float4 readPx(const float* src, const LSPPaletteGpuParams& p, int absX, int absY) {
    const int lx = absX - p.srcReadOriginX;
    const int ly = absY - p.srcReadOriginY;
    const int idx = ly * p.srcRowFloats + lx * 4;
    return make_float4(src[idx], src[idx + 1], src[idx + 2], src[idx + 3]);
}

__device__ float4 sampleBilinear(const float* src, const LSPPaletteGpuParams& p, float xf, float yf) {
    const int xMin = p.srcBoundsX1;
    const int yMin = p.srcBoundsY1;
    const int xMax = p.srcBoundsX2 - 1;
    const int yMax = p.srcBoundsY2 - 1;
    if (xMax < xMin || yMax < yMin)
        return make_float4(0.0f, 0.0f, 0.0f, 1.0f);
    xf = fminf(fmaxf(xf, static_cast<float>(xMin)), static_cast<float>(xMax));
    yf = fminf(fmaxf(yf, static_cast<float>(yMin)), static_cast<float>(yMax));
    const int x0 = static_cast<int>(floorf(xf));
    const int y0 = static_cast<int>(floorf(yf));
    const int xA = min(x0, xMax);
    const int yA = min(y0, yMax);
    const int xB = min(xA + 1, xMax);
    const int yB = min(yA + 1, yMax);
    const float tx = xf - static_cast<float>(xA);
    const float ty = yf - static_cast<float>(yA);
    const float4 c00 = readPx(src, p, xA, yA);
    const float4 c10 = readPx(src, p, xB, yA);
    const float4 c01 = readPx(src, p, xA, yB);
    const float4 c11 = readPx(src, p, xB, yB);
    return (1.0f - tx) * (1.0f - ty) * c00 + tx * (1.0f - ty) * c10 + (1.0f - tx) * ty * c01 + tx * ty * c11;
}

__device__ float sdfRoundRect(float px, float py, float bx0, float by0, float bx1, float by1, float rad) {
    const float cx = (bx0 + bx1) * 0.5f;
    const float cy = (by0 + by1) * 0.5f;
    const float hx = (bx1 - bx0) * 0.5f - rad;
    const float hy = (by1 - by0) * 0.5f - rad;
    const float qx = fabsf(px - cx) - hx;
    const float qy = fabsf(py - cy) - hy;
    const float ext = sqrtf(fmaxf(qx, 0.0f) * fmaxf(qx, 0.0f) + fmaxf(qy, 0.0f) * fmaxf(qy, 0.0f));
    const float interior = fminf(fmaxf(qx, qy), 0.0f);
    return ext + interior - rad;
}

__device__ float4 applySwatches(float4 rgb, float px, float py, const LSPPaletteGpuParams& p) {
    const float aa = 0.6f;
    for (int i = 0; i < p.swatchCount; ++i) {
        const LSPPaletteGpuSwatch s = p.swatches[i];
        float rad = s.rad;
        const float bw = s.bx1 - s.bx0;
        const float bh = s.by1 - s.by0;
        rad = fmaxf(0.0f, fminf(rad, fminf(bw, bh) * 0.5f - 0.25f));
        const float d = sdfRoundRect(px, py, s.bx0, s.by0, s.bx1, s.by1, rad);
        const float cov = fminf(fmaxf(0.5f - d / (2.0f * aa), 0.0f), 1.0f);
        if (cov <= 1.0e-4f)
            continue;
        const float3 sw = make_float3(s.r, s.g, s.b);
        if (cov >= 1.0f - 1.0e-4f)
            rgb = make_float4(sw.x, sw.y, sw.z, 1.0f);
        else {
            const float om = 1.0f - cov;
            rgb.x = rgb.x * om + sw.x * cov;
            rgb.y = rgb.y * om + sw.y * cov;
            rgb.z = rgb.z * om + sw.z * cov;
            rgb.w = 1.0f;
        }
    }
    return rgb;
}

__global__ void LSPPaletteKernel(const float* src, float* dst, LSPPaletteGpuParams p) {
    const int gx = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int gy = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (gx >= p.width || gy >= p.height)
        return;
    const int x = p.originX + gx;
    const int y = p.originY + gy;
    if (x < p.dstBoundsX1 || x >= p.dstBoundsX2 || y < p.dstBoundsY1 || y >= p.dstBoundsY2)
        return;
    const float fx = static_cast<float>(x) + 0.5f;
    const float fy = static_cast<float>(y) + 0.5f;
    float4 outRgb = make_float4(p.barR, p.barG, p.barB, 1.0f);
    if (x >= p.bgX0 && x < p.bgX1 && y >= p.bgY0 && y < p.bgY1) {
        outRgb = make_float4(p.barR, p.barG, p.barB, 1.0f);
    } else {
        const float pw = p.picX1 - p.picX0;
        const float ph = p.picY1 - p.picY0;
        const float sxSpan = static_cast<float>(p.srcBoundsX2 - p.srcBoundsX1);
        const float sySpan = static_cast<float>(p.srcBoundsY2 - p.srcBoundsY1);
        if (pw > 1.0e-4f && ph > 1.0e-4f && fx >= p.picX0 && fx < p.picX1 && fy >= p.picY0 && fy < p.picY1) {
            float u = 0.0f;
            float v = 0.0f;
            if (p.pictureCoverCrop != 0) {
                const float scale = fmaxf(pw / sxSpan, ph / sySpan);
                u = static_cast<float>(p.srcBoundsX1) + (sxSpan - pw / scale) * 0.5f + (fx - p.picX0) / scale;
                v = static_cast<float>(p.srcBoundsY1) + (sySpan - ph / scale) * 0.5f + (fy - p.picY0) / scale;
            } else {
                u = static_cast<float>(p.srcBoundsX1) + (fx - p.picX0) * sxSpan / pw;
                v = static_cast<float>(p.srcBoundsY1) + (fy - p.picY0) * sySpan / ph;
            }
            outRgb = sampleBilinear(src, p, u, v);
        }
    }
    outRgb = applySwatches(outRgb, fx, fy, p);
    const int didx = gy * p.dstRowFloats + gx * 4;
    dst[didx] = outRgb.x;
    dst[didx + 1] = outRgb.y;
    dst[didx + 2] = outRgb.z;
    dst[didx + 3] = outRgb.w;
}

} // namespace

extern "C" bool launchLSPPaletteKernel(
    const float* src, float* dst, int width, int height, const LSPPaletteGpuParams* params, void* cudaStream) {
    if (src == nullptr || dst == nullptr || params == nullptr || width <= 0 || height <= 0)
        return false;
    cudaStream_t stream = static_cast<cudaStream_t>(cudaStream);
    dim3 block(16, 16);
    dim3 grid((static_cast<unsigned>(width) + block.x - 1) / block.x, (static_cast<unsigned>(height) + block.y - 1) / block.y);
    LSPPaletteGpuParams p = *params;
    LSPPaletteKernel<<<grid, block, 0, stream>>>(src, dst, p);
    return cudaGetLastError() == cudaSuccess;
}
