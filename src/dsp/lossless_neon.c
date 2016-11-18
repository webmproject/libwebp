// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON variant of methods for lossless decoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#include "./lossless.h"
#include "./neon.h"

//------------------------------------------------------------------------------
// Colorspace conversion functions

#if !defined(WORK_AROUND_GCC)
// gcc 4.6.0 had some trouble (NDK-r9) with this code. We only use it for
// gcc-4.8.x at least.
static void ConvertBGRAToRGBA(const uint32_t* src,
                              int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~15);
  for (; src < end; src += 16) {
    uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    // swap B and R. (VSWP d0,d2 has no intrinsics equivalent!)
    const uint8x16_t tmp = pixel.val[0];
    pixel.val[0] = pixel.val[2];
    pixel.val[2] = tmp;
    vst4q_u8(dst, pixel);
    dst += 64;
  }
  VP8LConvertBGRAToRGBA_C(src, num_pixels & 15, dst);  // left-overs
}

static void ConvertBGRAToBGR(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~15);
  for (; src < end; src += 16) {
    const uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    const uint8x16x3_t tmp = { { pixel.val[0], pixel.val[1], pixel.val[2] } };
    vst3q_u8(dst, tmp);
    dst += 48;
  }
  VP8LConvertBGRAToBGR_C(src, num_pixels & 15, dst);  // left-overs
}

static void ConvertBGRAToRGB(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~15);
  for (; src < end; src += 16) {
    const uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    const uint8x16x3_t tmp = { { pixel.val[2], pixel.val[1], pixel.val[0] } };
    vst3q_u8(dst, tmp);
    dst += 48;
  }
  VP8LConvertBGRAToRGB_C(src, num_pixels & 15, dst);  // left-overs
}

#else  // WORK_AROUND_GCC

// gcc-4.6.0 fallback

static const uint8_t kRGBAShuffle[8] = { 2, 1, 0, 3, 6, 5, 4, 7 };

static void ConvertBGRAToRGBA(const uint32_t* src,
                              int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~1);
  const uint8x8_t shuffle = vld1_u8(kRGBAShuffle);
  for (; src < end; src += 2) {
    const uint8x8_t pixels = vld1_u8((uint8_t*)src);
    vst1_u8(dst, vtbl1_u8(pixels, shuffle));
    dst += 8;
  }
  VP8LConvertBGRAToRGBA_C(src, num_pixels & 1, dst);  // left-overs
}

static const uint8_t kBGRShuffle[3][8] = {
  {  0,  1,  2,  4,  5,  6,  8,  9 },
  { 10, 12, 13, 14, 16, 17, 18, 20 },
  { 21, 22, 24, 25, 26, 28, 29, 30 }
};

static void ConvertBGRAToBGR(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~7);
  const uint8x8_t shuffle0 = vld1_u8(kBGRShuffle[0]);
  const uint8x8_t shuffle1 = vld1_u8(kBGRShuffle[1]);
  const uint8x8_t shuffle2 = vld1_u8(kBGRShuffle[2]);
  for (; src < end; src += 8) {
    uint8x8x4_t pixels;
    INIT_VECTOR4(pixels,
                 vld1_u8((const uint8_t*)(src + 0)),
                 vld1_u8((const uint8_t*)(src + 2)),
                 vld1_u8((const uint8_t*)(src + 4)),
                 vld1_u8((const uint8_t*)(src + 6)));
    vst1_u8(dst +  0, vtbl4_u8(pixels, shuffle0));
    vst1_u8(dst +  8, vtbl4_u8(pixels, shuffle1));
    vst1_u8(dst + 16, vtbl4_u8(pixels, shuffle2));
    dst += 8 * 3;
  }
  VP8LConvertBGRAToBGR_C(src, num_pixels & 7, dst);  // left-overs
}

static const uint8_t kRGBShuffle[3][8] = {
  {  2,  1,  0,  6,  5,  4, 10,  9 },
  {  8, 14, 13, 12, 18, 17, 16, 22 },
  { 21, 20, 26, 25, 24, 30, 29, 28 }
};

