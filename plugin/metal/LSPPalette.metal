#include <metal_stdlib>
using namespace metal;

#include "../core/LSPPaletteGpuParamsShared.h"
#include "../core/LSPPaletteGpuDownsampleParams.h"

inline float4 readPx(const device float* src, constant LSPPaletteGpuParams& p, int absX, int absY) {
    const int lx = absX - p.srcReadOriginX;
    const int ly = absY - p.srcReadOriginY;
    const int idx = ly * p.srcRowFloats + lx * 4;
    return float4(src[idx], src[idx + 1], src[idx + 2], src[idx + 3]);
}

inline float4 sampleBilinear(const device float* src, constant LSPPaletteGpuParams& p, float xf, float yf) {
    const int xMin = p.srcBoundsX1;
    const int yMin = p.srcBoundsY1;
    const int xMax = p.srcBoundsX2 - 1;
    const int yMax = p.srcBoundsY2 - 1;
    if (xMax < xMin || yMax < yMin)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    xf = clamp(xf, float(xMin), float(xMax));
    yf = clamp(yf, float(yMin), float(yMax));
    const int x0 = int(floor(xf));
    const int y0 = int(floor(yf));
    const int xA = min(x0, xMax);
    const int yA = min(y0, yMax);
    const int xB = min(xA + 1, xMax);
    const int yB = min(yA + 1, yMax);
    const float tx = xf - float(xA);
    const float ty = yf - float(yA);
    const float4 c00 = readPx(src, p, xA, yA);
    const float4 c10 = readPx(src, p, xB, yA);
    const float4 c01 = readPx(src, p, xA, yB);
    const float4 c11 = readPx(src, p, xB, yB);
    return (1.0f - tx) * (1.0f - ty) * c00 + tx * (1.0f - ty) * c10 + (1.0f - tx) * ty * c01 + tx * ty * c11;
}

inline float sdfRoundRect(float px, float py, float bx0, float by0, float bx1, float by1, float rad) {
    const float cx = (bx0 + bx1) * 0.5f;
    const float cy = (by0 + by1) * 0.5f;
    const float hx = (bx1 - bx0) * 0.5f - rad;
    const float hy = (by1 - by0) * 0.5f - rad;
    const float qx = abs(px - cx) - hx;
    const float qy = abs(py - cy) - hy;
    const float qxm = max(qx, 0.0f);
    const float qym = max(qy, 0.0f);
    const float ext = sqrt(qxm * qxm + qym * qym);
    const float interior = min(max(qx, qy), 0.0f);
    return ext + interior - rad;
}

inline float4 applySwatches(float4 rgb, float px, float py, constant LSPPaletteGpuParams& p) {
    const float aa = 0.6f;
    for (int i = 0; i < p.swatchCount; ++i) {
        const LSPPaletteGpuSwatch s = p.swatches[i];
        float rad = s.rad;
        const float bw = s.bx1 - s.bx0;
        const float bh = s.by1 - s.by0;
        rad = max(0.0f, min(rad, min(bw, bh) * 0.5f - 0.25f));
        const float d = sdfRoundRect(px, py, s.bx0, s.by0, s.bx1, s.by1, rad);
        const float cov = clamp(0.5f - d / (2.0f * aa), 0.0f, 1.0f);
        if (cov <= 1.0e-4f)
            continue;
        const float3 sw = float3(s.r, s.g, s.b);
        if (cov >= 1.0f - 1.0e-4f)
            rgb = float4(sw, 1.0f);
        else {
            const float om = 1.0f - cov;
            rgb.xyz = rgb.xyz * om + sw * cov;
            rgb.w = 1.0f;
        }
    }
    return rgb;
}

