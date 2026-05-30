#pragma once

#include "ColorManagement.h"
#include "LSPPaletteExtract.h"
#include "ofxsImageEffect.h"

#include <vector>

namespace LSPPaletteComposite {

/** 0 Top, 1 Bottom, 2 Left, 3 Right — match paletteLayout choice order. */
constexpr int kLayoutTop = 0;
constexpr int kLayoutBottom = 1;
constexpr int kLayoutLeft = 2;
constexpr int kLayoutRight = 3;

struct Presentation {
    int layout = kLayoutBottom;
    /** Thickness of the palette strip: fraction of frame height (Top/Bottom) or width (Left/Right). */
    double stripFrac = 0.14;
    /** Gap strength 0–1 → pixel gap capped on short side; same for all layouts; not tied to strip Size. */
    double gapFrac = 0.22;
    /** Corner radius as a fraction of min(patch width, patch height). */
    double cornerFrac = 0.10;
    /** When true, palette strip spans full output width (top/bottom) or height (left/right); when false, strip matches the letterboxed picture span only. */
    bool fitPaletteToFrame = false;
    /** When true, picture **covers** the image cell inset from **frame** edges by **Gap** (px); the edge shared with the palette strip has **no** extra inset so the seam gap matches the strip layout (single gap). When false, picture **contains** in the same inset rect (letterbox). */
    bool imageFillCoverCrop = false;
    /** Background / letterbox: neutral linear RGB (R=G=B), clip space 0–1. */
    double bgLightness = 0.0;
};

/** Target for scaled picture (OFX pixel coords, y up). */
struct CompositeFrame {
    int innerX1 = 0;
    int innerY1 = 0;
    int innerX2 = 0;
    int innerY2 = 0;
    float picX0 = 0.0f;
    float picY0 = 0.0f;
    float picX1 = 0.0f;
    float picY1 = 0.0f;
    /** When true, **compositeImageCellCPU** maps **pic*** with object-fit **cover** (center crop); when false, linear stretch of full source to **pic*** (contain). */
    bool pictureCoverCrop = false;
    float barR = 0.5f;
    float barG = 0.5f;
    float barB = 0.5f;
};

/** Palette strip rasterization (CPU). */
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
    WorkshopColor::ColorPrimariesId primaries,
    const Presentation& pres,
    OverlayLayout& overlayOut,
    CompositeFrame& frameOut);

/** Draw swatches only (palette area should already match bar color). */
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
