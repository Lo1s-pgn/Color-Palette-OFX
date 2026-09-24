#include "LSPPaletteRenderCache.h"

#include "LSPPaletteAnalysis.h"
#include "LSPPaletteExtract.h"
#include "LSPPaletteExtractInternal.h"
#include "LSPPaletteImageAccess.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int kFingerprintGrid = LSPPaletteAnalysis::kFingerprintGrid;

uint64_t fnv1a64(uint64_t h, uint8_t b) {
    h ^= static_cast<uint64_t>(b);
    h *= 1099511628211ull;
    return h;
}

} // namespace

bool LSPPaletteRenderCache::extractParamsMatch(const ExtractKey& k) const {
    return extractKey_.patchCount == k.patchCount && extractKey_.sortOrder == k.sortOrder
        && extractKey_.primaries == k.primaries && extractKey_.transfer == k.transfer && extractKey_.srcW == k.srcW
        && extractKey_.srcH == k.srcH;
}

bool LSPPaletteRenderCache::clusterKeyMatches(const ExtractKey& k) const {
    return clusterKey_.patchCount == k.patchCount && clusterKey_.primaries == k.primaries
        && clusterKey_.transfer == k.transfer && clusterKey_.srcW == k.srcW && clusterKey_.srcH == k.srcH
        && clusterKey_.sourceFingerprint != 0 && clusterKey_.sourceFingerprint == k.sourceFingerprint;
}

bool LSPPaletteRenderCache::hasCachedPaletteForParams(const ExtractKey& k) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return extractParamsMatch(k) && !palette_.empty() && extractKey_.sourceFingerprint != 0;
}

bool LSPPaletteRenderCache::hasCachedClusterForParams(const ExtractKey& k) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clusterKey_.patchCount == k.patchCount && clusterKey_.primaries == k.primaries
        && clusterKey_.transfer == k.transfer && clusterKey_.srcW == k.srcW && clusterKey_.srcH == k.srcH
        && !paletteCluster_.empty() && clusterKey_.sourceFingerprint != 0;
}

bool LSPPaletteRenderCache::extractKeyMatches(const ExtractKey& k) const {
    (void)k.time;
    (void)extractKey_.time;
    return extractKey_.patchCount == k.patchCount && extractKey_.sortOrder == k.sortOrder
        && extractKey_.primaries == k.primaries && extractKey_.transfer == k.transfer && extractKey_.srcW == k.srcW
        && extractKey_.srcH == k.srcH && extractKey_.sourceFingerprint == k.sourceFingerprint;
}

bool LSPPaletteRenderCache::presKeyMatches(const PresentationKey& k) const {
    return presKey_.layout == k.layout && presKey_.fitPaletteToFrame == k.fitPaletteToFrame
        && presKey_.imageFillCoverCrop == k.imageFillCoverCrop && presKey_.stripFrac == k.stripFrac
        && presKey_.gapFrac == k.gapFrac && presKey_.cornerFrac == k.cornerFrac && presKey_.bgLightness == k.bgLightness
        && presKey_.dstW == k.dstW && presKey_.dstH == k.dstH;
}

uint64_t LSPPaletteRenderCache::computeSourceFingerprintFromSlab(const float* slab, int rowBytes, const OfxRectI& bounds) {
    if (!slab || rowBytes < 16)
        return 0;
    const int fw = bounds.x2 - bounds.x1;
    const int fh = bounds.y2 - bounds.y1;
    if (fw < 1 || fh < 1)
        return 0;
    uint64_t h = 14695981039346656037ull;
    for (int ty = 0; ty < kFingerprintGrid; ++ty) {
        const int sy = bounds.y1 + (ty * fh) / kFingerprintGrid;
        for (int tx = 0; tx < kFingerprintGrid; ++tx) {
            const int sx = bounds.x1 + (tx * fw) / kFingerprintGrid;
            const float* px = LSPPaletteImageAccess::rgbaAtFromSlab(bounds, rowBytes, slab, sx, sy);
            if (!px)
                continue;
            for (int c = 0; c < 3; ++c) {
                const int q = static_cast<int>(std::floor(px[c] * 255.0f + 0.5f));
                const uint8_t b = static_cast<uint8_t>(std::clamp(q, 0, 255));
                h = fnv1a64(h, b);
            }
        }
    }
    h = fnv1a64(h, static_cast<uint8_t>(fw & 0xff));
    h = fnv1a64(h, static_cast<uint8_t>((fw >> 8) & 0xff));
    h = fnv1a64(h, static_cast<uint8_t>(fh & 0xff));
    h = fnv1a64(h, static_cast<uint8_t>((fh >> 8) & 0xff));
    return h;
}

