// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON)
#include <arm_neon.h>

#include "src/enc/vp8i_enc.h"

//------------------------------------------------------------------------------

static uint32x2_t horizontal_add_uint32x4(const uint32x4_t a) {
  const uint64x2_t b = vpaddlq_u32(a);
  return vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                  vreinterpret_u32_u64(vget_high_u64(b)));
}

static score_t IsFlat_NEON(const int16_t* levels, int num_blocks,
                           score_t thresh) {
  const int16x8_t tst_ones = vdupq_n_s16(-1);
  uint32x4_t sum = vdupq_n_u32(0);

  for (int i = 0; i < num_blocks; ++i) {
    int16x8_t a_0 = vld1q_s16(levels);
    int16x8_t a_1 = vld1q_s16(levels + 8);
    uint16x8_t b_0, b_1;

    a_0 = vsetq_lane_s16(0, a_0, 0); // Set DC to zero.

    b_0 = vshrq_n_u16(vtstq_s16(a_0, tst_ones), 15);
    b_1 = vshrq_n_u16(vtstq_s16(a_1, tst_ones), 15);

    sum = vpadalq_u16(sum, b_0);
    sum = vpadalq_u16(sum, b_1);

    levels += 16;
  }
  return thresh >= vget_lane_u32(horizontal_add_uint32x4(sum), 0);
}


//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspQuantInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspQuantInitNEON(void) {
  VP8IsFlat = IsFlat_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8EncDspQuantInitNEON)

#endif  // WEBP_USE_NEON
