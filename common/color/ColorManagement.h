#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace WorkshopColor {

struct Vec2f {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Mat3f {
  float m[3][3] = {{0.0f, 0.0f, 0.0f},
                   {0.0f, 0.0f, 0.0f},
                   {0.0f, 0.0f, 0.0f}};
};

struct XyY {
  float x = 0.0f;
  float y = 0.0f;
  float Y = 0.0f;
};

// kPrimaries: alphabetical by label. Enum value == row index.
enum class ColorPrimariesId : int {
  AcesAp0 = 0,
  AcesAp1 = 1,
  ArriWideGamut3 = 2,
  ArriWideGamut4 = 3,
  CanonCinemaGamut = 4,
  DavinciWideGamut = 5,
  FilmlightEGamut = 6,
  FilmlightEGamut2 = 7,
  P3D60 = 8,
  P3D65 = 9,
  P3Dci = 10,
  PanasonicVGamut = 11,
  Rec709 = 12,
  Rec2020 = 13,
  RedWideGamutRGB = 14,
  SonySGamut3 = 15,
  SonySGamut3Cine = 16,
  Xyz = 17,
};

// Stable numeric ids (presets / switch). Declaration sorted A–Z by enumerator name.
enum class TransferFunctionId : int {
  AcesCct = 4,
  ArriLogC3 = 5,
  ArriLogC4 = 6,
  CanonLog2 = 7,
  CanonLog3 = 9,
  DavinciIntermediate = 3,
  FilmlightTLog = 11,
  FujiFLog2 = 13,
  Gamma22 = 14,
  Gamma24 = 2,
  Gamma26 = 15,
  Linear = 0,
  PanasonicVLog = 12,
  RedLog3G10 = 10,
  Rec2100Hlg = 17,
  SonySLog3 = 8,
  SRgb = 1,
  St2084PQ = 16,
};

enum class ChromaticityReferenceBasis : int {
  CieStandardObserver = 0,
  InputObserver = 1,
};

enum class ChromaticityDiagramSpace : int {
  Cie1931Xy = 0,
  Cie1976Uvp = 1,
  // Richard A. Kirk, CIC 2019: Yrg chromaticity (r,g) from CIE 2006 LMS via fitted XYZ→LMS.
  Kirk2019Rg = 2,
};

struct PrimariesDefinition {
  ColorPrimariesId id = ColorPrimariesId::Rec709;
  const char* key = "";
  const char* label = "";
  Vec2f red{};
  Vec2f green{};
  Vec2f blue{};
  Vec2f white{};
};

struct TransferFunctionDefinition {
  TransferFunctionId id = TransferFunctionId::Linear;
  const char* key = "";
  const char* label = "";
};

struct ChromaticityColorSpec {
  ColorPrimariesId inputPrimaries = ColorPrimariesId::Rec709;
  TransferFunctionId inputTransfer = TransferFunctionId::Gamma24;
  ChromaticityReferenceBasis referenceBasis = ChromaticityReferenceBasis::CieStandardObserver;
  ChromaticityDiagramSpace diagramSpace = ChromaticityDiagramSpace::Cie1931Xy;
  bool overlayEnabled = true;
  ColorPrimariesId overlayPrimaries = ColorPrimariesId::Rec709;
};

// Display-referred subset used by input UI choices.
std::size_t inputPrimariesCount();
const PrimariesDefinition& inputPrimariesDefinition(std::size_t index);
ColorPrimariesId inputPrimariesIdFromChoiceIndex(int index);
int inputPrimariesChoiceIndex(ColorPrimariesId id);

// Display-referred subset used by input UI choices.
std::size_t inputTransferFunctionCount();
const TransferFunctionDefinition& inputTransferFunctionDefinition(std::size_t index);
TransferFunctionId inputTransferFunctionIdFromChoiceIndex(int index);
int inputTransferFunctionChoiceIndex(TransferFunctionId id);

Vec2f whitePoint(ColorPrimariesId id);
Mat3f rgbToXyzMatrix(ColorPrimariesId id);
Mat3f xyzToRgbMatrix(ColorPrimariesId id);
Vec3f mul(const Mat3f& matrix, Vec3f v);

Vec3f decodeToLinear(Vec3f rgb, TransferFunctionId tf);
Vec3f encodeFromLinear(Vec3f rgb, TransferFunctionId tf);
Vec3f clamp(Vec3f rgb, float lo, float hi);
bool isFinite(Vec2f v);
bool isFinite(Vec3f v);

Vec3f xyToXyz(Vec2f xy, float Y = 1.0f);
XyY xyzToXyY(Vec3f xyz, Vec2f fallbackWhite);
Vec2f xyzToXy(Vec3f xyz, Vec2f fallbackWhite);
Vec2f xyToUvp(Vec2f xy);
Vec2f uvpToXy(Vec2f uv);
Vec2f xyzToKirkRg(Vec3f xyz);
// Bridge (r,g) in Kirk 2019 space to CIE xy for shared diagram machinery (grid, tick placement).
Vec2f kirkRgToXyChromaticity(Vec2f rg);

Vec2f standardObserverToInputObserver(Vec2f xy, ColorPrimariesId inputPrimaries);
Vec2f inputObserverToStandardObserver(Vec2f xy, ColorPrimariesId inputPrimaries);

const std::array<Vec3f, 82>& cie1931XyzCmfs5nm();
// Linear CIE 1931 2° XYZ from CMFs at 5 nm steps, interpolated (380–780 nm).
Vec3f cie1931CmfXyzInterpolated(float wavelengthNm);
// Display RGB (Alexa Wide Gamut linear after outset) via OKLAB.dctl pipeline (inset → OKLAB → inverse → outset).
Vec3f okLabDctlDisplayFromCmfXyz(Vec3f cmfXyz);
Vec3f okLabDctlDisplayFromXy(Vec2f xy, float Y);
Vec3f okLabDctlDisplayPurpleMixFromCmf(Vec3f cmfXyz380, Vec3f cmfXyz780, float t);

bool blackBodyChromaticity(float kelvin, Vec2f* xy);
float nearestBlackBodyTemperature(Vec2f xy, float minKelvin = 1000.0f, float maxKelvin = 20000.0f);
std::vector<Vec2f> blackBodyChromaticityCurve(float minKelvin, float maxKelvin, std::size_t steps);

}  // namespace WorkshopColor
