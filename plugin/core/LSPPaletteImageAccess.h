#pragma once

#include "ofxsCore.h"

#include <cstddef>
#include <cstdint>

namespace OFX {
class Image;
}

namespace LSPPaletteImageAccess {

inline const float* rgbaAtFromSlab(const OfxRectI& bounds, int rowBytes, const float* slabBase, int x, int y) {
    if (!slabBase || rowBytes < 16)
        return nullptr;
    if (x < bounds.x1 || x >= bounds.x2 || y < bounds.y1 || y >= bounds.y2)
        return nullptr;
    const auto* base = reinterpret_cast<const std::uint8_t*>(slabBase);
    return reinterpret_cast<const float*>(
        base + static_cast<std::size_t>(y - bounds.y1) * static_cast<std::size_t>(rowBytes)
            + static_cast<std::size_t>(x - bounds.x1) * sizeof(float) * 4u);
}

/** CPU OFX image: contiguous RGBA float slab from lower-left corner (OFX pixel coordinates). */
const float* cpuReadableSlab(OFX::Image* img);

} // namespace LSPPaletteImageAccess
