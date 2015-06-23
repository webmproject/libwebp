// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE4.1 variant of methods for lossless encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_SSE41)
#include <assert.h>
#include <smmintrin.h>
#include "./lossless.h"

//------------------------------------------------------------------------------
// Subtract-Green Transform

static void SubtractGreenFromBlueAndRed(uint32_t* argb_data, int num_pixels) {
  int i;
  const __m128i kCstShuffle = _mm_set_epi8(-1, 13, -1, 13, -1, 9, -1, 9,
                                           -1,  5, -1,  5, -1, 1, -1, 1);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((__m128i*)&argb_data[i]);
    const __m128i in_0g0g = _mm_shuffle_epi8(in, kCstShuffle);
    const __m128i out = _mm_sub_epi8(in, in_0g0g);
    _mm_storeu_si128((__m128i*)&argb_data[i], out);
  }
  // fallthrough and finish off with plain-C
  VP8LSubtractGreenFromBlueAndRed_C(argb_data + i, num_pixels - i);
}

//------------------------------------------------------------------------------
// Color Transform

static WEBP_INLINE void TransformColor(const VP8LMultipliers* const m,
                                       uint32_t* argb_data, int num_pixels) {
  // Shuffle constant to spread green and red to some *upper* byte locations.
  const __m128i kCst_g0rg = _mm_set_epi8(5, -1,  -1, -1, 6, -1, 5, -1,
                                         1, -1,  -1, -1, 2, -1, 1, -1);
  // Shuffling constant to collect deltas from uint32 to uint8 locations.
  const __m128i kCstShuffle = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1,
                                           -1, 12, -1,  8, -1,  4, -1,  0);
  // Used to collect the two parts of the delta (horizontal add) with madd.
  const __m128i kCstAdd = _mm_set1_epi16(1);
  // sign-extended multiplying constants, pre-shifted by 5.
#define CST(X)  (((int16_t)(m->X << 8)) >> 5)   // sign-extend
  const __m128i mults = _mm_set_epi16(
      CST(green_to_red_), 0, CST(red_to_blue_), CST(green_to_blue_),
      CST(green_to_red_), 0, CST(red_to_blue_), CST(green_to_blue_));
#undef CST

  int i;
  for (i = 0; i + 2 <= num_pixels; i += 2) {
    const __m128i in = _mm_loadl_epi64((__m128i*)&argb_data[i]); // argb
    const __m128i A = _mm_shuffle_epi8(in, kCst_g0rg);     // g | 0 | r | g
    const __m128i B = _mm_mulhi_epi16(A, mults);           // dr | 0 | db1 | db2
    const __m128i C = _mm_madd_epi16(B, kCstAdd);          // dr | 0 | db | 0
    const __m128i D = _mm_shuffle_epi8(C, kCstShuffle);    // 0 | dr | 0 | db
    const __m128i out = _mm_sub_epi8(in, D);
    _mm_storel_epi64((__m128i*)&argb_data[i], out);
  }
  // fallthrough and finish off with plain-C
  VP8LTransformColor_C(m, argb_data + i, num_pixels - i);
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitSSE41(void) {
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed;
  VP8LTransformColor = TransformColor;
}

#else  // !WEBP_USE_SSE41

WEBP_DSP_INIT_STUB(VP8LEncDspInitSSE41)

#endif  // WEBP_USE_SSE41
