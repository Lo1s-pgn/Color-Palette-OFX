#include "LSPPaletteProcessor.h"

#include "LSPPaletteLog.h"
#include "ofxsImageEffect.h"

#include <cstring>

namespace {

/** Row-wise RGBA passthrough — one address lookup per row + memcpy of the contiguous span,
 *  instead of a pair of getPixelAddress() calls per pixel. Each pixel is 4 floats (16 B). */
void copyRGBAWindow(OFX::Image* src, OFX::Image* dst, const OfxRectI& window) {
    if (!src || !dst)
        return;
    const int w = window.x2 - window.x1;
    if (w <= 0)
        return;
    const std::size_t rowBytes = static_cast<std::size_t>(w) * 4u * sizeof(float);
    for (int y = window.y1; y < window.y2; ++y) {
        void* d = dst->getPixelAddress(window.x1, y);
        const void* s = src->getPixelAddress(window.x1, y);
        if (!d || !s)
            continue;
        std::memcpy(d, s, rowBytes);
    }
}

} // namespace

LSPPaletteProcessor::LSPPaletteProcessor(OFX::ImageEffect& effect)
    : OFX::ImageProcessor(effect) {}

void LSPPaletteProcessor::setCompositorState(
    const LSPPaletteComposite::OverlayLayout& lay, const LSPPaletteComposite::CompositeFrame& frame, bool valid) {
    overlayLayout_ = lay;
    compositeFrame_ = frame;
    overlayValid_ = valid;
}

void LSPPaletteProcessor::multiThreadProcessImages(OfxRectI window) {
    if (!_dstImg || !srcImg_)
        return;
    const OfxRectI db = _dstImg->getBounds();
    const OfxRectI sb = srcImg_->getBounds();
    if (drawOverlay_ && overlayValid_) {
        LSPPaletteComposite::compositeImageCellCPU(
            _dstImg, srcImg_, db, sb, window, overlayLayout_, compositeFrame_);
    } else
        copyRGBAWindow(srcImg_, _dstImg, window);
}

void LSPPaletteProcessor::postProcess() {
    if (!_dstImg || !drawOverlay_ || !overlayValid_)
        return;
    LSPPaletteComposite::drawPaletteSwatchesFromLayout(_dstImg, _renderWindow, overlayLayout_);
}
