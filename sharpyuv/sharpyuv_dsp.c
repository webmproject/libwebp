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
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./sharpyuv_dsp.h"

#include <assert.h>
#include <stdlib.h>

#include "./sharpyuv_cpu.h"
#include "./sharpyuv_gamma.h"
#include "src/dsp/cpu.h"
#include "webp/types.h"

//-----------------------------------------------------------------------------

#if !WEBP_NEON_OMIT_C_CODE
static uint64_t SharpYuvUpdateY_C(const uint16_t* ref, const uint16_t* src,
                                  uint16_t* dst, int len, int bit_depth) {
  uint64_t diff = 0;
  int i;
  const int max_y = (1 << bit_depth) - 1;
  for (i = 0; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)dst[i] + diff_y;
    dst[i] = SharpYuvClip16(new_y, max_y);
    diff += (uint64_t)abs(diff_y);
  }
  return diff;
}

static void SharpYuvUpdateRGB_C(const int16_t* ref, const int16_t* src,
                                int16_t* dst, int len) {
  int i;
  for (i = 0; i < len; ++i) {
    const int diff_uv = ref[i] - src[i];
    dst[i] += diff_uv;
  }
}

static void SharpYuvFilterRow_C(const int16_t* A, const int16_t* B, int len,
                                const uint16_t* best_y, uint16_t* out,
                                int bit_depth) {
  int i;
  const int max_y = (1 << bit_depth) - 1;
  for (i = 0; i < len; ++i, ++A, ++B) {
    const int v0 = (A[0] * 9 + A[1] * 3 + B[0] * 3 + B[1] + 8) >> 4;
    const int v1 = (A[1] * 9 + A[0] * 3 + B[1] * 3 + B[0] + 8) >> 4;
    out[2 * i + 0] = SharpYuvClip16(best_y[2 * i + 0] + v0, max_y);
    out[2 * i + 1] = SharpYuvClip16(best_y[2 * i + 1] + v1, max_y);
  }
}

static void SharpYuvConvertRowY_C(const uint16_t* best_y,
                                  const int16_t* best_uv, int width, int uv_w,
                                  const int coeffs[4], int sfix,
                                  int yuv_bit_depth, void* y_out) {
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  int i;
  for (i = 0; i < width; ++i) {
    const int off = i >> 1;
    const int W = best_y[i];
    const int r = best_uv[off + 0 * uv_w] + W;
    const int g = best_uv[off + 1 * uv_w] + W;
    const int b = best_uv[off + 2 * uv_w] + W;
    const int y = SharpYuvConvertComponent(r, g, b, coeffs, sfix);
    if (yuv_bit_depth <= 8) {
      ((uint8_t*)y_out)[i] = (uint8_t)SharpYuvClip16(y, 255);
    } else {
      ((uint16_t*)y_out)[i] = SharpYuvClip16(y, yuv_max);
    }
  }
}

static void SharpYuvConvertRowUV_C(const int16_t* best_uv, int uv_w,
                                   const int coeffs_u[4], const int coeffs_v[4],
                                   int sfix, int yuv_bit_depth, void* u_out,
                                   void* v_out) {
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  int i;
  for (i = 0; i < uv_w; ++i) {
    const int r = best_uv[i + 0 * uv_w];
    const int g = best_uv[i + 1 * uv_w];
    const int b = best_uv[i + 2 * uv_w];
    const int u = SharpYuvConvertComponent(r, g, b, coeffs_u, sfix);
    const int v = SharpYuvConvertComponent(r, g, b, coeffs_v, sfix);
    if (yuv_bit_depth <= 8) {
      ((uint8_t*)u_out)[i] = (uint8_t)SharpYuvClip16(u, 255);
      ((uint8_t*)v_out)[i] = (uint8_t)SharpYuvClip16(v, 255);
    } else {
      ((uint16_t*)u_out)[i] = SharpYuvClip16(u, yuv_max);
      ((uint16_t*)v_out)[i] = SharpYuvClip16(v, yuv_max);
    }
  }
}

// Fast path for the (default, most common) sRGB transfer function, used by
// UpdateW/UpdateChroma in sharpyuv.c.
static void SharpYuvUpdateWSrgb_C(const uint16_t* src, uint16_t* dst, int w,
                                  int bit_depth) {
  int i = 0;
  do {
    const uint32_t R = SharpYuvGammaToLinear(src[0 * w + i], bit_depth,
                                             kSharpYuvTransferFunctionSrgb);
    const uint32_t G = SharpYuvGammaToLinear(src[1 * w + i], bit_depth,
                                             kSharpYuvTransferFunctionSrgb);
    const uint32_t B = SharpYuvGammaToLinear(src[2 * w + i], bit_depth,
                                             kSharpYuvTransferFunctionSrgb);
    const int Y = SharpYuvRGBToGray(R, G, B);
    dst[i] = SharpYuvLinearToGamma((uint32_t)Y, bit_depth,
                                   kSharpYuvTransferFunctionSrgb);
  } while (++i < w);
}

