// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Utilities for processing transparent channel.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_SSE2)
#include <emmintrin.h>

//------------------------------------------------------------------------------

static int DispatchAlpha(const uint8_t* alpha, int alpha_stride,
                         int width, int height,
                         uint8_t* dst, int dst_stride) {
  // alpha_and stores an 'and' operation of all the alpha[] values. The final
  // value is not 0xff if any of the alpha[] is not equal to 0xff.
  uint32_t alpha_and = 0xff;
  int i, j;
  const __m128i zero = _mm_setzero_si128();
  const __m128i rgb_mask = _mm_set1_epi32(0xffffff00u);  // to preserve RGB
  const __m128i all_0xff = _mm_set_epi32(~0u, ~0u, 0, 0);
  __m128i all_alphas = all_0xff;

  // We must be able to access 3 extra bytes after the last written byte
  // 'dst[4 * width - 4]', because we don't know if alpha is the first or the
  // last byte of the quadruplet.
  const int limit = (width - 1) >> 3;

  for (j = 0; j < height; ++j) {
    const uint8_t* in = alpha;
    __m128i* out = (__m128i*)dst;
    for (i = 0; i < limit; ++i) {
      // load 8 alpha bytes
      const __m128i a0 = _mm_loadl_epi64((__m128i*)in);   // zeroes upper bytes
      const __m128i a1 = _mm_unpacklo_epi8(a0, zero);
      const __m128i a2_lo = _mm_unpacklo_epi16(a1, zero);
      const __m128i a2_hi = _mm_unpackhi_epi16(a1, zero);
      // load 8 dst pixels (32 bytes)
      const __m128i b0_lo = _mm_loadu_si128(out + 0);
      const __m128i b0_hi = _mm_loadu_si128(out + 1);
      // mask dst alpha values
      const __m128i b1_lo = _mm_and_si128(b0_lo, rgb_mask);
      const __m128i b1_hi = _mm_and_si128(b0_hi, rgb_mask);
      // combine
      const __m128i b2_lo = _mm_or_si128(b1_lo, a2_lo);
      const __m128i b2_hi = _mm_or_si128(b1_hi, a2_hi);
      // store
      _mm_storeu_si128(out + 0, b2_lo);
      _mm_storeu_si128(out + 1, b2_hi);
      // accumulate eight alpha 'and' in parallel
      all_alphas = _mm_and_si128(all_alphas, a0);
      out += 2;
      in += 8;
    }
    for (; i < width; ++i) {
      const uint32_t alpha_value = alpha[i];
      dst[4 * i] = alpha_value;
      alpha_and &= alpha_value;
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }
  // Combine the eight alpha 'and' into a 8-bit mask.
  alpha_and &= _mm_movemask_epi8(_mm_cmpeq_epi8(all_alphas, all_0xff));
  return (alpha_and != 0xff);
}

#endif   // WEBP_USE_SSE2

//------------------------------------------------------------------------------
// Init function

extern void WebPInitAlphaProcessingSSE2(void);

void WebPInitAlphaProcessingSSE2(void) {
#if defined(WEBP_USE_SSE2)
  WebPDispatchAlpha = DispatchAlpha;
#endif
}
