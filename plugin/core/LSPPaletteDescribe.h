#ifndef LSP_PALETTE_DESCRIBE_H
#define LSP_PALETTE_DESCRIBE_H

#include "ofxsImageEffect.h"

void describePaletteInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum p_Context);

void applyPaletteHostRenderSupport(OFX::ImageEffectDescriptor& d, bool advertiseHostCuda, bool advertiseHostMetal);

#endif