static uint32_t ScaleDownSrgb_C(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                                int bit_depth) {
  const uint32_t A =
      SharpYuvGammaToLinear(a, bit_depth, kSharpYuvTransferFunctionSrgb);
  const uint32_t B =
      SharpYuvGammaToLinear(b, bit_depth, kSharpYuvTransferFunctionSrgb);
  const uint32_t C =
      SharpYuvGammaToLinear(c, bit_depth, kSharpYuvTransferFunctionSrgb);
  const uint32_t D =
      SharpYuvGammaToLinear(d, bit_depth, kSharpYuvTransferFunctionSrgb);
  return SharpYuvLinearToGamma((A + B + C + D + 2) >> 2, bit_depth,
                               kSharpYuvTransferFunctionSrgb);
}

static void SharpYuvUpdateChromaSrgb_C(const uint16_t* src1,
                                       const uint16_t* src2, int16_t* dst,
                                       int uv_w, int bit_depth) {
  int i = 0;
  do {
    const int r =
        (int)ScaleDownSrgb_C(src1[0 * uv_w + 0], src1[0 * uv_w + 1],
                             src2[0 * uv_w + 0], src2[0 * uv_w + 1], bit_depth);
    const int g =
        (int)ScaleDownSrgb_C(src1[2 * uv_w + 0], src1[2 * uv_w + 1],
                             src2[2 * uv_w + 0], src2[2 * uv_w + 1], bit_depth);
    const int b =
        (int)ScaleDownSrgb_C(src1[4 * uv_w + 0], src1[4 * uv_w + 1],
                             src2[4 * uv_w + 0], src2[4 * uv_w + 1], bit_depth);
    const int W = SharpYuvRGBToGray(r, g, b);
    dst[0 * uv_w] = (int16_t)(r - W);
    dst[1 * uv_w] = (int16_t)(g - W);
    dst[2 * uv_w] = (int16_t)(b - W);
    dst += 1;
    src1 += 2;
    src2 += 2;
  } while (++i < uv_w);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//-----------------------------------------------------------------------------

uint64_t (*SharpYuvUpdateY)(const uint16_t* src, const uint16_t* ref,
                            uint16_t* dst, int len, int bit_depth);
void (*SharpYuvUpdateRGB)(const int16_t* src, const int16_t* ref, int16_t* dst,
                          int len);
void (*SharpYuvFilterRow)(const int16_t* A, const int16_t* B, int len,
                          const uint16_t* best_y, uint16_t* out, int bit_depth);
void (*SharpYuvConvertRowY)(const uint16_t* best_y, const int16_t* best_uv,
                            int width, int uv_w, const int coeffs[4], int sfix,
                            int yuv_bit_depth, void* y_out);
void (*SharpYuvConvertRowUV)(const int16_t* best_uv, int uv_w,
                             const int coeffs_u[4], const int coeffs_v[4],
                             int sfix, int yuv_bit_depth, void* u_out,
                             void* v_out);
void (*SharpYuvUpdateWSrgb)(const uint16_t* src, uint16_t* dst, int w,
                            int bit_depth);
void (*SharpYuvUpdateChromaSrgb)(const uint16_t* src1, const uint16_t* src2,
                                 int16_t* dst, int uv_w, int bit_depth);

extern VP8CPUInfo SharpYuvGetCPUInfo;
extern void InitSharpYuvSSE2(void);
extern void InitSharpYuvAVX2(void);
extern void InitSharpYuvNEON(void);

void SharpYuvInitDsp(void) {
#if !WEBP_NEON_OMIT_C_CODE
  SharpYuvUpdateY = SharpYuvUpdateY_C;
  SharpYuvUpdateRGB = SharpYuvUpdateRGB_C;
  SharpYuvFilterRow = SharpYuvFilterRow_C;
  SharpYuvConvertRowY = SharpYuvConvertRowY_C;
  SharpYuvConvertRowUV = SharpYuvConvertRowUV_C;
  SharpYuvUpdateWSrgb = SharpYuvUpdateWSrgb_C;
  SharpYuvUpdateChromaSrgb = SharpYuvUpdateChromaSrgb_C;
#endif

  if (SharpYuvGetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (SharpYuvGetCPUInfo(kSSE2)) {
      InitSharpYuvSSE2();
    }
#endif  // WEBP_HAVE_SSE2
#if defined(WEBP_HAVE_AVX2)
    if (SharpYuvGetCPUInfo(kAVX2)) {
      InitSharpYuvAVX2();
    }
#endif  // WEBP_HAVE_AVX2
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (SharpYuvGetCPUInfo != NULL && SharpYuvGetCPUInfo(kNEON))) {
    InitSharpYuvNEON();
  }
#endif  // WEBP_HAVE_NEON

  assert(SharpYuvUpdateY != NULL);
  assert(SharpYuvUpdateRGB != NULL);
  assert(SharpYuvFilterRow != NULL);
  assert(SharpYuvConvertRowY != NULL);
  assert(SharpYuvConvertRowUV != NULL);
  assert(SharpYuvUpdateWSrgb != NULL);
  assert(SharpYuvUpdateChromaSrgb != NULL);
}
