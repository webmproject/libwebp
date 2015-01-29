// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 variant of alpha filters
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_SSE2)

#include <assert.h>
#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
// Helpful macro.

# define SANITY_CHECK(in, out)                                                 \
  assert(in != NULL);                                                          \
  assert(out != NULL);                                                         \
  assert(width > 0);                                                           \
  assert(height > 0);                                                          \
  assert(stride >= width);                                                     \
  assert(row >= 0 && num_rows > 0 && row + num_rows <= height);                \
  (void)height;  // Silence unused warning.

static void PredictLineTop(const uint8_t* src, const uint8_t* pred,
                           uint8_t* dst, int length, int inverse) {
  int i;
  const int max_pos = length & ~31;
  if (inverse) {
    for (i = 0; i < max_pos; i += 32) {
      const __m128i A0 = _mm_loadu_si128((const __m128i*)&src[i +  0]);
      const __m128i A1 = _mm_loadu_si128((const __m128i*)&src[i + 16]);
      const __m128i B0 = _mm_loadu_si128((const __m128i*)&pred[i +  0]);
      const __m128i B1 = _mm_loadu_si128((const __m128i*)&pred[i + 16]);
      const __m128i C0 = _mm_add_epi8(A0, B0);
      const __m128i C1 = _mm_add_epi8(A1, B1);
      _mm_storeu_si128((__m128i*)&dst[i +  0], C0);
      _mm_storeu_si128((__m128i*)&dst[i + 16], C1);
    }
    for (; i < length; ++i) dst[i] = src[i] + pred[i];
  } else {
    for (i = 0; i < max_pos; i += 32) {
      const __m128i A0 = _mm_loadu_si128((const __m128i*)&src[i +  0]);
      const __m128i A1 = _mm_loadu_si128((const __m128i*)&src[i + 16]);
      const __m128i B0 = _mm_loadu_si128((const __m128i*)&pred[i +  0]);
      const __m128i B1 = _mm_loadu_si128((const __m128i*)&pred[i + 16]);
      const __m128i C0 = _mm_sub_epi8(A0, B0);
      const __m128i C1 = _mm_sub_epi8(A1, B1);
      _mm_storeu_si128((__m128i*)&dst[i +  0], C0);
      _mm_storeu_si128((__m128i*)&dst[i + 16], C1);
    }
    for (; i < length; ++i) dst[i] = src[i] - pred[i];
  }
}

// Special case for left-based prediction (when preds==dst-1 or preds==src-1).
static void PredictLineLeft(const uint8_t* src, uint8_t* dst, int length,
                            int inverse) {
  int i;
  if (inverse) {
    const int max_pos = length & ~7;
    __m128i last =
        _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, dst[-1]);
    for (i = 0; i < max_pos; i += 8) {
      const __m128i A0 = _mm_loadl_epi64((const __m128i*)(src + i));
      const __m128i A1 = _mm_add_epi8(A0, last);
      const __m128i A2 = _mm_slli_si128(A1, 1);
      const __m128i A3 = _mm_add_epi8(A1, A2);
      const __m128i A4 = _mm_slli_si128(A3, 2);
      const __m128i A5 = _mm_add_epi8(A3, A4);
      const __m128i A6 = _mm_slli_si128(A5, 4);
      const __m128i A7 = _mm_add_epi8(A5, A6);
      _mm_storel_epi64((__m128i*)(dst + i), A7);
      last = _mm_srli_epi64(A7, 56);
    }
    for (; i < length; ++i) dst[i] = src[i] + dst[i - 1];
  } else {
    const int max_pos = length & ~31;
    for (i = 0; i < max_pos; i += 32) {
      const __m128i A0 = _mm_loadu_si128((const __m128i*)(src + i +  0    ));
      const __m128i B0 = _mm_loadu_si128((const __m128i*)(src + i +  0 - 1));
      const __m128i A1 = _mm_loadu_si128((const __m128i*)(src + i + 16    ));
      const __m128i B1 = _mm_loadu_si128((const __m128i*)(src + i + 16 - 1));
      const __m128i C0 = _mm_sub_epi8(A0, B0);
      const __m128i C1 = _mm_sub_epi8(A1, B1);
      _mm_storeu_si128((__m128i*)(dst + i +  0), C0);
      _mm_storeu_si128((__m128i*)(dst + i + 16), C1);
    }
    for (; i < length; ++i) dst[i] = src[i] - src[i - 1];
  }
}

static void PredictLineC(const uint8_t* src, const uint8_t* pred,
                         uint8_t* dst, int length, int inverse) {
  int i;
  if (inverse) {
    for (i = 0; i < length; ++i) dst[i] = src[i] + pred[i];
  } else {
    for (i = 0; i < length; ++i) dst[i] = src[i] - pred[i];
  }
}

//------------------------------------------------------------------------------
// Horizontal filter.

