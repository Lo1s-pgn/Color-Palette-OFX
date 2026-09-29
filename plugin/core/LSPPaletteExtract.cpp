#include "LSPPaletteExtract.h"

#include "LSPPaletteExtractInternal.h"
#include "LSPPaletteImageAccess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace {

constexpr int kPaletteMaxAnalysisSide = 300;
constexpr int kPaletteSampleCap = 400000;
// skip neer black pixels after decode
constexpr float kNearBlackLinearMaxChannel = 0.004f;

WorkshopColor::Vec3f linearPrimariesRgbToLinearSrgb(const WorkshopColor::Vec3f& linRgb,
    WorkshopColor::ColorPrimariesId primaries) {
    WorkshopColor::Mat3f toXyz = WorkshopColor::rgbToXyzMatrix(primaries);
    WorkshopColor::Vec3f xyz = WorkshopColor::mul(toXyz, linRgb);
    WorkshopColor::Mat3f toSrgb = WorkshopColor::xyzToRgbMatrix(WorkshopColor::ColorPrimariesId::Rec709);
    return WorkshopColor::mul(toSrgb, xyz);
}

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
    const WorkshopColor::Mat3f lpmFromLab = WorkshopColor::invert(labFromLpm);
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
    const WorkshopColor::Mat3f rgbFromLms = WorkshopColor::invert(lmsFromRgb);
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

float okLabHueClockwiseRad(const OkLab& o) {
    const float r = std::atan2(o.b, o.a);
    float h = -r;
    h = std::fmod(h + kTwoPi, kTwoPi);
    return h;
}

float okLabSaturationNorm(const OkLab& o) {
    const float L = std::max(o.L, 0.01f);
    return okLabChroma(o) / L;
}

float linearRgbBlackBodyKelvin(const WorkshopColor::Vec3f& linRgb, WorkshopColor::ColorPrimariesId primaries) {
    const WorkshopColor::Vec3f xyz = WorkshopColor::mul(WorkshopColor::rgbToXyzMatrix(primaries), linRgb);
    const WorkshopColor::Vec2f xy = WorkshopColor::xyzToXy(xyz, { 1.0f / 3.0f, 1.0f / 3.0f });
    const float kelvin = WorkshopColor::nearestBlackBodyTemperature(xy);
    return kelvin > 0.0f ? kelvin : 6500.0f;
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

// median cut in oklab, heckbert style splits
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

bool detailFinishPaletteCluster(std::vector<LSPPaletteExtractInternal::OkLabSample>& samples,
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

    return true;
}

void applyPaletteSortOrderImpl(std::vector<LSPPaletteExtract::Swatch>& outPalette,
    const LSPPaletteExtract::Settings& settings) {
    const int k = static_cast<int>(outPalette.size());
    if (k < 2)
        return;

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
        std::vector<float> kelvin(static_cast<std::size_t>(k));
        for (int j = 0; j < k; ++j)
            kelvin[static_cast<std::size_t>(j)] =
                linearRgbBlackBodyKelvin(outPalette[static_cast<std::size_t>(j)].linearRgb, settings.primaries);
        auto cmpTemperature = [&](int ia, int ib) -> bool {
            const float ta = kelvin[static_cast<std::size_t>(ia)];
            const float tb = kelvin[static_cast<std::size_t>(ib)];
            if (ta != tb)
                return ta < tb;
            const OkLab& a = labs[static_cast<std::size_t>(ia)];
            const OkLab& b = labs[static_cast<std::size_t>(ib)];
            if (a.L != b.L)
                return a.L < b.L;
            return ia < ib;
        };
        std::sort(order.begin(), order.end(), cmpTemperature);
    } else
        std::sort(order.begin(), order.end(), cmpWeight);

    std::vector<LSPPaletteExtract::Swatch> sorted(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i)
        sorted[static_cast<std::size_t>(i)] = outPalette[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])];
    outPalette.swap(sorted);
}

bool detailFinishPalette(std::vector<LSPPaletteExtractInternal::OkLabSample>& samples,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette,
    std::vector<LSPPaletteExtract::Swatch>* outClusterUnsorted) {
    if (!detailFinishPaletteCluster(samples, settings, outPalette))
        return false;
    if (outClusterUnsorted != nullptr)
        *outClusterUnsorted = outPalette;
    applyPaletteSortOrderImpl(outPalette, settings);
    return true;
}

} // namespace

bool LSPPaletteExtract::extractDominantColorsFromSlab(const float* slab,
    int rowBytes,
    const OfxRectI& sampleBounds,
    const Settings& settings,
    std::vector<Swatch>& outPalette,
    std::vector<Swatch>* outClusterUnsorted) {
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
    return LSPPaletteExtractInternal::finishPalette(packed, settings, outPalette, outClusterUnsorted);
}

bool LSPPaletteExtract::extractDominantColors(OFX::Image* src,
    const OfxRectI& sampleBounds,
    const Settings& settings,
    std::vector<Swatch>& outPalette,
    std::vector<Swatch>* outClusterUnsorted) {
    if (!src || !src->getPixelData())
        return false;
    const float* slab = LSPPaletteImageAccess::cpuReadableSlab(src);
    if (!slab)
        return false;
    return extractDominantColorsFromSlab(
        slab, src->getRowBytes(), sampleBounds, settings, outPalette, outClusterUnsorted);
}

namespace LSPPaletteExtractInternal {

bool finishPalette(std::vector<OkLabSample>& samples,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette,
    std::vector<LSPPaletteExtract::Swatch>* outClusterUnsorted) {
    return detailFinishPalette(samples, settings, outPalette, outClusterUnsorted);
}

void applyPaletteSortOrder(std::vector<LSPPaletteExtract::Swatch>& palette,
    const LSPPaletteExtract::Settings& settings) {
    applyPaletteSortOrderImpl(palette, settings);
}

} // namespace LSPPaletteExtractInternal
