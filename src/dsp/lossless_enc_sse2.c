// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 variant of methods for lossless encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_SSE2)
#include <assert.h>
#include <emmintrin.h>
#include "./lossless.h"

//------------------------------------------------------------------------------
// Subtract-Green Transform

static void SubtractGreenFromBlueAndRed(uint32_t* argb_data, int num_pixels) {
  const __m128i mask = _mm_set1_epi32(0x0000ff00);
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((__m128i*)&argb_data[i]);
    const __m128i in_00g0 = _mm_and_si128(in, mask);     // 00g0|00g0|...
    const __m128i in_0g00 = _mm_slli_epi32(in_00g0, 8);  // 0g00|0g00|...
    const __m128i in_000g = _mm_srli_epi32(in_00g0, 8);  // 000g|000g|...
    const __m128i in_0g0g = _mm_or_si128(in_0g00, in_000g);
    const __m128i out = _mm_sub_epi8(in, in_0g0g);
    _mm_storeu_si128((__m128i*)&argb_data[i], out);
  }
  // fallthrough and finish off with plain-C
  VP8LSubtractGreenFromBlueAndRed_C(argb_data + i, num_pixels - i);
}

//------------------------------------------------------------------------------
// Color Transform

static WEBP_INLINE __m128i ColorTransformDelta(__m128i color_pred,
                                               __m128i color) {
  // We simulate signed 8-bit multiplication as:
  // * Left shift the two (8-bit) numbers by 8 bits,
  // * Perform a 16-bit signed multiplication and retain the higher 16-bits.
  const __m128i color_pred_shifted = _mm_slli_epi32(color_pred, 8);
  const __m128i color_shifted = _mm_slli_epi32(color, 8);
  // Note: This performs multiplication on 8 packed 16-bit numbers, 4 of which
  // happen to be zeroes.
  const __m128i signed_mult =
      _mm_mulhi_epi16(color_pred_shifted, color_shifted);
  return _mm_srli_epi32(signed_mult, 5);
}

static WEBP_INLINE void TransformColor(const VP8LMultipliers* const m,
                                       uint32_t* argb_data,
                                       int num_pixels) {
  const __m128i g_to_r = _mm_set1_epi32(m->green_to_red_);       // multipliers
  const __m128i g_to_b = _mm_set1_epi32(m->green_to_blue_);
  const __m128i r_to_b = _mm_set1_epi32(m->red_to_blue_);

  int i;

  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((__m128i*)&argb_data[i]);
    const __m128i alpha_green_mask = _mm_set1_epi32(0xff00ff00);  // masks
    const __m128i red_mask = _mm_set1_epi32(0x00ff0000);
    const __m128i green_mask = _mm_set1_epi32(0x0000ff00);
    const __m128i lower_8bit_mask  = _mm_set1_epi32(0x000000ff);
    const __m128i ag = _mm_and_si128(in, alpha_green_mask);      // alpha, green
    const __m128i r = _mm_srli_epi32(_mm_and_si128(in, red_mask), 16);
    const __m128i g = _mm_srli_epi32(_mm_and_si128(in, green_mask), 8);
    const __m128i b = in;

    const __m128i r_delta = ColorTransformDelta(g_to_r, g);      // red
    const __m128i r_new =
        _mm_and_si128(_mm_sub_epi32(r, r_delta), lower_8bit_mask);
    const __m128i r_new_shifted = _mm_slli_epi32(r_new, 16);

    const __m128i b_delta_1 = ColorTransformDelta(g_to_b, g);    // blue
    const __m128i b_delta_2 = ColorTransformDelta(r_to_b, r);
    const __m128i b_delta = _mm_add_epi32(b_delta_1, b_delta_2);
    const __m128i b_new =
        _mm_and_si128(_mm_sub_epi32(b, b_delta), lower_8bit_mask);

    const __m128i out = _mm_or_si128(_mm_or_si128(ag, r_new_shifted), b_new);
    _mm_storeu_si128((__m128i*)&argb_data[i], out);
  }

  // Fall-back to C-version for left-overs.
  VP8LTransformColor_C(m, argb_data + i, num_pixels - i);
}

//------------------------------------------------------------------------------

#define LINE_SIZE 16    // 8 or 16
static void AddVector(const uint32_t* a, const uint32_t* b, uint32_t* out,
                      int size) {
  int i;
  assert(size % LINE_SIZE == 0);
  for (i = 0; i < size; i += LINE_SIZE) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i +  0]);
    const __m128i a1 = _mm_loadu_si128((const __m128i*)&a[i +  4]);
