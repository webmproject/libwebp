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
// Helpful macros.

# define SANITY_CHECK(in, out)                                                 \
  assert(in != NULL);                                                          \
  assert(out != NULL);                                                         \
  assert(width > 0);                                                           \
  assert(height > 0);                                                          \
  assert(stride >= width);                                                     \
  assert(row >= 0 && num_rows > 0 && row + num_rows <= height);                \
  (void)height;  // Silence unused warning.

// load eight u8 and widen to s16
#define LOAD_U8_TO_S16(A) vreinterpretq_s16_u16(vmovl_u8(vld1_u8(A)))

// shift u64 left or right by CNT amount
#define SHIFT_64(A, CNT, INSTR) \
  vreinterpret_u8_u64(INSTR(vreinterpret_u64_u8((A)), CNT))

// insert the low byte of X at the lower position of A, shifting the rest up.
//  A = abcdefgh, X = Xxxxxxxx, result = Xabcdefg
#define INSERT_LO_8(A, X)    vext_u8(vrev64_u8(X), (A), 7)
// insert the hi byte of X at the lower position of A, shifting the rest up.
//  A = abcdefgh, X = xxxxxxxX, result = Xabcdefg
#define INSERT_HI_8(A, X)    vext_u8((X), (A), 7)

// rotate left by N bytes
#define ROTATE_LEFT_N(A, N)   vext_u8((A), (A), (N))
// rotate right by N bytes
#define ROTATE_RIGHT_N(A, N)   vext_u8((A), (A), (8 - (N)) % 8)

static void PredictLineTop_NEON(const uint8_t* src, const uint8_t* pred,
                                uint8_t* dst, int length) {
  int i;
  assert(length >= 0);
  for (i = 0; i + 16 <= length; i += 16) {
    const uint8x16_t A = vld1q_u8(&src[i +  0]);
    const uint8x16_t B = vld1q_u8(&pred[i +  0]);
    const uint8x16_t C = vsubq_u8(A, B);
    vst1q_u8(&dst[i], C);
  }
  for (; i < length; ++i) dst[i] = src[i] - pred[i];
}

// Special case for left-based prediction (when preds==dst-1 or preds==src-1).
static void PredictLineLeft_NEON(const uint8_t* src, uint8_t* dst, int length) {
  int i;
  assert(length >= 0);
  for (i = 0; i + 16 <= length; i += 16) {
    const uint8x16_t A = vld1q_u8(&src[i     +  0]);
    const uint8x16_t B = vld1q_u8(&src[i     -  1]);
    const uint8x16_t C = vsubq_u8(A, B);
    vst1q_u8(&dst[i], C);
  }
  for (; i < length; ++i) dst[i] = src[i] - src[i - 1];
}

//------------------------------------------------------------------------------
// Horizontal filter.

