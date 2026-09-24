#pragma once

#include <cstddef>

#include "LSPPaletteGpuParamsShared.h"
#include "ofxCore.h"

namespace LSPPaletteComposite {
struct OverlayLayout;
struct CompositeFrame;
} // namespace LSPPaletteComposite

namespace LSPPaletteGpuParamsUtil {

void packFromLayout(const OfxRectI& dstBounds,
    const OfxRectI& srcBounds,
    const LSPPaletteComposite::OverlayLayout& overlay,
    const LSPPaletteComposite::CompositeFrame& frame,
    int width,
    int height,
    size_t srcRowBytes,
    size_t dstRowBytes,
    LSPPaletteGpuParams& out);

} // namespace LSPPaletteGpuParamsUtil
