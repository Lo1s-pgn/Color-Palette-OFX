#ifndef LSP_PALETTE_PROCESSOR_H
#define LSP_PALETTE_PROCESSOR_H

#include "LSPPaletteComposite.h"
#include "LSPPaletteRenderProcessor.h"
#include "ofxsProcessing.h"

class LSPPaletteProcessor : public OFX::ImageProcessor {
public:
    explicit LSPPaletteProcessor(OFX::ImageEffect& effect);

    void setSrcImg(OFX::Image* img) { srcImg_ = img; }
    void setDrawOverlay(bool v) { drawOverlay_ = v; }
    void setCompositorState(const LSPPaletteComposite::OverlayLayout& lay, const LSPPaletteComposite::CompositeFrame& frame, bool valid);
    void setRenderProcessor(LSPPaletteRenderProcessor* proc) { renderProc_ = proc; }

    void multiThreadProcessImages(OfxRectI window) override;
    void postProcess() override;

#if defined(__APPLE__)
    void processImagesMetal() override;
#endif
#if defined(OFX_SUPPORTS_CUDARENDER)
    void processImagesCuda() override;
#endif
#if defined(OFX_SUPPORTS_OPENCLRENDER)
    void processImagesOpenCL() override;
#endif

private:
    bool tryGpuComposite();
    bool tryMetalPassthrough();

    OFX::Image* srcImg_ = nullptr;
    bool drawOverlay_ = false;
    bool overlayValid_ = false;
    bool gpuCompositeDone_ = false;
    LSPPaletteComposite::OverlayLayout overlayLayout_;
    LSPPaletteComposite::CompositeFrame compositeFrame_;
    LSPPaletteRenderProcessor* renderProc_ = nullptr;
};

#endif
