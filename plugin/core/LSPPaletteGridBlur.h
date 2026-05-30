#pragma once

/** Optional Gaussian blur on a small row-major RGBA32F analysis grid (linear RGB in .xyz, .w unused or 1).
 *  Used to reduce texture detail before palette clustering. */
namespace LSPPaletteGridBlur {

/** Apple MPSImageGaussianBlur when Metal + MPS are available; copies src → dst (same size). Returns false to use CPU path. */
bool tryMpsGaussianBlur(const float* src, float* dst, int nw, int nh, float sigmaPixels);

/** CPU fallback blur (Van Vliet recursive Gaussian approximation); sigma in grid pixels. */
void cpuGaussianBlur(const float* src, float* dst, int nw, int nh, float sigmaPixels, float* tmpRowMajor);

} // namespace LSPPaletteGridBlur
