#include "LSPPaletteComposite.h"

#include <algorithm>
#include <cmath>

namespace {

WorkshopColor::Vec3f encodedForDraw(WorkshopColor::Vec3f linear, WorkshopColor::TransferFunctionId tf) {
    WorkshopColor::Vec3f enc = WorkshopColor::encodeFromLinear(linear, tf);
    return WorkshopColor::clamp(enc, 0.0f, 1.0f);
}

void setPixel(OFX::Image* dst, int x, int y, float r, float g, float b, float a, const OfxRectI& rw) {
    if (x < rw.x1 || x >= rw.x2 || y < rw.y1 || y >= rw.y2)
        return;
    float* p = static_cast<float*>(dst->getPixelAddress(x, y));
    if (!p)
        return;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

/** Premultiplied-style blend of swatch RGB over existing dst (alpha forced to 1). `cov` in [0,1]. */
void blendSwatchOver(OFX::Image* dst, int x, int y, float sr, float sg, float sb, float cov, const OfxRectI& rw) {
    if (x < rw.x1 || x >= rw.x2 || y < rw.y1 || y >= rw.y2)
        return;
    float* p = static_cast<float*>(dst->getPixelAddress(x, y));
    if (!p)
        return;
    const float om = 1.0f - cov;
    p[0] = p[0] * om + sr * cov;
    p[1] = p[1] * om + sg * cov;
    p[2] = p[2] * om + sb * cov;
    p[3] = 1.0f;
}

float sdfRoundRect(float px, float py, float bx0, float by0, float bx1, float by1, float rad) {
    const float cx = (bx0 + bx1) * 0.5f;
    const float cy = (by0 + by1) * 0.5f;
    const float hx = (bx1 - bx0) * 0.5f - rad;
    const float hy = (by1 - by0) * 0.5f - rad;
    const float qx = std::fabs(px - cx) - hx;
    const float qy = std::fabs(py - cy) - hy;
    const float qxm = std::max(qx, 0.0f);
    const float qym = std::max(qy, 0.0f);
    const float ext = std::sqrt(qxm * qxm + qym * qym);
    const float interior = std::min(std::max(qx, qy), 0.0f);
    return ext + interior - rad;
}

void fillRoundRect(OFX::Image* dst, float bx0, float by0, float bx1, float by1, float rad, float r, float g, float b,
    float a, const OfxRectI& rw) {
    rad = std::max(0.0f, std::min(rad, std::min((bx1 - bx0), (by1 - by0)) * 0.5f - 0.25f));
    /* Slight AA: signed distance d (negative inside). ~1.2 px transition width. */
    const float aa = 0.6f;
    const int pad = 2;
    const int ix0 = static_cast<int>(std::floor(bx0)) - pad;
    const int iy0 = static_cast<int>(std::floor(by0)) - pad;
    const int ix1 = static_cast<int>(std::ceil(bx1)) + pad;
    const int iy1 = static_cast<int>(std::ceil(by1)) + pad;
    for (int y = iy0; y < iy1; ++y)
        for (int x = ix0; x < ix1; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float d = sdfRoundRect(px, py, bx0, by0, bx1, by1, rad);
            const float cov = std::clamp(0.5f - d / (2.0f * aa), 0.0f, 1.0f);
            if (cov <= 1.0e-4f)
                continue;
            if (cov >= 1.0f - 1.0e-4f)
                setPixel(dst, x, y, r, g, b, a, rw);
            else
                blendSwatchOver(dst, x, y, r, g, b, cov, rw);
        }
}

void readPx(const OFX::Image* src, int x, int y, float* p) {
    const float* s = static_cast<const float*>(src->getPixelAddress(x, y));
    if (!s) {
        p[0] = p[1] = p[2] = 0.0f;
        p[3] = 1.0f;
        return;
    }
    p[0] = s[0];
    p[1] = s[1];
    p[2] = s[2];
    p[3] = s[3];
}

void sampleBilinear(const OFX::Image* src, const OfxRectI& sb, float xf, float yf, float* out) {
    const int xMin = sb.x1;
    const int yMin = sb.y1;
    const int xMax = sb.x2 - 1;
    const int yMax = sb.y2 - 1;
    if (xMax < xMin || yMax < yMin) {
        out[0] = out[1] = out[2] = 0.0f;
        out[3] = 1.0f;
        return;
    }
    xf = std::clamp(xf, static_cast<float>(xMin), static_cast<float>(xMax));
    yf = std::clamp(yf, static_cast<float>(yMin), static_cast<float>(yMax));
    const int x0 = static_cast<int>(std::floor(xf));
    const int y0 = static_cast<int>(std::floor(yf));
    const int xA = std::min(x0, xMax);
    const int yA = std::min(y0, yMax);
    const int xB = std::min(xA + 1, xMax);
    const int yB = std::min(yA + 1, yMax);
    const float tx = xf - static_cast<float>(xA);
    const float ty = yf - static_cast<float>(yA);
    float c00[4], c10[4], c01[4], c11[4];
    readPx(src, xA, yA, c00);
    readPx(src, xB, yA, c10);
    readPx(src, xA, yB, c01);
    readPx(src, xB, yB, c11);
    for (int i = 0; i < 4; ++i)
        out[i] = (1.0f - tx) * (1.0f - ty) * c00[i] + tx * (1.0f - ty) * c10[i] + (1.0f - tx) * ty * c01[i]
            + tx * ty * c11[i];
}

} // namespace

/** Pixel size of source scaled to fit inside innerW×innerH (letterbox fit). */
static void fitPlacedInInner(int innerW, int innerH, int swSrc, int shSrc, double* outPw, double* outPh) {
    const double sc = std::min(static_cast<double>(innerW) / static_cast<double>(swSrc),
        static_cast<double>(innerH) / static_cast<double>(shSrc));
    *outPw = static_cast<double>(swSrc) * sc;
    *outPh = static_cast<double>(shSrc) * sc;
}

bool LSPPaletteComposite::buildCompositePlan(const OfxRectI& dstBounds,
    const OfxRectI& srcBounds,
    const std::vector<LSPPaletteExtract::Swatch>& palette,
    WorkshopColor::TransferFunctionId transfer,
    WorkshopColor::ColorPrimariesId primaries,
    const Presentation& pres,
    OverlayLayout& out,
    CompositeFrame& frameOut) {
    out = {};
    frameOut = {};
    if (palette.empty())
        return false;

    const int k = static_cast<int>(palette.size());
    const int W = dstBounds.x2 - dstBounds.x1;
    const int H = dstBounds.y2 - dstBounds.y1;
    if (W < 16 || H < 16)
        return false;

    const int ix1 = dstBounds.x1;
    const int iy1 = dstBounds.y1;
    const int ix2 = dstBounds.x2;
    const int iy2 = dstBounds.y2;

    const int swSrc = srcBounds.x2 - srcBounds.x1;
    const int shSrc = srcBounds.y2 - srcBounds.y1;
    if (swSrc < 1 || shSrc < 1)
        return false;

    const float Ln = static_cast<float>(std::clamp(pres.bgLightness, 0.0, 1.0));
    const float Lr = Ln;
    const float Lg = Ln;
    const float Lb = Ln;
    out.barR = Lr;
    out.barG = Lg;
    out.barB = Lb;
    frameOut.barR = Lr;
    frameOut.barG = Lg;
    frameOut.barB = Lb;

    const int layout = pres.layout;
    const bool horizontal = (layout == kLayoutTop || layout == kLayoutBottom);

    const double stripFrac = std::clamp(pres.stripFrac, 0.05, 0.55);
    const double cornerFrac = std::clamp(pres.cornerFrac, 0.0, 0.49);

    const int shortSide = std::min(W, H);
    const double gapStrength = std::clamp(pres.gapFrac, 0.0, 1.0);
    const int gapPxRaw = static_cast<int>(std::lround(gapStrength * 72.0));
    const int gapPx = std::clamp(gapPxRaw, 0, std::max(0, shortSide / 4));

    int patchH = 0;
    int patchW = 0;
    int gap = 0;
    int stripOuter = 0;
    double picPW = 0.0;
    double picPH = 0.0;
    int placedWi = 0;
    int placedHi = 0;

    if (horizontal) {
        stripOuter = 0;
        bool settled = false;
        for (int it = 0; it < 128; ++it) {
            const int innerH = H - stripOuter;
            if (innerH < 4)
                return false;
            fitPlacedInInner(W, innerH, swSrc, shSrc, &picPW, &picPH);
            int ph = std::max(4, static_cast<int>(std::lround(stripFrac * picPH)));
            int g = std::min(gapPx, std::max(0, ph - 4));
            int stripTry = ph + 2 * g;
            int pwTry = static_cast<int>(std::floor((picPW - static_cast<double>((k + 1) * g)) / static_cast<double>(k)));
            while (pwTry < 4 && g > 0) {
                --g;
                pwTry = static_cast<int>(std::floor((picPW - static_cast<double>((k + 1) * g)) / static_cast<double>(k)));
                stripTry = ph + 2 * g;
            }
            while (pwTry < 4 && ph > 4) {
                --ph;
                g = std::min(gapPx, std::max(0, ph - 4));
                pwTry = static_cast<int>(std::floor((picPW - static_cast<double>((k + 1) * g)) / static_cast<double>(k)));
                stripTry = ph + 2 * g;
            }
            if (pwTry < 4 || stripTry >= H)
                return false;
            if (stripTry == stripOuter || std::abs(stripTry - stripOuter) <= 1) {
                patchH = ph;
                gap = g;
                patchW = pwTry;
                stripOuter = stripTry;
                settled = true;
                break;
            }
            stripOuter = stripTry;
        }
        if (!settled)
            return false;
        fitPlacedInInner(W, H - stripOuter, swSrc, shSrc, &picPW, &picPH);
        placedWi = std::max(4, static_cast<int>(std::lround(picPW)));
        placedHi = std::max(4, static_cast<int>(std::lround(picPH)));
    } else {
        stripOuter = 0;
        bool settled = false;
        for (int it = 0; it < 128; ++it) {
            const int innerW = W - stripOuter;
            if (innerW < 4)
                return false;
            fitPlacedInInner(innerW, H, swSrc, shSrc, &picPW, &picPH);
            int pwStr = std::max(4, static_cast<int>(std::lround(stripFrac * picPW)));
            int g = std::min(gapPx, std::max(0, pwStr - 4));
            int stripTry = pwStr + 2 * g;
            int phTry = static_cast<int>(std::floor((picPH - static_cast<double>((k + 1) * g)) / static_cast<double>(k)));
            while (phTry < 4 && g > 0) {
                --g;
                phTry = static_cast<int>(std::floor((picPH - static_cast<double>((k + 1) * g)) / static_cast<double>(k)));
                stripTry = pwStr + 2 * g;
            }
            while (phTry < 4 && pwStr > 4) {
                --pwStr;
                g = std::min(gapPx, std::max(0, pwStr - 4));
                phTry = static_cast<int>(std::floor((picPH - static_cast<double>((k + 1) * g)) / static_cast<double>(k)));
                stripTry = pwStr + 2 * g;
            }
            if (phTry < 4 || stripTry >= W)
                return false;
            if (stripTry == stripOuter || std::abs(stripTry - stripOuter) <= 1) {
                patchW = pwStr;
                gap = g;
                patchH = phTry;
                stripOuter = stripTry;
                settled = true;
                break;
            }
            stripOuter = stripTry;
        }
        if (!settled)
            return false;
        fitPlacedInInner(W - stripOuter, H, swSrc, shSrc, &picPW, &picPH);
        placedWi = std::max(4, static_cast<int>(std::lround(picPW)));
        placedHi = std::max(4, static_cast<int>(std::lround(picPH)));
    }

    const int picOffX = (W - placedWi) / 2;
    const int picOffY = (H - placedHi) / 2;

    const bool fitFrame = pres.fitPaletteToFrame;
    int patchWUse = patchW;
    int patchHUse = patchH;
    if (fitFrame) {
        if (horizontal) {
            patchWUse = std::max(4, static_cast<int>(std::floor(
                (static_cast<double>(W) - static_cast<double>(k + 1) * static_cast<double>(gap)) / static_cast<double>(k))));
            if (patchWUse < 4 || k * patchWUse + (k + 1) * gap > W)
                return false;
        } else {
            patchHUse = std::max(4, static_cast<int>(std::floor(
                (static_cast<double>(H) - static_cast<double>(k + 1) * static_cast<double>(gap)) / static_cast<double>(k))));
            if (patchHUse < 4 || k * patchHUse + (k + 1) * gap > H)
                return false;
        }
    }

    const int cornerDim = std::min(patchWUse, patchHUse);
    const int rad = std::clamp(static_cast<int>(std::lround(cornerFrac * static_cast<double>(cornerDim))), 0,
        std::min(patchWUse, patchHUse) / 2);

    out.swatches.clear();
    out.swatches.reserve(static_cast<std::size_t>(k));

    auto appendSwatchRect = [&](int left, int bottomY, int pw, int ph, const LSPPaletteExtract::Swatch& swatch) {
        WorkshopColor::Vec3f enc = encodedForDraw(swatch.linearRgb, transfer);
        OverlayLayout::SwatchDraw d;
        d.bx0 = static_cast<float>(left);
        d.by0 = static_cast<float>(bottomY);
        d.bx1 = static_cast<float>(left + pw);
        d.by1 = static_cast<float>(bottomY + ph);
        d.rad = static_cast<float>(rad);
        d.r = enc.x;
        d.g = enc.y;
        d.b = enc.z;
        out.swatches.push_back(d);
    };

    int imgX1 = 0, imgY1 = 0, imgX2 = 0, imgY2 = 0;
    const int rowContentW = k * patchWUse + (k + 1) * gap;
    const int colContentH = k * patchHUse + (k + 1) * gap;

    if (layout == kLayoutBottom) {
        const int stripH = stripOuter;
        if (fitFrame) {
            out.bgX0 = ix1;
            out.bgX1 = ix2;
        } else {
            out.bgX0 = ix1 + picOffX;
            out.bgX1 = ix1 + picOffX + placedWi;
        }
        out.bgY0 = iy1;
        out.bgY1 = iy1 + stripH;
        const int spanW = fitFrame ? W : placedWi;
        const int startBase = fitFrame ? ix1 : (ix1 + picOffX);
        const int startX = startBase + static_cast<int>(std::lround((static_cast<double>(spanW) - static_cast<double>(rowContentW)) * 0.5));
        const int bottomY = iy1 + gap;
        for (int i = 0; i < k; ++i) {
            const int lx = startX + gap + i * (patchWUse + gap);
            appendSwatchRect(lx, bottomY, patchWUse, patchHUse, palette[static_cast<std::size_t>(i)]);
        }
        imgX1 = ix1;
        imgY1 = iy1 + stripH;
        imgX2 = ix2;
        imgY2 = iy2;
    } else if (layout == kLayoutTop) {
        const int stripH = stripOuter;
        out.bgY0 = iy2 - stripH;
        out.bgY1 = iy2;
        if (fitFrame) {
            out.bgX0 = ix1;
            out.bgX1 = ix2;
        } else {
            out.bgX0 = ix1 + picOffX;
            out.bgX1 = ix1 + picOffX + placedWi;
        }
        const int spanW = fitFrame ? W : placedWi;
        const int startBase = fitFrame ? ix1 : (ix1 + picOffX);
        const int startX = startBase + static_cast<int>(std::lround((static_cast<double>(spanW) - static_cast<double>(rowContentW)) * 0.5));
        const int bottomY = iy2 - gap - patchHUse;
        for (int i = 0; i < k; ++i) {
            const int lx = startX + gap + i * (patchWUse + gap);
            appendSwatchRect(lx, bottomY, patchWUse, patchHUse, palette[static_cast<std::size_t>(i)]);
        }
        imgX1 = ix1;
        imgY1 = iy1;
        imgX2 = ix2;
        imgY2 = iy2 - stripH;
    } else if (layout == kLayoutLeft) {
        const int stripW = stripOuter;
        out.bgX0 = ix1;
        out.bgX1 = ix1 + stripW;
        if (fitFrame) {
            out.bgY0 = iy1;
            out.bgY1 = iy2;
        } else {
            out.bgY0 = iy1 + picOffY;
            out.bgY1 = iy1 + picOffY + placedHi;
        }
        const int spanH = fitFrame ? H : placedHi;
        const int startBaseY = fitFrame ? iy1 : (iy1 + picOffY);
        const int startY = startBaseY + static_cast<int>(std::lround((static_cast<double>(spanH) - static_cast<double>(colContentH)) * 0.5));
        for (int i = 0; i < k; ++i) {
            const int bottomY = startY + gap + i * (patchHUse + gap);
            appendSwatchRect(ix1 + gap, bottomY, patchWUse, patchHUse, palette[static_cast<std::size_t>(i)]);
        }
        imgX1 = ix1 + stripW;
        imgY1 = iy1;
        imgX2 = ix2;
        imgY2 = iy2;
    } else {
        const int stripW = stripOuter;
        out.bgX0 = ix2 - stripW;
        out.bgX1 = ix2;
        if (fitFrame) {
            out.bgY0 = iy1;
            out.bgY1 = iy2;
        } else {
            out.bgY0 = iy1 + picOffY;
            out.bgY1 = iy1 + picOffY + placedHi;
        }
        const int spanH = fitFrame ? H : placedHi;
        const int startBaseY = fitFrame ? iy1 : (iy1 + picOffY);
        const int startY = startBaseY + static_cast<int>(std::lround((static_cast<double>(spanH) - static_cast<double>(colContentH)) * 0.5));
        for (int i = 0; i < k; ++i) {
            const int bottomY = startY + gap + i * (patchHUse + gap);
            appendSwatchRect(ix2 - stripW + gap, bottomY, patchWUse, patchHUse, palette[static_cast<std::size_t>(i)]);
        }
        imgX1 = ix1;
        imgY1 = iy1;
        imgX2 = ix2 - stripW;
        imgY2 = iy2;
    }

    const int imgW = imgX2 - imgX1;
    const int imgH2 = imgY2 - imgY1;
    if (imgW < 4 || imgH2 < 4)
        return false;

    /* Inset picture from frame edges by **gap** on sides that touch the output frame. On the edge that
     * touches the palette strip, margin is **0** — the strip layout already leaves **gap** between swatches
     * and the image seam, so an extra inset there would double the visible gap. */
    int mL = gap;
    int mR = gap;
    int mB = gap;
    int mT = gap;
    if (layout == kLayoutBottom)
        mB = 0;
    else if (layout == kLayoutTop)
        mT = 0;
    else if (layout == kLayoutLeft)
        mL = 0;
    else
        mR = 0;

    int tw = imgW - mL - mR;
    int thIn = imgH2 - mB - mT;
    tw = std::max(4, tw);
    thIn = std::max(4, thIn);
    const float ix0 = static_cast<float>(imgX1 + mL);
    const float iy0 = static_cast<float>(imgY1 + mB);
    const float pw = static_cast<float>(tw);
    const float ph = static_cast<float>(thIn);

    if (pres.imageFillCoverCrop) {
        const float sxSpanF = static_cast<float>(swSrc);
        const float sySpanF = static_cast<float>(shSrc);
        const float scale = std::max(pw / sxSpanF, ph / sySpanF);
        if (!(scale > 1.0e-10f))
            return false;
        frameOut.pictureCoverCrop = true;
        frameOut.picX0 = ix0;
        frameOut.picY0 = iy0;
        frameOut.picX1 = ix0 + pw;
        frameOut.picY1 = iy0 + ph;
    } else {
        frameOut.pictureCoverCrop = false;
        const double scale = std::min(static_cast<double>(pw) / static_cast<double>(swSrc),
            static_cast<double>(ph) / static_cast<double>(shSrc));
        const float placedW = static_cast<float>(static_cast<double>(swSrc) * scale);
        const float placedH = static_cast<float>(static_cast<double>(shSrc) * scale);
        frameOut.picX0 = ix0 + (pw - placedW) * 0.5f;
        frameOut.picY0 = iy0 + (ph - placedH) * 0.5f;
        frameOut.picX1 = frameOut.picX0 + placedW;
        frameOut.picY1 = frameOut.picY0 + placedH;
    }

    frameOut.innerX1 = imgX1;
    frameOut.innerY1 = imgY1;
    frameOut.innerX2 = imgX2;
    frameOut.innerY2 = imgY2;

    (void)primaries;
    return !out.swatches.empty();
}

void LSPPaletteComposite::drawPaletteSwatchesFromLayout(
    OFX::Image* dst, const OfxRectI& renderWindow, const OverlayLayout& layout) {
    if (!dst || layout.swatches.empty())
        return;
    for (const auto& s : layout.swatches)
        fillRoundRect(dst, s.bx0, s.by0, s.bx1, s.by1, s.rad, s.r, s.g, s.b, 1.0f, renderWindow);
}

void LSPPaletteComposite::compositeImageCellCPU(OFX::Image* dst,
    OFX::Image* src,
    const OfxRectI& dstBounds,
    const OfxRectI& srcBounds,
    const OfxRectI& renderWindow,
    const OverlayLayout& layout,
    const CompositeFrame& frame) {
    if (!dst || !src)
        return;

    const float br = frame.barR;
    const float bgc = frame.barG;
    const float bb = frame.barB;

    const float pw = frame.picX1 - frame.picX0;
    const float ph = frame.picY1 - frame.picY0;
    const float sxSpan = static_cast<float>(srcBounds.x2 - srcBounds.x1);
    const float sySpan = static_cast<float>(srcBounds.y2 - srcBounds.y1);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        if (y < dstBounds.y1 || y >= dstBounds.y2)
            continue;
        bool rowHasPicture = false;
        if (pw > 1e-4f && ph > 1e-4f) {
            const float fy = static_cast<float>(y) + 0.5f;
            if (fy >= frame.picY0 && fy < frame.picY1) {
                const float x0 = static_cast<float>(renderWindow.x1) + 0.5f;
                const float x1 = static_cast<float>(renderWindow.x2) - 0.5f;
                if (x1 >= frame.picX0 && x0 < frame.picX1)
                    rowHasPicture = true;
            }
        }
        if (!rowHasPicture) {
            float* rowBase = static_cast<float*>(dst->getPixelAddress(renderWindow.x1, y));
            if (rowBase) {
                const int xEnd = std::min(renderWindow.x2, dstBounds.x2);
                for (int x = std::max(renderWindow.x1, dstBounds.x1); x < xEnd; ++x) {
                    float* p = rowBase + static_cast<std::size_t>(x - renderWindow.x1) * 4u;
                    p[0] = br;
                    p[1] = bgc;
                    p[2] = bb;
                    p[3] = 1.0f;
                }
            }
            continue;
        }
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            if (x < dstBounds.x1 || x >= dstBounds.x2)
                continue;

            if (x >= layout.bgX0 && x < layout.bgX1 && y >= layout.bgY0 && y < layout.bgY1) {
                setPixel(dst, x, y, br, bgc, bb, 1.0f, renderWindow);
                continue;
            }

            const float fx = static_cast<float>(x) + 0.5f;
            const float fy = static_cast<float>(y) + 0.5f;
            if (pw > 1e-4f && ph > 1e-4f && fx >= frame.picX0 && fx < frame.picX1 && fy >= frame.picY0 && fy < frame.picY1) {
                float u = 0.0f;
                float v = 0.0f;
                if (frame.pictureCoverCrop) {
                    const float scale = std::max(pw / sxSpan, ph / sySpan);
                    u = static_cast<float>(srcBounds.x1) + (sxSpan - pw / scale) * 0.5f + (fx - frame.picX0) / scale;
                    v = static_cast<float>(srcBounds.y1) + (sySpan - ph / scale) * 0.5f + (fy - frame.picY0) / scale;
                } else {
                    u = static_cast<float>(srcBounds.x1) + (fx - frame.picX0) * sxSpan / pw;
                    v = static_cast<float>(srcBounds.y1) + (fy - frame.picY0) * sySpan / ph;
                }
                float px[4];
                sampleBilinear(src, srcBounds, u, v, px);
                setPixel(dst, x, y, px[0], px[1], px[2], px[3], renderWindow);
            } else
                setPixel(dst, x, y, br, bgc, bb, 1.0f, renderWindow);
        }
    }
}
