// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON variant of alpha filters
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_NEON)

#include <assert.h>
#include "./neon.h"

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

#define LOAD_U8_TO_S16(A) vreinterpretq_s16_u16(vmovl_u8(vld1_u8(A)))

#define SHIFT_64(A, CNT, INSTR) \
  vreinterpret_u8_u64(INSTR(vreinterpret_u64_u8((A)), CNT))

static void PredictLineTopNEON(const uint8_t* src, const uint8_t* pred,
                               uint8_t* dst, int length) {
  int i;
  assert(length >= 0);
  for (i = 0; i + 32 <= length; i += 32) {
    const uint8x16_t A0 = vld1q_u8(&src[i +  0]);
    const uint8x16_t A1 = vld1q_u8(&src[i + 16]);
    const uint8x16_t B0 = vld1q_u8(&pred[i +  0]);
    const uint8x16_t B1 = vld1q_u8(&pred[i + 16]);
    const uint8x16_t C0 = vsubq_u8(A0, B0);
    const uint8x16_t C1 = vsubq_u8(A1, B1);
    vst1q_u8(&dst[i +  0], C0);
    vst1q_u8(&dst[i + 16], C1);
  }
  for (; i < length; ++i) dst[i] = src[i] - pred[i];
}

// Special case for left-based prediction (when preds==dst-1 or preds==src-1).
static void PredictLineLeftNEON(const uint8_t* src, uint8_t* dst, int length) {
  int i;
  assert(length >= 0);
  for (i = 0; i + 32 <= length; i += 32) {
    const uint8x16_t A0 = vld1q_u8(&src[i     +  0]);
    const uint8x16_t B0 = vld1q_u8(&src[i     -  1]);
    const uint8x16_t A1 = vld1q_u8(&src[i + 16 + 0]);
    const uint8x16_t B1 = vld1q_u8(&src[i + 16 - 1]);
    const uint8x16_t C0 = vsubq_u8(A0, B0);
    const uint8x16_t C1 = vsubq_u8(A1, B1);
    vst1q_u8(&dst[i +  0], C0);
    vst1q_u8(&dst[i + 16], C1);
  }
  for (; i < length; ++i) dst[i] = src[i] - src[i - 1];
}

//------------------------------------------------------------------------------
// Horizontal filter.

