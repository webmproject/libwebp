// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA variant of alpha filters
//
// Author: Prashant Patil (prashant.patil@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MSA)

#include "./msa_macro.h"

#include <assert.h>

static WEBP_INLINE void PredictLineInverse0(const uint8_t* src,
                                            const uint8_t* pred,
                                            uint8_t* dst, int length) {
  int i;
  v16u8 src0, pred0, dst0;
  while (length >= 32) {
    v16u8 src1, pred1, dst1;
    LD_UB2(src, 16, src0, src1);
    LD_UB2(pred, 16, pred0, pred1);
    SUB2(src0, pred0, src1, pred1, dst0, dst1);
    ST_UB2(dst0, dst1, dst, 16);
    src += 32;
    pred += 32;
    dst += 32;
    length -= 32;
  }
  if (length > 0) {
    if (length >= 16) {
      src0 = LD_UB(src);
      pred0 = LD_UB(pred);
      dst0 = src0 - pred0;
      ST_UB(dst0, dst);
      src += 16;
      pred += 16;
      dst += 16;
      length -= 16;
    }
    for (i = 0; i < length; i++) {
      dst[i] = src[i] - pred[i];
    }
  }
}

static WEBP_INLINE void PredictLineInverse1(const uint8_t* src,
                                            const uint8_t* pred,
                                            uint8_t* dst, int length) {
  int i;
  v16u8 src0, pred0, dst0;
  while (length >= 32) {
    v16u8 src1, pred1, dst1;
    LD_UB2(src, 16, src0, src1);
    LD_UB2(pred, 16, pred0, pred1);
    ADD2(src0, pred0, src1, pred1, dst0, dst1);
    ST_UB2(dst0, dst1, dst, 16);
    src += 32;
    pred += 32;
    dst += 32;
    length -= 32;
  }
  if (length > 0) {
    if (length >= 16) {
      src0 = LD_UB(src);
      pred0 = LD_UB(pred);
      dst0 = src0 + pred0;
      ST_UB(dst0, dst);
      src += 16;
      pred += 16;
      dst += 16;
      length -= 16;
    }
    for (i = 0; i < length; i++) {
      dst[i] = src[i] + pred[i];
    }
  }
}

static WEBP_INLINE void PredictLineTop(const uint8_t* src, const uint8_t* pred,
                                       uint8_t* dst, int length, int inverse)
{
  assert(length >= 0);
  if (inverse) {
    PredictLineInverse1(src, pred, dst, length);
  } else {
    PredictLineInverse0(src, pred, dst, length);
  }
}

static WEBP_INLINE void PredictLine(const uint8_t* src, const uint8_t* pred,
                                    uint8_t* dst, int length, int inverse) {
  int i;
  if (inverse) {
    for (i = 0; i < length; ++i) dst[i] = src[i] + pred[i];
  } else {
    PredictLineInverse0(src, pred, dst, length);
  }
}

//------------------------------------------------------------------------------
// Helpful macro.

#define SANITY_CHECK(in, out)  \
  assert(in != NULL);          \
  assert(out != NULL);         \
  assert(width > 0);           \
  assert(height > 0);          \
  assert(stride >= width);

//------------------------------------------------------------------------------
// Horrizontal filter

static void HorizontalFilter(const uint8_t* data, int width, int height,
                             int stride, uint8_t* filtered_data) {
  const uint8_t* preds = data;
  const uint8_t* in = data;
  uint8_t* out = filtered_data;
  int row = 1;
  SANITY_CHECK(in, out);

  // Leftmost pixel is the same as input for topmost scanline.
  out[0] = in[0];
  PredictLine(in + 1, preds, out + 1, width - 1, 0);
  preds += stride;
  in += stride;
  out += stride;
  // Filter line-by-line.
  while (row < height) {
    // Leftmost pixel is predicted from above.
    PredictLine(in, preds - stride, out, 1, 0);
    PredictLine(in + 1, preds, out + 1, width - 1, 0);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Gradient filter

static void GradientFilter(const uint8_t* data, int width, int height,
                           int stride, uint8_t* filtered_data) {
  const uint8_t* in = data;
  const uint8_t* preds = data;
  uint8_t* out = filtered_data;
  int row = 1;
  SANITY_CHECK(in, out);

  // left prediction for top scan-line
  out[0] = in[0];
  PredictLine(in + 1, preds, out + 1, width - 1, 0);
  preds += stride;
  in += stride;
  out += stride;
  // Filter line-by-line.
  while (row < height) {
    int w;
    int length = width - 1;
    const uint8_t* ptemp_pred = preds + 1;
    const uint8_t* ptemp_in = in + 1;
    uint8_t* ptemp_out = out + 1;
    const v16i8 zero = { 0 };
    out[0] = in[0] - preds[- stride];
    while (length >= 16) {
      v16u8 pred0, dst0;
      v8i16 a0, a1, b0, b1, c0, c1;
      const v16u8 tmp0 = LD_UB(ptemp_pred - 1);
      const v16u8 tmp1 = LD_UB(ptemp_pred - stride);
      const v16u8 tmp2 = LD_UB(ptemp_pred - stride - 1);
      const v16u8 src0 = LD_UB(ptemp_in);
      ILVRL_B2_SH(zero, tmp0, a0, a1);
      ILVRL_B2_SH(zero, tmp1, b0, b1);
      ILVRL_B2_SH(zero, tmp2, c0, c1);
      ADD2(a0, b0, a1, b1, a0, a1);
      SUB2(a0, c0, a1, c1, a0, a1);
      CLIP_SH2_0_255(a0, a1);
      pred0 = (v16u8)__msa_pckev_b((v16i8)a1, (v16i8)a0);
      dst0 = src0 - pred0;
      ST_UB(dst0, ptemp_out);
      ptemp_pred += 16;
      ptemp_in += 16;
      ptemp_out += 16;
      length -= 16;
    }
    for (w = 0; w < length; ++w) {
      const int pred = ptemp_pred[w - 1] + ptemp_pred[w - stride] -
                       ptemp_pred[w - stride - 1];
      ptemp_out[w] = ptemp_in[w] - (pred < 0 ? 0 : (pred > 255 ? 255 : pred));
    }
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter

static void VerticalFilter(const uint8_t* data, int width, int height,
                           int stride, uint8_t* filtered_data) {
  const uint8_t* in = data;
  const uint8_t* preds = data;
  uint8_t* out = filtered_data;
  int row = 1;
  SANITY_CHECK(in, out);

  // Very first top-left pixel is copied.
  out[0] = in[0];
  // Rest of top scan-line is left-predicted.
  PredictLine(in + 1, preds, out + 1, width - 1, 0);
  in += stride;
  out += stride;

  // Filter line-by-line.
  while (row < height) {
    PredictLineTop(in, preds, out, width, 0);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

#undef SANITY_CHECK

//------------------------------------------------------------------------------
// Entry point

extern void VP8FiltersInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitMSA(void) {
  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8FiltersInitMSA)

#endif  // WEBP_USE_MSA
