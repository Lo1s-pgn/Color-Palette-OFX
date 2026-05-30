#ifndef LSP_PALETTE_PROCESSOR_H
#define LSP_PALETTE_PROCESSOR_H

#include "LSPPaletteComposite.h"
#include "ofxsProcessing.h"

class LSPPaletteProcessor : public OFX::ImageProcessor {
public:
    explicit LSPPaletteProcessor(OFX::ImageEffect& effect);

    void setSrcImg(OFX::Image* img) { srcImg_ = img; }
    void setDrawOverlay(bool v) { drawOverlay_ = v; }
    void setCompositorState(const LSPPaletteComposite::OverlayLayout& lay, const LSPPaletteComposite::CompositeFrame& frame, bool valid);
    void multiThreadProcessImages(OfxRectI window) override;
    void postProcess() override;

private:
    OFX::Image* srcImg_ = nullptr;
    bool drawOverlay_ = false;
    bool overlayValid_ = false;
    LSPPaletteComposite::OverlayLayout overlayLayout_;
    LSPPaletteComposite::CompositeFrame compositeFrame_;
};

#endif