static void ConvertBGRAToRGB(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~7);
  const uint8x8_t shuffle0 = vld1_u8(kRGBShuffle[0]);
  const uint8x8_t shuffle1 = vld1_u8(kRGBShuffle[1]);
  const uint8x8_t shuffle2 = vld1_u8(kRGBShuffle[2]);
  for (; src < end; src += 8) {
    uint8x8x4_t pixels;
    INIT_VECTOR4(pixels,
                 vld1_u8((const uint8_t*)(src + 0)),
                 vld1_u8((const uint8_t*)(src + 2)),
                 vld1_u8((const uint8_t*)(src + 4)),
                 vld1_u8((const uint8_t*)(src + 6)));
    vst1_u8(dst +  0, vtbl4_u8(pixels, shuffle0));
    vst1_u8(dst +  8, vtbl4_u8(pixels, shuffle1));
    vst1_u8(dst + 16, vtbl4_u8(pixels, shuffle2));
    dst += 8 * 3;
  }
  VP8LConvertBGRAToRGB_C(src, num_pixels & 7, dst);  // left-overs
}

#endif   // !WORK_AROUND_GCC


//------------------------------------------------------------------------------
// Predictor Transform

static WEBP_INLINE uint32_t ClampedAddSubtractFull_NEON(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const uint8x8_t C0 = vreinterpret_u8_u32(vdup_n_u32(c0));
  const uint8x8_t C1 = vreinterpret_u8_u32(vdup_n_u32(c1));
  const uint16x8_t sum = vaddl_u8(C0, C1);
  const uint8x8_t C2 = vreinterpret_u8_u32(vdup_n_u32(c2));
  const uint16x8_t A2 = vmovl_u8(C2);
  // Saturated subtract.
  const uint16x8_t diff = vqsubq_u16(sum, A2);
  const uint8x8_t res = vqmovn_u16(diff);
  const uint32_t output = vget_lane_u32(vreinterpret_u32_u8(res), 0);
  return output;
}


