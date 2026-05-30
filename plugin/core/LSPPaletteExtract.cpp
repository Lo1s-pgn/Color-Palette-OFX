#include "LSPPaletteExtract.h"

#include "LSPPaletteExtractInternal.h"
#include "LSPPaletteImageAccess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

namespace {

constexpr int kPaletteMaxAnalysisSide = 300;
constexpr int kPaletteSampleCap = 400000;
/** Ignore pixels whose linear RGB max channel is below this (decoding uses INPUT COLOR transfer). */
constexpr float kNearBlackLinearMaxChannel = 0.004f;

WorkshopColor::Vec3f linearPrimariesRgbToLinearSrgb(const WorkshopColor::Vec3f& linRgb,
    WorkshopColor::ColorPrimariesId primaries) {
    WorkshopColor::Mat3f toXyz = WorkshopColor::rgbToXyzMatrix(primaries);
    WorkshopColor::Vec3f xyz = WorkshopColor::mul(toXyz, linRgb);
    WorkshopColor::Mat3f toSrgb = WorkshopColor::xyzToRgbMatrix(WorkshopColor::ColorPrimariesId::Rec709);
    return WorkshopColor::mul(toSrgb, xyz);
}

/** Björn Ottosson OKLAB from linear Rec.709 RGB (same LMS/cbrt/M matrix as OKLAB.dctl forward core). */
struct OkLab {
    float L = 0.0f;
    float a = 0.0f;
    float b = 0.0f;
};

OkLab linearSrgbToOkLab(const WorkshopColor::Vec3f& rgb) {
    const float r = rgb.x;
    const float g = rgb.y;
    const float bl = rgb.z;
    const float lms_l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * bl;
    const float lms_m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * bl;
    const float lms_s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * bl;
    const float lp = std::cbrt(lms_l);
    const float mp = std::cbrt(lms_m);
    const float sp = std::cbrt(lms_s);
    OkLab o;
    o.L = 0.2104542553f * lp + 0.7936177850f * mp - 0.0040720468f * sp;
    o.a = 1.9779984951f * lp - 2.4285922050f * mp + 0.4505937099f * sp;
    o.b = 0.0259040371f * lp + 0.7827717662f * mp - 0.8086757660f * sp;
    return o;
}

WorkshopColor::Mat3f invertMat3(const WorkshopColor::Mat3f& matrix) {
    const float a00 = matrix.m[0][0];
    const float a01 = matrix.m[0][1];
    const float a02 = matrix.m[0][2];
    const float a10 = matrix.m[1][0];
    const float a11 = matrix.m[1][1];
    const float a12 = matrix.m[1][2];
    const float a20 = matrix.m[2][0];
    const float a21 = matrix.m[2][1];
    const float a22 = matrix.m[2][2];

    const float c00 = a11 * a22 - a12 * a21;
    const float c01 = a02 * a21 - a01 * a22;
    const float c02 = a01 * a12 - a02 * a11;
    const float c10 = a12 * a20 - a10 * a22;
    const float c11 = a00 * a22 - a02 * a20;
    const float c12 = a02 * a10 - a00 * a12;
    const float c20 = a10 * a21 - a11 * a20;
    const float c21 = a01 * a20 - a00 * a21;
    const float c22 = a00 * a11 - a01 * a10;

    const float det = a00 * c00 + a01 * c10 + a02 * c20;
    if (std::fabs(det) <= 1e-12f)
        return {};
    const float invDet = 1.0f / det;
    WorkshopColor::Mat3f out{};
    out.m[0][0] = c00 * invDet;
    out.m[0][1] = c01 * invDet;
    out.m[0][2] = c02 * invDet;
    out.m[1][0] = c10 * invDet;
    out.m[1][1] = c11 * invDet;
    out.m[1][2] = c12 * invDet;
    out.m[2][0] = c20 * invDet;
    out.m[2][1] = c21 * invDet;
    out.m[2][2] = c22 * invDet;
    return out;
}

WorkshopColor::Vec3f okLabToLinearSrgb(const OkLab& o) {
    WorkshopColor::Mat3f labFromLpm{};
    labFromLpm.m[0][0] = 0.2104542553f;
    labFromLpm.m[0][1] = 0.7936177850f;
    labFromLpm.m[0][2] = -0.0040720468f;
    labFromLpm.m[1][0] = 1.9779984951f;
    labFromLpm.m[1][1] = -2.4285922050f;
    labFromLpm.m[1][2] = 0.4505937099f;
    labFromLpm.m[2][0] = 0.0259040371f;
    labFromLpm.m[2][1] = 0.7827717662f;
    labFromLpm.m[2][2] = -0.8086757660f;
    const WorkshopColor::Mat3f lpmFromLab = invertMat3(labFromLpm);
    const WorkshopColor::Vec3f lpm =
        WorkshopColor::mul(lpmFromLab, WorkshopColor::Vec3f{ o.L, o.a, o.b });
    const float lms_l = lpm.x * lpm.x * lpm.x;
    const float lms_m = lpm.y * lpm.y * lpm.y;
    const float lms_s = lpm.z * lpm.z * lpm.z;
    WorkshopColor::Mat3f lmsFromRgb{};
    lmsFromRgb.m[0][0] = 0.4122214708f;
    lmsFromRgb.m[0][1] = 0.5363325363f;
    lmsFromRgb.m[0][2] = 0.0514459929f;
    lmsFromRgb.m[1][0] = 0.2119034982f;
    lmsFromRgb.m[1][1] = 0.6806995451f;
    lmsFromRgb.m[1][2] = 0.1073969566f;
    lmsFromRgb.m[2][0] = 0.0883024619f;
    lmsFromRgb.m[2][1] = 0.2817188376f;
    lmsFromRgb.m[2][2] = 0.6299787005f;
    const WorkshopColor::Mat3f rgbFromLms = invertMat3(lmsFromRgb);
    return WorkshopColor::mul(rgbFromLms, WorkshopColor::Vec3f{ lms_l, lms_m, lms_s });
}

WorkshopColor::Vec3f okLabToLinearPrimariesRgb(const OkLab& ol, WorkshopColor::ColorPrimariesId primaries) {
    const WorkshopColor::Vec3f srgb = okLabToLinearSrgb(ol);
    const WorkshopColor::Mat3f toXyz709 = WorkshopColor::rgbToXyzMatrix(WorkshopColor::ColorPrimariesId::Rec709);
    const WorkshopColor::Vec3f xyz = WorkshopColor::mul(toXyz709, srgb);
    const WorkshopColor::Mat3f toRgb = WorkshopColor::xyzToRgbMatrix(primaries);
    return WorkshopColor::mul(toRgb, xyz);
}

OkLab linearRgbToOkLabForSort(const WorkshopColor::Vec3f& linRgb, WorkshopColor::ColorPrimariesId primaries) {
    WorkshopColor::Vec3f srgb = linearPrimariesRgbToLinearSrgb(linRgb, primaries);
    return linearSrgbToOkLab(srgb);
}

float okLabChroma(const OkLab& o) {
    return std::sqrt(o.a * o.a + o.b * o.b);
}

constexpr float kTwoPi = 6.2831855f;

/** Clockwise hue in radians from +a (0°), increasing 0 → 2π for a clockwise sweep in the a–b plane. */
float okLabHueClockwiseRad(const OkLab& o) {
    const float r = std::atan2(o.b, o.a);
    float h = -r;
    h = std::fmod(h + kTwoPi, kTwoPi);
    return h;
}

/** LSH-style saturation: chroma normalized by OKLAB L (avoids pure-LCH ordering). */
float okLabSaturationNorm(const OkLab& o) {
    const float L = std::max(o.L, 0.01f);
    return okLabChroma(o) / L;
}

float okLabDist2(const OkLab& u, const OkLab& v) {
    const float dL = u.L - v.L;
    const float da = u.a - v.a;
    const float db = u.b - v.b;
    return dL * dL + da * da + db * db;
}

double openPathOkLabTotalLen(const std::vector<int>& path, const std::vector<OkLab>& labs, int k) {
    double s = 0.0;
    for (int i = 0; i < k - 1; ++i) {
        const int ia = path[static_cast<std::size_t>(i)];
        const int ib = path[static_cast<std::size_t>(i + 1)];
        s += std::sqrt(static_cast<double>(
            okLabDist2(labs[static_cast<std::size_t>(ia)], labs[static_cast<std::size_t>(ib)])));
    }
    return s;
}

void greedyNnOpenPathFromStart(std::vector<int>& path, const std::vector<OkLab>& labs, int k, int startIdx) {
    path.assign(static_cast<std::size_t>(k), 0);
    std::vector<unsigned char> used(static_cast<std::size_t>(k), 0);
    path[0] = startIdx;
    used[static_cast<std::size_t>(startIdx)] = 1;
    int cur = startIdx;
    for (int t = 1; t < k; ++t) {
        int best = -1;
        float bestD2 = std::numeric_limits<float>::max();
        for (int j = 0; j < k; ++j) {
            if (used[static_cast<std::size_t>(j)] != 0)
                continue;
            const float d2 =
                okLabDist2(labs[static_cast<std::size_t>(j)], labs[static_cast<std::size_t>(cur)]);
            if (d2 < bestD2 || (d2 == bestD2 && (best < 0 || j < best))) {
                bestD2 = d2;
                best = j;
            }
        }
        if (best < 0)
            break;
        path[static_cast<std::size_t>(t)] = best;
        used[static_cast<std::size_t>(best)] = 1;
        cur = best;
    }
}

/** Open-path 2-opt in OKLAB edge length; vertex path[0] stays fixed (multi-start NN picks the start). */
void shortenOpenPathOkLab2Opt(std::vector<int>& path, const std::vector<OkLab>& labs, int k) {
    if (k < 4)
        return;
    auto edgeLen = [&](int swatchA, int swatchB) -> float {
        return std::sqrt(okLabDist2(labs[static_cast<std::size_t>(swatchA)], labs[static_cast<std::size_t>(swatchB)]));
    };
    for (;;) {
        bool improved = false;
        for (int i = 1; i <= k - 2; ++i) {
            for (int j = i + 1; j <= k - 2; ++j) {
                const int ia = path[static_cast<std::size_t>(i - 1)];
                const int ib = path[static_cast<std::size_t>(i)];
                const int ic = path[static_cast<std::size_t>(j)];
                const int jd = path[static_cast<std::size_t>(j + 1)];
                const float remove = edgeLen(ia, ib) + edgeLen(ic, jd);
                const float add = edgeLen(ia, ic) + edgeLen(ib, jd);
                if (add < remove - 1e-7f) {
                    std::reverse(path.begin() + i, path.begin() + j + 1);
                    improved = true;
                }
            }
        }
        if (!improved)
            break;
    }
}

/** Prefer shorter OKLAB path length; within tolerance prefer larger OKLAB L[start]-L[end]; then lex path. */
bool smoothCandidateBeats(double lenN, double dropN, const std::vector<int>& pathN, double lenB, double dropB,
    const std::vector<int>& pathB) {
    const double scale = std::max(1.0, std::max(lenN, lenB));
    const double tol = std::max(5e-5, 1e-7 * scale);
    if (lenN + tol < lenB)
        return true;
    if (lenB + tol < lenN)
        return false;
    constexpr double kDropTol = 1e-6;
    if (dropN > dropB + kDropTol)
        return true;
    if (dropN + kDropTol < dropB)
        return false;
    return pathN < pathB;
}

/** Shortest OKLAB open path via multi-start greedy nearest-neighbor + 2-opt; bias light→dark; stable ties. */
void orderSmoothOkLabMultiStart(std::vector<int>& order, const std::vector<OkLab>& labs, int k) {
    if (k < 2)
        return;
    std::vector<int> path;
    double bestLen = 0.0;
    double bestDrop = 0.0;
    std::vector<int> bestPath;
    bool have = false;
    for (int s = 0; s < k; ++s) {
        greedyNnOpenPathFromStart(path, labs, k, s);
        shortenOpenPathOkLab2Opt(path, labs, k);
        const double len = openPathOkLabTotalLen(path, labs, k);
        const double drop = static_cast<double>(labs[static_cast<std::size_t>(path[0])].L)
            - static_cast<double>(labs[static_cast<std::size_t>(path[static_cast<std::size_t>(k - 1)])].L);
        if (!have || smoothCandidateBeats(len, drop, path, bestLen, bestDrop, bestPath)) {
            have = true;
            bestLen = len;
            bestDrop = drop;
            bestPath = std::move(path);
        }
    }
    order.swap(bestPath);
    if (labs[static_cast<std::size_t>(order[0])].L < labs[static_cast<std::size_t>(order[static_cast<std::size_t>(k - 1)])].L)
        std::reverse(order.begin(), order.end());
}

struct OkLabW {
    OkLab lab;
    double w = 1.0;
};

float okLabAxisComponent(const OkLab& o, int axis) {
    return axis == 0 ? o.L : (axis == 1 ? o.a : o.b);
}

int longestOkLabAxis(float spanL, float spanA, float spanB) {
    int ax = 0;
    float s = spanL;
    if (spanA > s) {
        ax = 1;
        s = spanA;
    }
    if (spanB > s)
        ax = 2;
    return ax;
}

void permuteOkLabCentersCanonical(std::vector<OkLab>& centers, std::vector<double>& mass) {
    const int k = static_cast<int>(centers.size());
    if (k < 2)
        return;
    std::vector<int> ord(static_cast<std::size_t>(k));
    std::iota(ord.begin(), ord.end(), 0);
    std::sort(ord.begin(), ord.end(), [&](int ia, int ib) {
        const OkLab& a = centers[static_cast<std::size_t>(ia)];
        const OkLab& b = centers[static_cast<std::size_t>(ib)];
        if (a.L != b.L)
            return a.L < b.L;
        if (a.a != b.a)
            return a.a < b.a;
        if (a.b != b.b)
            return a.b < b.b;
        return ia < ib;
    });
    std::vector<OkLab> nc(static_cast<std::size_t>(k));
    std::vector<double> nm(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i) {
        const int j = ord[static_cast<std::size_t>(i)];
        nc[static_cast<std::size_t>(i)] = centers[static_cast<std::size_t>(j)];
        nm[static_cast<std::size_t>(i)] = mass[static_cast<std::size_t>(j)];
    }
    centers.swap(nc);
    mass.swap(nm);
}

/** Heckbert-style median cut on weighted OKLAB samples (axis-aligned splits). */
bool medianCutOkLab(const std::vector<OkLabW>& samples, int k, std::vector<OkLab>& outCenters, std::vector<double>& outMass) {
    const int n = static_cast<int>(samples.size());
    if (n < 1 || k < 1)
        return false;
    k = std::min(k, n);
    std::vector<std::vector<std::size_t>> boxes;
    std::vector<std::size_t> all(static_cast<std::size_t>(n));
    std::iota(all.begin(), all.end(), 0);
    boxes.push_back(std::move(all));

    while (static_cast<int>(boxes.size()) < k) {
        int bestBi = -1;
        double bestScore = -1.0;
        for (int bi = 0; bi < static_cast<int>(boxes.size()); ++bi) {
            const auto& box = boxes[static_cast<std::size_t>(bi)];
            if (box.size() < 2)
                continue;
            float minL = samples[box[0]].lab.L, maxL = minL;
            float minA = samples[box[0]].lab.a, maxA = minA;
            float minB = samples[box[0]].lab.b, maxB = minB;
            double wsum = 0.0;
            for (std::size_t idx : box) {
                const OkLab& lb = samples[idx].lab;
                minL = std::min(minL, lb.L);
                maxL = std::max(maxL, lb.L);
                minA = std::min(minA, lb.a);
                maxA = std::max(maxA, lb.a);
                minB = std::min(minB, lb.b);
                maxB = std::max(maxB, lb.b);
                wsum += samples[idx].w;
            }
            const float spanL = maxL - minL;
            const float spanA = maxA - minA;
            const float spanB = maxB - minB;
            const float span = std::max({ spanL, spanA, spanB });
            const double score = static_cast<double>(span) * wsum;
            if (score > bestScore || (score == bestScore && (bestBi < 0 || bi < bestBi))) {
                bestScore = score;
                bestBi = bi;
            }
        }
        if (bestBi < 0) {
            std::size_t bestSz = 0;
            int bi = -1;
            for (int b = 0; b < static_cast<int>(boxes.size()); ++b) {
                const std::size_t sz = boxes[static_cast<std::size_t>(b)].size();
                if (sz > bestSz) {
                    bestSz = sz;
                    bi = b;
                }
            }
            if (bi < 0 || bestSz < 2)
                break;
            std::vector<std::size_t>& fbox = boxes[static_cast<std::size_t>(bi)];
            std::sort(fbox.begin(), fbox.end());
            const std::size_t mid = std::max<std::size_t>(1u, bestSz / 2);
            std::vector<std::size_t> left(fbox.begin(), fbox.begin() + static_cast<std::ptrdiff_t>(mid));
            std::vector<std::size_t> right(fbox.begin() + static_cast<std::ptrdiff_t>(mid), fbox.end());
            fbox = std::move(left);
            boxes.push_back(std::move(right));
            continue;
        }

        std::vector<std::size_t>& box = boxes[static_cast<std::size_t>(bestBi)];
        float minL = samples[box[0]].lab.L, maxL = minL;
        float minA = samples[box[0]].lab.a, maxA = minA;
        float minB = samples[box[0]].lab.b, maxB = minB;
        for (std::size_t idx : box) {
            const OkLab& lb = samples[idx].lab;
            minL = std::min(minL, lb.L);
            maxL = std::max(maxL, lb.L);
            minA = std::min(minA, lb.a);
            maxA = std::max(maxA, lb.a);
            minB = std::min(minB, lb.b);
            maxB = std::max(maxB, lb.b);
        }
        const float spanL = maxL - minL;
        const float spanA = maxA - minA;
        const float spanB = maxB - minB;
        const int axis = longestOkLabAxis(spanL, spanA, spanB);

        std::sort(box.begin(), box.end(), [&](std::size_t ia, std::size_t ib) {
            const float va = okLabAxisComponent(samples[ia].lab, axis);
            const float vb = okLabAxisComponent(samples[ib].lab, axis);
            if (va != vb)
                return va < vb;
            return ia < ib;
        });

        double totalW = 0.0;
        for (std::size_t idx : box)
            totalW += samples[idx].w;
        const double half = totalW * 0.5;
        std::size_t cut = 0;
        if (spanL <= 1e-12f && spanA <= 1e-12f && spanB <= 1e-12f) {
            cut = box.size() / 2 - 1;
        } else {
            double cum = 0.0;
            for (; cut + 1 < box.size(); ++cut) {
                cum += samples[box[cut]].w;
                if (cum >= half)
                    break;
            }
        }
        if (cut >= box.size() - 1)
            cut = box.size() / 2 > 0 ? box.size() / 2 - 1 : 0;

        std::vector<std::size_t> left(box.begin(), box.begin() + static_cast<std::ptrdiff_t>(cut + 1));
        std::vector<std::size_t> right(box.begin() + static_cast<std::ptrdiff_t>(cut + 1), box.end());
        if (right.empty()) {
            left.assign(box.begin(), box.begin() + 1);
            right.assign(box.begin() + 1, box.end());
        }
        box = std::move(left);
        boxes.push_back(std::move(right));
    }

    const int kb = static_cast<int>(boxes.size());
    outCenters.resize(static_cast<std::size_t>(kb));
    outMass.assign(static_cast<std::size_t>(kb), 0.0);
    for (int i = 0; i < kb; ++i) {
        double sumL = 0.0, sumA = 0.0, sumB = 0.0, wsum = 0.0;
        for (std::size_t idx : boxes[static_cast<std::size_t>(i)]) {
            const double w = samples[idx].w;
            wsum += w;
            sumL += static_cast<double>(samples[idx].lab.L) * w;
            sumA += static_cast<double>(samples[idx].lab.a) * w;
            sumB += static_cast<double>(samples[idx].lab.b) * w;
        }
        outMass[static_cast<std::size_t>(i)] = wsum;
        if (wsum > 1e-30) {
            outCenters[static_cast<std::size_t>(i)].L = static_cast<float>(sumL / wsum);
            outCenters[static_cast<std::size_t>(i)].a = static_cast<float>(sumA / wsum);
            outCenters[static_cast<std::size_t>(i)].b = static_cast<float>(sumB / wsum);
        }
    }
    return true;
}

WorkshopColor::Vec3f linearFromEncodedRgb(float r, float g, float b, WorkshopColor::TransferFunctionId tf) {
    WorkshopColor::Vec3f enc{ r, g, b };
    enc = WorkshopColor::clamp(enc, -1.0f, 1.0f);
    return WorkshopColor::decodeToLinear(enc, tf);
}

bool linearIsNearBlack(const WorkshopColor::Vec3f& lin) {
    const float mx = std::max(std::max(lin.x, lin.y), lin.z);
    return mx < kNearBlackLinearMaxChannel;
}

/** Average decoded linear RGB over center + 4-neighborhood (skipping near-black taps) for stability with fewer samples than 3×3. */
bool averageLinearCross5Cell(const OfxRectI& bounds, int rowBytes, const float* slab, int cx, int cy,
    WorkshopColor::TransferFunctionId tf, WorkshopColor::Vec3f& outLin) {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    int cnt = 0;
    static constexpr int kOff[5][2] = { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    for (const auto& d : kOff) {
        int sx = cx + d[0];
        int sy = cy + d[1];
        if (sx < bounds.x1 || sx >= bounds.x2 || sy < bounds.y1 || sy >= bounds.y2)
            continue;
        const float* px = LSPPaletteImageAccess::rgbaAtFromSlab(bounds, rowBytes, slab, sx, sy);
        if (!px)
            continue;
        WorkshopColor::Vec3f lin = linearFromEncodedRgb(px[0], px[1], px[2], tf);
        if (linearIsNearBlack(lin))
            continue;
        ax += lin.x;
        ay += lin.y;
        az += lin.z;
        ++cnt;
    }
    if (cnt < 1)
        return false;
    const float inv = 1.0f / static_cast<float>(cnt);
    outLin = WorkshopColor::Vec3f{ ax * inv, ay * inv, az * inv };
    return !linearIsNearBlack(outLin);
}

bool detailFinishPalette(std::vector<LSPPaletteExtractInternal::OkLabSample>& samples,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette) {
    outPalette.clear();
    std::vector<OkLabW> okSamples;
    okSamples.reserve(samples.size());
    for (const LSPPaletteExtractInternal::OkLabSample& s : samples) {
        OkLabW w;
        w.lab.L = s.L;
        w.lab.a = s.a;
        w.lab.b = s.b;
        w.w = s.w;
        okSamples.push_back(w);
    }
    int k = std::clamp(settings.patchCount, 3, 24);
    const int sampleCount = static_cast<int>(okSamples.size());
    if (sampleCount < 1)
        return false;
    if (sampleCount < k)
        k = sampleCount;
    if (k < 1)
        return false;

    std::vector<double> mass;
    std::vector<OkLab> okCenters;
    if (!medianCutOkLab(okSamples, k, okCenters, mass))
        return false;
    k = static_cast<int>(okCenters.size());
    if (k < 1)
        return false;
    permuteOkLabCentersCanonical(okCenters, mass);
    double tm = 0.0;
    for (double v : mass)
        tm += v;
    if (tm <= 1e-30)
        tm = 1.0;

    outPalette.resize(static_cast<std::size_t>(k));
    for (int j = 0; j < k; ++j) {
        WorkshopColor::Vec3f lin =
            okLabToLinearPrimariesRgb(okCenters[static_cast<std::size_t>(j)], settings.primaries);
        lin = WorkshopColor::clamp(lin, -2.0f, 8.0f);
        outPalette[static_cast<std::size_t>(j)].linearRgb = lin;
        outPalette[static_cast<std::size_t>(j)].weight =
            tm > 0.0 ? static_cast<float>(mass[static_cast<std::size_t>(j)] / tm) : 1.0f / static_cast<float>(k);
    }

    auto cmpWeight = [&](int a, int b) {
        return outPalette[static_cast<std::size_t>(a)].weight > outPalette[static_cast<std::size_t>(b)].weight;
    };

    std::vector<int> order(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i)
        order[static_cast<std::size_t>(i)] = i;

    std::vector<OkLab> labs(static_cast<std::size_t>(k));
    for (int j = 0; j < k; ++j)
        labs[static_cast<std::size_t>(j)] =
            linearRgbToOkLabForSort(outPalette[static_cast<std::size_t>(j)].linearRgb, settings.primaries);

    if (settings.sortOrder == 0) {
        std::sort(order.begin(), order.end(), cmpWeight);
    } else if (settings.sortOrder == 1) {
        auto cmpLightness = [&](int ia, int ib) -> bool {
            const OkLab& a = labs[static_cast<std::size_t>(ia)];
            const OkLab& b = labs[static_cast<std::size_t>(ib)];
            if (a.L != b.L)
                return a.L < b.L;
            const float ca = okLabChroma(a);
            const float cb = okLabChroma(b);
            if (ca != cb)
                return ca < cb;
            return ia < ib;
        };
        std::sort(order.begin(), order.end(), cmpLightness);
    } else if (settings.sortOrder == 2) {
        constexpr float kHueSortAchromaticChroma = 0.002f;
        auto cmpHueOrder = [&](int ia, int ib) -> bool {
            const OkLab& a = labs[static_cast<std::size_t>(ia)];
            const OkLab& b = labs[static_cast<std::size_t>(ib)];
            const float ca = okLabChroma(a);
            const float cb = okLabChroma(b);
            const bool acha = ca < kHueSortAchromaticChroma;
            const bool achb = cb < kHueSortAchromaticChroma;
            if (acha != achb)
                return !acha;
            if (acha) {
                if (a.L != b.L)
                    return a.L < b.L;
                return ia < ib;
            }
            const float ha = okLabHueClockwiseRad(a);
            const float hb = okLabHueClockwiseRad(b);
            if (ha != hb)
                return ha < hb;
            if (a.L != b.L)
                return a.L < b.L;
            if (ca != cb)
                return ca < cb;
            return ia < ib;
        };
        std::sort(order.begin(), order.end(), cmpHueOrder);
    } else if (settings.sortOrder == 3) {
        auto cmpSaturation = [&](int ia, int ib) -> bool {
            const float sa = okLabSaturationNorm(labs[static_cast<std::size_t>(ia)]);
            const float sb = okLabSaturationNorm(labs[static_cast<std::size_t>(ib)]);
            if (sa != sb)
                return sa < sb;
            const OkLab& a = labs[static_cast<std::size_t>(ia)];
            const OkLab& b = labs[static_cast<std::size_t>(ib)];
            if (a.L != b.L)
                return a.L < b.L;
            const float ca = okLabChroma(a);
            const float cb = okLabChroma(b);
            if (ca != cb)
                return ca < cb;
            return ia < ib;
        };
        std::sort(order.begin(), order.end(), cmpSaturation);
    } else if (settings.sortOrder == 4) {
        orderSmoothOkLabMultiStart(order, labs, k);
    } else
        std::sort(order.begin(), order.end(), cmpWeight);

    std::vector<LSPPaletteExtract::Swatch> sorted(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i)
        sorted[static_cast<std::size_t>(i)] = outPalette[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])];
    outPalette.swap(sorted);

