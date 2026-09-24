#pragma OPENCL EXTENSION cl_khr_fp64 : enable

typedef struct {
    float bx0, by0, bx1, by1, rad, r, g, b, pad;
} PaletteSwatch;

typedef struct {
    int width, height, originX, originY;
    int srcBoundsX1, srcBoundsY1, srcBoundsX2, srcBoundsY2;
    int dstBoundsX1, dstBoundsY1, dstBoundsX2, dstBoundsY2;
    int bgX0, bgY0, bgX1, bgY1;
    float picX0, picY0, picX1, picY1;
    int pictureCoverCrop;
    float barR, barG, barB;
    int swatchCount;
    PaletteSwatch swatches[24];
    int srcRowFloats, dstRowFloats;
    int srcReadOriginX, srcReadOriginY;
} PaletteParams;

float4 readPx(__global const float* src, PaletteParams p, int absX, int absY) {
    int lx = absX - p.srcReadOriginX;
    int ly = absY - p.srcReadOriginY;
    int idx = ly * p.srcRowFloats + lx * 4;
    return (float4)(src[idx], src[idx + 1], src[idx + 2], src[idx + 3]);
}

float4 sampleBilinear(__global const float* src, PaletteParams p, float xf, float yf) {
    int xMin = p.srcBoundsX1, yMin = p.srcBoundsY1;
    int xMax = p.srcBoundsX2 - 1, yMax = p.srcBoundsY2 - 1;
    if (xMax < xMin || yMax < yMin) return (float4)(0.f, 0.f, 0.f, 1.f);
    xf = clamp(xf, (float)xMin, (float)xMax);
    yf = clamp(yf, (float)yMin, (float)yMax);
    int x0 = (int)floor(xf), y0 = (int)floor(yf);
    int xA = min(x0, xMax), yA = min(y0, yMax);
    int xB = min(xA + 1, xMax), yB = min(yA + 1, yMax);
    float tx = xf - (float)xA, ty = yf - (float)yA;
    float4 c00 = readPx(src, p, xA, yA);
    float4 c10 = readPx(src, p, xB, yA);
    float4 c01 = readPx(src, p, xA, yB);
    float4 c11 = readPx(src, p, xB, yB);
    return (1.f - tx) * (1.f - ty) * c00 + tx * (1.f - ty) * c10 + (1.f - tx) * ty * c01 + tx * ty * c11;
}

float sdfRoundRect(float px, float py, float bx0, float by0, float bx1, float by1, float rad) {
    float cx = (bx0 + bx1) * 0.5f, cy = (by0 + by1) * 0.5f;
    float hx = (bx1 - bx0) * 0.5f - rad, hy = (by1 - by0) * 0.5f - rad;
    float qx = fabs(px - cx) - hx, qy = fabs(py - cy) - hy;
    float ext = sqrt(fmax(qx, 0.f) * fmax(qx, 0.f) + fmax(qy, 0.f) * fmax(qy, 0.f));
    float interior = fmin(fmax(qx, qy), 0.f);
    return ext + interior - rad;
}

float4 applySwatches(float4 rgb, float px, float py, PaletteParams p) {
    const float aa = 0.6f;
    for (int i = 0; i < p.swatchCount; ++i) {
        PaletteSwatch s = p.swatches[i];
        float rad = s.rad;
        float bw = s.bx1 - s.bx0, bh = s.by1 - s.by0;
        rad = fmax(0.f, fmin(rad, fmin(bw, bh) * 0.5f - 0.25f));
        float d = sdfRoundRect(px, py, s.bx0, s.by0, s.bx1, s.by1, rad);
        float cov = clamp(0.5f - d / (2.f * aa), 0.f, 1.f);
        if (cov <= 1e-4f) continue;
        float3 sw = (float3)(s.r, s.g, s.b);
        if (cov >= 1.f - 1e-4f) rgb = (float4)(sw, 1.f);
        else {
            float om = 1.f - cov;
            rgb.xyz = rgb.xyz * om + sw * cov;
            rgb.w = 1.f;
        }
    }
    return rgb;
}

__kernel void LSPPaletteKernel(__global const float* src, __global float* dst, __global const PaletteParams* pBuf) {
    PaletteParams p = *pBuf;
    int gx = get_global_id(0);
    int gy = get_global_id(1);
    if (gx >= p.width || gy >= p.height) return;
    int x = p.originX + gx, y = p.originY + gy;
    if (x < p.dstBoundsX1 || x >= p.dstBoundsX2 || y < p.dstBoundsY1 || y >= p.dstBoundsY2) return;
    float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
    float4 outRgb = (float4)(p.barR, p.barG, p.barB, 1.f);
    if (x >= p.bgX0 && x < p.bgX1 && y >= p.bgY0 && y < p.bgY1) {
        outRgb = (float4)(p.barR, p.barG, p.barB, 1.f);
    } else {
        float pw = p.picX1 - p.picX0, ph = p.picY1 - p.picY0;
        float sxSpan = (float)(p.srcBoundsX2 - p.srcBoundsX1);
        float sySpan = (float)(p.srcBoundsY2 - p.srcBoundsY1);
        if (pw > 1e-4f && ph > 1e-4f && fx >= p.picX0 && fx < p.picX1 && fy >= p.picY0 && fy < p.picY1) {
            float u, v;
            if (p.pictureCoverCrop != 0) {
                float scale = fmax(pw / sxSpan, ph / sySpan);
                u = (float)p.srcBoundsX1 + (sxSpan - pw / scale) * 0.5f + (fx - p.picX0) / scale;
                v = (float)p.srcBoundsY1 + (sySpan - ph / scale) * 0.5f + (fy - p.picY0) / scale;
            } else {
                u = (float)p.srcBoundsX1 + (fx - p.picX0) * sxSpan / pw;
                v = (float)p.srcBoundsY1 + (fy - p.picY0) * sySpan / ph;
            }
            outRgb = sampleBilinear(src, p, u, v);
        }
    }
    outRgb = applySwatches(outRgb, fx, fy, p);
    int didx = gy * p.dstRowFloats + gx * 4;
    dst[didx] = outRgb.x;
    dst[didx + 1] = outRgb.y;
    dst[didx + 2] = outRgb.z;
    dst[didx + 3] = outRgb.w;
}
