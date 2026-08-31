// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Speed-critical functions for Sharp YUV.

#ifndef WEBP_SHARPYUV_SHARPYUV_DSP_H_
#define WEBP_SHARPYUV_SHARPYUV_DSP_H_

#include "./sharpyuv_cpu.h"
#include "webp/types.h"

// Shared fixed-point building blocks, reused by the C fallback and SIMD tails
#define SHARPYUV_YUV_FIX 16  // fixed-point precision for RGB->YUV

// Clamps v to [0, max].
static WEBP_INLINE uint16_t SharpYuvClip16(int v, int max) {
  return (v < 0) ? 0 : (v > max) ? (uint16_t)max : (uint16_t)v;
}

// (13933*r + 46871*g + 4732*b + 32768) >> 16, in exact int64 precision:
// r/g/b are usually <= 65536 (the sRGB-linear range), but some transfer
// functions' EOTF (e.g. Smpte428) can push them to ~71501, and even the
// bounded sRGB case comes within 1 of overflowing 32 bits at full white.
static WEBP_INLINE int SharpYuvRGBToGray(int64_t r, int64_t g, int64_t b) {
  const int64_t luma =
      13933 * r + 46871 * g + 4732 * b + (1LL << (SHARPYUV_YUV_FIX - 1));
  return (int)(luma >> SHARPYUV_YUV_FIX);
}

// round((coeffs[0]*r + coeffs[1]*g + coeffs[2]*b + coeffs[3]) >>
// (SHARPYUV_YUV_FIX + sfix)), fit in int64's
static WEBP_INLINE int SharpYuvConvertComponent(int r, int g, int b,
                                                const int coeffs[4], int sfix) {
  const int64_t srounder = 1LL << (SHARPYUV_YUV_FIX + sfix - 1);
  const int64_t luma = (int64_t)coeffs[0] * r + (int64_t)coeffs[1] * g +
                       (int64_t)coeffs[2] * b + coeffs[3] + srounder;
  return (int)(luma >> (SHARPYUV_YUV_FIX + sfix));
}

extern uint64_t (*SharpYuvUpdateY)(const uint16_t* src, const uint16_t* ref,
                                   uint16_t* dst, int len, int bit_depth);
extern void (*SharpYuvUpdateRGB)(const int16_t* src, const int16_t* ref,
                                 int16_t* dst, int len);
extern void (*SharpYuvFilterRow)(const int16_t* A, const int16_t* B, int len,
                                 const uint16_t* best_y, uint16_t* out,
                                 int bit_depth);

// Reconstructs one row of the final Y plane from the W (luma proxy) row and
// the corresponding (2x subsampled) RGB-offset row. 'width' is the number of
// output samples, 'uv_w' the stride between the r/g/b planes of best_uv.
// coeffs is a yuv_matrix->rgb_to_y-style {kr, kg, kb, add} row (see
// RGBToYUVComponent in sharpyuv.c, which this must stay in sync with).
// y_out points to a uint8_t buffer if yuv_bit_depth <= 8, uint16_t otherwise.
extern void (*SharpYuvConvertRowY)(const uint16_t* best_y,
                                   const int16_t* best_uv, int width, int uv_w,
                                   const int coeffs[4], int sfix,
                                   int yuv_bit_depth, void* y_out);

// Same, for one row of the U/V planes (no upsampling involved).
extern void (*SharpYuvConvertRowUV)(const int16_t* best_uv, int uv_w,
                                    const int coeffs_u[4],
                                    const int coeffs_v[4], int sfix,
                                    int yuv_bit_depth, void* u_out,
                                    void* v_out);

// Fast paths for the (default, most common) kSharpYuvTransferFunctionSrgb
// transfer function, used by UpdateW/UpdateChroma in sharpyuv.c. The caller
// is responsible for checking transfer_type and falling back to the
// (unchanged) generic per-pixel path for any other transfer function.
extern void (*SharpYuvUpdateWSrgb)(const uint16_t* src, uint16_t* dst, int w,
                                   int bit_depth);
extern void (*SharpYuvUpdateChromaSrgb)(const uint16_t* src1,
                                        const uint16_t* src2, int16_t* dst,
                                        int uv_w, int bit_depth);

void SharpYuvInitDsp(void);

#endif  // WEBP_SHARPYUV_SHARPYUV_DSP_H_