static WEBP_INLINE void DoHorizontalFilter_NEON(const uint8_t* in,
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
    PredictLineLeft_NEON(in + 1, out + 1, width - 1);
    row = 1;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    // Leftmost pixel is predicted from above.
    out[0] = in[0] - in[-stride];
    PredictLineLeft_NEON(in + 1, out + 1, width - 1);
    ++row;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter.

static WEBP_INLINE void DoVerticalFilter_NEON(const uint8_t* in,
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
    PredictLineLeft_NEON(in + 1, out + 1, width - 1);
    row = 1;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    PredictLineTop_NEON(in, in - stride, out, width);
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

static void GradientPredictDirect_NEON(const uint8_t* const row,
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

static WEBP_INLINE void DoGradientFilter_NEON(const uint8_t* in,
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
    PredictLineLeft_NEON(in + 1, out + 1, width - 1);
    row = 1;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    out[0] = in[0] - in[-stride];
    GradientPredictDirect_NEON(in + 1, in + 1 - stride, out + 1, width - 1);
    ++row;
    in += stride;
    out += stride;
  }
}

#undef SANITY_CHECK

//------------------------------------------------------------------------------

static void HorizontalFilter_NEON(const uint8_t* data, int width, int height,
                                  int stride, uint8_t* filtered_data) {
  DoHorizontalFilter_NEON(data, width, height, stride, 0, height,
                          filtered_data);
}

static void VerticalFilter_NEON(const uint8_t* data, int width, int height,
                               int stride, uint8_t* filtered_data) {
  DoVerticalFilter_NEON(data, width, height, stride, 0, height,
                        filtered_data);
}

static void GradientFilter_NEON(const uint8_t* data, int width, int height,
                               int stride, uint8_t* filtered_data) {
  DoGradientFilter_NEON(data, width, height, stride, 0, height,
                        filtered_data);
}

//------------------------------------------------------------------------------
// Inverse transforms


static void HorizontalUnfilter_NEON(const uint8_t* prev, const uint8_t* in,
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

static void VerticalUnfilter_NEON(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_NEON(NULL, in, out, width);
  } else {
    int i;
    assert(width >= 0);
    for (i = 0; i + 16 <= width; i += 16) {
      const uint8x16_t A = vld1q_u8(&in[i]);
      const uint8x16_t B = vld1q_u8(&prev[i]);
      const uint8x16_t C = vaddq_u8(A, B);
      vst1q_u8(&out[i], C);
    }
    for (; i < width; ++i) out[i] = in[i] + prev[i];
  }
}

#define GRAD_PROCESS_LANE(L)  do {  \
  const uint8x8_t tmp1 = ROTATE_RIGHT_N(pred, 1);  /* rotate predictor in */   \
  const int16x8_t tmp2 = vaddq_s16(BC, vreinterpretq_s16_u16(vmovl_u8(tmp1))); \
  const uint8x8_t delta = vqmovun_s16(tmp2);                                   \
  pred = vadd_u8(D, delta);                                                    \
  out = INSERT_HI_8(out, ROTATE_LEFT_N(pred, ((L) + 1) % 8));                  \
} while (0)

static void GradientPredictInverse_NEON(const uint8_t* const in,
                                        const uint8_t* const top,
                                        uint8_t* const row, int length) {
  if (length > 0) {
    int i;
    uint8x8_t pred = vdup_n_u8(row[-1]);   // left sample
    uint8x8_t out = vdup_n_u8(0);
    for (i = 0; i + 8 <= length; i += 8) {
      const int16x8_t B = LOAD_U8_TO_S16(&top[i + 0]);
      const int16x8_t C = LOAD_U8_TO_S16(&top[i - 1]);
      const int16x8_t BC = vsubq_s16(B, C);  // unclipped gradient basis B - C
      const uint8x8_t D = vld1_u8(&in[i]);   // base input
      GRAD_PROCESS_LANE(0);
      GRAD_PROCESS_LANE(1);
      GRAD_PROCESS_LANE(2);
      GRAD_PROCESS_LANE(3);
      GRAD_PROCESS_LANE(4);
      GRAD_PROCESS_LANE(5);
      GRAD_PROCESS_LANE(6);
      GRAD_PROCESS_LANE(7);
      vst1_u8(&row[i], vrev64_u8(out));
    }
    for (; i < length; ++i) {
      row[i] = in[i] + GradientPredictorC(row[i - 1], top[i], top[i - 1]);
    }
  }
}

static void GradientUnfilter_NEON(const uint8_t* prev, const uint8_t* in,
                                 uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_NEON(NULL, in, out, width);
  } else {
    out[0] = in[0] + prev[0];  // predict from above
    GradientPredictInverse_NEON(in + 1, prev + 1, out + 1, width - 1);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8FiltersInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitNEON(void) {
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter_NEON;
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter_NEON;
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter_NEON;

  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_NEON;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_NEON;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8FiltersInitNEON)

#endif  // WEBP_USE_NEON
