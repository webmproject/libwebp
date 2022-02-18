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

#include "src/dsp/cpu.h"

//-----------------------------------------------------------------------------

#if !WEBP_NEON_OMIT_C_CODE
#define MAX_Y ((1 << 10) - 1)    // 10b precision over 16b-arithmetic
static uint16_t clip_y(int v) {
  return (v < 0) ? 0 : (v > MAX_Y) ? MAX_Y : (uint16_t)v;
}

static uint64_t SharpYUVUpdateY_C(const uint16_t* ref, const uint16_t* src,
                                  uint16_t* dst, int len) {
  uint64_t diff = 0;
  int i;
  for (i = 0; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)dst[i] + diff_y;
    dst[i] = clip_y(new_y);
    diff += (uint64_t)abs(diff_y);
  }
  return diff;
}

static void SharpYUVUpdateRGB_C(const int16_t* ref, const int16_t* src,
                                int16_t* dst, int len) {
  int i;
  for (i = 0; i < len; ++i) {
    const int diff_uv = ref[i] - src[i];
    dst[i] += diff_uv;
  }
}

static void SharpYUVFilterRow_C(const int16_t* A, const int16_t* B, int len,
                                const uint16_t* best_y, uint16_t* out) {
  int i;
  for (i = 0; i < len; ++i, ++A, ++B) {
    const int v0 = (A[0] * 9 + A[1] * 3 + B[0] * 3 + B[1] + 8) >> 4;
    const int v1 = (A[1] * 9 + A[0] * 3 + B[1] * 3 + B[0] + 8) >> 4;
    out[2 * i + 0] = clip_y(best_y[2 * i + 0] + v0);
    out[2 * i + 1] = clip_y(best_y[2 * i + 1] + v1);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#undef MAX_Y

//-----------------------------------------------------------------------------

uint64_t (*SharpYUVUpdateY)(const uint16_t* src, const uint16_t* ref,
                            uint16_t* dst, int len);
void (*SharpYUVUpdateRGB)(const int16_t* src, const int16_t* ref, int16_t* dst,
                          int len);
void (*SharpYUVFilterRow)(const int16_t* A, const int16_t* B, int len,
                          const uint16_t* best_y, uint16_t* out);

extern void InitSharpYUVSSE2(void);
extern void InitSharpYUVNEON(void);

WEBP_DSP_INIT_FUNC(InitSharpYuv) {

#if !WEBP_NEON_OMIT_C_CODE
  SharpYUVUpdateY = SharpYUVUpdateY_C;
  SharpYUVUpdateRGB = SharpYUVUpdateRGB_C;
  SharpYUVFilterRow = SharpYUVFilterRow_C;
#endif

#if defined(WEBP_HAVE_SSE2)
  if (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kSSE2)) {
    InitSharpYUVSSE2();
  }
#endif  // WEBP_HAVE_SSE2

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    InitSharpYUVNEON();
  }
#endif  // WEBP_HAVE_NEON

  assert(SharpYUVUpdateY != NULL);
  assert(SharpYUVUpdateRGB != NULL);
  assert(SharpYUVFilterRow != NULL);
}
