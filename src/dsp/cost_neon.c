// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// ARM NEON version of cost functions

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON)

#include "src/dsp/neon.h"
#include "src/enc/cost_enc.h"

static const uint8_t position[16] = { 1, 2,  3,  4,  5,  6,  7,  8,
                                      9, 10, 11, 12, 13, 14, 15, 16 };

static void SetResidualCoeffs_NEON(const int16_t* const coeffs,
                                   VP8Residual* const res) {
  const int16x8_t minus_one = vdupq_n_s16(-1);
  const int16x8_t coeffs_0 = vld1q_s16(coeffs);
  const int16x8_t coeffs_1 = vld1q_s16(coeffs + 8);
  const uint16x8_t eob_0 = vtstq_s16(coeffs_0, minus_one);
  const uint16x8_t eob_1 = vtstq_s16(coeffs_1, minus_one);
  const uint8x16_t eob = vcombine_u8(vqmovn_u16(eob_0), vqmovn_u16(eob_1));
  const uint8x16_t masked = vandq_u8(eob, vld1q_u8(position));

#ifdef __aarch64__
  res->last = vmaxvq_u8(masked) - 1;
#else
  const uint8x8_t eob_8x8 = vmax_u8(vget_low_u8(masked), vget_high_u8(masked));
  const uint16x8_t eob_16x8 = vmovl_u8(eob_8x8);
  const uint16x4_t eob_16x4 =
      vmax_u16(vget_low_u16(eob_16x8), vget_high_u16(eob_16x8));
  const uint32x4_t eob_32x4 = vmovl_u16(eob_16x4);
  uint32x2_t eob_32x2 =
      vmax_u32(vget_low_u32(eob_32x4), vget_high_u32(eob_32x4));
  eob_32x2 = vpmax_u32(eob_32x2, eob_32x2);

  vst1_lane_s32(&res->last, vreinterpret_s32_u32(eob_32x2), 0);
  --res->last;
#endif  // __aarch64__

  res->coeffs = coeffs;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspCostInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspCostInitNEON(void) {
  VP8SetResidualCoeffs = SetResidualCoeffs_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8EncDspCostInitNEON)

#endif  // WEBP_USE_NEON
