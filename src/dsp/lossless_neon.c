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

#define LOAD_U32_AS_U8(VALUE) vreinterpret_u8_u32(vdup_n_u32((VALUE)))
#define STORE_U8_AS_U32(VALUE) vget_lane_u32(vreinterpret_u32_u8((VALUE)), 0);

#define LOAD_U32_AS_U8Q(VALUE) vreinterpretq_u8_u32(vdupq_n_u32((VALUE)))
#define LOAD_U32P_AS_U8Q(VALUE) vreinterpretq_u8_u32(vld1q_u32((VALUE)))
#define STORE_U8Q_AS_U32P(OUT, IN) vst1q_u32((OUT), vreinterpretq_u32_u8((IN)));

static WEBP_INLINE uint32_t ClampedAddSubtractFull_NEON(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const uint8x8_t C0 = LOAD_U32_AS_U8(c0);
  const uint8x8_t C1 = LOAD_U32_AS_U8(c1);
  const uint16x8_t sum = vaddl_u8(C0, C1);
  const uint8x8_t C2 = LOAD_U32_AS_U8(c2);
  const uint16x8_t A2 = vmovl_u8(C2);
  // Saturated subtract.
  const uint16x8_t diff = vqsubq_u16(sum, A2);
  const uint8x8_t res = vqmovn_u16(diff);
  const uint32_t output = STORE_U8_AS_U32(res);
  return output;
}

static WEBP_INLINE uint8x8_t Average2_u8_NEON(uint32_t a0, uint32_t a1) {
  const uint8x8_t A0 = LOAD_U32_AS_U8(a0);
  const uint8x8_t A1 = LOAD_U32_AS_U8(a1);
  return vhadd_u8(A0, A1);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf_NEON(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const uint8x8_t avg = Average2_u8_NEON(c0, c1);
  // Remove one to c2 when bigger than avg.
  const uint8x8_t C2 = LOAD_U32_AS_U8(c2);
  const uint8x8_t cmp = vcgt_u8(C2, avg);
  const uint8x8_t C2_1 = vadd_u8(C2, cmp);
  // Compute half of the difference between avg and c2.
  const int8x8_t diff_avg = vreinterpret_s8_u8(vhsub_u8(avg, C2_1));
  // Compute the sum with avg and saturate.
  const int16x8_t avg_16 = vreinterpretq_s16_u16(vmovl_u8(avg));
  const uint8x8_t res = vqmovun_s16(vaddw_s8(avg_16, diff_avg));
  const uint32_t output = STORE_U8_AS_U32(res);
  return output;
}

static WEBP_INLINE uint32_t Average2_NEON(uint32_t a0, uint32_t a1) {
  const uint8x8_t avg_u8x8 = Average2_u8_NEON(a0, a1);
  const uint32_t avg = STORE_U8_AS_U32(avg_u8x8);
  return avg;
}

static WEBP_INLINE uint32_t Average3_NEON(uint32_t a0, uint32_t a1,
                                          uint32_t a2) {
  const uint8x8_t avg0 = Average2_u8_NEON(a0, a2);
  const uint8x8_t A1 = LOAD_U32_AS_U8(a1);
  const uint32_t avg = STORE_U8_AS_U32(vhadd_u8(avg0, A1));
  return avg;
}

static WEBP_INLINE uint32_t Average4_NEON(uint32_t a0, uint32_t a1, uint32_t a2,
                                          uint32_t a3) {
  const uint8x8_t avg0 = Average2_u8_NEON(a0, a1);
  const uint8x8_t avg1 = Average2_u8_NEON(a2, a3);
  const uint32_t avg = STORE_U8_AS_U32(vhadd_u8(avg0, avg1));
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
static uint32_t Predictor12_NEON(uint32_t left, const uint32_t* const top) {
  return ClampedAddSubtractFull_NEON(left, top[0], top[-1]);
}
static uint32_t Predictor13_NEON(uint32_t left, const uint32_t* const top) {
  return ClampedAddSubtractHalf_NEON(left, top[0], top[-1]);
}

// Batch versions of those functions.

// Predictor0: ARGB_BLACK.
static void PredictorAdd0_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* out) {
  int i;
  const uint8x16_t black = vreinterpretq_u8_u32(vdupq_n_u32(ARGB_BLACK));
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOAD_U32P_AS_U8Q(&in[i]);
    const uint8x16_t res = vaddq_u8(src, black);
    STORE_U8Q_AS_U32P(&out[i], res);
  }
  VP8LPredictorsAdd_C[0](in + i, upper + i, num_pixels - i, out + i);
}

