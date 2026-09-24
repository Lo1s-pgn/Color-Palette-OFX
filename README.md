# LSP - Color Palette (OFX)

**Color Palette** is an OFX plug-in for DaVinci Resolve. It extracts a dominant-color palette with median-cut in [OKLAB](https://bottosson.github.io/posts/oklab/) and composites it with the picture.

## What it does

- **Extract palette** — Median-cut regions in OKLAB; **Patch count** from 3 to 24 swatches.
- **Sort swatches** — Weight, Lightness, Hue, Saturation, or Temperature.
- **Composite strip** — Layout (top / bottom / left / right), **Size**, **Gap**, **Corner**, optional **Full frame** crop fill.

![LSP - Color Palette demonstration](img/PALETTE_01.jpg)

## Platform

| OS | Notes |
|----|-------|
| **macOS** 11.0+ | Universal arm64 + x86_64 by default |
| **Windows** | 64-bit (DaVinci Resolve) |
| **Linux** | x86-64 (DaVinci Resolve) |

Build on each platform separately. Each build writes a versioned release folder:

| Platform | Release folder |
|----------|----------------|
| macOS | `release/LSP_Color_Palette_<version>_macos/` |
| Windows | `release/LSP_Color_Palette_<version>_windows/` |
| Linux | `release/LSP_Color_Palette_<version>_linux/` |

## Build

**CMake** is the only build system. See [tools/README.md](tools/README.md) for helper scripts.

### Prerequisites

- **CMake** 3.22+
- **macOS:** Xcode Command Line Tools
- **Windows:** Visual Studio 2022 (MSVC), **Ninja**
- **Linux:** GCC or Clang, **Ninja**

### macOS

```bash
./tools/macos/colorpalette_build.sh
```

Or manually:

```bash
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --target colorpalette_all --parallel
```

Configure options: `-DPALETTE_OFX_FAT_ARCHS=OFF` (single-arch), `-DOFX_SDK_PATH=...`

### Windows

```powershell
tools\windows\colorpalette_build.bat
```

Or manually (Developer Command Prompt or after `vcvars64.bat`):

```powershell
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build/windows --target colorpalette_all --parallel
```

### Linux

```bash
./tools/linux/colorpalette_build.sh
```

Or manually:

```bash
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --target colorpalette_all --parallel
```

## Installation

Copy the bundle from your platform’s release folder into the host OFX plug-ins directory, then restart DaVinci Resolve.

| Platform | OFX folder |
|----------|------------|
| macOS (all users) | `/Library/OFX/Plugins/` |
| macOS (current user) | `~/Library/OFX/Plugins/` |
| Windows | `C:\Program Files\Common Files\OFX\Plugins\` |
| Linux (system) | `/usr/OFX/Plugins/` |
| Linux (user) | `~/.local/share/OFX/Plugins/` |

**Resolve OFX cache** (delete if the plug-in does not appear after upgrade):
- macOS: `~/Library/Application Support/Blackmagic Design/DaVinci Resolve/OFXPluginCacheV2.xml`
- Windows: `%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml`
- Linux: `~/.local/share/DaVinciResolve/OFXPluginCacheV2.xml`

## Log file

| Platform | Path |
|----------|------|
| macOS | `~/Library/Application Support/LSP/ColorPalette.log` |
| Windows | `%LOCALAPPDATA%\LSP\Palette.log` |
| Linux | `~/.cache/LSP/ColorPalette.log` |

Use **Open Log** in the plug-in SUPPORT section.

## macOS Gatekeeper (unsigned builds)

```bash
BUNDLE="/Library/OFX/Plugins/LSP_Color_Palette_<version>.ofx.bundle"

sudo chmod -R 755 "$BUNDLE"
sudo chown -R root:wheel "$BUNDLE"   # skip for ~/Library/OFX/Plugins/
sudo xattr -dr com.apple.quarantine "$BUNDLE"
sudo codesign --force --deep --sign - "$BUNDLE"
```

Quit and relaunch Resolve.

## SDK

Vendored minimal OpenFX SDK in `openfx-sdk/`. Override with `-DOFX_SDK_PATH=...` if needed.

## License

Distributed under **GNU GPL v3**. See [LICENSE](LICENSE).
