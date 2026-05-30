#pragma once

#include "LSPPaletteComposite.h"
#include "LSPPaletteExtract.h"
#include "LSPPaletteGpuParams.h"
#include "ColorManagement.h"

#include <cstdint>
#include <vector>

/** Per-instance cache for extraction + composite plan (playback/scrub). */
struct LSPPaletteRenderCache {
    struct ExtractKey {
        double time = -1.0;
        int patchCount = 0;
        int sortOrder = 0;
        WorkshopColor::ColorPrimariesId primaries = WorkshopColor::ColorPrimariesId::Rec709;
        WorkshopColor::TransferFunctionId transfer = WorkshopColor::TransferFunctionId::Gamma24;
        int srcW = 0;
        int srcH = 0;
        uint64_t sourceFingerprint = 0;
    };

    struct PresentationKey {
        int layout = 0;
        bool fitPaletteToFrame = false;
        bool imageFillCoverCrop = false;
        double stripFrac = 0.0;
        double gapFrac = 0.0;
        double cornerFrac = 0.0;
        double bgLightness = 0.0;
        int dstW = 0;
        int dstH = 0;
    };

    ExtractKey extractKey_{};
    PresentationKey presKey_{};
    std::vector<LSPPaletteExtract::Swatch> palette_;
    LSPPaletteComposite::OverlayLayout overlay_{};
    LSPPaletteComposite::CompositeFrame frame_{};
    bool havePlan_ = false;
    uint64_t cacheHits_ = 0;
    uint64_t cacheMisses_ = 0;

    bool extractKeyMatches(const ExtractKey& k) const;
    bool extractParamsMatch(const ExtractKey& k) const;
    bool presKeyMatches(const PresentationKey& k) const;

    /** Params match and palette cached (fingerprint not checked). */
    bool hasCachedPaletteForParams(const ExtractKey& k) const;

    /** Lightweight hash of downsampled source RGB (see LSPPaletteRenderCache.cpp). */
    static uint64_t computeSourceFingerprint(OFX::Image* src, const OfxRectI& bounds);

    static uint64_t computeSourceFingerprintFromSlab(const float* slab, int rowBytes, const OfxRectI& bounds);

    bool tryGetCachedExtract(const ExtractKey& key, std::vector<LSPPaletteExtract::Swatch>& outPalette);

    void storeExtract(const ExtractKey& key, const std::vector<LSPPaletteExtract::Swatch>& palette);

    bool tryGetCachedPlan(const PresentationKey& key,
        LSPPaletteComposite::OverlayLayout& overlay,
        LSPPaletteComposite::CompositeFrame& frame,
        bool& havePlan);

    void storePlan(const PresentationKey& key,
        const LSPPaletteComposite::OverlayLayout& overlay,
        const LSPPaletteComposite::CompositeFrame& frame,
        bool havePlan);

    void invalidateAll();
    void bumpMiss();
};
