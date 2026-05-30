#ifndef LSP_PALETTE_PLUGIN_H
#define LSP_PALETTE_PLUGIN_H

#include "ofxsImageEffect.h"

class LSPPalettePluginFactory : public OFX::PluginFactoryHelper<LSPPalettePluginFactory> {
public:
    LSPPalettePluginFactory();
    void load() override {}
    void unload() override {}
    void describe(OFX::ImageEffectDescriptor& p_Desc) override;
    void describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum p_Context) override;
    OFX::ImageEffect* createInstance(OfxImageEffectHandle p_Handle, OFX::ContextEnum p_Context) override;
};

#endif
