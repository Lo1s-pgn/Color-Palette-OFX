# GPU parity and fallback QA (LSP Color Palette)

Manual checklist for verifying CPU/GPU output parity and backend fallbacks across OFX hosts.

## Environment

| Variable | Effect |
|----------|--------|
| `LSP_PALETTE_GPU_STAGE_DEBUG=1` | Log backend choice and cache hit/miss per frame |
| `LSP_PALETTE_FORCE_STAGE_COPY=1` | Force host staging path (bypass direct layout) |
| `LSP_PALETTE_METAL_RENDER_MODE=INTERNAL` | Disable host Metal buffers on macOS |
| `LSP_PALETTE_DISABLE_OPENCL=1` | Skip OpenCL on Windows/Linux |
| `LSP_PALETTE_FORCE_OPENCL=1` | Prefer OpenCL over CUDA when both built |

## Pixel parity (CPU vs GPU)

1. Apply effect on a still with all four **Layout** choices.
2. Toggle **Full frame** on/off.
3. Export reference with host GPU disabled (if supported) or `LSP_PALETTE_METAL_RENDER_MODE=INTERNAL` on macOS.
4. Re-enable GPU; compare RGB (ignore alpha). Max delta should be negligible (bilinear + SDF AA).

## Swatch edge cases

- `palettePatchCount` = 24, large `paletteCorner`, minimum strip size.
- Confirm rounded swatches match CPU reference in the strip.

## Backend fallback matrix

| Platform | Primary | Fallback chain |
|----------|---------|----------------|
| macOS | Host Metal | Internal Metal → CPU |
| Windows | Host CUDA | Internal CUDA → OpenCL → CPU |
| Linux (CUDA on) | Host CUDA / CUDA | OpenCL → CPU |
| Linux (CUDA off) | OpenCL | CPU |

Force each fallback via host settings or env vars above; output must remain valid (no crash, no black frame).

## Extract pipeline (macOS Metal)

| Stage | Where |
|-------|--------|
| Downsample (analysis slab) | GPU — `LSPPaletteDownsampleKernel` |
| OKLab sample gather | GPU — `LSPPaletteGatherSamplesKernel` |
| Median cut + palette sort | CPU — `LSPPaletteExtract.cpp` |

On cache miss with debug logging, expect **`trace: extract begin`** then **`extract_gpu_gather`** when the Metal gather path runs.

## Playback / cache

1. Set `LSP_PALETTE_GPU_STAGE_DEBUG=1`.
2. Play timeline: expect **cache hits** when image and params are unchanged between frames.
3. Scrub with constant picture: extract should not rerun every frame after first miss.

## Row pitch

Use `LSP_PALETTE_FORCE_STAGE_COPY=1` on hosts with padded row bytes; compare to default path.
