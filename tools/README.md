# Tools

Optional helpers for building the plug-in. **CMake** is the only build system (see root [README.md](../README.md)).

Each platform build writes:

`release/LSP_Color_Palette_<version>_macos/`, `_windows/`, or `_linux/`

containing **`LSP_Color_Palette_<version>.ofx.bundle`**.

## Build

| Script | Platform | Purpose |
|--------|----------|---------|
| [macos/colorpalette_build.sh](macos/colorpalette_build.sh) | macOS | Configure `build/macos/` → versioned macOS release folder |
| [windows/colorpalette_build.bat](windows/colorpalette_build.bat) | Windows | Configure `build/windows/` → versioned Windows release folder |
| [linux/colorpalette_build.sh](linux/colorpalette_build.sh) | Linux | Configure `build/linux/` → versioned Linux release folder |

Equivalent manual commands:

```bash
# macOS
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --target colorpalette_all
```

```powershell
# Windows
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build/windows --target colorpalette_all
```

```bash
# Linux
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --target colorpalette_all
```

Install the `.ofx.bundle` from your platform's release folder — see root README **Installation**.

## GPU QA

[palette_gpu_parity.md](palette_gpu_parity.md) — manual checklist for CPU vs GPU extract/composite parity, env vars, backend fallbacks, and playback cache behavior (`extract_cache_fast`, `metal_mmcq`, etc.).

## GitHub remote

This project tracks **`https://github.com/Lo1s-pgn/Color-Palette-OFX`**. If `.git` was lost or the folder was copied without history, run from the repo root:

```bash
./tools/relink_github.sh
```

If the GitHub repo is still named **`Simple-Palette-OFX`**, rename it on GitHub first, or run:

```bash
GITHUB_REPO=Simple-Palette-OFX ./tools/relink_github.sh
```