static WEBP_INLINE void DoHorizontalFilter(const uint8_t* in,
                                           int width, int height, int stride,
                                           int row, int num_rows,
                                           int inverse, uint8_t* out) {
  const uint8_t* preds;
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;
  preds = inverse ? out : in;

  if (row == 0) {
    // Leftmost pixel is the same as input for topmost scanline.
    out[0] = in[0];
    PredictLineLeft(in + 1, out + 1, width - 1, inverse);
    row = 1;
    preds += stride;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    // Leftmost pixel is predicted from above.
    PredictLineC(in, preds - stride, out, 1, inverse);
    PredictLineLeft(in + 1, out + 1, width - 1, inverse);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter.

static WEBP_INLINE void DoVerticalFilter(const uint8_t* in,
                                         int width, int height, int stride,
                                         int row, int num_rows,
                                         int inverse, uint8_t* out) {
  const uint8_t* preds;
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;
  preds = inverse ? out : in;

  if (row == 0) {
    // Very first top-left pixel is copied.
    out[0] = in[0];
    // Rest of top scan-line is left-predicted.
    PredictLineLeft(in + 1, out + 1, width - 1, inverse);
    row = 1;
    in += stride;
    out += stride;
  } else {
    // We are starting from in-between. Make sure 'preds' points to prev row.
    preds -= stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    PredictLineTop(in, preds, out, width, inverse);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Gradient filter.

static WEBP_INLINE int GradientPredictorC(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;  // clip to 8bit
}

static void GradientPredictDirect(const uint8_t* const row,
                                  const uint8_t* const top,
                                  uint8_t* const out, int length) {
  const int max_pos = length & ~7;
  int i;
  const __m128i zero = _mm_setzero_si128();
  for (i = 0; i < max_pos; i += 8) {
    const __m128i A0 = _mm_loadl_epi64((const __m128i*)&row[i - 1]);
    const __m128i B0 = _mm_loadl_epi64((const __m128i*)&top[i]);
    const __m128i C0 = _mm_loadl_epi64((const __m128i*)&top[i - 1]);
    const __m128i D = _mm_loadl_epi64((const __m128i*)&row[i]);
    const __m128i A1 = _mm_unpacklo_epi8(A0, zero);
    const __m128i B1 = _mm_unpacklo_epi8(B0, zero);
    const __m128i C1 = _mm_unpacklo_epi8(C0, zero);
    const __m128i E = _mm_add_epi16(A1, B1);
    const __m128i F = _mm_sub_epi16(E, C1);
    const __m128i G = _mm_packus_epi16(F, zero);
    const __m128i H = _mm_sub_epi8(D, G);
    _mm_storel_epi64((__m128i*)(out + i), H);
  }
  for (; i < length; ++i) {
    out[i] = row[i] - GradientPredictorC(row[i - 1], top[i], top[i - 1]);
  }
}

static WEBP_INLINE void DoGradientFilter(const uint8_t* in,
                                         int width, int height, int stride,
                                         int row, int num_rows,
                                         int inverse, uint8_t* out) {
  const uint8_t* preds;
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;
  preds = inverse ? out : in;

  // left prediction for top scan-line
  if (row == 0) {
    out[0] = in[0];
    PredictLineLeft(in + 1, out + 1, width - 1, inverse);
    row = 1;
    preds += stride;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    int w;
    // leftmost pixel: predict from above.
    PredictLineC(in, preds - stride, out, 1, inverse);
    if (inverse) {
      for (w = 1; w < width; ++w) {
        const int pred = GradientPredictorC(out[w - 1],
                                            out[w - stride],
                                            out[w - stride - 1]);
        out[w] = in[w] + pred;
      }
    } else {
      GradientPredictDirect(in + 1, in + 1 - stride, out + 1, width - 1);
    }
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

#undef SANITY_CHECK

//------------------------------------------------------------------------------

static void HorizontalFilter(const uint8_t* data, int width, int height,
                             int stride, uint8_t* filtered_data) {
  DoHorizontalFilter(data, width, height, stride, 0, height, 0, filtered_data);
}

static void VerticalFilter(const uint8_t* data, int width, int height,
                           int stride, uint8_t* filtered_data) {
  DoVerticalFilter(data, width, height, stride, 0, height, 0, filtered_data);
}


static void GradientFilter(const uint8_t* data, int width, int height,
                           int stride, uint8_t* filtered_data) {
  DoGradientFilter(data, width, height, stride, 0, height, 0, filtered_data);
}


//------------------------------------------------------------------------------

static void VerticalUnfilter(int width, int height, int stride, int row,
                             int num_rows, uint8_t* data) {
  DoVerticalFilter(data, width, height, stride, row, num_rows, 1, data);
}

static void HorizontalUnfilter(int width, int height, int stride, int row,
                               int num_rows, uint8_t* data) {
  DoHorizontalFilter(data, width, height, stride, row, num_rows, 1, data);
}

static void GradientUnfilter(int width, int height, int stride, int row,
                             int num_rows, uint8_t* data) {
  DoGradientFilter(data, width, height, stride, row, num_rows, 1, data);
}

//------------------------------------------------------------------------------

#endif    // WEBP_USE_SSE2

extern void VP8FiltersInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitSSE2(void) {
#if defined(WEBP_USE_SSE2)
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter;
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter;
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter;

  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter;
#endif
}
