#include "LSPPaletteGpuParams.h"

#include "LSPPaletteComposite.h"
#include "ofxCore.h"

namespace LSPPaletteGpuParamsUtil {

void packFromLayout(const OfxRectI& dstBounds,
    const OfxRectI& srcBounds,
    const LSPPaletteComposite::OverlayLayout& overlay,
    const LSPPaletteComposite::CompositeFrame& frame,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    LSPPaletteGpuParams& out) {
    out.width = width;
    out.height = height;
    out.originX = dstBounds.x1;
    out.originY = dstBounds.y1;
    out.srcBoundsX1 = srcBounds.x1;
    out.srcBoundsY1 = srcBounds.y1;
    out.srcBoundsX2 = srcBounds.x2;
    out.srcBoundsY2 = srcBounds.y2;
    out.dstBoundsX1 = dstBounds.x1;
    out.dstBoundsY1 = dstBounds.y1;
    out.dstBoundsX2 = dstBounds.x2;
    out.dstBoundsY2 = dstBounds.y2;
    out.bgX0 = overlay.bgX0;
    out.bgY0 = overlay.bgY0;
    out.bgX1 = overlay.bgX1;
    out.bgY1 = overlay.bgY1;
    out.picX0 = frame.picX0;
    out.picY0 = frame.picY0;
    out.picX1 = frame.picX1;
    out.picY1 = frame.picY1;
    out.pictureCoverCrop = frame.pictureCoverCrop ? 1 : 0;
    out.barR = frame.barR;
    out.barG = frame.barG;
    out.barB = frame.barB;
    const int n = static_cast<int>(overlay.swatches.size());
    out.swatchCount = n > kLSPPaletteMaxGpuSwatches ? kLSPPaletteMaxGpuSwatches : n;
    for (int i = 0; i < out.swatchCount; ++i) {
        const auto& s = overlay.swatches[static_cast<std::size_t>(i)];
        out.swatches[i].bx0 = s.bx0;
        out.swatches[i].by0 = s.by0;
        out.swatches[i].bx1 = s.bx1;
        out.swatches[i].by1 = s.by1;
        out.swatches[i].rad = s.rad;
        out.swatches[i].r = s.r;
        out.swatches[i].g = s.g;
        out.swatches[i].b = s.b;
        out.swatches[i].pad = 0.0f;
    }
    const size_t packed = static_cast<size_t>(width) * 4u * sizeof(float);
    out.srcRowFloats = static_cast<int>((srcRowBytes > 0 ? srcRowBytes : packed) / sizeof(float));
    out.dstRowFloats = static_cast<int>((dstRowBytes > 0 ? dstRowBytes : packed) / sizeof(float));
    out.srcReadOriginX = srcBounds.x1;
    out.srcReadOriginY = srcBounds.y1;
}

} // namespace LSPPaletteGpuParamsUtil