uint64_t LSPPaletteRenderCache::computeSourceFingerprint(OFX::Image* src, const OfxRectI& bounds) {
    if (!src || !src->getPixelData())
        return 0;
    const float* slab = LSPPaletteImageAccess::cpuReadableSlab(src);
    if (!slab)
        return 0;
    return computeSourceFingerprintFromSlab(slab, src->getRowBytes(), bounds);
}

bool LSPPaletteRenderCache::tryGetCachedExtract(const ExtractKey& key, std::vector<LSPPaletteExtract::Swatch>& outPalette) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!extractKeyMatches(key))
        return false;
    outPalette = palette_;
    ++cacheHits_;
    return true;
}

void LSPPaletteRenderCache::storeExtract(const ExtractKey& key, const std::vector<LSPPaletteExtract::Swatch>& palette) {
    std::lock_guard<std::mutex> lock(mutex_);
    extractKey_ = key;
    palette_ = palette;
}

void LSPPaletteRenderCache::storeCluster(const ExtractKey& key, const std::vector<LSPPaletteExtract::Swatch>& clusterPalette) {
    std::lock_guard<std::mutex> lock(mutex_);
    clusterKey_ = key;
    clusterKey_.sortOrder = 0;
    paletteCluster_ = clusterPalette;
}

bool LSPPaletteRenderCache::tryResortCachedCluster(const ExtractKey& key,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!clusterKeyMatches(key) || paletteCluster_.empty())
        return false;
    outPalette = paletteCluster_;
    LSPPaletteExtractInternal::applyPaletteSortOrder(outPalette, settings);
    extractKey_ = key;
    palette_ = outPalette;
    ++cacheHits_;
    return true;
}

bool LSPPaletteRenderCache::tryGetCachedPlan(const PresentationKey& key,
    const ExtractKey& extractKey,
    LSPPaletteComposite::OverlayLayout& overlay,
    LSPPaletteComposite::CompositeFrame& frame,
    bool& havePlan) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!presKeyMatches(key) || palette_.empty() || !havePlan_)
        return false;
    if (planExtractKey_.patchCount != extractKey.patchCount || planExtractKey_.sortOrder != extractKey.sortOrder
        || planExtractKey_.primaries != extractKey.primaries || planExtractKey_.transfer != extractKey.transfer
        || planExtractKey_.sourceFingerprint != extractKey.sourceFingerprint)
        return false;
    overlay = overlay_;
    frame = frame_;
    havePlan = havePlan_;
    ++cacheHits_;
    return true;
}

void LSPPaletteRenderCache::storePlan(const PresentationKey& key,
    const ExtractKey& extractKey,
    const LSPPaletteComposite::OverlayLayout& overlay,
    const LSPPaletteComposite::CompositeFrame& frame,
    bool havePlan) {
    std::lock_guard<std::mutex> lock(mutex_);
    presKey_ = key;
    planExtractKey_ = extractKey;
    overlay_ = overlay;
    frame_ = frame;
    havePlan_ = havePlan;
}

void LSPPaletteRenderCache::invalidateAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    extractKey_.time = -1.0;
    clusterKey_.sourceFingerprint = 0;
    planExtractKey_.sourceFingerprint = 0;
    presKey_.dstW = -1;
    palette_.clear();
    paletteCluster_.clear();
    havePlan_ = false;
}

void LSPPaletteRenderCache::invalidateExtract() {
    std::lock_guard<std::mutex> lock(mutex_);
    extractKey_.sourceFingerprint = 0;
    clusterKey_.sourceFingerprint = 0;
    paletteCluster_.clear();
    planExtractKey_.sourceFingerprint = 0;
    presKey_.dstW = -1;
    havePlan_ = false;
}

void LSPPaletteRenderCache::invalidatePlan() {
    std::lock_guard<std::mutex> lock(mutex_);
    presKey_.dstW = -1;
    planExtractKey_.sourceFingerprint = 0;
    havePlan_ = false;
}

void LSPPaletteRenderCache::bumpMiss() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++cacheMisses_;
}

bool LSPPaletteRenderCache::copyStalePalette(std::vector<LSPPaletteExtract::Swatch>& outPalette) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (palette_.empty())
        return false;
    outPalette = palette_;
    return true;
}
