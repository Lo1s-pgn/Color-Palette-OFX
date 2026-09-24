#include "LSPPaletteImageAccess.h"

#include "ofxsImageEffect.h"

namespace LSPPaletteImageAccess {

const float* cpuReadableSlab(OFX::Image* img) {
    if (!img)
        return nullptr;
    const OfxRectI bounds = img->getBounds();
    const int rowBytes = img->getRowBytes();
    const int h = bounds.y2 - bounds.y1;
    if (h < 1 || rowBytes < 16)
        return nullptr;
    return static_cast<const float*>(img->getPixelAddress(bounds.x1, bounds.y1));
}

} // namespace LSPPaletteImageAccess
