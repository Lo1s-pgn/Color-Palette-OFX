#include <metal_stdlib>
using namespace metal;

#include "../core/LSPPaletteGpuExtractShared.h"

namespace {

constant int kTfGamma22 = 0;
constant int kTfGamma24 = 1;
constant int kTfGamma26 = 2;
constant int kTfHlg = 3;
constant int kTfPq = 4;
constant int kTfSrgb = 5;

inline float spow(float v, float e) {
    if (v == 0.0f)
        return 0.0f;
    return sign(v) * pow(abs(v), e);
}

inline float pqToLinear(float x) {
    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 32.0f;
    const float c1 = 107.0f / 128.0f;
    const float c2 = 2413.0f / 128.0f;
    const float c3 = 2392.0f / 128.0f;
    const float ax = abs(x);
    const float y = pow(ax, 1.0f / m2);
    float denom = c2 - c3 * y;
    if (abs(denom) < 1.0e-6f)
        denom = denom < 0.0f ? -1.0e-6f : 1.0e-6f;
    const float r = (y - c1) / denom;
    const float L = pow(max(r, 0.0f), 1.0f / m1);
    return sign(x) * L;
}

inline float3 hlgToLinear(float3 rgb) {
    float3 o;
    o.x = rgb.x <= 0.5f ? rgb.x * rgb.x / 3.0f
                        : (exp((rgb.x - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
    o.y = rgb.y <= 0.5f ? rgb.y * rgb.y / 3.0f
                        : (exp((rgb.y - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
    o.z = rgb.z <= 0.5f ? rgb.z * rgb.z / 3.0f
                        : (exp((rgb.z - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
    const float Ys = 0.2627f * o.x + 0.6780f * o.y + 0.0593f * o.z;
    const float s = pow(max(Ys, 0.0f), 0.2f);
    return float3(o.x * s, o.y * s, o.z * s);
}

inline float decodeChannel(float x, int transferChoice) {
    switch (transferChoice) {
    case kTfSrgb: {
        const float a = abs(x);
        const float d = (a <= 0.04045f) ? (a / 12.92f) : pow((a + 0.055f) / 1.055f, 2.4f);
        return sign(x) * d;
    }
    case kTfGamma24:
        return spow(x, 2.4f);
    case kTfGamma22:
        return spow(x, 2.2f);
    case kTfGamma26:
        return spow(x, 2.6f);
    case kTfPq:
        return pqToLinear(x);
    case kTfHlg:
        return x;
    default:
        return x;
    }
    return x;
}

inline float3 decodeRgb(float3 enc, int transferChoice) {
    if (transferChoice == kTfHlg)
        return hlgToLinear(enc);
    return float3(decodeChannel(enc.x, transferChoice),
        decodeChannel(enc.y, transferChoice),
        decodeChannel(enc.z, transferChoice));
}

inline float3 mulMat3(constant float* m, float3 v) {
    return float3(m[0] * v.x + m[1] * v.y + m[2] * v.z,
        m[3] * v.x + m[4] * v.y + m[5] * v.z,
        m[6] * v.x + m[7] * v.y + m[8] * v.z);
}

inline float3 linearPrimariesToSrgb(float3 lin, constant float* primariesToSrgb) {
    return mulMat3(primariesToSrgb, lin);
}

inline float3 linearSrgbToOkLab(float3 rgb) {
    const float r = rgb.x;
    const float g = rgb.y;
    const float b = rgb.z;
    const float lms_l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    const float lms_m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    const float lms_s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
    const float lp = pow(max(lms_l, 0.0f), 1.0f / 3.0f);
    const float mp = pow(max(lms_m, 0.0f), 1.0f / 3.0f);
    const float sp = pow(max(lms_s, 0.0f), 1.0f / 3.0f);
    return float3(0.2104542553f * lp + 0.7936177850f * mp - 0.0040720468f * sp,
        1.9779984951f * lp - 2.4285922050f * mp + 0.4505937099f * sp,
        0.0259040371f * lp + 0.7827717662f * mp - 0.8086757660f * sp);
}

inline float4 readRgba(const device float* slab, int rowFloats, int x, int y) {
    const int idx = y * rowFloats + x * 4;
    return float4(slab[idx], slab[idx + 1], slab[idx + 2], slab[idx + 3]);
}

inline bool averageLinearCross5(const device float* slab,
    int rowFloats,
    int slabW,
    int slabH,
    int cx,
    int cy,
    int transferChoice,
    constant float* primariesToSrgb,
    float nearBlack,
    thread float3& outLin) {
    float3 acc = float3(0.0f);
    int cnt = 0;
    const int2 offs[5] = { int2(0, 0), int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1) };
    for (int i = 0; i < 5; ++i) {
        const int sx = cx + offs[i].x;
        const int sy = cy + offs[i].y;
        if (sx < 0 || sy < 0 || sx >= slabW || sy >= slabH)
            continue;
        const float4 enc = readRgba(slab, rowFloats, sx, sy);
        float3 lin = decodeRgb(float3(enc.x, enc.y, enc.z), transferChoice);
        lin = linearPrimariesToSrgb(lin, primariesToSrgb);
        const float mx = max(max(lin.x, lin.y), lin.z);
        if (mx < nearBlack)
            continue;
        acc += lin;
        cnt++;
    }
    if (cnt < 1)
        return false;
    outLin = acc / float(cnt);
    return true;
}

} // namespace

kernel void LSPPaletteGatherSamplesKernel(const device float* slab [[buffer(0)]],
    device LSPPaletteGpuOkLabSample* outSamples [[buffer(1)]],
    constant LSPPaletteGpuExtractGatherParams& p [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]]) {
    const int tx = int(gid.x);
    const int ty = int(gid.y);
    if (tx >= p.gridW || ty >= p.gridH)
        return;
    const int idx = ty * p.gridW + tx;
    if (idx >= p.sampleCap) {
        outSamples[idx].valid = 0;
        return;
    }

    int sx = (tx * p.slabWidth) / p.gridW;
    int sy = (ty * p.slabHeight) / p.gridH;
    sx = clamp(sx, 0, p.slabWidth - 1);
    sy = clamp(sy, 0, p.slabHeight - 1);

    float3 lin = float3(0.0f);
    LSPPaletteGpuOkLabSample s{};
    s.valid = 0;
    s.w = 1.0f;
    if (averageLinearCross5(slab, p.slabRowFloats, p.slabWidth, p.slabHeight, sx, sy, p.transferChoice, p.primariesToSrgb,
            p.nearBlackLinear, lin)) {
        const float3 ok = linearSrgbToOkLab(lin);
        s.L = ok.x;
        s.a = ok.y;
        s.b = ok.z;
        s.valid = 1;
    }
    outSamples[idx] = s;
}
