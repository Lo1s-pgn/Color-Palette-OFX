#pragma once

/** Metal / C++ shared POD for OKLAB sample gather (median cut stays on CPU). */
struct LSPPaletteGpuOkLabSample {
    float L;
    float a;
    float b;
    float w;
    int valid;
};

struct LSPPaletteGpuExtractGatherParams {
    int slabWidth;
    int slabHeight;
    int slabRowFloats;
    int gridW;
    int gridH;
    int sampleCap;
    int transferChoice;
    float nearBlackLinear;
    float primariesToSrgb[9];
};
