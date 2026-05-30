// OFX parameter and clip descriptors for Color Palette.
#include "LSPPaletteDescribe.h"
#include "LSPPaletteComposite.h"
#include "LSPPaletteConstants.h"
#include "ColorManagement.h"
#include "version_gen.h"
#include "ofxsImageEffect.h"

namespace {
OFX::GroupParamDescriptor* addGroup(OFX::ImageEffectDescriptor& d, const char* name, const char* label, const char* hint,
    bool open) {
    OFX::GroupParamDescriptor* g = d.defineGroupParam(name);
    g->setLabels(label, label, label);
    g->setHint(hint);
    g->setOpen(open);
    return g;
}
} // namespace

void describePaletteInContext(OFX::ImageEffectDescriptor& d, OFX::ContextEnum /*context*/) {
    OFX::ClipDescriptor* src = d.defineClip(kOfxImageEffectSimpleSourceClipName);
    src->addSupportedComponent(OFX::ePixelComponentRGBA);
    src->setTemporalClipAccess(false);
    src->setSupportsTiles(kSupportsTiles);
    src->setIsMask(false);

    OFX::ClipDescriptor* dst = d.defineClip(kOfxImageEffectOutputClipName);
    dst->addSupportedComponent(OFX::ePixelComponentRGBA);
    dst->addSupportedComponent(OFX::ePixelComponentAlpha);
    dst->setSupportsTiles(kSupportsTiles);

    OFX::PageParamDescriptor* page = d.definePageParam("Controls");

    OFX::GroupParamDescriptor* grpInput = addGroup(d, "paletteGrpInput", "INPUT COLOR", "Source RGB decode.", true);

    OFX::ChoiceParamDescriptor* inpGamut = d.defineChoiceParam("paletteInputPrimaries");
    inpGamut->setLabels("Gamut", "Gamut", "Gamut");
    inpGamut->setHint("RGB primaries.");
    for (std::size_t i = 0; i < WorkshopColor::inputPrimariesCount(); ++i)
        inpGamut->appendOption(WorkshopColor::inputPrimariesDefinition(i).label);
    inpGamut->setDefault(
        WorkshopColor::inputPrimariesChoiceIndex(WorkshopColor::ColorPrimariesId::Rec709));
    inpGamut->setAnimates(false);
    inpGamut->setParent(*grpInput);

    OFX::ChoiceParamDescriptor* inpTf = d.defineChoiceParam("paletteInputTransfer");
    inpTf->setLabels("Transfer function", "Transfer function", "Transfer function");
    inpTf->setHint("EOTF to linear.");
    for (std::size_t i = 0; i < WorkshopColor::inputTransferFunctionCount(); ++i)
        inpTf->appendOption(WorkshopColor::inputTransferFunctionDefinition(i).label);
    inpTf->setDefault(
        WorkshopColor::inputTransferFunctionChoiceIndex(WorkshopColor::TransferFunctionId::Gamma24));
    inpTf->setAnimates(false);
    inpTf->setParent(*grpInput);

    OFX::GroupParamDescriptor* grpPalette = addGroup(d, "paletteGrpPalette", "PALETTE", "Layout and palette strip.", true);

    OFX::IntParamDescriptor* patchN = d.defineIntParam("palettePatchCount");
    patchN->setLabels("Patch count", "Patch count", "Patch count");
    patchN->setHint("Number of median-cut regions (OKLAB).");
    patchN->setRange(3, 24);
    patchN->setDisplayRange(3, 24);
    patchN->setDefault(8);
    patchN->setAnimates(false);
    patchN->setParent(*grpPalette);

    OFX::ChoiceParamDescriptor* sortOrder = d.defineChoiceParam("paletteSortOrder");
    sortOrder->setLabels("Sort order", "Sort order", "Sort order");
    sortOrder->setHint("Swatch order. Smooth = shortest OKLAB path (multi-start + 2-opt), favors light→dark ends; median-cut slots sorted by OKLAB for stable IDs.");
    sortOrder->appendOption("Weight");
    sortOrder->appendOption("Lightness");
    sortOrder->appendOption("Hue");
    sortOrder->appendOption("Saturation");
    sortOrder->appendOption("Smooth");
    sortOrder->setDefault(0);
    sortOrder->setAnimates(false);
    sortOrder->setParent(*grpPalette);

    OFX::ChoiceParamDescriptor* layout = d.defineChoiceParam("paletteLayout");
    layout->setLabels("Layout", "Layout", "Layout");
    layout->setHint("Strip side.");
    layout->appendOption("Top");
    layout->appendOption("Bottom");
    layout->appendOption("Left");
    layout->appendOption("Right");
    layout->setDefault(LSPPaletteComposite::kLayoutBottom);
    layout->setAnimates(false);
    layout->setParent(*grpPalette);

    OFX::BooleanParamDescriptor* fullFrame = d.defineBooleanParam("paletteFullFrame");
    fullFrame->setLabels("Full frame", "Full frame", "Full frame");
    fullFrame->setHint("Wide strip + crop fill.");
    fullFrame->setDefault(false);
    fullFrame->setAnimates(false);
    fullFrame->setParent(*grpPalette);

    OFX::DoubleParamDescriptor* stripThick = d.defineDoubleParam("palettePatchSize");
    stripThick->setLabels("Size", "Size", "Size");
    stripThick->setHint("Strip vs image.");
    stripThick->setRange(0.05, 0.55);
    stripThick->setDisplayRange(0.08, 0.25);
    stripThick->setDefault(0.14);
    stripThick->setAnimates(false);
    stripThick->setParent(*grpPalette);

    OFX::DoubleParamDescriptor* gapR = d.defineDoubleParam("paletteGap");
    gapR->setLabels("Gap", "Gap", "Gap");
    gapR->setHint("Swatch + picture margin.");
    gapR->setRange(0.0, 1.0);
    gapR->setDisplayRange(0.0, 0.5);
    gapR->setDefault(0.22);
    gapR->setAnimates(false);
    gapR->setParent(*grpPalette);

    OFX::DoubleParamDescriptor* corner = d.defineDoubleParam("paletteCorner");
    corner->setLabels("Corner", "Corner", "Corner");
    corner->setHint("Swatch roundness.");
    corner->setRange(0.0, 0.49);
    corner->setDisplayRange(0.0, 0.25);
    corner->setDefault(0.10);
    corner->setAnimates(false);
    corner->setParent(*grpPalette);

    OFX::DoubleParamDescriptor* bgLight = d.defineDoubleParam("paletteBackgroundLightness");
    bgLight->setLabels("Background", "Background", "Background");
    bgLight->setHint("Neutral gray.");
    bgLight->setRange(0.0, 1.0);
    bgLight->setDisplayRange(0.0, 1.0);
    bgLight->setDefault(0.0);
    bgLight->setAnimates(false);
    bgLight->setParent(*grpPalette);

    OFX::GroupParamDescriptor* grpSupport = addGroup(d, "paletteGrpSupport", "SUPPORT", "Help links.", false);

    OFX::StringParamDescriptor* credits = d.defineStringParam("supportCreditsLabel");
    credits->setLabels("Credits", "Credits", "Credits");
    credits->setDefault(std::string("Made by Loïs Plagnard — LSP - Color Palette ") + PLUGIN_VERSION_STR);
    credits->setStringType(OFX::eStringTypeLabel);
    credits->setAnimates(false);
    credits->setParent(*grpSupport);

    OFX::PushButtonParamDescriptor* helpBtn = d.definePushButtonParam("supportWebsite");
    helpBtn->setLabels("Help", "Help", "GitHub");
    helpBtn->setParent(*grpSupport);

    OFX::PushButtonParamDescriptor* bugBtn = d.definePushButtonParam("supportReportIssue");
    bugBtn->setLabels("Report a bug", "Report a bug", "Issues");
    bugBtn->setParent(*grpSupport);

    OFX::PushButtonParamDescriptor* logBtn = d.definePushButtonParam("supportOpenLog");
    logBtn->setLabels("Open Log", "Open Log", "Log file");
    logBtn->setParent(*grpSupport);

    page->addChild(*grpInput);
    page->addChild(*grpPalette);
    page->addChild(*grpSupport);
}

void applyPaletteHostRenderSupport(OFX::ImageEffectDescriptor& d, bool advertiseHostCuda, bool advertiseHostMetal) {
#if defined(OFX_SUPPORTS_CUDARENDER)
    d.setSupportsCudaRender(advertiseHostCuda);
    d.setSupportsCudaStream(advertiseHostCuda);
#elif defined(__APPLE__)
    d.setSupportsMetalRender(advertiseHostMetal);
    d.setSupportsCudaRender(false);
    d.setSupportsCudaStream(false);
#else
    (void)advertiseHostMetal;
    d.setSupportsCudaRender(false);
    d.setSupportsCudaStream(false);
#endif
}