static WEBP_INLINE uint8x8_t Average2_u8_NEON(uint32_t a0, uint32_t a1) {
  const uint8x8_t A0 = vreinterpret_u8_u32(vdup_n_u32(a0));
  const uint8x8_t A1 = vreinterpret_u8_u32(vdup_n_u32(a1));
  return vhadd_u8(A0, A1);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf_NEON(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const uint8x8_t avg = Average2_u8_NEON(c0, c1);
  // Remove one to c2 when bigger than avg.
  const uint8x8_t C2 = vreinterpret_u8_u32(vdup_n_u32(c2));
  const uint8x8_t cmp = vcgt_u8(C2, avg);
  const uint8x8_t C2_1 = vadd_u8(C2, cmp);
  // Compute half of the difference between avg and c2.
  const int8x8_t diff_avg = vreinterpret_s8_u8(vhsub_u8(avg, C2_1));
  // Compute the sum with avg and saturate.
  const int16x8_t avg_16 = vreinterpretq_s16_u16(vmovl_u8(avg));
  const uint8x8_t res = vqmovun_s16(vaddw_s8(avg_16, diff_avg));
  const uint32_t output = vget_lane_u32(vreinterpret_u32_u8(res), 0);
  return output;
}

static WEBP_INLINE uint32_t Select_NEON(uint32_t a, uint32_t b, uint32_t c) {
  const uint32x2_t A_32 = vdup_n_u32(a);
  const uint8x8_t A = vreinterpret_u8_u32(A_32);
  const uint8x8_t C = vreinterpret_u8_u32(vdup_n_u32(c));
  const uint8x8_t pa = vabd_u8(A, C);
  const uint32x2_t B_32 = vdup_n_u32(b);
  const uint8x8_t B = vreinterpret_u8_u32(B_32);
  const uint8x8_t pb = vabd_u8(B, C);
  const int16x4_t diff = vreinterpret_s16_u16(vget_high_u16(vsubl_u8(pb, pa)));
  // Horizontal add the adjacent pairs twice to get the sum of the first four
  // signed 16-bit integers. The first add cannot be vpaddl_s16 as it would
  // return a int32x2_t which would lead to a int64x1_t for the second one
  // (which would be hard to deal with).
  const int16x4_t sum = vpadd_s16(diff, diff);
  const int32x2_t pa_minus_pb = vpaddl_s16(sum);
  const int32x2_t zero = vdup_n_s32(0);
  const uint32x2_t cmp = vcle_s32(pa_minus_pb, zero);
  const uint32x2_t output = vbsl_u32(cmp, B_32, A_32);
  return vget_lane_u32(output, 0);
}

static WEBP_INLINE uint32_t Average2_NEON(uint32_t a0, uint32_t a1) {
  const uint8x8_t avg_u8x8 = Average2_u8_NEON(a0, a1);
  const uint32x2_t avg_u32x2 = vreinterpret_u32_u8(avg_u8x8);
  const uint32_t avg = vget_lane_u32(avg_u32x2, 0);
  return avg;
}

static WEBP_INLINE uint32_t Average3_NEON(uint32_t a0, uint32_t a1,
                                          uint32_t a2) {
  const uint8x8_t avg0 = Average2_u8_NEON(a0, a2);
  const uint8x8_t A1 = vreinterpret_u8_u32(vdup_n_u32(a1));
  const uint32x2_t avg1 = vreinterpret_u32_u8(vhadd_u8(avg0, A1));
  const uint32_t avg = vget_lane_u32(avg1, 0);
  return avg;
}

static WEBP_INLINE uint32_t Average4_NEON(uint32_t a0, uint32_t a1,
                                          uint32_t a2, uint32_t a3) {
  const uint8x8_t avg0 = Average2_u8_NEON(a0, a1);
  const uint8x8_t avg1 = Average2_u8_NEON(a2, a3);
  const uint32x2_t avg2 = vreinterpret_u32_u8(vhadd_u8(avg0, avg1));
  const uint32_t avg = vget_lane_u32(avg2, 0);
  return avg;
}

static uint32_t Predictor5_NEON(uint32_t left, const uint32_t* const top) {
  return Average3_NEON(left, top[0], top[1]);
}
static uint32_t Predictor6_NEON(uint32_t left, const uint32_t* const top) {
  return Average2_NEON(left, top[-1]);
}
static uint32_t Predictor7_NEON(uint32_t left, const uint32_t* const top) {
  return Average2_NEON(left, top[0]);
}
static uint32_t Predictor8_NEON(uint32_t left, const uint32_t* const top) {
  (void)left;
  return Average2_NEON(top[-1], top[0]);
}
static uint32_t Predictor9_NEON(uint32_t left, const uint32_t* const top) {
  (void)left;
  return Average2_NEON(top[0], top[1]);
}
static uint32_t Predictor10_NEON(uint32_t left, const uint32_t* const top) {
  return Average4_NEON(left, top[-1], top[0], top[1]);
}
static uint32_t Predictor11_NEON(uint32_t left, const uint32_t* const top) {
  return Select_NEON(top[0], left, top[-1]);
}
static uint32_t Predictor12_NEON(uint32_t left, const uint32_t* const top) {
  return ClampedAddSubtractFull_NEON(left, top[0], top[-1]);
}
static uint32_t Predictor13_NEON(uint32_t left, const uint32_t* const top) {
  return ClampedAddSubtractHalf_NEON(left, top[0], top[-1]);
}

//------------------------------------------------------------------------------
// Subtract-Green Transform

// vtbl?_u8 are marked unavailable for iOS arm64 with Xcode < 6.3, use
// non-standard versions there.
#if defined(__APPLE__) && defined(__aarch64__) && \
    defined(__apple_build_version__) && (__apple_build_version__< 6020037)
#define USE_VTBLQ
#endif

#ifdef USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[16] = {
  1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13, 255
};

static WEBP_INLINE uint8x16_t DoGreenShuffle(const uint8x16_t argb,
                                             const uint8x16_t shuffle) {
  return vcombine_u8(vtbl1q_u8(argb, vget_low_u8(shuffle)),
                     vtbl1q_u8(argb, vget_high_u8(shuffle)));
}
#else  // !USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[8] = { 1, 255, 1, 255, 5, 255, 5, 255  };

static WEBP_INLINE uint8x16_t DoGreenShuffle(const uint8x16_t argb,
                                             const uint8x8_t shuffle) {
  return vcombine_u8(vtbl1_u8(vget_low_u8(argb), shuffle),
                     vtbl1_u8(vget_high_u8(argb), shuffle));
}
#endif  // USE_VTBLQ