#if (LINE_SIZE == 16)
    const __m128i a2 = _mm_loadu_si128((const __m128i*)&a[i +  8]);
    const __m128i a3 = _mm_loadu_si128((const __m128i*)&a[i + 12]);
#endif
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&b[i +  0]);
    const __m128i b1 = _mm_loadu_si128((const __m128i*)&b[i +  4]);
#if (LINE_SIZE == 16)
    const __m128i b2 = _mm_loadu_si128((const __m128i*)&b[i +  8]);
    const __m128i b3 = _mm_loadu_si128((const __m128i*)&b[i + 12]);
#endif
    _mm_storeu_si128((__m128i*)&out[i +  0], _mm_add_epi32(a0, b0));
    _mm_storeu_si128((__m128i*)&out[i +  4], _mm_add_epi32(a1, b1));
#if (LINE_SIZE == 16)
    _mm_storeu_si128((__m128i*)&out[i +  8], _mm_add_epi32(a2, b2));
    _mm_storeu_si128((__m128i*)&out[i + 12], _mm_add_epi32(a3, b3));
#endif
  }
}

static void AddVectorEq(const uint32_t* a, uint32_t* out, int size) {
  int i;
  assert(size % LINE_SIZE == 0);
  for (i = 0; i < size; i += LINE_SIZE) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i +  0]);
    const __m128i a1 = _mm_loadu_si128((const __m128i*)&a[i +  4]);
#if (LINE_SIZE == 16)
    const __m128i a2 = _mm_loadu_si128((const __m128i*)&a[i +  8]);
    const __m128i a3 = _mm_loadu_si128((const __m128i*)&a[i + 12]);
#endif
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&out[i +  0]);
    const __m128i b1 = _mm_loadu_si128((const __m128i*)&out[i +  4]);
#if (LINE_SIZE == 16)
    const __m128i b2 = _mm_loadu_si128((const __m128i*)&out[i +  8]);
    const __m128i b3 = _mm_loadu_si128((const __m128i*)&out[i + 12]);
#endif
    _mm_storeu_si128((__m128i*)&out[i +  0], _mm_add_epi32(a0, b0));
    _mm_storeu_si128((__m128i*)&out[i +  4], _mm_add_epi32(a1, b1));
#if (LINE_SIZE == 16)
    _mm_storeu_si128((__m128i*)&out[i +  8], _mm_add_epi32(a2, b2));
    _mm_storeu_si128((__m128i*)&out[i + 12], _mm_add_epi32(a3, b3));
#endif
  }
}
#undef LINE_SIZE

// Note we are adding uint32_t's as *signed* int32's (using _mm_add_epi32). But
// that's ok since the histogram values are less than 1<<28 (max picture size).
static void HistogramAdd(const VP8LHistogram* const a,
                         const VP8LHistogram* const b,
                         VP8LHistogram* const out) {
  int i;
  const int literal_size = VP8LHistogramNumCodes(a->palette_code_bits_);
  assert(a->palette_code_bits_ == b->palette_code_bits_);
  if (b != out) {
    AddVector(a->literal_, b->literal_, out->literal_, NUM_LITERAL_CODES);
    AddVector(a->red_, b->red_, out->red_, NUM_LITERAL_CODES);
    AddVector(a->blue_, b->blue_, out->blue_, NUM_LITERAL_CODES);
    AddVector(a->alpha_, b->alpha_, out->alpha_, NUM_LITERAL_CODES);
  } else {
    AddVectorEq(a->literal_, out->literal_, NUM_LITERAL_CODES);
    AddVectorEq(a->red_, out->red_, NUM_LITERAL_CODES);
    AddVectorEq(a->blue_, out->blue_, NUM_LITERAL_CODES);
    AddVectorEq(a->alpha_, out->alpha_, NUM_LITERAL_CODES);
  }
  for (i = NUM_LITERAL_CODES; i < literal_size; ++i) {
    out->literal_[i] = a->literal_[i] + b->literal_[i];
  }
  for (i = 0; i < NUM_DISTANCE_CODES; ++i) {
    out->distance_[i] = a->distance_[i] + b->distance_[i];
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitSSE2(void) {
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed;
  VP8LTransformColor = TransformColor;
  VP8LHistogramAdd = HistogramAdd;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(VP8LEncDspInitSSE2)

#endif  // WEBP_USE_SSE2
