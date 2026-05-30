#!/usr/bin/env bash
# Linux build helper — configure + build (release/LSP_Color_Palette_<version>_linux/).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build/linux --target colorpalette_all --parallel "$(nproc 2>/dev/null || echo 4)"
