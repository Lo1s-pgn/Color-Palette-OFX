#pragma once

// oklab gather on gpu, cut on cpu
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
