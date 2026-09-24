# Plugin sources — LSP - Color Palette

## Core OFX

- **`core/LSPPalettePlugin.cpp`** — **`ImageEffect`**: **`render`** (fingerprint cache, palette extract, composite plan cache, host Metal/CUDA/OpenCL or **`LSPPaletteProcessor`**), cache invalidation, SUPPORT actions.
- **`core/LSPPaletteDescribe.cpp`** — **INPUT COLOR**, **PALETTE**, **SUPPORT**; host render flags (Metal on macOS, CUDA on Windows).
- **`core/LSPPaletteRenderCache.cpp`** — Extract/plan cache keyed by source fingerprint + params.
- **`core/LSPPaletteRuntimeEnv.cpp`** — **`LSP_PALETTE_GPU_STAGE_DEBUG`**, Metal/CUDA/OpenCL composite preferences.

## Palette extract (OKLab median cut)

- **`core/LSPPaletteExtract.cpp`** — CPU gather fallback, **`medianCutOkLab`**, **`applyPaletteSortOrder`**.
- **`core/LSPPaletteExtractInternal.h`** — Shared **`finishPalette`** (median cut + sort).
- **`core/LSPPaletteGpuExtractShared.h`** — Gather kernel params / sample struct (C++ / Metal).
- **`core/LSPPaletteAnalysis.cpp`** — Downscaled grid sizing; CPU downscale when needed.
- **`metal/LSPPaletteExtract.metal`** — **`LSPPaletteGatherSamplesKernel`** (decode, primaries→sRGB, OKLab).
- **`metal/LSPPaletteExtractGpu.mm`** — Metal gather + CPU **`finishPalette`**.
- **`metal/LSPPaletteMetalStage.mm`** — Host-buffer downsample + staging for extract.

## Composite and render

- **`core/LSPPaletteComposite.cpp`** — **`buildCompositePlan`**, CPU swatch draw.
- **`core/LSPPaletteProcessor.cpp`** — CPU composite; internal GPU via **`LSPPaletteRenderProcessor`**.
- **`core/LSPPaletteRenderProcessor.cpp`** — Metal / CUDA / OpenCL composite.
- **`core/LSPPaletteGpuParams.cpp`** — GPU param packing.

## Image access

- **`core/LSPPaletteImageAccess.cpp`** — **`cpuReadableSlab`** for CPU float RGBA only. Do not use **`getPixelAddress`** on Metal/CUDA device buffers.

## GPU backends

| Path | Files |
|------|--------|
| **Metal** | **`metal/LSPPalette.metal`** (composite + downsample), **`metal/LSPPaletteExtract.metal`** (gather) → **`LSPPalette.metallib`**; **`LSPPaletteMetal.mm`**, **`LSPPaletteMetalStage.mm`**, **`LSPPaletteExtractGpu.mm`** |
| **CUDA** | **`cuda/LSPPalette.cu`** (composite) |
| **OpenCL** | **`opencl/LSPPalette.cl`** (composite) |

## Other

- **`core/LSPPaletteUtil.cpp`**, **`LSPPaletteLog.h`**, **`LSPPaletteConstants.h`**
- **`../common/color/ColorManagement.*`** — Gamut / transfer decode (six UI transfers mirrored in GPU kernels).

**Coordinates:** OFX image space (origin bottom-left, **y up**).
