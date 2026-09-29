#pragma once

#include "ColorManagement.h"
#include "LSPPaletteExtract.h"
#include "ofxsImageEffect.h"

#include <vector>

namespace LSPPaletteComposite {

// layout choice order: 0 top, 1 bottom, 2 left, 3 right
constexpr int kLayoutTop = 0;
constexpr int kLayoutBottom = 1;
constexpr int kLayoutLeft = 2;
constexpr int kLayoutRight = 3;

struct Presentation {
    int layout = kLayoutBottom;
    double stripFrac = 0.14;
    double gapFrac = 0.22;
    double cornerFrac = 0.10;
    // full width strip vs only as wide as the picture
    bool fitPaletteToFrame = false;
    // cover fills the inset cell, contain letterboxes inside it
    bool imageFillCoverCrop = false;
    double bgLightness = 0.0;
};

// picture placement, OFX coords y up
struct CompositeFrame {
    int innerX1 = 0;
    int innerY1 = 0;
    int innerX2 = 0;
    int innerY2 = 0;
    float picX0 = 0.0f;
    float picY0 = 0.0f;
    float picX1 = 0.0f;
    float picY1 = 0.0f;
    bool pictureCoverCrop = false;
    float barR = 0.5f;
    float barG = 0.5f;
    float barB = 0.5f;
};

struct OverlayLayout {
    float barR = 0.5f;
    float barG = 0.5f;
    float barB = 0.5f;
    int bgX0 = 0;
    int bgY0 = 0;
    int bgX1 = 0;
    int bgY1 = 0;
    struct SwatchDraw {
        float bx0 = 0.0f;
        float by0 = 0.0f;
        float bx1 = 0.0f;
        float by1 = 0.0f;
        float rad = 0.0f;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };
    std::vector<SwatchDraw> swatches;
};

bool buildCompositePlan(const OfxRectI& dstBounds,
    const OfxRectI& srcBounds,
    const std::vector<LSPPaletteExtract::Swatch>& palette,
    WorkshopColor::TransferFunctionId transfer,
    const Presentation& pres,
    OverlayLayout& overlayOut,
    CompositeFrame& frameOut);

// swatches only, bar color should already be there
void drawPaletteSwatchesFromLayout(OFX::Image* dst,
    const OfxRectI& renderWindow,
    const OverlayLayout& layout);

void compositeImageCellCPU(OFX::Image* dst,
    OFX::Image* src,
    const OfxRectI& dstBounds,
    const OfxRectI& srcBounds,
    const OfxRectI& renderWindow,
    const OverlayLayout& layout,
    const CompositeFrame& frame);

} // namespace LSPPaletteComposite
