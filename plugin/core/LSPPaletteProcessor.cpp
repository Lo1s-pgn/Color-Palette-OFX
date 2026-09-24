#include "LSPPaletteProcessor.h"

#include "LSPPaletteLog.h"
#include "LSPPaletteRender.h"
#include "LSPPaletteRuntimeEnv.h"
#include "ofxsImageEffect.h"

#include <cstring>

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
#include "LSPPaletteMetal.h"
#endif

namespace {

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
    gpuCompositeDone_ = false;
}

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
bool LSPPaletteProcessor::tryMetalPassthrough() {
    if (!_isEnabledMetalRender || _pMetalCmdQ == nullptr || !_dstImg || !srcImg_)
        return false;
    const void* srcMetal = srcImg_->getPixelData();
    void* dstMetal = _dstImg->getPixelData();
    if (srcMetal == nullptr || dstMetal == nullptr)
        return false;
    if (srcMetal == dstMetal)
        return true;
    const OfxRectI db = _dstImg->getBounds();
    const int width = db.x2 - db.x1;
    const int height = db.y2 - db.y1;
    if (width <= 0 || height <= 0)
        return false;
    const int srcRb = srcImg_->getRowBytes();
    const int dstRb = _dstImg->getRowBytes();
    const size_t srcRowBytes = srcRb < 0 ? static_cast<size_t>(-srcRb) : static_cast<size_t>(srcRb);
    const size_t dstRowBytes = dstRb < 0 ? static_cast<size_t>(-dstRb) : static_cast<size_t>(dstRb);
    return LSPPaletteMetal::blitCopyHostBuffers(
        srcMetal, dstMetal, width, height, srcRowBytes, dstRowBytes, db.x1, db.y1, _pMetalCmdQ);
}
#endif

bool LSPPaletteProcessor::tryGpuComposite() {
    if (!renderProc_ || !_dstImg || !srcImg_ || !drawOverlay_ || !overlayValid_)
        return false;
    const OfxRectI db = _dstImg->getBounds();
    const int width = db.x2 - db.x1;
    const int height = db.y2 - db.y1;
    if (width <= 0 || height <= 0)
        return false;
    const size_t rowBytes = static_cast<size_t>(width) * 4u * sizeof(float);
    const int srcRb = srcImg_->getRowBytes();
    const int dstRb = _dstImg->getRowBytes();
    const size_t srcRowBytes = srcRb < 0 ? static_cast<size_t>(-srcRb) : static_cast<size_t>(srcRb);
    const size_t dstRowBytes = dstRb < 0 ? static_cast<size_t>(-dstRb) : static_cast<size_t>(dstRb);

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
    if (_isEnabledMetalRender && _pMetalCmdQ != nullptr && LSPPaletteRuntimeEnv::preferHostMetal()) {
        const void* srcMetal = srcImg_->getPixelData();
        void* dstMetal = _dstImg->getPixelData();
        if (LSPPaletteMetal::hostPointerIsMTLBuffer(srcMetal)
            && LSPPaletteMetal::hostPointerIsMTLBuffer(dstMetal)
            && renderProc_->renderMetalHostBuffers(srcMetal, dstMetal, width, height, srcRowBytes, dstRowBytes, _pMetalCmdQ)) {
            gpuCompositeDone_ = true;
            LSPPaletteRuntimeEnv::logGpuBackend("metal_host_proc");
            return true;
        }
        LSPPaletteRuntimeEnv::logGpuBackend("metal_host_proc_failed");
        LSP_PALETTE_LOG_ERROR("metal_host_composite_failed");
        return false;
    }
#endif

#if defined(LSP_PALETTE_HAS_CUDA)
    if (_isEnabledCudaRender && _pCudaStream != nullptr) {
        const float* srcDev = static_cast<const float*>(srcImg_->getPixelData());
        float* dstDev = static_cast<float*>(_dstImg->getPixelData());
        if (srcDev != nullptr && dstDev != nullptr
            && renderProc_->renderCUDAHostBuffers(srcDev, dstDev, width, height, srcRowBytes, dstRowBytes, _pCudaStream)) {
            gpuCompositeDone_ = true;
            LSPPaletteRuntimeEnv::logGpuBackend("cuda_host_proc");
            return true;
        }
        return false;
    }
#endif

    if (_isEnabledMetalRender || _isEnabledCudaRender)
        return false;

    const LSPPaletteRowLayout srcLay = detectPaletteRowLayout(srcImg_, db, height, rowBytes);
    const LSPPaletteRowLayout dstLay = detectPaletteRowLayout(_dstImg, db, height, rowBytes);
    if (!LSPPaletteRuntimeEnv::forceStageCopyEnabled() && srcLay.valid && dstLay.valid) {
        const bool preferCuda = _isEnabledCudaRender;
        if (renderProc_->renderWithLayout(
                srcLay.base, dstLay.base, width, height, srcLay.pitchBytes, dstLay.pitchBytes, preferCuda)) {
            gpuCompositeDone_ = true;
            return true;
        }
    }
    float* srcBase = static_cast<float*>(srcImg_->getPixelData());
    float* dstBase = static_cast<float*>(_dstImg->getPixelData());
    if (srcBase && dstBase
        && renderProc_->renderWithLayout(srcBase, dstBase, width, height, srcRowBytes, dstRowBytes, _isEnabledCudaRender)) {
        gpuCompositeDone_ = true;
        return true;
    }
    return false;
}

#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
void LSPPaletteProcessor::processImagesMetal() {
    if (drawOverlay_ && overlayValid_ && tryGpuComposite())
        return;
    if (tryMetalPassthrough())
        return;
    LSP_PALETTE_LOG_ERROR("metal_passthrough_failed");
}
#endif

#if defined(OFX_SUPPORTS_CUDARENDER)
void LSPPaletteProcessor::processImagesCuda() {
    if (tryGpuComposite())
        return;
    if (!drawOverlay_ || !overlayValid_)
        copyRGBAWindow(srcImg_, _dstImg, _renderWindow);
}
#endif

#if defined(OFX_SUPPORTS_OPENCLRENDER)
void LSPPaletteProcessor::processImagesOpenCL() {
    if (tryGpuComposite())
        return;
    multiThreadProcessImages(_renderWindow);
}
#endif

void LSPPaletteProcessor::multiThreadProcessImages(OfxRectI window) {
    if (!_dstImg || !srcImg_)
        return;
    if (gpuCompositeDone_)
        return;
    if (_isEnabledMetalRender || _isEnabledCudaRender)
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
    if (gpuCompositeDone_ || !_dstImg || !drawOverlay_ || !overlayValid_)
        return;
#if defined(__APPLE__) && !defined(LSP_PALETTE_VIEWER_CPU_ONLY)
    if (_isEnabledMetalRender)
        return;
#endif
    LSPPaletteComposite::drawPaletteSwatchesFromLayout(_dstImg, _renderWindow, overlayLayout_);
}
