#pragma once

/** Metal downsample kernel POD (C++/Metal shared). */
struct LSPPaletteGpuDownsampleParams {
    int srcReadOriginX;
    int srcReadOriginY;
    int srcRowFloats;
    int srcW;
    int srcH;
    int dstW;
    int dstH;
};
