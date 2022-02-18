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

#if defined(WEBP_USE_SSE2)
#include <stdlib.h>
#include <emmintrin.h>
#endif

extern void InitSharpYUVSSE2(void);

#if defined(WEBP_USE_SSE2)

#define MAX_Y ((1 << 10) - 1)    // 10b precision over 16b-arithmetic
static uint16_t clip_y(int v) {
  return (v < 0) ? 0 : (v > MAX_Y) ? MAX_Y : (uint16_t)v;
}

static uint64_t SharpYUVUpdateY_SSE2(const uint16_t* ref, const uint16_t* src,
                                     uint16_t* dst, int len) {
  uint64_t diff = 0;
  uint32_t tmp[4];
  int i;
  const __m128i zero = _mm_setzero_si128();
  const __m128i max = _mm_set1_epi16(MAX_Y);
  const __m128i one = _mm_set1_epi16(1);
  __m128i sum = zero;

  for (i = 0; i + 8 <= len; i += 8) {
    const __m128i A = _mm_loadu_si128((const __m128i*)(ref + i));
    const __m128i B = _mm_loadu_si128((const __m128i*)(src + i));
    const __m128i C = _mm_loadu_si128((const __m128i*)(dst + i));
    const __m128i D = _mm_sub_epi16(A, B);       // diff_y
    const __m128i E = _mm_cmpgt_epi16(zero, D);  // sign (-1 or 0)
    const __m128i F = _mm_add_epi16(C, D);       // new_y
    const __m128i G = _mm_or_si128(E, one);      // -1 or 1
    const __m128i H = _mm_max_epi16(_mm_min_epi16(F, max), zero);
    const __m128i I = _mm_madd_epi16(D, G);      // sum(abs(...))
    _mm_storeu_si128((__m128i*)(dst + i), H);
    sum = _mm_add_epi32(sum, I);
  }
  _mm_storeu_si128((__m128i*)tmp, sum);
  diff = tmp[3] + tmp[2] + tmp[1] + tmp[0];
  for (; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)dst[i] + diff_y;
    dst[i] = clip_y(new_y);
    diff += (uint64_t)abs(diff_y);
  }
  return diff;
}

static void SharpYUVUpdateRGB_SSE2(const int16_t* ref, const int16_t* src,
                                   int16_t* dst, int len) {
  int i = 0;
  for (i = 0; i + 8 <= len; i += 8) {
    const __m128i A = _mm_loadu_si128((const __m128i*)(ref + i));
    const __m128i B = _mm_loadu_si128((const __m128i*)(src + i));
    const __m128i C = _mm_loadu_si128((const __m128i*)(dst + i));
    const __m128i D = _mm_sub_epi16(A, B);   // diff_uv
    const __m128i E = _mm_add_epi16(C, D);   // new_uv
    _mm_storeu_si128((__m128i*)(dst + i), E);
  }
  for (; i < len; ++i) {
    const int diff_uv = ref[i] - src[i];
    dst[i] += diff_uv;
  }
}

static void SharpYUVFilterRow_SSE2(const int16_t* A, const int16_t* B, int len,
                                   const uint16_t* best_y, uint16_t* out) {
  int i;
  const __m128i kCst8 = _mm_set1_epi16(8);
  const __m128i max = _mm_set1_epi16(MAX_Y);
  const __m128i zero = _mm_setzero_si128();
  for (i = 0; i + 8 <= len; i += 8) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)(A + i + 0));
    const __m128i a1 = _mm_loadu_si128((const __m128i*)(A + i + 1));
    const __m128i b0 = _mm_loadu_si128((const __m128i*)(B + i + 0));
    const __m128i b1 = _mm_loadu_si128((const __m128i*)(B + i + 1));
    const __m128i a0b1 = _mm_add_epi16(a0, b1);
    const __m128i a1b0 = _mm_add_epi16(a1, b0);
    const __m128i a0a1b0b1 = _mm_add_epi16(a0b1, a1b0);  // A0+A1+B0+B1
    const __m128i a0a1b0b1_8 = _mm_add_epi16(a0a1b0b1, kCst8);
    const __m128i a0b1_2 = _mm_add_epi16(a0b1, a0b1);    // 2*(A0+B1)
    const __m128i a1b0_2 = _mm_add_epi16(a1b0, a1b0);    // 2*(A1+B0)
    const __m128i c0 = _mm_srai_epi16(_mm_add_epi16(a0b1_2, a0a1b0b1_8), 3);
    const __m128i c1 = _mm_srai_epi16(_mm_add_epi16(a1b0_2, a0a1b0b1_8), 3);
    const __m128i d0 = _mm_add_epi16(c1, a0);
    const __m128i d1 = _mm_add_epi16(c0, a1);
    const __m128i e0 = _mm_srai_epi16(d0, 1);
    const __m128i e1 = _mm_srai_epi16(d1, 1);
    const __m128i f0 = _mm_unpacklo_epi16(e0, e1);
    const __m128i f1 = _mm_unpackhi_epi16(e0, e1);
    const __m128i g0 = _mm_loadu_si128((const __m128i*)(best_y + 2 * i + 0));
    const __m128i g1 = _mm_loadu_si128((const __m128i*)(best_y + 2 * i + 8));
    const __m128i h0 = _mm_add_epi16(g0, f0);
    const __m128i h1 = _mm_add_epi16(g1, f1);
    const __m128i i0 = _mm_max_epi16(_mm_min_epi16(h0, max), zero);
    const __m128i i1 = _mm_max_epi16(_mm_min_epi16(h1, max), zero);
    _mm_storeu_si128((__m128i*)(out + 2 * i + 0), i0);
    _mm_storeu_si128((__m128i*)(out + 2 * i + 8), i1);
  }
  for (; i < len; ++i) {
    //   (9 * A0 + 3 * A1 + 3 * B0 + B1 + 8) >> 4 =
    // = (8 * A0 + 2 * (A1 + B0) + (A0 + A1 + B0 + B1 + 8)) >> 4
    // We reuse the common sub-expressions.
    const int a0b1 = A[i + 0] + B[i + 1];
    const int a1b0 = A[i + 1] + B[i + 0];
    const int a0a1b0b1 = a0b1 + a1b0 + 8;
    const int v0 = (8 * A[i + 0] + 2 * a1b0 + a0a1b0b1) >> 4;
    const int v1 = (8 * A[i + 1] + 2 * a0b1 + a0a1b0b1) >> 4;
    out[2 * i + 0] = clip_y(best_y[2 * i + 0] + v0);
    out[2 * i + 1] = clip_y(best_y[2 * i + 1] + v1);
  }
}
#undef MAX_Y

//------------------------------------------------------------------------------

extern void InitSharpYUVSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void InitSharpYUVSSE2(void) {
  SharpYUVUpdateY = SharpYUVUpdateY_SSE2;
  SharpYUVUpdateRGB = SharpYUVUpdateRGB_SSE2;
  SharpYUVFilterRow = SharpYUVFilterRow_SSE2;
}
#else  // !WEBP_USE_SSE2

void InitSharpYUVSSE2(void) {}

#endif  // WEBP_USE_SSE2
