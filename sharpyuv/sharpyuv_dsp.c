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

#include "sharpyuv/sharpyuv_dsp.h"

#include <assert.h>
#include <stdlib.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_cpu.h"
#include "src/webp/types.h"

//-----------------------------------------------------------------------------

#if !WEBP_NEON_OMIT_C_CODE
static uint16_t clip(int v, int max) {
  return (v < 0) ? 0 : (v > max) ? max : (uint16_t)v;
}

static uint64_t SharpYuvUpdateY_C(const uint16_t* ref, const uint16_t* src,
                                  uint16_t* dst, int len, int bit_depth) {
  uint64_t diff = 0;
  int i;
  const int max_y = (1 << bit_depth) - 1;
  for (i = 0; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)dst[i] + diff_y;
    dst[i] = clip(new_y, max_y);
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
    out[2 * i + 0] = clip(best_y[2 * i + 0] + v0, max_y);
    out[2 * i + 1] = clip(best_y[2 * i + 1] + v1, max_y);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#define YUV_FIX 16  // fixed-point precision for RGB->YUV
static const int kYuvHalf = 1 << (YUV_FIX - 1);

// Maps a value in [0, (256 << YUV_FIX) - 1] to [0,
// precomputed_scores_table_sampling - 1]. It is important that the extremal
// values are preserved and 1:1 mapped:
//  ConvertValue(0) = 0
//  ConvertValue((256 << 16) - 1) = rgb_sampling_size - 1
static int SharpYuvConvertValueToSampledIdx(int v, int rgb_sampling_size) {
  v = (v + kYuvHalf) >> YUV_FIX;
  v = (v < 0) ? 0 : (v > 255) ? 255 : v;
  return (v * (rgb_sampling_size - 1)) / 255;
}

#undef YUV_FIX

static int SharpYuvConvertToYuvSharpnessIndex(
    int r, int g, int b, const SharpYuvConversionMatrix* matrix,
    int precomputed_scores_table_sampling) {
  const int y = SharpYuvConvertValueToSampledIdx(
      matrix->rgb_to_y[0] * r + matrix->rgb_to_y[1] * g +
          matrix->rgb_to_y[2] * b + matrix->rgb_to_y[3],
      precomputed_scores_table_sampling);
  const int u = SharpYuvConvertValueToSampledIdx(
      matrix->rgb_to_u[0] * r + matrix->rgb_to_u[1] * g +
          matrix->rgb_to_u[2] * b + matrix->rgb_to_u[3],
      precomputed_scores_table_sampling);
  const int v = SharpYuvConvertValueToSampledIdx(
      matrix->rgb_to_v[0] * r + matrix->rgb_to_v[1] * g +
          matrix->rgb_to_v[2] * b + matrix->rgb_to_v[3],
      precomputed_scores_table_sampling);
  return y + u * precomputed_scores_table_sampling +
         v * precomputed_scores_table_sampling *
             precomputed_scores_table_sampling;
}

static void SharpYuvRowToYuvSharpnessIndex_C(
    const uint8_t* r_ptr, const uint8_t* g_ptr, const uint8_t* b_ptr,
    int rgb_step, int rgb_bit_depth, int width, uint16_t* dst,
    const SharpYuvConversionMatrix* matrix,
    int precomputed_scores_table_sampling) {
  int i;
  assert(rgb_bit_depth == 8);
  (void)rgb_bit_depth;  // Unused for now.
  for (i = 0; i < width;
       ++i, r_ptr += rgb_step, g_ptr += rgb_step, b_ptr += rgb_step) {
    dst[i] =
        SharpYuvConvertToYuvSharpnessIndex(r_ptr[0], g_ptr[0], b_ptr[0], matrix,
                                           precomputed_scores_table_sampling);
  }
}

//-----------------------------------------------------------------------------

uint64_t (*SharpYuvUpdateY)(const uint16_t* src, const uint16_t* ref,
                            uint16_t* dst, int len, int bit_depth);
void (*SharpYuvUpdateRGB)(const int16_t* src, const int16_t* ref, int16_t* dst,
                          int len);
void (*SharpYuvFilterRow)(const int16_t* A, const int16_t* B, int len,
                          const uint16_t* best_y, uint16_t* out, int bit_depth);
void (*SharpYuvRowToYuvSharpnessIndex)(const uint8_t* r_ptr,
                                       const uint8_t* g_ptr,
                                       const uint8_t* b_ptr, int rgb_step,
                                       int rgb_bit_depth, int width,
                                       uint16_t* dst,
                                       const SharpYuvConversionMatrix* matrix,
                                       int precomputed_scores_table_sampling);

extern VP8CPUInfo SharpYuvGetCPUInfo;
extern void InitSharpYuvSSE2(void);
extern void InitSharpYuvNEON(void);

void SharpYuvInitDsp(void) {
#if !WEBP_NEON_OMIT_C_CODE
  SharpYuvUpdateY = SharpYuvUpdateY_C;
  SharpYuvUpdateRGB = SharpYuvUpdateRGB_C;
  SharpYuvFilterRow = SharpYuvFilterRow_C;
#endif
  // There is only a C version for now so always include it.
  SharpYuvRowToYuvSharpnessIndex = SharpYuvRowToYuvSharpnessIndex_C;

  if (SharpYuvGetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (SharpYuvGetCPUInfo(kSSE2)) {
      InitSharpYuvSSE2();
    }
#endif  // WEBP_HAVE_SSE2
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
}
