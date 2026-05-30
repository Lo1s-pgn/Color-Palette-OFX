# Plugin sources — LSP - Color Palette

- **`core/LSPPalettePlugin.cpp`** — **`ImageEffect`**: **`render`** (**`extractDominantColors`** every call, **`buildCompositePlan`**, **`LSPPaletteProcessor`**), **`changedParam`** (Help / Issues / Open Log), session log header.
- **`core/LSPPaletteDescribe.cpp`** — **INPUT COLOR**, **PALETTE** (layout, **`paletteFullFrame`**, …), **SUPPORT**.
- **`core/LSPPaletteExtract.cpp`** — OKLAB **median cut** on weighted grid samples; inverse OKLAB → linear primaries RGB; optional blur; canonical OKLAB slot order; strip order from **`paletteSortOrder`** (Weight / Lightness / Hue / Saturation).
- **`core/LSPPaletteGridBlur.{h,cpp}`** — Separable Gaussian (replicate edge) on **RGBA32F** row-major grids for extraction.
- **`metal/LSPPaletteGridBlurMPS.mm`** — **`MPSImageGaussianBlur`** path for the same grid when Metal/MPS succeed (macOS only, Objective-C++).
- **`core/LSPPaletteGridBlurMPSStub.cpp`** — Non-macOS stub: **`tryMpsGaussianBlur`** returns false (CPU path always used).
- **`core/LSPPaletteComposite.cpp`** — **`buildCompositePlan`**: strip + picture layout; **Gap** insets; **`paletteFullFrame`** = wide strip + cover; neutral **bgLightness** bar; **`drawPaletteSwatchesFromLayout`**. **OFX coords**: bottom-left, **y up**.
- **`core/LSPPaletteImageAccess.cpp`** — **`cpuReadableSlab`** for OFX CPU float RGBA (`getPixelAddress`); **`rgbaAtFromSlab`** for row-major slab indexing.
- **`core/LSPPaletteProcessor.cpp`** — CPU multi-thread **`compositeImageCellCPU`** vs full-window copy; **`postProcess`** draws rounded swatches.
- **`core/LSPPaletteUtil.cpp`** — Open URL / log file (macOS **`open`**, Windows **`ShellExecuteA`**, Linux **`xdg-open`**).
- **`core/LSPPaletteLog.h`** — Mutex logging; platform-specific log paths under **`LSP/ColorPalette.log`**.
- **`core/LSPPaletteConstants.h`** — **`kPluginIdentifier`**, repo URLs, **`LSP/Color`**, **`version_gen.h`**.
- **`../common/color/ColorManagement.*`** — WorkshopColor (gamut / transfer / encode / decode).