static void AddGreenToBlueAndRed(uint32_t* argb_data, int num_pixels) {
  const uint32_t* const end = argb_data + (num_pixels & ~3);
#ifdef USE_VTBLQ
  const uint8x16_t shuffle = vld1q_u8(kGreenShuffle);
#else
  const uint8x8_t shuffle = vld1_u8(kGreenShuffle);
#endif
  for (; argb_data < end; argb_data += 4) {
    const uint8x16_t argb = vld1q_u8((uint8_t*)argb_data);
    const uint8x16_t greens = DoGreenShuffle(argb, shuffle);
    vst1q_u8((uint8_t*)argb_data, vaddq_u8(argb, greens));
  }
  // fallthrough and finish off with plain-C
  VP8LAddGreenToBlueAndRed_C(argb_data, num_pixels & 3);
}

//------------------------------------------------------------------------------
// Color Transform

static void TransformColorInverse(const VP8LMultipliers* const m,
                                  uint32_t* argb_data, int num_pixels) {
  // sign-extended multiplying constants, pre-shifted by 6.
#define CST(X)  (((int16_t)(m->X << 8)) >> 6)
  const int16_t rb[8] = {
    CST(green_to_blue_), CST(green_to_red_),
    CST(green_to_blue_), CST(green_to_red_),
    CST(green_to_blue_), CST(green_to_red_),
    CST(green_to_blue_), CST(green_to_red_)
  };
  const int16x8_t mults_rb = vld1q_s16(rb);
  const int16_t b2[8] = {
    0, CST(red_to_blue_), 0, CST(red_to_blue_),
    0, CST(red_to_blue_), 0, CST(red_to_blue_),
  };
  const int16x8_t mults_b2 = vld1q_s16(b2);
#undef CST
#ifdef USE_VTBLQ
  static const uint8_t kg0g0[16] = {
    255, 1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13
  };
  const uint8x16_t shuffle = vld1q_u8(kg0g0);
#else
  static const uint8_t k0g0g[8] = { 255, 1, 255, 1, 255, 5, 255, 5 };
  const uint8x8_t shuffle = vld1_u8(k0g0g);
#endif
  const uint32x4_t mask_ag = vdupq_n_u32(0xff00ff00u);
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t in = vld1q_u8((uint8_t*)(argb_data + i));
    const uint32x4_t a0g0 = vandq_u32(vreinterpretq_u32_u8(in), mask_ag);
    // 0 g 0 g
    const uint8x16_t greens = DoGreenShuffle(in, shuffle);
    // x dr  x db1
    const int16x8_t A = vqdmulhq_s16(vreinterpretq_s16_u8(greens), mults_rb);
    // x r'  x   b'
    const int8x16_t B = vaddq_s8(vreinterpretq_s8_u8(in),
                                 vreinterpretq_s8_s16(A));
    // r' 0   b' 0
    const int16x8_t C = vshlq_n_s16(vreinterpretq_s16_s8(B), 8);
    // x db2  0  0
    const int16x8_t D = vqdmulhq_s16(C, mults_b2);
    // 0  x db2  0
    const uint32x4_t E = vshrq_n_u32(vreinterpretq_u32_s16(D), 8);
    // r' x  b'' 0
    const int8x16_t F = vaddq_s8(vreinterpretq_s8_u32(E),
                                 vreinterpretq_s8_s16(C));
    // 0  r'  0  b''
    const uint16x8_t G = vshrq_n_u16(vreinterpretq_u16_s8(F), 8);
    const uint32x4_t out = vorrq_u32(vreinterpretq_u32_u16(G), a0g0);
    vst1q_u32(argb_data + i, out);
  }
  // Fall-back to C-version for left-overs.
  VP8LTransformColorInverse_C(m, argb_data + i, num_pixels - i);
}

#undef USE_VTBLQ

//------------------------------------------------------------------------------
// Entry point

extern void VP8LDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitNEON(void) {
  VP8LPredictors[5] = Predictor5_NEON;
  VP8LPredictors[6] = Predictor6_NEON;
  VP8LPredictors[7] = Predictor7_NEON;
  VP8LPredictors[8] = Predictor8_NEON;
  VP8LPredictors[9] = Predictor9_NEON;
  VP8LPredictors[10] = Predictor10_NEON;
  VP8LPredictors[11] = Predictor11_NEON;
  VP8LPredictors[12] = Predictor12_NEON;
  VP8LPredictors[13] = Predictor13_NEON;

  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB;

  VP8LAddGreenToBlueAndRed = AddGreenToBlueAndRed;
  VP8LTransformColorInverse = TransformColorInverse;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8LDspInitNEON)

#endif  // WEBP_USE_NEON
