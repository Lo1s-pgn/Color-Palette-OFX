#pragma once

#include "LSPPaletteExtract.h"

#include <vector>

namespace LSPPaletteExtractInternal {

struct OkLabSample {
    float L = 0.0f;
    float a = 0.0f;
    float b = 0.0f;
    double w = 1.0;
};

/** Median-cut + sort (CPU). */
bool finishPalette(std::vector<OkLabSample>& samples,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette);

} // namespace LSPPaletteExtractInternal
