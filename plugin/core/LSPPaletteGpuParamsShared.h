#pragma once

/** Metal/CUDA/OpenCL/C++ shared POD (no C++ std headers). */
#ifndef kLSPPaletteMaxGpuSwatches
#define kLSPPaletteMaxGpuSwatches 24
#endif

struct LSPPaletteGpuSwatch {
    float bx0;
    float by0;
    float bx1;
    float by1;
    float rad;
    float r;
    float g;
    float b;
    float pad;
};

struct LSPPaletteGpuParams {
    int width;
    int height;
    int originX;
    int originY;
    int srcBoundsX1;
    int srcBoundsY1;
    int srcBoundsX2;
    int srcBoundsY2;
    int dstBoundsX1;
    int dstBoundsY1;
    int dstBoundsX2;
    int dstBoundsY2;
    int bgX0;
    int bgY0;
    int bgX1;
    int bgY1;
    float picX0;
    float picY0;
    float picX1;
    float picY1;
    int pictureCoverCrop;
    float barR;
    float barG;
    float barB;
    int swatchCount;
    LSPPaletteGpuSwatch swatches[kLSPPaletteMaxGpuSwatches];
    int srcRowFloats;
    int dstRowFloats;
    int srcReadOriginX;
    int srcReadOriginY;
};
