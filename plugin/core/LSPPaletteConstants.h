#ifndef LSP_PALETTE_CONSTANTS_H
#define LSP_PALETTE_CONSTANTS_H

#include "version_gen.h"

#define kPluginName "LSP - Color Palette " PLUGIN_VERSION_STR
#define kPluginGrouping "LSP - Color"
#define kPluginDescription \
    "LSP - Color Palette — dominant colors (OKLab median cut), gamut/transfer-aware decode, overlay presentation (OFX)."
#define kPluginIdentifier PLUGIN_OFX_IDENTIFIER
#define kPluginVersionMajor PLUGIN_VERSION_MAJOR
#define kPluginVersionMinor PLUGIN_VERSION_MINOR
#define kSupportsTiles false
#define kSupportsMultiResolution false
#define kSupportsMultipleClipPARs false

#define kPaletteRepoUrl "https://github.com/Lo1s-pgn/Color-Palette-OFX"
#define kPaletteIssuesUrl "https://github.com/Lo1s-pgn/Color-Palette-OFX/issues"

#endif
