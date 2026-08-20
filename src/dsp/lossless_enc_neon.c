// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON variant of methods for lossless encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"
#include "src/dsp/neon.h"

//------------------------------------------------------------------------------
// Subtract-Green Transform

// vtbl?_u8 are marked unavailable for iOS arm64 with Xcode < 6.3, use
// non-standard versions there.
#if defined(__APPLE__) && WEBP_AARCH64 && defined(__apple_build_version__) && \
    (__apple_build_version__ < 6020037)
#define USE_VTBLQ
#endif

#ifdef USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[16] = {1, 255, 1, 255, 5,  255, 5,  255,
                                          9, 255, 9, 255, 13, 255, 13, 255};

static WEBP_INLINE uint8x16_t DoGreenShuffle_NEON(const uint8x16_t argb,
                                                  const uint8x16_t shuffle) {
  return vcombine_u8(vtbl1q_u8(argb, vget_low_u8(shuffle)),
                     vtbl1q_u8(argb, vget_high_u8(shuffle)));
}
#else   // !USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[8] = {1, 255, 1, 255, 5, 255, 5, 255};

static WEBP_INLINE uint8x16_t DoGreenShuffle_NEON(const uint8x16_t argb,
                                                  const uint8x8_t shuffle) {
  return vcombine_u8(vtbl1_u8(vget_low_u8(argb), shuffle),
                     vtbl1_u8(vget_high_u8(argb), shuffle));
}
#endif  // USE_VTBLQ

static void SubtractGreenFromBlueAndRed_NEON(uint32_t* argb_data,
                                             int num_pixels) {
  const uint32_t* const end = argb_data + (num_pixels & ~3);
#ifdef USE_VTBLQ
  const uint8x16_t shuffle = vld1q_u8(kGreenShuffle);
#else
  const uint8x8_t shuffle = vld1_u8(kGreenShuffle);
#endif
  for (; argb_data < end; argb_data += 4) {
    const uint8x16_t argb = vld1q_u8((uint8_t*)argb_data);
    const uint8x16_t greens = DoGreenShuffle_NEON(argb, shuffle);
    vst1q_u8((uint8_t*)argb_data, vsubq_u8(argb, greens));
  }
  // fallthrough and finish off with plain-C
  VP8LSubtractGreenFromBlueAndRed_C(argb_data, num_pixels & 3);
}

//------------------------------------------------------------------------------
// Color Transform

static void TransformColor_NEON(const VP8LMultipliers* WEBP_RESTRICT const m,
                                uint32_t* WEBP_RESTRICT argb_data,
                                int num_pixels) {
  // sign-extended multiplying constants, pre-shifted by 6.
#define CST(X) (((int16_t)(m->X << 8)) >> 6)
  const int16_t rb[8] = {CST(green_to_blue), CST(green_to_red),
                         CST(green_to_blue), CST(green_to_red),
                         CST(green_to_blue), CST(green_to_red),
                         CST(green_to_blue), CST(green_to_red)};
  const int16x8_t mults_rb = vld1q_s16(rb);
  const int16_t b2[8] = {
      0, CST(red_to_blue), 0, CST(red_to_blue),
      0, CST(red_to_blue), 0, CST(red_to_blue),
  };
  const int16x8_t mults_b2 = vld1q_s16(b2);
#undef CST
#ifdef USE_VTBLQ
  static const uint8_t kg0g0[16] = {255, 1, 255, 1, 255, 5,  255, 5,
                                    255, 9, 255, 9, 255, 13, 255, 13};
  const uint8x16_t shuffle = vld1q_u8(kg0g0);
#else
  static const uint8_t k0g0g[8] = {255, 1, 255, 1, 255, 5, 255, 5};
  const uint8x8_t shuffle = vld1_u8(k0g0g);
#endif
  const uint32x4_t mask_rb = vdupq_n_u32(0x00ff00ffu);  // red-blue masks
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t in = vld1q_u8((uint8_t*)(argb_data + i));
    // 0 g 0 g
    const uint8x16_t greens = DoGreenShuffle_NEON(in, shuffle);
    // x dr  x db1
    const int16x8_t A = vqdmulhq_s16(vreinterpretq_s16_u8(greens), mults_rb);
    // r 0   b   0
    const int16x8_t B = vshlq_n_s16(vreinterpretq_s16_u8(in), 8);
    // x db2 0   0
    const int16x8_t C = vqdmulhq_s16(B, mults_b2);
    // 0 0   x db2
    const uint32x4_t D = vshrq_n_u32(vreinterpretq_u32_s16(C), 16);
    // x dr  x  db
    const int8x16_t E =
        vaddq_s8(vreinterpretq_s8_u32(D), vreinterpretq_s8_s16(A));
    // 0 dr  0  db
    const uint32x4_t F = vandq_u32(vreinterpretq_u32_s8(E), mask_rb);
    const int8x16_t out =
        vsubq_s8(vreinterpretq_s8_u8(in), vreinterpretq_s8_u32(F));
    vst1q_s8((int8_t*)(argb_data + i), out);
  }
  // fallthrough and finish off with plain-C
  VP8LTransformColor_C(m, argb_data + i, num_pixels - i);
}

#undef USE_VTBLQ