// Predictor1: left.
static void PredictorAdd1_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* out) {
  int i;
  const uint8x16_t zero = LOAD_U32_AS_U8Q(0);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    // a | b | c | d
    const uint8x16_t src = LOAD_U32P_AS_U8Q(&in[i]);
    // 0 | a | b | c
    const uint8x16_t shift0 = vextq_u8(src, zero, 4);
    // a | a + b | b + c | c + d
    const uint8x16_t sum0 = vaddq_u8(src, shift0);
    // 0 | 0 | a | a + b
    const uint8x16_t shift1 = vextq_u8(sum0, zero, 8);
    // a | a + b | a + b + c | a + b + c + d
    const uint8x16_t sum1 = vaddq_u8(sum0, shift1);
    const uint8x16_t prev = LOAD_U32_AS_U8Q(out[i - 1]);
    const uint8x16_t res = vaddq_u8(sum1, prev);
    STORE_U8Q_AS_U32P(&out[i], res);
  }
  VP8LPredictorsAdd_C[1](in + i, upper + i, num_pixels - i, out + i);
}

// Macro that adds 32-bit integers from IN using mod 256 arithmetic
// per 8 bit channel.
#define GENERATE_PREDICTOR_1(X, IN)                                       \
static void PredictorAdd##X##_NEON(const uint32_t* in,                    \
                                   const uint32_t* upper, int num_pixels, \
                                   uint32_t* out) {                       \
  int i;                                                                  \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                              \
    const uint8x16_t src = LOAD_U32P_AS_U8Q(&in[i]);                      \
    const uint8x16_t other = LOAD_U32P_AS_U8Q(&(IN));                     \
    const uint8x16_t res = vaddq_u8(src, other);                          \
    STORE_U8Q_AS_U32P(&out[i], res);                                      \
  }                                                                       \
  VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);   \
}
// Predictor2: Top.
GENERATE_PREDICTOR_1(2, upper[i])
// Predictor3: Top-right.
GENERATE_PREDICTOR_1(3, upper[i + 1])
// Predictor4: Top-left.
GENERATE_PREDICTOR_1(4, upper[i - 1])
#undef GENERATE_PREDICTOR_1
/*
// Due to averages with integers, values cannot be accumulated in parallel for
// predictors 5 to 10.
GENERATE_PREDICTOR_ADD(5)
GENERATE_PREDICTOR_ADD(6)
GENERATE_PREDICTOR_ADD(7)
GENERATE_PREDICTOR_ADD(8)
GENERATE_PREDICTOR_ADD(9)
GENERATE_PREDICTOR_ADD(10)
*/

// Predictor11: select.
static void PredictorAdd11_SSE(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* out) {
  int i, j;
  const uint8x16_t zero = LOAD_U32_AS_U8Q(0);
  for (i = 0; i < num_pixels - 4; i += 4) {
    const uint32x4_t T_32 = vld1q_u32(&upper[i]);
    const uint8x16_t T = vreinterpretq_u8_u32(T_32);
    uint8x16_t TL = LOAD_U32P_AS_U8Q(&upper[i - 1]);
    // |T - TL|
    uint8x16_t pTTL = vabdq_u8(T, TL);
    // in + T
    uint8x16_t src = LOAD_U32P_AS_U8Q(&in[i]);
    uint32x4_t sumTin =
        vreinterpretq_u32_u8(vaddq_u8(src, LOAD_U32P_AS_U8Q(&upper[i])));
    for (j = 0; j < 4; ++j) {
      const uint8x16_t L = LOAD_U32_AS_U8Q(out[i + j - 1]);
      const uint8x16_t pLTL = vabdq_u8(L, TL);  // |L - TL|
      const int16x8_t diff = vreinterpretq_s16_u16(
          vsubl_u8(vget_high_u8(pLTL), vget_high_u8(pTTL)));
      // Horizontal add the adjacent pairs twice to get the sum of the first
      // four
      // signed 16-bit integers. The first add cannot be vpaddl_s16 as it would
      // return a int32x2_t which would lead to a int64x1_t for the second one
      // (which would be hard to deal with).
      const int16x8_t sum = vpaddq_s16(diff, diff);
      const int32x4_t pa_minus_pb = vpaddlq_s16(sum);
      const uint32x4_t cmp = vcleq_s32(pa_minus_pb, zero);
      const uint32x4_t srcL = vreinterpretq_u32_u8(vaddq_u8(src, L));
      // Add to top (pre-computed) or left.
      const uint32x4_t output = vbslq_u32(cmp, sumTin, srcL);
      out[i + j] = vgetq_lane_u32(output, 0);
      // Shift the pre-computed value for the next iteration.
      pTTL = vextq_u8(zero, pTTL, 12);
      TL = vextq_u8(zero, TL, 12);
      src = vextq_u8(zero, src, 12);
      sumTin = vextq_u8(zero, sumTin, 12);
    }
  }
  VP8LPredictorsAdd_C[11](in + i, upper + i, num_pixels - i, out + i);
}
/*
// Predictor12: ClampedAddSubtractFull.
static void PredictorAdd12_SSE(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* out) {
  int i, j;
  const __m128i zero = _mm_setzero_si128();
  // -4 to not read outside of memory.
  for (i = 0; i < num_pixels - 4; i += 2) {
    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i T8 = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i T = _mm_unpacklo_epi8(T8, zero);
    const __m128i TL8 = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i TL = _mm_unpacklo_epi8(TL8, zero);
    __m128i diff = _mm_sub_epi16(T, TL);
    for (j = 0; j < 2; ++j) {
      const __m128i L8 = _mm_cvtsi32_si128(out[i + j - 1]);
      const __m128i L = _mm_unpacklo_epi8(L8, zero);
      const __m128i all = _mm_add_epi16(L, diff);
      const __m128i alls = _mm_packus_epi16(all, all);
      out[i + j] = _mm_cvtsi128_si32(AddPixels(src, alls));
      // Shift the pre-computed value for the next iteration.
      diff = _mm_srli_si128(diff, 8);
      src = _mm_srli_si128(src, 4);
    }
  }
  VP8LPredictorsAdd_C[12](in + i, upper + i, num_pixels - i, out + i);
}
// Due to averages with integers, values cannot be accumulated in parallel for
// predictors 13.
GENERATE_PREDICTOR_ADD(13)
*/
#undef LOAD_U32_AS_U8Q
#undef LOAD_U32P_AS_U8Q
#undef STORE_U8Q_AS_U32P

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

