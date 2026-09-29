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

// ordre menu gamut (labels A a Z)
enum class ColorPrimariesId : int {
  P3D60 = 0,
  P3D65 = 1,
  P3Dci = 2,
  Rec709 = 3,
  Rec2020 = 4,
};

// transfer menu order, same indices as Metal kTf*
enum class TransferFunctionId : int {
  Gamma22 = 0,
  Gamma24 = 1,
  Gamma26 = 2,
  Rec2100Hlg = 3,
  St2084PQ = 4,
  SRgb = 5,
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
  TransferFunctionId id = TransferFunctionId::Gamma24;
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
Mat3f invert(const Mat3f& matrix);
Vec3f mul(const Mat3f& matrix, Vec3f v);

Vec3f decodeToLinear(Vec3f rgb, TransferFunctionId tf);
Vec3f encodeFromLinear(Vec3f rgb, TransferFunctionId tf);
Vec3f clamp(Vec3f rgb, float lo, float hi);

Vec3f xyToXyz(Vec2f xy, float Y = 1.0f);
Vec2f xyzToXy(Vec3f xyz, Vec2f fallbackWhite);

// McCamy CCT from xy, clamped (default 1000..20000)
float nearestBlackBodyTemperature(Vec2f xy, float minKelvin = 1000.0f, float maxKelvin = 20000.0f);

}  // namespace WorkshopColor
