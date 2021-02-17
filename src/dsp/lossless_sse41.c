// Copyright 2021 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE41 variant of methods for lossless decoder

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE41)

#include "src/dsp/common_sse41.h"
#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"
#include <assert.h>
#include <emmintrin.h>

//------------------------------------------------------------------------------
// Color-space conversion functions

#define ARGB_TO_RGB_SSE41 do {                        \
  while (num_pixels >= 16) {                          \
    const __m128i in0 = _mm_loadu_si128(in + 0);      \
    const __m128i in1 = _mm_loadu_si128(in + 1);      \
    const __m128i in2 = _mm_loadu_si128(in + 2);      \
    const __m128i in3 = _mm_loadu_si128(in + 3);      \
    const __m128i a0 = _mm_shuffle_epi8(in0, perm0);  \
    const __m128i a1 = _mm_shuffle_epi8(in1, perm1);  \
    const __m128i a2 = _mm_shuffle_epi8(in2, perm2);  \
    const __m128i a3 = _mm_shuffle_epi8(in3, perm3);  \
    const __m128i b0 = _mm_blend_epi16(a0, a1, 0xc0); \
    const __m128i b1 = _mm_blend_epi16(a1, a2, 0xf0); \
    const __m128i b2 = _mm_blend_epi16(a2, a3, 0xfc); \
    _mm_storeu_si128(out + 0, b0);                    \
    _mm_storeu_si128(out + 1, b1);                    \
    _mm_storeu_si128(out + 2, b2);                    \
    in += 4;                                          \
    out += 3;                                         \
    num_pixels -= 16;                                 \
  }                                                   \
} while (0)

static void ConvertBGRAToRGB_SSE41(const uint32_t* src, int num_pixels,
                                  uint8_t* dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm0 = _mm_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9,
                                      8, 14, 13, 12, -1, -1, -1, -1);
  const __m128i perm1 = _mm_shuffle_epi32(perm0, 0x39);
  const __m128i perm2 = _mm_shuffle_epi32(perm0, 0x4e);
  const __m128i perm3 = _mm_shuffle_epi32(perm0, 0x93);

  ARGB_TO_RGB_SSE41;

  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToBGR_SSE41(const uint32_t* src,
                                  int num_pixels, uint8_t* dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm0 = _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10,
                                      12, 13, 14, -1, -1, -1, -1);
  const __m128i perm1 = _mm_shuffle_epi32(perm0, 0x39);
  const __m128i perm2 = _mm_shuffle_epi32(perm0, 0x4e);
  const __m128i perm3 = _mm_shuffle_epi32(perm0, 0x93);

  ARGB_TO_RGB_SSE41();

  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToBGR_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

#undef  // ARGB_TO_RGB_SSE41

//------------------------------------------------------------------------------

static void ConvertBGRAToRGBA_SSE41(const uint32_t* src,
                                   int num_pixels, uint8_t* dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7,
                                     10, 9, 8, 11, 14, 13, 12, 15);

  while (num_pixels >= 16) {
    const __m128i in0 = _mm_loadu_si128(in + 0);
    const __m128i in1 = _mm_loadu_si128(in + 1);
    const __m128i in2 = _mm_loadu_si128(in + 2);
    const __m128i in3 = _mm_loadu_si128(in + 3);
    const __m128i a0 = _mm_shuffle_epi8(in0, perm);
    const __m128i a1 = _mm_shuffle_epi8(in1, perm);
    const __m128i a2 = _mm_shuffle_epi8(in2, perm);
    const __m128i a3 = _mm_shuffle_epi8(in3, perm);
    _mm_storeu_si128(out + 0, a0);
    _mm_storeu_si128(out + 1, a1);
    _mm_storeu_si128(out + 2, a2);
    _mm_storeu_si128(out + 3, a3);
    in += 4;
    out += 4;
    num_pixels -= 16;
  }
  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGBA_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LDspInitSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitSSE41(void) {
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB_SSE41;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR_SSE41;
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA_SSE41;
}

#else  // !WEBP_USE_SSE41

WEBP_DSP_INIT_STUB(VP8LDspInitSSE41)

#endif  // WEBP_USE_SSE41