static WEBP_INLINE void DoHorizontalFilterNEON(const uint8_t* in,
                                               int width, int height,
                                               int stride,
                                               int row, int num_rows,
                                               uint8_t* out) {
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;

  if (row == 0) {
    // Leftmost pixel is the same as input for topmost scanline.
    out[0] = in[0];
    PredictLineLeftNEON(in + 1, out + 1, width - 1);
    row = 1;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    // Leftmost pixel is predicted from above.
    out[0] = in[0] - in[-stride];
    PredictLineLeftNEON(in + 1, out + 1, width - 1);
    ++row;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter.

static WEBP_INLINE void DoVerticalFilterNEON(const uint8_t* in,
                                             int width, int height, int stride,
                                             int row, int num_rows,
                                             uint8_t* out) {
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;

  if (row == 0) {
    // Very first top-left pixel is copied.
    out[0] = in[0];
    // Rest of top scan-line is left-predicted.
    PredictLineLeftNEON(in + 1, out + 1, width - 1);
    row = 1;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    PredictLineTopNEON(in, in - stride, out, width);
    ++row;
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

static void GradientPredictDirectNEON(const uint8_t* const row,
                                      const uint8_t* const top,
                                      uint8_t* const out, int length) {
  int i;
  for (i = 0; i + 8 <= length; i += 8) {
    const uint8x8_t A = vld1_u8(&row[i - 1]);
    const uint8x8_t B = vld1_u8(&top[i + 0]);
    const int16x8_t C = vreinterpretq_s16_u16(vaddl_u8(A, B));
    const int16x8_t D = LOAD_U8_TO_S16(&top[i - 1]);
    const uint8x8_t E  = vqmovun_s16(vsubq_s16(C, D));
    const uint8x8_t F  = vld1_u8(&row[i + 0]);
    vst1_u8(&out[i], vsub_u8(F, E));
  }
  for (; i < length; ++i) {
    out[i] = row[i] - GradientPredictorC(row[i - 1], top[i], top[i - 1]);
  }
}

static WEBP_INLINE void DoGradientFilterNEON(const uint8_t* in,
                                             int width, int height,
                                             int stride,
                                             int row, int num_rows,
                                         uint8_t* out) {
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;

  // left prediction for top scan-line
  if (row == 0) {
    out[0] = in[0];
    PredictLineLeftNEON(in + 1, out + 1, width - 1);
    row = 1;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    out[0] = in[0] - in[-stride];
    GradientPredictDirectNEON(in + 1, in + 1 - stride, out + 1, width - 1);
    ++row;
    in += stride;
    out += stride;
  }
}

#undef SANITY_CHECK

//------------------------------------------------------------------------------

static void HorizontalFilterNEON(const uint8_t* data, int width, int height,
                                 int stride, uint8_t* filtered_data) {
  DoHorizontalFilterNEON(data, width, height, stride, 0, height, filtered_data);
}

static void VerticalFilterNEON(const uint8_t* data, int width, int height,
                               int stride, uint8_t* filtered_data) {
  DoVerticalFilterNEON(data, width, height, stride, 0, height, filtered_data);
}

static void GradientFilterNEON(const uint8_t* data, int width, int height,
                               int stride, uint8_t* filtered_data) {
  DoGradientFilterNEON(data, width, height, stride, 0, height, filtered_data);
}

//------------------------------------------------------------------------------
// Inverse transforms


static void HorizontalUnfilterNEON(const uint8_t* prev, const uint8_t* in,
                                   uint8_t* out, int width) {
  int i;
  uint8x8_t last;
  out[0] = in[0] + (prev == NULL ? 0 : prev[0]);
  if (width <= 1) return;
  last = vld1_lane_u8(&out[0], vdup_n_u8(0), 0);
  for (i = 1; i + 8 <= width; i += 8) {
    const uint8x8_t A0 = vld1_u8(&in[i]);
    const uint8x8_t A1 = vadd_u8(A0, last);
    const uint8x8_t A2 = SHIFT_64(A1,  8, vshl_n_u64);
    const uint8x8_t A3 = vadd_u8(A1, A2);
    const uint8x8_t A4 = SHIFT_64(A3, 16, vshl_n_u64);
    const uint8x8_t A5 = vadd_u8(A3, A4);
    const uint8x8_t A6 = SHIFT_64(A5, 32, vshl_n_u64);
    const uint8x8_t A7 = vadd_u8(A5, A6);
    vst1_u8(&out[i], A7);
    last = SHIFT_64(A7, 56, vshr_n_u64);
  }
  for (; i < width; ++i) out[i] = in[i] + out[i - 1];
}

static void VerticalUnfilterNEON(const uint8_t* prev, const uint8_t* in,
                                 uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilterNEON(NULL, in, out, width);
  } else {
    int i;
    assert(width >= 0);
    for (i = 0; i + 32 <= width; i += 32) {
      const uint8x16_t A0 = vld1q_u8(&in[i +  0]);
      const uint8x16_t A1 = vld1q_u8(&in[i + 16]);
      const uint8x16_t B0 = vld1q_u8(&prev[i +  0]);
      const uint8x16_t B1 = vld1q_u8(&prev[i + 16]);
      const uint8x16_t C0 = vaddq_u8(A0, B0);
      const uint8x16_t C1 = vaddq_u8(A1, B1);
      vst1q_u8(&out[i +  0], C0);
      vst1q_u8(&out[i + 16], C1);
    }
    for (; i < width; ++i) out[i] = in[i] + prev[i];
  }
}

static void GradientPredictInverseNEON(const uint8_t* const in,
                                       const uint8_t* const top,
                                       uint8_t* const row, int length) {
  if (length > 0) {
    int i;
    uint8x8_t A = vld1_lane_u8(&row[-1], vdup_n_u8(0), 0);   // left sample
    for (i = 0; i + 8 <= length; i += 8) {
      const int16x8_t B = LOAD_U8_TO_S16(&top[i + 0]);
      const int16x8_t C = LOAD_U8_TO_S16(&top[i - 1]);
      const uint8x8_t D = vld1_u8(&in[i]);   // base input
      const int16x8_t E = vsubq_s16(B, C);   // unclipped gradient basis B - C
      uint8x8_t out = vdup_n_u8(0);          // accumulator for output
      uint8x8_t mask_hi = vset_lane_u8(0xff, vdup_n_u8(0), 0);
      int k = 8;
      while (1) {
        // A + B - C
        const int16x8_t tmp3 = vaddq_s16(E, vreinterpretq_s16_u16(vmovl_u8(A)));
        const uint8x8_t tmp4 = vqmovun_s16(tmp3);        // saturate delta
        const uint8x8_t tmp5 = vadd_u8(tmp4, D);         // add to in[]
        A = vand_u8(tmp5, mask_hi);                      // 1-complement clip
        out = vorr_u8(out, A);                           // accumulate output
        if (--k == 0) break;
        A = SHIFT_64(A,  8, vshl_n_u64);                 // rotate left sample
        mask_hi = SHIFT_64(mask_hi, 8, vshl_n_u64);      // rotate mask
      }
      vst1_u8(&row[i], out);
      A = SHIFT_64(A, 56, vshr_n_u64);
    }
    for (; i < length; ++i) {
      row[i] = in[i] + GradientPredictorC(row[i - 1], top[i], top[i - 1]);
    }
  }
}

static void GradientUnfilterNEON(const uint8_t* prev, const uint8_t* in,
                                 uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilterNEON(NULL, in, out, width);
  } else {
    out[0] = in[0] + prev[0];  // predict from above
    GradientPredictInverseNEON(in + 1, prev + 1, out + 1, width - 1);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8FiltersInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitNEON(void) {
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilterNEON;
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilterNEON;
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilterNEON;

  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilterNEON;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilterNEON;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilterNEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8FiltersInitNEON)

#endif  // WEBP_USE_NEON
