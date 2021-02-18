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

static void ConvertBGRAToRGBA_SSE41(const uint32_t* src,
                                    int num_pixels, uint8_t* dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7,
                                     10, 9, 8, 11, 14, 13, 12, 15);

  while (num_pixels >= 8) {
    const __m128i in0 = _mm_loadu_si128(in + 0);
    const __m128i in1 = _mm_loadu_si128(in + 1);
    const __m128i a0 = _mm_shuffle_epi8(in0, perm);
    const __m128i a1 = _mm_shuffle_epi8(in1, perm);
    _mm_storeu_si128(out + 0, a0);
    _mm_storeu_si128(out + 1, a1);
    in += 2;
    out += 2;
    num_pixels -= 8;
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
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA_SSE41;
}

#else  // !WEBP_USE_SSE41

WEBP_DSP_INIT_STUB(VP8LDspInitSSE41)

#endif  // WEBP_USE_SSE41
