#pragma once

#include <cstddef>

#include "ofxsImageEffect.h"

struct LSPPaletteRowLayout {
    float* base = nullptr;
    size_t pitchBytes = 0;
    bool valid = false;
    bool contiguous = false;
};

LSPPaletteRowLayout detectPaletteRowLayout(OFX::Image* img, const OfxRectI& bounds, int height, size_t rowBytes);
