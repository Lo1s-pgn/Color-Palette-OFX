#include "LSPPaletteGridBlur.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace LSPPaletteGridBlur {

namespace {

int vanVlietRadiusForSigma(float sigma) {
    const float s = std::max(sigma, 0.05f);
    int r = static_cast<int>(std::ceil(2.5 * static_cast<double>(s)));
    return std::max(r, 1);
}

void fillVanVlietCoeffsFromRadius(int radius, float coeffs[4]) {
    const double m0 = 1.16680;
    const double m1 = 1.10783;
    const double m2 = 1.40586;
    const double m1sq = m1 * m1;
    const double m2sq = m2 * m2;
    const float sigma = (radius < 1 ? 1.0f : static_cast<float>(radius)) / 2.4f;
    const double nnsigma = sigma < 0.1f ? 0.1f : sigma;
    const double q = (nnsigma < 3.556)
        ? (-0.2568 + 0.5784 * nnsigma + 0.0561 * nnsigma * nnsigma)
        : (2.5091 + 0.9804 * (nnsigma - 3.556));
    const double qsq = q * q;
    const double scale = (m0 + q) * (m1sq + m2sq + 2 * m1 * q + qsq);
    const double b1 = -q * (2 * m0 * m1 + m1sq + m2sq + (2 * m0 + 4 * m1) * q + 3 * qsq) / scale;
    const double b2 = qsq * (m0 + 2 * m1 + 3 * q) / scale;
    const double b3 = -qsq * q / scale;
    const double B = (m0 * (m1sq + m2sq)) / scale;
    coeffs[0] = static_cast<float>(B);
    coeffs[1] = static_cast<float>(-b1);
    coeffs[2] = static_cast<float>(-b2);
    coeffs[3] = static_cast<float>(-b3);
}

struct Rgb3 {
    float r;
    float g;
    float b;
};

inline Rgb3 loadRgb(const float* px) {
    return Rgb3{px[0], px[1], px[2]};
}

inline void storeRgb(float* px, const Rgb3& v) {
    px[0] = v.r;
    px[1] = v.g;
    px[2] = v.b;
    px[3] = 1.0f;
}

inline Rgb3 mulRgb(const Rgb3& v, float s) {
    return Rgb3{v.r * s, v.g * s, v.b * s};
}

inline Rgb3 addRgb(const Rgb3& a, const Rgb3& b) {
    return Rgb3{a.r + b.r, a.g + b.g, a.b + b.b};
}

void vanVlietPass1D(const float* srcBase, float* dstBase, int count, int stepFloats, const float coeffs[4]) {
    if (count < 1)
        return;

    const float B = coeffs[0];
    const float f1 = coeffs[1];
    const float f2 = coeffs[2];
    const float f3 = coeffs[3];
    const int warm = std::min(128, count - 1);

    const Rgb3 edge0 = loadRgb(srcBase);
    Rgb3 s0 = edge0;
    Rgb3 s1 = edge0;
    Rgb3 s2 = edge0;

    for (int k = warm; k >= 1; --k) {
        const int idx = std::min(k, count - 1);
        const Rgb3 v = loadRgb(srcBase + idx * stepFloats);
        const Rgb3 out = addRgb(mulRgb(v, B), addRgb(mulRgb(s0, f1), addRgb(mulRgb(s1, f2), mulRgb(s2, f3))));
        s2 = s1;
        s1 = s0;
        s0 = out;
    }

    for (int i = 0; i < count; ++i) {
        const Rgb3 v = loadRgb(srcBase + i * stepFloats);
        const Rgb3 out = addRgb(mulRgb(v, B), addRgb(mulRgb(s0, f1), addRgb(mulRgb(s1, f2), mulRgb(s2, f3))));
        storeRgb(dstBase + i * stepFloats, out);
        s2 = s1;
        s1 = s0;
        s0 = out;
    }

    const Rgb3 edge1 = loadRgb(dstBase + (count - 1) * stepFloats);
    s0 = edge1;
    s1 = edge1;
    s2 = edge1;

    for (int k = warm; k >= 1; --k) {
        const int idx = std::max(0, count - k);
        const Rgb3 v = loadRgb(dstBase + idx * stepFloats);
        const Rgb3 out = addRgb(mulRgb(v, B), addRgb(mulRgb(s0, f1), addRgb(mulRgb(s1, f2), mulRgb(s2, f3))));
        s2 = s1;
        s1 = s0;
        s0 = out;
    }

    for (int i = count - 1; i >= 0; --i) {
        const Rgb3 v = loadRgb(dstBase + i * stepFloats);
        const Rgb3 out = addRgb(mulRgb(v, B), addRgb(mulRgb(s0, f1), addRgb(mulRgb(s1, f2), mulRgb(s2, f3))));
        storeRgb(dstBase + i * stepFloats, out);
        s2 = s1;
        s1 = s0;
        s0 = out;
    }
}

} // namespace

void cpuGaussianBlur(const float* src, float* dst, int nw, int nh, float sigmaPixels, float* tmpRowMajor) {
    if (!src || !dst || !tmpRowMajor || nw < 2 || nh < 2)
        return;
    const int radius = vanVlietRadiusForSigma(sigmaPixels);
    float coeffs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    fillVanVlietCoeffsFromRadius(radius, coeffs);

    // Horizontal: src -> tmp
    for (int y = 0; y < nh; ++y) {
        const float* srcRow = src + (y * nw * 4);
        float* tmpRow = tmpRowMajor + (y * nw * 4);
        vanVlietPass1D(srcRow, tmpRow, nw, 4, coeffs);
    }

    // Vertical: tmp -> dst
    for (int x = 0; x < nw; ++x) {
        const float* srcCol = tmpRowMajor + (x * 4);
        float* dstCol = dst + (x * 4);
        vanVlietPass1D(srcCol, dstCol, nh, nw * 4, coeffs);
    }
}

} // namespace LSPPaletteGridBlur
