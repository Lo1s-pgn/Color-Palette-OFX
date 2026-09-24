#pragma once

#include <cstddef>

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

// primaries table sorted A to Z in the ui
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

// transfer ids for presets, names sorted A to Z
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

// input dropdown choices only
std::size_t inputPrimariesCount();
const PrimariesDefinition& inputPrimariesDefinition(std::size_t index);
ColorPrimariesId inputPrimariesIdFromChoiceIndex(int index);
int inputPrimariesChoiceIndex(ColorPrimariesId id);

std::size_t inputTransferFunctionCount();
const TransferFunctionDefinition& inputTransferFunctionDefinition(std::size_t index);
TransferFunctionId inputTransferFunctionIdFromChoiceIndex(int index);
int inputTransferFunctionChoiceIndex(TransferFunctionId id);

Mat3f rgbToXyzMatrix(ColorPrimariesId id);
Mat3f xyzToRgbMatrix(ColorPrimariesId id);
Vec3f mul(const Mat3f& matrix, Vec3f v);

Vec3f decodeToLinear(Vec3f rgb, TransferFunctionId tf);
Vec3f encodeFromLinear(Vec3f rgb, TransferFunctionId tf);
Vec3f clamp(Vec3f rgb, float lo, float hi);
bool isFinite(Vec2f v);

Vec3f xyToXyz(Vec2f xy, float Y = 1.0f);
XyY xyzToXyY(Vec3f xyz, Vec2f fallbackWhite);
Vec2f xyzToXy(Vec3f xyz, Vec2f fallbackWhite);

bool blackBodyChromaticity(float kelvin, Vec2f* xy);
float nearestBlackBodyTemperature(Vec2f xy, float minKelvin = 1000.0f, float maxKelvin = 20000.0f);

}  // namespace WorkshopColor
