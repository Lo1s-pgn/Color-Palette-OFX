#include "ColorManagement.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace WorkshopColor {
namespace {

constexpr float kOneThird = 1.0f / 3.0f;
constexpr double kPlanckC1 = 3.741771852e-16;
constexpr double kPlanckC2 = 1.438776877e-2;

// UI order: alphabetical by label (A–Z).
constexpr std::array<PrimariesDefinition, 18> kPrimaries = {{
    {ColorPrimariesId::AcesAp0, "aces_ap0", "ACES AP0", {0.7347f, 0.2653f}, {0.0f, 1.0f}, {0.0001f, -0.0770f}, {0.32168f, 0.33767f}},
    {ColorPrimariesId::AcesAp1, "aces_ap1", "ACES AP1", {0.7130f, 0.2930f}, {0.1650f, 0.8300f}, {0.1280f, 0.0444f}, {0.32168f, 0.33767f}},
    {ColorPrimariesId::ArriWideGamut3, "arri_wide_gamut_3", "ARRI Wide Gamut 3", {0.6840f, 0.3130f}, {0.2210f, 0.8480f}, {0.0861f, -0.1020f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::ArriWideGamut4, "arri_wide_gamut_4", "ARRI Wide Gamut 4", {0.7347f, 0.2653f}, {0.1424f, 0.8576f}, {0.0991f, -0.0308f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::CanonCinemaGamut, "canon_cinema_gamut", "Canon Cinema Gamut", {0.7400f, 0.2700f}, {0.1700f, 1.1400f}, {0.0800f, -0.1000f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::DavinciWideGamut, "davinci_wide_gamut", "DaVinci Wide Gamut", {0.8000f, 0.3130f}, {0.1682f, 0.9877f}, {0.0790f, -0.1155f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::FilmlightEGamut, "filmlight_e_gamut", "FilmLight E-Gamut", {0.8000f, 0.3177f}, {0.1800f, 0.9000f}, {0.0650f, -0.0805f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::FilmlightEGamut2, "filmlight_e_gamut_2", "FilmLight E-Gamut 2", {0.8300f, 0.3100f}, {0.1500f, 0.9500f}, {0.0650f, -0.0805f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::P3D60, "p3_d60", "P3-D60", {0.6800f, 0.3200f}, {0.2650f, 0.6900f}, {0.1500f, 0.0600f}, {0.32168f, 0.33767f}},
    {ColorPrimariesId::P3D65, "p3_d65", "P3-D65", {0.6800f, 0.3200f}, {0.2650f, 0.6900f}, {0.1500f, 0.0600f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::P3Dci, "p3_dci", "DCI-P3", {0.6800f, 0.3200f}, {0.2650f, 0.6900f}, {0.1500f, 0.0600f}, {0.314f, 0.351f}},
    {ColorPrimariesId::PanasonicVGamut, "panasonic_v_gamut", "Panasonic V-Gamut", {0.7300f, 0.2800f}, {0.1650f, 0.8400f}, {0.1000f, -0.0300f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::Rec709, "rec709", "Rec.709", {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::Rec2020, "rec2020", "Rec.2020", {0.7080f, 0.2920f}, {0.1700f, 0.7970f}, {0.1310f, 0.0460f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::RedWideGamutRGB, "red_wide_gamut_rgb", "RED Wide Gamut RGB", {0.780308f, 0.304253f}, {0.121595f, 1.493994f}, {0.095612f, -0.084589f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::SonySGamut3, "sony_s_gamut3", "Sony S-Gamut3", {0.7300f, 0.2800f}, {0.1400f, 0.8550f}, {0.1000f, -0.0500f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::SonySGamut3Cine, "sony_s_gamut3_cine", "Sony S-Gamut3.Cine", {0.7660f, 0.2750f}, {0.2250f, 0.8000f}, {0.0890f, -0.0870f}, {0.3127f, 0.3290f}},
    {ColorPrimariesId::Xyz, "xyz", "XYZ", {1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {kOneThird, kOneThird}},
}};

// UI order: alphabetical by label (A–Z).
constexpr std::array<TransferFunctionDefinition, 18> kTransferFunctions = {{
    {TransferFunctionId::AcesCct, "acescct", "ACEScct"},
    {TransferFunctionId::ArriLogC3, "arri_logc3", "ARRI LogC3"},
    {TransferFunctionId::ArriLogC4, "arri_logc4", "ARRI LogC4"},
    {TransferFunctionId::CanonLog2, "canon_log2", "Canon Log 2"},
    {TransferFunctionId::CanonLog3, "canon_log3", "Canon Log 3"},
    {TransferFunctionId::DavinciIntermediate, "davinci_intermediate", "DaVinci Intermediate"},
    {TransferFunctionId::Gamma22, "gamma_22", "Gamma 2.2"},
    {TransferFunctionId::Gamma24, "gamma_24", "Gamma 2.4"},
    {TransferFunctionId::Gamma26, "gamma_26", "Gamma 2.6"},
    {TransferFunctionId::FilmlightTLog, "filmlight_tlog", "FilmLight T-Log"},
    {TransferFunctionId::FujiFLog2, "fuji_flog2", "Fuji F-Log2"},
    {TransferFunctionId::Rec2100Hlg, "rec2100_hlg", "HLG (BT.2100)"},
    {TransferFunctionId::Linear, "linear", "Linear"},
    {TransferFunctionId::PanasonicVLog, "panasonic_vlog", "Panasonic V-Log"},
    {TransferFunctionId::RedLog3G10, "red_log3g10", "RED Log3G10"},
    {TransferFunctionId::SonySLog3, "sony_slog3", "S-Log3 (Sony)"},
    {TransferFunctionId::St2084PQ, "st2084_pq", "ST 2084 PQ"},
    {TransferFunctionId::SRgb, "srgb", "sRGB"},
}};

// Input choices are intentionally explicit and display-referred only.
constexpr std::array<ColorPrimariesId, 5> kInputPrimaries = {{
    ColorPrimariesId::P3D60,
    ColorPrimariesId::P3D65,
    ColorPrimariesId::P3Dci,
    ColorPrimariesId::Rec709,
    ColorPrimariesId::Rec2020,
}};

constexpr std::array<TransferFunctionId, 6> kInputTransferFunctions = {{
    TransferFunctionId::Gamma22,
    TransferFunctionId::Gamma24,
    TransferFunctionId::Gamma26,
    TransferFunctionId::Rec2100Hlg,
    TransferFunctionId::St2084PQ,
    TransferFunctionId::SRgb,
}};

constexpr std::array<Vec3f, 82> kCie1931XyzCmfs5nm = {{
    {0.001368f, 0.000039f, 0.006450f}, {0.002236f, 0.000064f, 0.010550f},
    {0.004243f, 0.000120f, 0.020050f}, {0.007650f, 0.000217f, 0.036210f},
    {0.014310f, 0.000396f, 0.067850f}, {0.023190f, 0.000640f, 0.110200f},
    {0.043510f, 0.001210f, 0.207400f}, {0.077630f, 0.002180f, 0.371300f},
    {0.134380f, 0.004000f, 0.645600f}, {0.214770f, 0.007300f, 1.039050f},
    {0.283900f, 0.011600f, 1.385600f}, {0.328500f, 0.016840f, 1.622960f},
    {0.348280f, 0.023000f, 1.747060f}, {0.348060f, 0.029800f, 1.782600f},
    {0.336200f, 0.038000f, 1.772110f}, {0.318700f, 0.048000f, 1.744100f},
    {0.290800f, 0.060000f, 1.669200f}, {0.251100f, 0.073900f, 1.528100f},
    {0.195360f, 0.090980f, 1.287640f}, {0.142100f, 0.112600f, 1.041900f},
    {0.095640f, 0.139020f, 0.812950f}, {0.057950f, 0.169300f, 0.616200f},
    {0.032010f, 0.208020f, 0.465180f}, {0.014700f, 0.258600f, 0.353300f},
    {0.004900f, 0.323000f, 0.272000f}, {0.002400f, 0.407300f, 0.212300f},
    {0.009300f, 0.503000f, 0.158200f}, {0.029100f, 0.608200f, 0.111700f},
    {0.063270f, 0.710000f, 0.078250f}, {0.109600f, 0.793200f, 0.057250f},
    {0.165500f, 0.862000f, 0.042160f}, {0.225750f, 0.914850f, 0.029840f},
    {0.290400f, 0.954000f, 0.020300f}, {0.359700f, 0.980300f, 0.013400f},
    {0.433450f, 0.994950f, 0.008750f}, {0.512050f, 1.000000f, 0.005750f},
    {0.594500f, 0.995000f, 0.003900f}, {0.678400f, 0.978600f, 0.002750f},
    {0.762100f, 0.952000f, 0.002100f}, {0.842500f, 0.915400f, 0.001800f},
    {0.916300f, 0.870000f, 0.001650f}, {0.978600f, 0.816300f, 0.001400f},
    {1.026300f, 0.757000f, 0.001100f}, {1.056700f, 0.694900f, 0.001000f},
    {1.062200f, 0.631000f, 0.000800f}, {1.045600f, 0.566800f, 0.000600f},
    {1.002600f, 0.503000f, 0.000340f}, {0.938400f, 0.441200f, 0.000240f},
    {0.854450f, 0.381000f, 0.000190f}, {0.751400f, 0.321000f, 0.000100f},
    {0.642400f, 0.265000f, 0.000050f}, {0.541900f, 0.217000f, 0.000030f},
    {0.447900f, 0.175000f, 0.000020f}, {0.360800f, 0.138200f, 0.000010f},
    {0.283500f, 0.107000f, 0.000000f}, {0.218700f, 0.081600f, 0.000000f},
    {0.164900f, 0.061000f, 0.000000f}, {0.121200f, 0.044580f, 0.000000f},
    {0.087400f, 0.032000f, 0.000000f}, {0.063600f, 0.023200f, 0.000000f},
    {0.046770f, 0.017000f, 0.000000f}, {0.032900f, 0.011920f, 0.000000f},
    {0.022700f, 0.008210f, 0.000000f}, {0.015840f, 0.005723f, 0.000000f},
    {0.011359f, 0.004102f, 0.000000f}, {0.008111f, 0.002929f, 0.000000f},
    {0.005790f, 0.002091f, 0.000000f}, {0.004109f, 0.001484f, 0.000000f},
    {0.002899f, 0.001047f, 0.000000f}, {0.002049f, 0.000740f, 0.000000f},
    {0.001440f, 0.000520f, 0.000000f}, {0.001000f, 0.000361f, 0.000000f},
    {0.000690f, 0.000249f, 0.000000f}, {0.000476f, 0.000172f, 0.000000f},
    {0.000332f, 0.000120f, 0.000000f}, {0.000235f, 0.000085f, 0.000000f},
    {0.000166f, 0.000060f, 0.000000f}, {0.000117f, 0.000042f, 0.000000f},
    {0.000083f, 0.000030f, 0.000000f}, {0.000059f, 0.000021f, 0.000000f},
    {0.000042f, 0.000015f, 0.000000f}, {0.001368f, 0.000039f, 0.006450f},
}};

inline float clampf(float v, float lo, float hi) {
  return std::fmin(std::fmax(v, lo), hi);
}

inline float signPreservingPow(float value, float exponent) {
  if (value == 0.0f) return 0.0f;
  return std::copysign(std::pow(std::fabs(value), exponent), value);
}

inline float exp10Compat(float x) {
  return std::exp2(x * 3.3219280948873626f);
}

// ST 2084 PQ and BT.2100 HLG — same formulas as ME_OpenDRT-OFX OpenDRTCPUCore.h (eotf_pq / eotf_hlg).
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

std::size_t transferFunctionIndex(TransferFunctionId id) {
  const auto it = std::find_if(
      kTransferFunctions.begin(), kTransferFunctions.end(),
      [id](const TransferFunctionDefinition& def) { return def.id == id; });
  if (it == kTransferFunctions.end()) return 0;
  return static_cast<std::size_t>(std::distance(kTransferFunctions.begin(), it));
}

std::size_t primariesIndex(ColorPrimariesId id) {
  return static_cast<std::size_t>(std::clamp(static_cast<int>(id), 0, static_cast<int>(kPrimaries.size() - 1)));
}

const PrimariesDefinition& primariesDefinitionFromId(ColorPrimariesId id) {
  return kPrimaries[primariesIndex(id)];
}

const TransferFunctionDefinition& transferFunctionDefinitionFromId(TransferFunctionId id) {
  return kTransferFunctions[transferFunctionIndex(id)];
}

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

float decodeChannel(float x, TransferFunctionId tf);

float encodeChannel(float lin, TransferFunctionId tf) {
  switch (tf) {
    case TransferFunctionId::Linear:
      return lin;
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
    case TransferFunctionId::DavinciIntermediate: {
      constexpr float kDiv = 10.44426855f;
      constexpr float kLinCut = 0.02740668f / kDiv;
      if (lin <= kLinCut) return lin * kDiv;
      return (std::log2(lin + 0.0075f) + 7.0f) * 0.07329248f;
    }
    case TransferFunctionId::AcesCct:
      // Closed-form inverse of decodeChannel(AcesCct); avoids bisection (very slow per pixel when blending
      // wide-gamut inputs produces HDR linear values and the default [lo,hi] bracket needs many expansions).
      if (std::isfinite(lin) && lin >= 0.0f) {
        constexpr float kXCut = 0.155251141552511f;
        constexpr float kOffset = 0.0729055341958355f;
        constexpr float kDivLin = 10.5402377416545f;
        const float linCut = (kXCut - kOffset) / kDivLin;
        if (lin <= linCut) {
          return lin * kDivLin + kOffset;
        }
        return (std::log2(std::max(lin, 1e-30f)) + 9.72f) / 17.52f;
      }
      [[fallthrough]];
    default:
      break;
  }
  if (!std::isfinite(lin)) return 0.0f;
  auto evalMinusLin = [lin, tf](float enc) { return decodeChannel(enc, tf) - lin; };
  float lo = -0.25f;
  // Start with a wider upper bound when linear light is already bright; reduces expansion-loop iterations
  // for log-like curves after HDR / wide-gamut processing.
  float hi = (lin > 4.0f) ? std::min(512.0f, std::max(32.0f, lin * 2.0f)) : 8.0f;
  float fLo = evalMinusLin(lo);
  float fHi = evalMinusLin(hi);
  for (int expand = 0; expand < 24 && fLo * fHi > 0.0f; ++expand) {
    hi += 4.0f;
    fHi = evalMinusLin(hi);
  }
  if (fLo * fHi > 0.0f) {
    lo = -5.0f;
    hi = 25.0f;
    fLo = evalMinusLin(lo);
    fHi = evalMinusLin(hi);
  }
  if (fLo * fHi > 0.0f) return clampf(lin, 0.0f, 1.0f);
  for (int i = 0; i < 40; ++i) {
    const float mid = 0.5f * (lo + hi);
    const float fMid = evalMinusLin(mid);
    if (fLo * fMid <= 0.0f) {
      hi = mid;
      fHi = fMid;
    } else {
      lo = mid;
      fLo = fMid;
    }
  }
  return 0.5f * (lo + hi);
}

float decodeChannel(float x, TransferFunctionId tf) {
  switch (tf) {
    case TransferFunctionId::Linear:
      return x;
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
    case TransferFunctionId::DavinciIntermediate:
      return x <= 0.02740668f ? x / 10.44426855f : std::exp2(x / 0.07329248f - 7.0f) - 0.0075f;
    case TransferFunctionId::AcesCct:
      return x <= 0.155251141552511f ? (x - 0.0729055341958355f) / 10.5402377416545f
                                     : std::exp2(x * 17.52f - 9.72f);
    case TransferFunctionId::ArriLogC3:
      return x < 5.367655f * 0.010591f + 0.092809f
                 ? (x - 0.092809f) / 5.367655f
                 : (exp10Compat((x - 0.385537f) / 0.247190f) - 0.052272f) / 5.555556f;
    case TransferFunctionId::ArriLogC4:
      return x < -0.7774983977293537f
                 ? x * 0.3033266726886969f - 0.7774983977293537f
                 : (std::exp2(14.0f * (x - 0.09286412512218964f) / 0.9071358748778103f + 6.0f) - 64.0f) / 2231.8263090676883f;
    case TransferFunctionId::CanonLog2: {
      constexpr float kCut = 0.092864125f;
      constexpr float kScale = 0.24136077f;
      constexpr float kGain = 87.099375f;
      const float decoded = x < kCut ? -(exp10Compat((kCut - x) / kScale) - 1.0f) / kGain
                                     : (exp10Compat((x - kCut) / kScale) - 1.0f) / kGain;
      return decoded * 0.9f;
    }
    case TransferFunctionId::SonySLog3:
      return x < 171.2102946929f / 1023.0f
                 ? (x * 1023.0f - 95.0f) * 0.01125f / (171.2102946929f - 95.0f)
                 : (exp10Compat((x * 1023.0f - 420.0f) / 261.5f) * 0.19f - 0.01f);
    case TransferFunctionId::CanonLog3:
      if (x < 0.04076162f) {
        return -(exp10Compat((0.069886632f - x) / 0.42889912f) - 1.0f) / 14.98325f;
      }
      if (x <= 0.105357102f) {
        return (x - 0.073059361f) / 2.3069815f;
      }
      return (exp10Compat((x - 0.073059361f) / 0.36726845f) - 1.0f) / 14.98325f;
    case TransferFunctionId::RedLog3G10:
      return x < 0.0f ? (x / 15.1927f) - 0.01f
                      : (exp10Compat(x / 0.224282f) - 1.0f) / 155.975327f - 0.01f;
    case TransferFunctionId::FilmlightTLog:
      return x < 0.075f ? (x - 0.075f) / 16.184376489665897f
                        : std::exp((x - 0.5520126568606655f) / 0.09232902596577353f) - 0.0057048244042473785f;
    case TransferFunctionId::PanasonicVLog:
      return x < 0.181f ? (x - 0.125f) / 5.6f : exp10Compat((x - 0.598206f) / 0.241514f) - 0.00873f;
    case TransferFunctionId::FujiFLog2:
      return x < 0.100686685370811f ? (x - 0.092864f) / 8.799461f
                                    : (exp10Compat((x - 0.384316f) / 0.245281f) / 5.555556f - 0.064829f / 5.555556f);
    default:
      return x;
  }
}

}  // namespace

std::size_t inputPrimariesCount() { return kInputPrimaries.size(); }

const PrimariesDefinition& inputPrimariesDefinition(std::size_t index) {
  const std::size_t n = kInputPrimaries.size();
  const std::size_t clamped = std::min(index, n - 1);
  return primariesDefinitionFromId(kInputPrimaries[clamped]);
}

ColorPrimariesId inputPrimariesIdFromChoiceIndex(int index) {
  const std::size_t n = kInputPrimaries.size();
  const std::size_t clamped = static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(n - 1)));
  return kInputPrimaries[clamped];
}

int inputPrimariesChoiceIndex(ColorPrimariesId id) {
  const std::size_t n = kInputPrimaries.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (kInputPrimaries[i] == id) return static_cast<int>(i);
  }
  return 0;
}

std::size_t inputTransferFunctionCount() { return kInputTransferFunctions.size(); }

const TransferFunctionDefinition& inputTransferFunctionDefinition(std::size_t index) {
  const std::size_t n = kInputTransferFunctions.size();
  const std::size_t clamped = std::min(index, n - 1);
  return transferFunctionDefinitionFromId(kInputTransferFunctions[clamped]);
}

TransferFunctionId inputTransferFunctionIdFromChoiceIndex(int index) {
  const std::size_t n = kInputTransferFunctions.size();
  const std::size_t clamped = static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(n - 1)));
  return kInputTransferFunctions[clamped];
}

int inputTransferFunctionChoiceIndex(TransferFunctionId id) {
  const std::size_t n = kInputTransferFunctions.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (kInputTransferFunctions[i] == id) return static_cast<int>(i);
  }
  return 0;
}

Vec2f whitePoint(ColorPrimariesId id) {
  return primariesDefinitionFromId(id).white;
}

Mat3f rgbToXyzMatrix(ColorPrimariesId id) {
  const PrimariesDefinition& primaries = primariesDefinitionFromId(id);
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

bool isFinite(Vec2f v) {
  return std::isfinite(v.x) && std::isfinite(v.y);
}

bool isFinite(Vec3f v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3f xyToXyz(Vec2f xy, float Y) {
  if (std::fabs(xy.y) <= 1e-8f) {
    return {xy.x, Y, 1.0f - xy.x};
  }
  return {xy.x * Y / xy.y, Y, (1.0f - xy.x - xy.y) * Y / xy.y};
}

XyY xyzToXyY(Vec3f xyz, Vec2f fallbackWhite) {
  if (std::fabs(xyz.y) <= 1e-8f) {
    return {fallbackWhite.x, fallbackWhite.y, 0.0f};
  }
  const float sum = xyz.x + xyz.y + xyz.z;
  if (std::fabs(sum) <= 1e-8f) {
    return {fallbackWhite.x, fallbackWhite.y, xyz.y};
  }
  return {xyz.x / sum, xyz.y / sum, xyz.y};
}

Vec2f xyzToXy(Vec3f xyz, Vec2f fallbackWhite) {
  const XyY xyY = xyzToXyY(xyz, fallbackWhite);
  return {xyY.x, xyY.y};
}

Vec2f xyToUvp(Vec2f xy) {
  const float d = -2.0f * xy.x + 12.0f * xy.y + 3.0f;
  if (std::fabs(d) <= 1e-12f) return {};
  const float up = (4.0f * xy.x) / d;
  const float vp = (9.0f * xy.y) / d;
  Vec2f out{up, vp};
  return isFinite(out) ? out : Vec2f{};
}

Vec2f uvpToXy(Vec2f uv) {
  const float denom = 1.0f + 0.5f * uv.x - (4.0f / 3.0f) * uv.y;
  if (std::fabs(denom) <= 1e-12f) return {};
  const float d = 3.0f / denom;
  Vec2f out{uv.x * d * 0.25f, uv.y * d / 9.0f};
  return isFinite(out) ? out : Vec2f{};
}

// Kirk (2019) CIC: MATRIX_XYZ_TO_LMS_KIRK2019 (Colour / paper); approximate 1931 XYZ → 2006 LMS.
constexpr Mat3f kKirk2019XyzToLms{{{0.257085f, 0.859943f, -0.031061f},
                                     {-0.394427f, 1.175800f, 0.106423f},
                                     {0.064856f, -0.076250f, 0.559067f}}};

const Mat3f& kirk2019LmsToXyzMatrix() {
  static const Mat3f kInv = invert(kKirk2019XyzToLms);
  return kInv;
}

Vec2f xyzToKirkRg(Vec3f xyz) {
  const float kNan = std::numeric_limits<float>::quiet_NaN();
  const Vec2f kInvalid{kNan, kNan};
  if (!isFinite(xyz)) return kInvalid;
  const Vec3f lms = mul(kKirk2019XyzToLms, xyz);
  const float sum = lms.x + lms.y + lms.z;
  if (std::fabs(sum) <= 1e-12f) return kInvalid;
  const float l = lms.x / sum;
  const float m = lms.y / sum;
  const float r = 1.0671f * l - 0.6873f * m + 0.02062f;
  const float g = -0.0362f * l + 1.7182f * m - 0.05155f;
  Vec2f out{r, g};
  return isFinite(out) ? out : kInvalid;
}

Vec2f kirkRgToXyChromaticity(Vec2f rg) {
  const float kNan = std::numeric_limits<float>::quiet_NaN();
  const Vec2f kInvalid{kNan, kNan};
  if (!isFinite(rg)) return kInvalid;
  const float l = 0.95f * rg.x + 0.38f * rg.y;
  const float m = 0.02f * rg.x + 0.59f * rg.y + 0.03f;
  const float s = 1.0f - l - m;
  if (l < -0.02f || m < -0.02f || s < -0.02f) return kInvalid;
  constexpr float kWl = 0.68990272f;
  constexpr float kWm = 0.34832189f;
  const float denom = kWl * l + kWm * m;
  if (std::fabs(denom) <= 1e-12f) return kInvalid;
  const float a = 1.0f / denom;
  const Vec3f lms{a * l, a * m, a * s};
  const Vec3f xyz = mul(kirk2019LmsToXyzMatrix(), lms);
  if (!isFinite(xyz)) return kInvalid;
  return xyzToXy(xyz, {kOneThird, kOneThird});
}

Vec2f standardObserverToInputObserver(Vec2f xy, ColorPrimariesId inputPrimaries) {
  if (inputPrimaries == ColorPrimariesId::Xyz) return xy;
  const Vec3f xyz = xyToXyz(xy);
  const Vec3f rgb = mul(xyzToRgbMatrix(inputPrimaries), xyz);
  return xyzToXy(rgb, {kOneThird, kOneThird});
}

Vec2f inputObserverToStandardObserver(Vec2f xy, ColorPrimariesId inputPrimaries) {
  if (inputPrimaries == ColorPrimariesId::Xyz) return xy;
  const Vec3f rgb = xyToXyz(xy);
  const Vec3f xyz = mul(rgbToXyzMatrix(inputPrimaries), rgb);
  return xyzToXy(xyz, {kOneThird, kOneThird});
}

const std::array<Vec3f, 82>& cie1931XyzCmfs5nm() {
  return kCie1931XyzCmfs5nm;
}

bool blackBodyChromaticity(float kelvin, Vec2f* xy) {
  if (!xy || !std::isfinite(kelvin) || kelvin < 1000.0f || kelvin > 25000.0f) return false;

  double X = 0.0;
  double Y = 0.0;
  double Z = 0.0;
  for (std::size_t i = 0; i + 1 < kCie1931XyzCmfs5nm.size(); ++i) {
    const double wavelengthNm = 380.0 + static_cast<double>(i) * 5.0;
    const double wavelengthM = wavelengthNm * 1.0e-9;
    const double exponent = kPlanckC2 / (wavelengthM * static_cast<double>(kelvin));
    const double spd = kPlanckC1 /
                       (std::pow(wavelengthM, 5.0) * std::max(std::exp(exponent) - 1.0, 1e-30));
    X += spd * static_cast<double>(kCie1931XyzCmfs5nm[i].x);
    Y += spd * static_cast<double>(kCie1931XyzCmfs5nm[i].y);
    Z += spd * static_cast<double>(kCie1931XyzCmfs5nm[i].z);
  }

  const double sum = X + Y + Z;
  if (!(sum > 1e-30)) return false;
  xy->x = static_cast<float>(X / sum);
  xy->y = static_cast<float>(Y / sum);
  return isFinite(*xy);
}

float nearestBlackBodyTemperature(Vec2f xy, float minKelvin, float maxKelvin) {
  if (!isFinite(xy)) return 0.0f;
  minKelvin = std::clamp(minKelvin, 1000.0f, 25000.0f);
  maxKelvin = std::clamp(maxKelvin, minKelvin, 25000.0f);

  auto distanceSqAt = [xy](float kelvin) {
    Vec2f sample{};
    if (!blackBodyChromaticity(kelvin, &sample)) return std::numeric_limits<float>::max();
    const float dx = sample.x - xy.x;
    const float dy = sample.y - xy.y;
    return dx * dx + dy * dy;
  };

  float bestKelvin = minKelvin;
  float bestDistanceSq = std::numeric_limits<float>::max();
  const float coarseStep = 100.0f;
  for (float kelvin = minKelvin; kelvin <= maxKelvin; kelvin += coarseStep) {
    const float distanceSq = distanceSqAt(kelvin);
    if (distanceSq < bestDistanceSq) {
      bestDistanceSq = distanceSq;
      bestKelvin = kelvin;
    }
  }

  for (float step : {10.0f, 1.0f}) {
    const float localMin = std::max(minKelvin, bestKelvin - coarseStep);
    const float localMax = std::min(maxKelvin, bestKelvin + coarseStep);
    for (float kelvin = localMin; kelvin <= localMax; kelvin += step) {
      const float distanceSq = distanceSqAt(kelvin);
      if (distanceSq < bestDistanceSq) {
        bestDistanceSq = distanceSq;
        bestKelvin = kelvin;
      }
    }
  }

  return bestKelvin;
}

std::vector<Vec2f> blackBodyChromaticityCurve(float minKelvin, float maxKelvin, std::size_t steps) {
  std::vector<Vec2f> curve;
  if (steps < 2) return curve;
  minKelvin = std::clamp(minKelvin, 1000.0f, 25000.0f);
  maxKelvin = std::clamp(maxKelvin, minKelvin, 25000.0f);
  curve.reserve(steps);
  for (std::size_t i = 0; i < steps; ++i) {
    const float t = steps > 1 ? static_cast<float>(i) / static_cast<float>(steps - 1) : 0.0f;
    const float kelvin = minKelvin + (maxKelvin - minKelvin) * t;
    Vec2f sample{};
    if (blackBodyChromaticity(kelvin, &sample)) {
      curve.push_back(sample);
    }
  }
  return curve;
}

Vec3f cie1931CmfXyzInterpolated(float wavelengthNm) {
  const auto& tab = cie1931XyzCmfs5nm();
  const std::size_t last = tab.size() - 1u;
  const float u = (wavelengthNm - 380.0f) / 5.0f;
  const float uMax = static_cast<float>(last);
  const float uc = std::clamp(u, 0.0f, uMax);
  const std::size_t i = static_cast<std::size_t>(std::floor(uc));
  const float f = uc - static_cast<float>(i);
  const Vec3f& a = tab[std::min(i, last)];
  const Vec3f& b = tab[std::min(i + 1u, last)];
  return {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f};
}

namespace {

// OKLAB.dctl (Nico Wieseneder): Alexa Wide Gamut basis, rgb inset/outset, Björn Ottosson OKLAB.

Vec3f rgbInsetAwgDctl(Vec3f rgb) {
  return {rgb.x * 0.8357302628472827f + rgb.y * 0.07575160430529301f + rgb.z * 0.08851819789301629f,
          rgb.x * 0.07272987026297614f + rgb.y * 0.8387515418967071f + rgb.z * 0.08851798996797691f,
          rgb.x * 0.07272966839689886f + rgb.y * 0.07575121559727113f + rgb.z * 0.8515177877383528f};
}

Vec3f rgbOutsetAwgDctl(Vec3f rgb) {
  return {rgb.x * 1.215294430519138f + rgb.y * -0.09928130201704202f + rgb.z * -0.11601342100604005f,
          rgb.x * -0.09532096746629887f + rgb.y * 1.2113347084396262f + rgb.z * -0.11601316464611539f,
          rgb.x * -0.09532071196581424f + rgb.y * -0.09928081562487852f + rgb.z * 1.1946030611860257f};
}

Mat3f xyzToAwgDctlMatrix() {
  Mat3f m{};
  m.m[0][0] = 1.789065477f;
  m.m[0][1] = -0.482533851f;
  m.m[0][2] = -0.200075779f;
  m.m[1][0] = -0.639848612f;
  m.m[1][1] = 1.396399948f;
  m.m[1][2] = 0.194432282f;
  m.m[2][0] = -0.041531525f;
  m.m[2][1] = 0.082335361f;
  m.m[2][2] = 0.878868460f;
  return m;
}

Vec3f awgToOklabDctlFromInsetAwg(Vec3f rgb) {
  const float X = (rgb.x * 0.63800764f) + (rgb.y * 0.21470386f) + (rgb.z * 0.09774445f);
  const float Y = (rgb.x * 0.29195377f) + (rgb.y * 0.82384104f) + (rgb.z * -0.11579482f);
  const float Z = (rgb.x * 0.00279827f) + (rgb.y * -0.06703423f) + (rgb.z * 1.15329373f);
  const float l = (X * 0.8189330101f) + (Y * 0.3618667424f) + (Z * -0.1288597137f);
  const float m = (X * 0.0329845436f) + (Y * 0.9293118715f) + (Z * 0.0361456387f);
  const float s = (X * 0.0482003018f) + (Y * 0.2643662691f) + (Z * 0.6338517070f);
  const float lPrime = std::cbrt(l);
  const float mPrime = std::cbrt(m);
  const float sPrime = std::cbrt(s);
  return {(lPrime * 0.2104542553f) + (mPrime * 0.7936177850f) + (sPrime * -0.0040720468f),
          (lPrime * 1.9779984951f) + (mPrime * -2.4285922050f) + (sPrime * 0.4505937099f),
          (lPrime * 0.0259040371f) + (mPrime * 0.7827717662f) + (sPrime * -0.8086757660f)};
}

Vec3f oklabToAwgDctlBeforeOutset(Vec3f lab) {
  const float lPrime = (lab.x * 1.00000000f) + (lab.y * 0.39633784f) + (lab.z * 0.21580376f);
  const float mPrime = (lab.x * 1.00000000f) + (lab.y * -0.10556135f) + (lab.z * -0.06385418f);
  const float sPrime = (lab.x * 1.00000000f) + (lab.y * -0.08948418f) + (lab.z * -1.29148555f);
  const float l = std::copysign(std::pow(std::fabs(lPrime), 3.0f), lPrime);
  const float m = std::copysign(std::pow(std::fabs(mPrime), 3.0f), mPrime);
  const float s = std::copysign(std::pow(std::fabs(sPrime), 3.0f), sPrime);
  const float X = (l * 1.2270138511f) + (m * -0.55779998065f) + (s * 0.2812561489f);
  const float Y = (l * -0.0405801784f) + (m * 1.1122568696f) + (s * -0.0716766786f);
  const float Z = (l * -0.0763812845f) + (m * -0.4214819784f) + (s * 1.5861632204f);
  return {(X * 1.78906548f) + (Y * -0.48253384f) + (Z * -0.20007578f),
          (X * -0.63984859f) + (Y * 1.39639986f) + (Z * 0.19443229f),
          (X * -0.04153153f) + (Y * 0.08233536f) + (Z * 0.8788684f)};
}

Vec3f okLabDctlForwardXyzToLab(Vec3f xyz) {
  const Vec3f awg = mul(xyzToAwgDctlMatrix(), xyz);
  return awgToOklabDctlFromInsetAwg(rgbInsetAwgDctl(awg));
}

Vec3f okLabDctlDisplayFromLab(Vec3f lab) {
  const Vec3f awgLin = oklabToAwgDctlBeforeOutset(lab);
  const Vec3f out = rgbOutsetAwgDctl(awgLin);
  return {std::clamp(out.x, 0.0f, 1.0f), std::clamp(out.y, 0.0f, 1.0f), std::clamp(out.z, 0.0f, 1.0f)};
}

Vec3f okLabDctlDisplayFromXyzUnscaled(Vec3f xyz) {
  return okLabDctlDisplayFromLab(okLabDctlForwardXyzToLab(xyz));
}

}  // namespace

Vec3f okLabDctlDisplayFromCmfXyz(Vec3f cmfXyz) {
  const float mx = std::max(cmfXyz.x, std::max(cmfXyz.y, cmfXyz.z));
  if (!(mx > 1e-20f)) {
    return {0.0f, 0.0f, 0.0f};
  }
  const Vec3f scaled{cmfXyz.x / mx, cmfXyz.y / mx, cmfXyz.z / mx};
  return okLabDctlDisplayFromXyzUnscaled(scaled);
}

Vec3f okLabDctlDisplayFromXy(Vec2f xy, float Y) {
  return okLabDctlDisplayFromXyzUnscaled(xyToXyz(xy, Y));
}

Vec3f okLabDctlDisplayPurpleMixFromCmf(Vec3f cmfXyz380, Vec3f cmfXyz780, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  const float m0 = std::max(cmfXyz380.x, std::max(cmfXyz380.y, cmfXyz380.z));
  const float m1 = std::max(cmfXyz780.x, std::max(cmfXyz780.y, cmfXyz780.z));
  if (!(m0 > 1e-20f) || !(m1 > 1e-20f)) {
    return {0.0f, 0.0f, 0.0f};
  }
  const Vec3f a{cmfXyz380.x / m0, cmfXyz380.y / m0, cmfXyz380.z / m0};
  const Vec3f b{cmfXyz780.x / m1, cmfXyz780.y / m1, cmfXyz780.z / m1};
  const Vec3f labA = okLabDctlForwardXyzToLab(a);
  const Vec3f labB = okLabDctlForwardXyzToLab(b);
  const Vec3f lab{labA.x + (labB.x - labA.x) * t, labA.y + (labB.y - labA.y) * t,
                   labA.z + (labB.z - labA.z) * t};
  return okLabDctlDisplayFromLab(lab);
}

}  // namespace WorkshopColor