//------------------------------------------------------------------------------
// Shannon entropy

static WEBP_INLINE uint32_t HorizontalSum_NEON(uint32x4_t v) {
#if WEBP_AARCH64
  return vaddvq_u32(v);
#else
  const uint32x2_t sum2 = vadd_u32(vget_low_u32(v), vget_high_u32(v));
  const uint32x2_t sum1 = vpadd_u32(sum2, sum2);
  return vget_lane_u32(sum1, 0);
#endif
}

// Unconditional scalar sum of VP8LFastSLog2() over the 4 lanes
static WEBP_INLINE uint64_t SumFastSLog2x4_NEON(uint32x4_t v) {
  return VP8LFastSLog2(vgetq_lane_u32(v, 0)) +
         VP8LFastSLog2(vgetq_lane_u32(v, 1)) +
         VP8LFastSLog2(vgetq_lane_u32(v, 2)) +
         VP8LFastSLog2(vgetq_lane_u32(v, 3));
}

static uint64_t CombinedShannonEntropy_NEON(const uint32_t X[256],
                                            const uint32_t Y[256]) {
  int i;
  uint64_t retval = 0;
  uint32_t sumX, sumXY;

  // unconditionally process the non-zero elements of the array.
  // It's important to disable unrolling here, to avoid 11x bloated code.
  {
    uint32x4_t accX0 = vdupq_n_u32(0), accX1 = vdupq_n_u32(0);
    uint32x4_t accX2 = vdupq_n_u32(0), accX3 = vdupq_n_u32(0);
    uint32x4_t accY0 = vdupq_n_u32(0), accY1 = vdupq_n_u32(0);
    uint32x4_t accY2 = vdupq_n_u32(0), accY3 = vdupq_n_u32(0);
    WEBP_NO_UNROLL
    for (i = 0; i < 256; i += 16) {
      accX0 = vaddq_u32(accX0, vld1q_u32(X + i + 0));
      accX1 = vaddq_u32(accX1, vld1q_u32(X + i + 4));
      accX2 = vaddq_u32(accX2, vld1q_u32(X + i + 8));
      accX3 = vaddq_u32(accX3, vld1q_u32(X + i + 12));
      accY0 = vaddq_u32(accY0, vld1q_u32(Y + i + 0));
      accY1 = vaddq_u32(accY1, vld1q_u32(Y + i + 4));
      accY2 = vaddq_u32(accY2, vld1q_u32(Y + i + 8));
      accY3 = vaddq_u32(accY3, vld1q_u32(Y + i + 12));
    }
    sumX = HorizontalSum_NEON(
        vaddq_u32(vaddq_u32(accX0, accX1), vaddq_u32(accX2, accX3)));
    sumXY = sumX + HorizontalSum_NEON(vaddq_u32(vaddq_u32(accY0, accY1),
                                                vaddq_u32(accY2, accY3)));
  }

  WEBP_NO_UNROLL
  for (i = 0; i < 256; i += 16) {
    const uint32x4_t x0 = vld1q_u32(X + i + 0);
    const uint32x4_t x1 = vld1q_u32(X + i + 4);
    const uint32x4_t x2 = vld1q_u32(X + i + 8);
    const uint32x4_t x3 = vld1q_u32(X + i + 12);
    const uint32x4_t y0 = vld1q_u32(Y + i + 0);
    const uint32x4_t y1 = vld1q_u32(Y + i + 4);
    const uint32x4_t y2 = vld1q_u32(Y + i + 8);
    const uint32x4_t y3 = vld1q_u32(Y + i + 12);
    // skip SLog2() if the 16-wide chunk is zero in both X and Y
    const uint32x4_t or_xy =
        vorrq_u32(vorrq_u32(vorrq_u32(x0, x1), vorrq_u32(x2, x3)),
                  vorrq_u32(vorrq_u32(y0, y1), vorrq_u32(y2, y3)));
    const uint32x2_t or2 = vorr_u32(vget_low_u32(or_xy), vget_high_u32(or_xy));
    if ((vget_lane_u32(or2, 0) | vget_lane_u32(or2, 1)) != 0) {
      const uint32x4_t xy0 = vaddq_u32(x0, y0);
      const uint32x4_t xy1 = vaddq_u32(x1, y1);
      const uint32x4_t xy2 = vaddq_u32(x2, y2);
      const uint32x4_t xy3 = vaddq_u32(x3, y3);
      retval += SumFastSLog2x4_NEON(x0) + SumFastSLog2x4_NEON(xy0) +
                SumFastSLog2x4_NEON(x1) + SumFastSLog2x4_NEON(xy1) +
                SumFastSLog2x4_NEON(x2) + SumFastSLog2x4_NEON(xy2) +
                SumFastSLog2x4_NEON(x3) + SumFastSLog2x4_NEON(xy3);
    }
  }
  retval = VP8LFastSLog2(sumX) + VP8LFastSLog2(sumXY) - retval;
  return retval;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitNEON(void) {
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed_NEON;
  VP8LTransformColor = TransformColor_NEON;
  VP8LCombinedShannonEntropy = CombinedShannonEntropy_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8LEncDspInitNEON)

#endif  // WEBP_USE_NEON