inline float4 readPxDown(const device float* src, constant LSPPaletteGpuDownsampleParams& p, float xf, float yf) {
    const float xMin = float(p.srcReadOriginX);
    const float yMin = float(p.srcReadOriginY);
    const float xMax = xMin + float(p.srcW) - 1.0f;
    const float yMax = yMin + float(p.srcH) - 1.0f;
    xf = clamp(xf, xMin, xMax);
    yf = clamp(yf, yMin, yMax);
    const int x0 = int(floor(xf));
    const int y0 = int(floor(yf));
    const int xA = min(x0, int(xMax));
    const int yA = min(y0, int(yMax));
    const int xB = min(xA + 1, int(xMax));
    const int yB = min(yA + 1, int(yMax));
    const float tx = xf - float(xA);
    const float ty = yf - float(yA);
    const int lxA = xA - p.srcReadOriginX;
    const int lyA = yA - p.srcReadOriginY;
    const int lxB = xB - p.srcReadOriginX;
    const int lyB = yB - p.srcReadOriginY;
    const int idx00 = lyA * p.srcRowFloats + lxA * 4;
    const int idx10 = lyA * p.srcRowFloats + lxB * 4;
    const int idx01 = lyB * p.srcRowFloats + lxA * 4;
    const int idx11 = lyB * p.srcRowFloats + lxB * 4;
    const float4 c00 = float4(src[idx00], src[idx00 + 1], src[idx00 + 2], src[idx00 + 3]);
    const float4 c10 = float4(src[idx10], src[idx10 + 1], src[idx10 + 2], src[idx10 + 3]);
    const float4 c01 = float4(src[idx01], src[idx01 + 1], src[idx01 + 2], src[idx01 + 3]);
    const float4 c11 = float4(src[idx11], src[idx11 + 1], src[idx11 + 2], src[idx11 + 3]);
    return (1.0f - tx) * (1.0f - ty) * c00 + tx * (1.0f - ty) * c10 + (1.0f - tx) * ty * c01 + tx * ty * c11;
}

kernel void LSPPaletteDownsampleKernel(const device float* src [[buffer(0)]],
    device float* dst [[buffer(1)]],
    constant LSPPaletteGpuDownsampleParams& p [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]]) {
    if (int(gid.x) >= p.dstW || int(gid.y) >= p.dstH)
        return;
    const float u = (float(gid.x) + 0.5f) / float(p.dstW);
    const float v = (float(gid.y) + 0.5f) / float(p.dstH);
    const float xf = float(p.srcReadOriginX) + u * float(p.srcW);
    const float yf = float(p.srcReadOriginY) + v * float(p.srcH);
    const float4 rgba = readPxDown(src, p, xf, yf);
    const int didx = int(gid.y) * p.dstW * 4 + int(gid.x) * 4;
    dst[didx] = rgba.x;
    dst[didx + 1] = rgba.y;
    dst[didx + 2] = rgba.z;
    dst[didx + 3] = rgba.w;
}

kernel void LSPPaletteCompositeKernel(const device float* src [[buffer(0)]],
    device float* dst [[buffer(1)]],
    constant LSPPaletteGpuParams& p [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]]) {
    if (int(gid.x) >= p.width || int(gid.y) >= p.height)
        return;
    const int x = p.originX + int(gid.x);
    const int y = p.originY + int(gid.y);
    if (x < p.dstBoundsX1 || x >= p.dstBoundsX2 || y < p.dstBoundsY1 || y >= p.dstBoundsY2)
        return;

    const float fx = float(x) + 0.5f;
    const float fy = float(y) + 0.5f;
    float4 outRgb = float4(p.barR, p.barG, p.barB, 1.0f);

    if (x >= p.bgX0 && x < p.bgX1 && y >= p.bgY0 && y < p.bgY1) {
        outRgb = float4(p.barR, p.barG, p.barB, 1.0f);
    } else {
        const float pw = p.picX1 - p.picX0;
        const float ph = p.picY1 - p.picY0;
        const float sxSpan = float(p.srcBoundsX2 - p.srcBoundsX1);
        const float sySpan = float(p.srcBoundsY2 - p.srcBoundsY1);
        if (pw > 1.0e-4f && ph > 1.0e-4f && fx >= p.picX0 && fx < p.picX1 && fy >= p.picY0 && fy < p.picY1) {
            float u = 0.0f;
            float v = 0.0f;
            if (p.pictureCoverCrop != 0) {
                const float scale = max(pw / sxSpan, ph / sySpan);
                u = float(p.srcBoundsX1) + (sxSpan - pw / scale) * 0.5f + (fx - p.picX0) / scale;
                v = float(p.srcBoundsY1) + (sySpan - ph / scale) * 0.5f + (fy - p.picY0) / scale;
            } else {
                u = float(p.srcBoundsX1) + (fx - p.picX0) * sxSpan / pw;
                v = float(p.srcBoundsY1) + (fy - p.picY0) * sySpan / ph;
            }
            outRgb = sampleBilinear(src, p, u, v);
        }
    }

    outRgb = applySwatches(outRgb, fx, fy, p);

    const int didx = int(gid.y) * p.dstRowFloats + int(gid.x) * 4;
    dst[didx] = outRgb.x;
    dst[didx + 1] = outRgb.y;
    dst[didx + 2] = outRgb.z;
    dst[didx + 3] = outRgb.w;
}
