# Build helpers

Optional scripts to configure and build the plug-in with CMake. Each writes a versioned, platform-tagged folder under `release/`.

| Script | Platform | Output folder |
|--------|----------|---------------|
| `macos/colorpalette_build.sh` | macOS | `release/LSP_Color_Palette_<version>_macos/` |
| `windows/colorpalette_build.bat` | Windows | `release/LSP_Color_Palette_<version>_windows/` |
| `linux/colorpalette_build.sh` | Linux | `release/LSP_Color_Palette_<version>_linux/` |

Pass extra CMake configure flags after the script name on macOS/Linux (e.g. `-DPALETTE_OFX_FAT_ARCHS=OFF`).

**Windows:** requires Visual Studio with MSVC and Ninja.

**Linux:** requires Ninja and a C++ compiler (GCC or Clang).

GPU QA checklist: [palette_gpu_parity.md](palette_gpu_parity.md).
