#include "ColorManagement.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace WorkshopColor {
namespace {

constexpr std::array<PrimariesDefinition, 5> kPrimaries = {{
    {ColorPrimariesId::P3D60, "p3_d60", "P3-D60", {0.6800f, 0.3200f}, {0.2650f, 0.6900f}, {0.1500f, 0.0600f}, {0.32168f, 0.33767f}},
    {ColorPrimariesId::P3D65, "p3_d65", "P3-D65", {0.6800f, 0.3200f}, {0.2650f, 0.6900f}, {0.1500f, 0.0600f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::P3Dci, "p3_dci", "DCI-P3", {0.6800f, 0.3200f}, {0.2650f, 0.6900f}, {0.1500f, 0.0600f}, {0.314f, 0.351f}},
    {ColorPrimariesId::Rec709, "rec709", "Rec.709", {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::Rec2020, "rec2020", "Rec.2020", {0.7080f, 0.2920f}, {0.1700f, 0.7970f}, {0.1310f, 0.0460f}, {0.3127f, 0.3290f}},
}};

constexpr std::array<TransferFunctionDefinition, 6> kTransferFunctions = {{
    {TransferFunctionId::Gamma22, "gamma_22", "Gamma 2.2"},
    {TransferFunctionId::Gamma24, "gamma_24", "Gamma 2.4"},
    {TransferFunctionId::Gamma26, "gamma_26", "Gamma 2.6"},
    {TransferFunctionId::Rec2100Hlg, "rec2100_hlg", "HLG (BT.2100)"},
    {TransferFunctionId::St2084PQ, "st2084_pq", "ST 2084 PQ"},
    {TransferFunctionId::SRgb, "srgb", "sRGB"},
}};

inline float clampf(float v, float lo, float hi) {
  return std::fmin(std::fmax(v, lo), hi);
}

inline float signPreservingPow(float value, float exponent) {
  if (value == 0.0f) return 0.0f;
  return std::copysign(std::pow(std::fabs(value), exponent), value);
}

inline float pqToLinear(float x) {
  const float m1 = 2610.0f / 16384.0f;
  const float m2 = 2523.0f / 32.0f;
  const float c1 = 107.0f / 128.0f;
  const float c2 = 2413.0f / 128.0f;
  const float c3 = 2392.0f / 128.0f;
  const float ax = std::fabs(x);
  const float y = std::pow(ax, 1.0f / m2);
  float denom = c2 - c3 * y;
  if (std::fabs(denom) < 1e-6f) denom = denom < 0.0f ? -1e-6f : 1e-6f;
  const float r = (y - c1) / denom;
  const float L = std::pow(std::fmax(r, 0.0f), 1.0f / m1);
  return std::copysign(L, x);
}

inline float linearToPq(float x) {
  const float m1 = 2610.0f / 16384.0f;
  const float m2 = 2523.0f / 32.0f;
  const float c1 = 107.0f / 128.0f;
  const float c2 = 2413.0f / 128.0f;
  const float c3 = 2392.0f / 128.0f;
  const float ax = std::fabs(x);
  const float y = std::pow(ax, m1);
  const float num = c1 + c2 * y;
  float den = 1.0f + c3 * y;
  if (std::fabs(den) < 1e-6f) den = den < 0.0f ? -1e-6f : 1e-6f;
  const float r = num / den;
  const float out = std::pow(std::fmax(r, 0.0f), m2);
  return std::copysign(out, x);
}

inline Vec3f hlgToLinear(Vec3f rgb) {
  rgb.x = rgb.x <= 0.5f ? rgb.x * rgb.x / 3.0f
                          : (std::exp((rgb.x - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
  rgb.y = rgb.y <= 0.5f ? rgb.y * rgb.y / 3.0f
                          : (std::exp((rgb.y - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
  rgb.z = rgb.z <= 0.5f ? rgb.z * rgb.z / 3.0f
                          : (std::exp((rgb.z - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
  const float Ys = 0.2627f * rgb.x + 0.6780f * rgb.y + 0.0593f * rgb.z;
  const float s = std::pow(std::fmax(Ys, 0.0f), 0.2f);
  return {rgb.x * s, rgb.y * s, rgb.z * s};
}

inline Vec3f linearToHlg(Vec3f rgb) {
  const float Yd = 0.2627f * rgb.x + 0.6780f * rgb.y + 0.0593f * rgb.z;
  const float ypow = std::pow(std::fmax(Yd, 0.0f), (1.0f - 1.2f) / 1.2f);
  rgb.x *= ypow;
  rgb.y *= ypow;
  rgb.z *= ypow;
  rgb.x = rgb.x <= 1.0f / 12.0f ? std::sqrt(std::fmax(0.0f, 3.0f * rgb.x))
                                  : 0.17883277f * std::log(12.0f * rgb.x - 0.28466892f) + 0.55991073f;
  rgb.y = rgb.y <= 1.0f / 12.0f ? std::sqrt(std::fmax(0.0f, 3.0f * rgb.y))
                                  : 0.17883277f * std::log(12.0f * rgb.y - 0.28466892f) + 0.55991073f;
  rgb.z = rgb.z <= 1.0f / 12.0f ? std::sqrt(std::fmax(0.0f, 3.0f * rgb.z))
                                  : 0.17883277f * std::log(12.0f * rgb.z - 0.28466892f) + 0.55991073f;
  return rgb;
}

std::size_t primariesIndex(ColorPrimariesId id) {
  return static_cast<std::size_t>(
      std::clamp(static_cast<int>(id), 0, static_cast<int>(kPrimaries.size() - 1)));
}

std::size_t transferFunctionIndex(TransferFunctionId id) {
  return static_cast<std::size_t>(
      std::clamp(static_cast<int>(id), 0, static_cast<int>(kTransferFunctions.size() - 1)));
}

float encodeChannel(float lin, TransferFunctionId tf) {
  switch (tf) {
    case TransferFunctionId::SRgb: {
      const float a = std::fabs(lin);
      const float enc = (a <= 0.0031308f) ? (a * 12.92f) : (1.055f * std::pow(a, 1.0f / 2.4f) - 0.055f);
      return std::copysign(enc, lin);
    }
    case TransferFunctionId::Gamma24:
      return signPreservingPow(lin, 1.0f / 2.4f);
    case TransferFunctionId::Gamma22:
      return signPreservingPow(lin, 1.0f / 2.2f);
    case TransferFunctionId::Gamma26:
      return signPreservingPow(lin, 1.0f / 2.6f);
    case TransferFunctionId::St2084PQ:
      return linearToPq(lin);
    case TransferFunctionId::Rec2100Hlg:
      break;
  }
  return lin;
}

float decodeChannel(float x, TransferFunctionId tf) {
  switch (tf) {
    case TransferFunctionId::SRgb: {
      const float a = std::fabs(x);
      const float decoded = (a <= 0.04045f) ? (a / 12.92f)
                                            : std::pow((a + 0.055f) / 1.055f, 2.4f);
      return std::copysign(decoded, x);
    }
    case TransferFunctionId::Gamma24:
      return signPreservingPow(x, 2.4f);
    case TransferFunctionId::Gamma22:
      return signPreservingPow(x, 2.2f);
    case TransferFunctionId::Gamma26:
      return signPreservingPow(x, 2.6f);
    case TransferFunctionId::St2084PQ:
      return pqToLinear(x);
    case TransferFunctionId::Rec2100Hlg:
      break;
  }
  return x;
}

}  // namespace

Mat3f invert(const Mat3f& matrix) {
  const float a00 = matrix.m[0][0];
  const float a01 = matrix.m[0][1];
  const float a02 = matrix.m[0][2];
  const float a10 = matrix.m[1][0];
  const float a11 = matrix.m[1][1];
  const float a12 = matrix.m[1][2];
  const float a20 = matrix.m[2][0];
  const float a21 = matrix.m[2][1];
  const float a22 = matrix.m[2][2];

  const float c00 = a11 * a22 - a12 * a21;
  const float c01 = a02 * a21 - a01 * a22;
  const float c02 = a01 * a12 - a02 * a11;
  const float c10 = a12 * a20 - a10 * a22;
  const float c11 = a00 * a22 - a02 * a20;
  const float c12 = a02 * a10 - a00 * a12;
  const float c20 = a10 * a21 - a11 * a20;
  const float c21 = a01 * a20 - a00 * a21;
  const float c22 = a00 * a11 - a01 * a10;

  const float det = a00 * c00 + a01 * c10 + a02 * c20;
  if (std::fabs(det) <= 1e-12f) return {};

  const float invDet = 1.0f / det;
  Mat3f out{};
  out.m[0][0] = c00 * invDet;
  out.m[0][1] = c01 * invDet;
  out.m[0][2] = c02 * invDet;
  out.m[1][0] = c10 * invDet;
  out.m[1][1] = c11 * invDet;
  out.m[1][2] = c12 * invDet;
  out.m[2][0] = c20 * invDet;
  out.m[2][1] = c21 * invDet;
  out.m[2][2] = c22 * invDet;
  return out;
}

std::size_t inputPrimariesCount() { return kPrimaries.size(); }

const PrimariesDefinition& inputPrimariesDefinition(std::size_t index) {
  return kPrimaries[primariesIndex(static_cast<ColorPrimariesId>(static_cast<int>(index)))];
}

ColorPrimariesId inputPrimariesIdFromChoiceIndex(int index) {
  return static_cast<ColorPrimariesId>(
      std::clamp(index, 0, static_cast<int>(kPrimaries.size() - 1)));
}

int inputPrimariesChoiceIndex(ColorPrimariesId id) {
  return static_cast<int>(primariesIndex(id));
}

std::size_t inputTransferFunctionCount() { return kTransferFunctions.size(); }

const TransferFunctionDefinition& inputTransferFunctionDefinition(std::size_t index) {
  return kTransferFunctions[transferFunctionIndex(
      static_cast<TransferFunctionId>(static_cast<int>(index)))];
}

TransferFunctionId inputTransferFunctionIdFromChoiceIndex(int index) {
  return static_cast<TransferFunctionId>(
      std::clamp(index, 0, static_cast<int>(kTransferFunctions.size() - 1)));
}

int inputTransferFunctionChoiceIndex(TransferFunctionId id) {
  return static_cast<int>(transferFunctionIndex(id));
}

Mat3f rgbToXyzMatrix(ColorPrimariesId id) {
  const PrimariesDefinition& primaries = kPrimaries[primariesIndex(id)];
  const Vec3f red = xyToXyz(primaries.red);
  const Vec3f green = xyToXyz(primaries.green);
  const Vec3f blue = xyToXyz(primaries.blue);
  const Vec3f white = xyToXyz(primaries.white);

  Mat3f primariesMatrix{};
  primariesMatrix.m[0][0] = red.x;
  primariesMatrix.m[0][1] = green.x;
  primariesMatrix.m[0][2] = blue.x;
  primariesMatrix.m[1][0] = red.y;
  primariesMatrix.m[1][1] = green.y;
  primariesMatrix.m[1][2] = blue.y;
  primariesMatrix.m[2][0] = red.z;
  primariesMatrix.m[2][1] = green.z;
  primariesMatrix.m[2][2] = blue.z;

  const Mat3f inv = invert(primariesMatrix);
  const Vec3f scale = mul(inv, white);

  Mat3f out = primariesMatrix;
  for (int row = 0; row < 3; ++row) {
    out.m[row][0] *= scale.x;
    out.m[row][1] *= scale.y;
    out.m[row][2] *= scale.z;
  }
  return out;
}

Mat3f xyzToRgbMatrix(ColorPrimariesId id) {
  return invert(rgbToXyzMatrix(id));
}

Vec3f mul(const Mat3f& matrix, Vec3f v) {
  return {
      matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z,
      matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z,
      matrix.m[2][0] * v.x + matrix.m[2][1] * v.y + matrix.m[2][2] * v.z,
  };
}

Vec3f decodeToLinear(Vec3f rgb, TransferFunctionId tf) {
  if (tf == TransferFunctionId::Rec2100Hlg) return hlgToLinear(rgb);
  return {decodeChannel(rgb.x, tf), decodeChannel(rgb.y, tf), decodeChannel(rgb.z, tf)};
}

Vec3f encodeFromLinear(Vec3f rgb, TransferFunctionId tf) {
  if (tf == TransferFunctionId::Rec2100Hlg) return linearToHlg(rgb);
  return {encodeChannel(rgb.x, tf), encodeChannel(rgb.y, tf), encodeChannel(rgb.z, tf)};
}

Vec3f clamp(Vec3f rgb, float lo, float hi) {
  return {clampf(rgb.x, lo, hi), clampf(rgb.y, lo, hi), clampf(rgb.z, lo, hi)};
}

Vec3f xyToXyz(Vec2f xy, float Y) {
  if (std::fabs(xy.y) <= 1e-8f) {
    return {xy.x, Y, 1.0f - xy.x};
  }
  return {xy.x * Y / xy.y, Y, (1.0f - xy.x - xy.y) * Y / xy.y};
}

Vec2f xyzToXy(Vec3f xyz, Vec2f fallbackWhite) {
  if (std::fabs(xyz.y) <= 1e-8f) {
    return fallbackWhite;
  }
  const float sum = xyz.x + xyz.y + xyz.z;
  if (std::fabs(sum) <= 1e-8f) {
    return fallbackWhite;
  }
  return {xyz.x / sum, xyz.y / sum};
}

// approx McCamy pour le tri
float nearestBlackBodyTemperature(Vec2f xy, float minKelvin, float maxKelvin) {
  if (!std::isfinite(xy.x) || !std::isfinite(xy.y)) return 0.0f;
  minKelvin = std::clamp(minKelvin, 1000.0f, 20000.0f);
  maxKelvin = std::clamp(maxKelvin, minKelvin, 20000.0f);

  const float denom = xy.y - 0.1858f;
  if (std::fabs(denom) <= 1e-8f) return std::clamp(6500.0f, minKelvin, maxKelvin);

  const float n = (xy.x - 0.3320f) / denom;
  const float cct = ((-449.0f * n + 3525.0f) * n - 6823.3f) * n + 5520.33f;
  if (!std::isfinite(cct) || cct <= 0.0f) return 0.0f;
  return std::clamp(cct, minKelvin, maxKelvin);
}

}  // namespace WorkshopColor