static void AddGreenToBlueAndRed(const uint32_t* src, int num_pixels,
                                 uint32_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~3);
#ifdef USE_VTBLQ
  const uint8x16_t shuffle = vld1q_u8(kGreenShuffle);
#else
  const uint8x8_t shuffle = vld1_u8(kGreenShuffle);
#endif
  for (; src < end; src += 4, dst += 4) {
    const uint8x16_t argb = vld1q_u8((const uint8_t*)src);
    const uint8x16_t greens = DoGreenShuffle(argb, shuffle);
    vst1q_u8((uint8_t*)dst, vaddq_u8(argb, greens));
  }
  // fallthrough and finish off with plain-C
  VP8LAddGreenToBlueAndRed_C(src, num_pixels & 3, dst);
}

//------------------------------------------------------------------------------
// Color Transform

static void TransformColorInverse(const VP8LMultipliers* const m,
                                  const uint32_t* const src, int num_pixels,
                                  uint32_t* dst) {
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
    const uint8x16_t in = vld1q_u8((const uint8_t*)(src + i));
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
    vst1q_u32(dst + i, out);
  }
  // Fall-back to C-version for left-overs.
  VP8LTransformColorInverse_C(m, src + i, num_pixels - i, dst + i);
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
  VP8LPredictors[12] = Predictor12_NEON;
  VP8LPredictors[13] = Predictor13_NEON;

  VP8LPredictorsAdd[0] = PredictorAdd0_NEON;
  VP8LPredictorsAdd[1] = PredictorAdd1_NEON;
  VP8LPredictorsAdd[2] = PredictorAdd2_NEON;
  VP8LPredictorsAdd[3] = PredictorAdd3_NEON;
  VP8LPredictorsAdd[4] = PredictorAdd4_NEON;
  /*VP8LPredictorsAdd[5] = PredictorAdd5;
  VP8LPredictorsAdd[6] = PredictorAdd6;
  VP8LPredictorsAdd[7] = PredictorAdd7;
  VP8LPredictorsAdd[8] = PredictorAdd8;
  VP8LPredictorsAdd[9] = PredictorAdd9;
  VP8LPredictorsAdd[10] = PredictorAdd10;*/
  VP8LPredictorsAdd[11] = PredictorAdd11_SSE;
  /*VP8LPredictorsAdd[12] = PredictorAdd12_SSE;
  VP8LPredictorsAdd[13] = PredictorAdd13;*/

  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB;

  VP8LAddGreenToBlueAndRed = AddGreenToBlueAndRed;
  VP8LTransformColorInverse = TransformColorInverse;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8LDspInitNEON)

#endif  // WEBP_USE_NEON
