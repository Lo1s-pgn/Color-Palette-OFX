#pragma once

#include "LSPPaletteComposite.h"
#include "LSPPaletteExtract.h"
#include "LSPPaletteGpuParams.h"
#include "ColorManagement.h"

#include <cstdint>
#include <mutex>
#include <vector>

// keeps palette and layout around while scrubbing playback
struct LSPPaletteRenderCache {
    struct ExtractKey {
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
    ExtractKey clusterKey_{};
    ExtractKey planExtractKey_{};
    PresentationKey presKey_{};
    std::vector<LSPPaletteExtract::Swatch> palette_;
    std::vector<LSPPaletteExtract::Swatch> paletteCluster_;
    LSPPaletteComposite::OverlayLayout overlay_{};
    LSPPaletteComposite::CompositeFrame frame_{};
    bool havePlan_ = false;
    uint64_t cacheHits_ = 0;
    uint64_t cacheMisses_ = 0;
    mutable std::mutex mutex_;

    bool extractKeyMatches(const ExtractKey& k) const;
    bool extractParamsMatch(const ExtractKey& k) const;
    bool clusterKeyMatches(const ExtractKey& k) const;
    bool presKeyMatches(const PresentationKey& k) const;

    bool hasCachedPaletteForParams(const ExtractKey& k) const;

    bool hasCachedClusterForParams(const ExtractKey& k) const;

    static uint64_t computeSourceFingerprint(OFX::Image* src, const OfxRectI& bounds);

    static uint64_t computeSourceFingerprintFromSlab(const float* slab, int rowBytes, const OfxRectI& bounds);

    bool tryGetCachedExtract(const ExtractKey& key, std::vector<LSPPaletteExtract::Swatch>& outPalette);

    void storeExtract(const ExtractKey& key, const std::vector<LSPPaletteExtract::Swatch>& palette);

    void storeCluster(const ExtractKey& key, const std::vector<LSPPaletteExtract::Swatch>& clusterPalette);

    bool tryResortCachedCluster(const ExtractKey& key,
        const LSPPaletteExtract::Settings& settings,
        std::vector<LSPPaletteExtract::Swatch>& outPalette);

    bool tryGetCachedPlan(const PresentationKey& key,
        const ExtractKey& extractKey,
        LSPPaletteComposite::OverlayLayout& overlay,
        LSPPaletteComposite::CompositeFrame& frame,
        bool& havePlan);

    // ancienne palette
    bool copyStalePalette(std::vector<LSPPaletteExtract::Swatch>& outPalette) const;

    void storePlan(const PresentationKey& key,
        const ExtractKey& extractKey,
        const LSPPaletteComposite::OverlayLayout& overlay,
        const LSPPaletteComposite::CompositeFrame& frame,
        bool havePlan);

    void invalidateAll();
    void invalidateExtract();
    void invalidatePlan();
    void bumpMiss();
};
