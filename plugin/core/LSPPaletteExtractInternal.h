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

// median cut then sort, optional unsorted cluster copy
bool finishPalette(std::vector<OkLabSample>& samples,
    const LSPPaletteExtract::Settings& settings,
    std::vector<LSPPaletteExtract::Swatch>& outPalette,
    std::vector<LSPPaletteExtract::Swatch>* outClusterUnsorted = nullptr);

// tri des swatches
void applyPaletteSortOrder(std::vector<LSPPaletteExtract::Swatch>& palette,
    const LSPPaletteExtract::Settings& settings);

} // namespace LSPPaletteExtractInternal
