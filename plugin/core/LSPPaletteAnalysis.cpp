#include "LSPPaletteAnalysis.h"

#include "LSPPaletteImageAccess.h"

#include <algorithm>
#include <cmath>

namespace LSPPaletteAnalysis {

void computeDownscaledSize(int fullW, int fullH, int maxSide, int& outW, int& outH) {
    outW = fullW;
    outH = fullH;
    if (fullW < 1 || fullH < 1 || maxSide < 1)
        return;
    const int longSide = std::max(fullW, fullH);
    if (longSide <= maxSide)
        return;
    const float scale = static_cast<float>(maxSide) / static_cast<float>(longSide);
    outW = std::max(1, static_cast<int>(std::floor(static_cast<float>(fullW) * scale)));
    outH = std::max(1, static_cast<int>(std::floor(static_cast<float>(fullH) * scale)));
}

OfxRectI makeTightBounds(int outW, int outH) {
    OfxRectI b{};
    b.x1 = 0;
    b.y1 = 0;
    b.x2 = outW;
    b.y2 = outH;
    return b;
}

bool downscaleRgbaSlab(const float* src,
    int srcRowBytes,
    const OfxRectI& srcBounds,
    int maxSide,
    std::vector<float>& outPacked,
    int& outW,
    int& outH) {
    if (!src || srcRowBytes < 16)
        return false;
    const int fw = srcBounds.x2 - srcBounds.x1;
    const int fh = srcBounds.y2 - srcBounds.y1;
    if (fw < 1 || fh < 1)
        return false;
    computeDownscaledSize(fw, fh, maxSide, outW, outH);
    const std::size_t nFloats = static_cast<std::size_t>(outW) * static_cast<std::size_t>(outH) * 4u;
    outPacked.resize(nFloats);
    const int dstRowFloats = outW * 4;
    for (int dy = 0; dy < outH; ++dy) {
        const int sy = srcBounds.y1 + (dy * fh) / outH;
        for (int dx = 0; dx < outW; ++dx) {
            const int sx = srcBounds.x1 + (dx * fw) / outW;
            const float* px = LSPPaletteImageAccess::rgbaAtFromSlab(srcBounds, srcRowBytes, src, sx, sy);
            float* dst = outPacked.data() + static_cast<std::size_t>(dy) * static_cast<std::size_t>(dstRowFloats)
                + static_cast<std::size_t>(dx) * 4u;
            if (px) {
                dst[0] = px[0];
                dst[1] = px[1];
                dst[2] = px[2];
                dst[3] = px[3];
            } else {
                dst[0] = dst[1] = dst[2] = 0.0f;
                dst[3] = 1.0f;
            }
        }
    }
    return true;
}

} // namespace LSPPaletteAnalysis