    return true;
}

} // namespace

bool LSPPaletteExtract::extractDominantColorsFromSlab(const float* slab,
    int rowBytes,
    const OfxRectI& sampleBounds,
    const Settings& settings,
    std::vector<Swatch>& outPalette) {
    outPalette.clear();
    if (!slab || rowBytes < 16)
        return false;

    const int fw = sampleBounds.x2 - sampleBounds.x1;
    const int fh = sampleBounds.y2 - sampleBounds.y1;
    if (fw < 2 || fh < 2)
        return false;

    int k = std::clamp(settings.patchCount, 3, 24);
    const int maxSide = kPaletteMaxAnalysisSide;
    const int sampleCap = kPaletteSampleCap;

    int nw = fw;
    int nh = fh;
    const int longSide = std::max(fw, fh);
    if (longSide > maxSide) {
        const float scale = static_cast<float>(maxSide) / static_cast<float>(longSide);
        nw = std::max(2, static_cast<int>(std::floor(static_cast<float>(fw) * scale)));
        nh = std::max(2, static_cast<int>(std::floor(static_cast<float>(fh) * scale)));
    }

    const int gridCells = nw * nh;
    std::vector<OkLabW> okSamples;
    okSamples.reserve(static_cast<std::size_t>(std::min(gridCells, sampleCap)));

    for (int idx = 0; idx < gridCells; ++idx) {
        if (static_cast<int>(okSamples.size()) >= sampleCap)
            break;
        const int tx = idx % nw;
        const int ty = idx / nw;
        int sx = sampleBounds.x1 + (tx * fw) / nw;
        int sy = sampleBounds.y1 + (ty * fh) / nh;
        sx = std::clamp(sx, sampleBounds.x1, sampleBounds.x2 - 1);
        sy = std::clamp(sy, sampleBounds.y1, sampleBounds.y2 - 1);
        if (!LSPPaletteImageAccess::rgbaAtFromSlab(sampleBounds, rowBytes, slab, sx, sy))
            continue;
        WorkshopColor::Vec3f lin{};
        if (!averageLinearCross5Cell(sampleBounds, rowBytes, slab, sx, sy, settings.transfer, lin))
            continue;
        okSamples.push_back({ linearRgbToOkLabForSort(lin, settings.primaries), 1.0 });
    }

    std::vector<LSPPaletteExtractInternal::OkLabSample> packed;
    packed.reserve(okSamples.size());
    for (const OkLabW& s : okSamples) {
        LSPPaletteExtractInternal::OkLabSample o;
        o.L = s.lab.L;
        o.a = s.lab.a;
        o.b = s.lab.b;
        o.w = s.w;
        packed.push_back(o);
    }
    return LSPPaletteExtractInternal::finishPalette(packed, settings, outPalette);
}

bool LSPPaletteExtract::extractDominantColors(OFX::Image* src, const OfxRectI& sampleBounds,
    const Settings& settings, std::vector<Swatch>& outPalette) {
    if (!src || !src->getPixelData())
        return false;
    const float* slab = LSPPaletteImageAccess::cpuReadableSlab(src);
    if (!slab)
        return false;
    return extractDominantColorsFromSlab(slab, src->getRowBytes(), sampleBounds, settings, outPalette);
}

namespace LSPPaletteExtractInternal {

bool finishPalette(std::vector<OkLabSample>& samples,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette) {
    return detailFinishPalette(samples, settings, outPalette);
}

} // namespace LSPPaletteExtractInternal
