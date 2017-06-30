// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebAssembly (WASM) version of some decoding functions.
//
// Based on dec_sse2.c

#include "../dec/vp8i_dec.h"
#include "../utils/utils.h"
#include "./dsp.h"

#if defined(WEBP_USE_WASM)

typedef int32_t int32x4 __attribute__((__vector_size__(16)));
typedef uint32_t uint32x4 __attribute__((__vector_size__(16)));
typedef int16_t int16x8 __attribute__((__vector_size__(16)));
typedef uint16_t uint16x8 __attribute__((__vector_size__(16)));
typedef int8_t int8x16 __attribute__((__vector_size__(16)));
typedef uint8_t uint8x16 __attribute__((__vector_size__(16)));

//------------------------------------------------------------------------------
//

static WEBP_INLINE uint8x16 get_16_bytes(uint8_t* dst) {
  uint8x16 a;
  memcpy(&a, dst, 16);
  return a;
}

static WEBP_INLINE uint8x16 get_8_bytes(uint8_t* dst) {
  uint8x16 a;
  memcpy(&a, dst, 8);
  return a;
}

static WEBP_INLINE uint8x16 splat_uint8(uint32_t val) {
  uint8x16 a;
  a[0] = val;
  a = (uint8x16)__builtin_shufflevector(a, a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0);
  return a;
}

static WEBP_INLINE uint32x4 cvt32_to_128(uint32_t x) {
  uint32x4 value = (uint32x4){0};
  value[0] = x;
  return value;
}

//------------------------------------------------------------------------------
// 4x4 predictions

static void VE4(uint8_t* dst) {  // vertical
  const uint8x16 zero = (uint8x16){0};
  const uint16x8 two = (uint16x8){2, 2, 2, 2, 2, 2, 2, 2};
  const uint8x16 top = get_8_bytes(dst - BPS - 1);
  const uint16x8 ABCDEFGH = (uint16x8)__builtin_shufflevector(
      top, zero, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16);
  const uint16x8 BCDEFGHX = (uint16x8)__builtin_shufflevector(
      top, zero, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 16, 16);
  const uint16x8 CDEFGHXX = (uint16x8)__builtin_shufflevector(
      top, zero, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 16, 16, 16, 16);
  const uint16x8 avg3 =
      (ABCDEFGH + BCDEFGHX + BCDEFGHX + CDEFGHXX + two) >> two;
  const uint32x4 vals = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  int i;
  for (i = 0; i < 4; ++i) {
    WebPUint32ToMem(dst + i * BPS, vals[0]);
  }
}

static void RD4(uint8_t* dst) {  // Down-right
  const uint8x16 zero = (uint8x16){0};
  const uint16x8 two = (uint16x8){2, 2, 2, 2, 2, 2, 2, 2};
  const uint8x16 top = get_8_bytes(dst - BPS - 1);
  const uint32_t I = dst[-1 + 0 * BPS];
  const uint32_t J = dst[-1 + 1 * BPS];
  const uint32_t K = dst[-1 + 2 * BPS];
  const uint32_t L = dst[-1 + 3 * BPS];
  const uint8x16 LKJI_____ =
      (uint8x16)cvt32_to_128(L | (K << 8) | (J << 16) | (I << 24));
  const uint8x16 lkjixabcd = (uint8x16)__builtin_shufflevector(
      (uint8x16)LKJI_____, top, 0, 1, 2, 3, 16, 17, 18, 19, 20, 31, 31, 31, 31,
      31, 31, 31);
  const uint16x8 LKJIXABC = (uint16x8)__builtin_shufflevector(
      lkjixabcd, zero, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16);
  const uint16x8 KJIXABCD_ = (uint16x8)__builtin_shufflevector(
      lkjixabcd, zero, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 8, 16);
  const uint16x8 JIXABCD__ = (uint16x8)__builtin_shufflevector(
      lkjixabcd, zero, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 8, 16, 9, 16);
  const uint16x8 avg3 =
      (LKJIXABC + KJIXABCD_ + KJIXABCD_ + JIXABCD__ + two) >> two;
  const uint32x4 vals0 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 6, 8, 10, 12, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16);
  const uint32x4 vals1 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 4, 6, 8, 10, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals2 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 2, 4, 6, 8, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals3 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  WebPUint32ToMem(dst + 0 * BPS, vals0[0]);
  WebPUint32ToMem(dst + 1 * BPS, vals1[0]);
  WebPUint32ToMem(dst + 2 * BPS, vals2[0]);
  WebPUint32ToMem(dst + 3 * BPS, vals3[0]);
}

//------------------------------------------------------------------------------
// Luma 16x16

static WEBP_INLINE void Put16(uint8_t v, uint8_t* dst) {
  int j;
  const uint8x16 values = splat_uint8(v);
  for (j = 0; j < 16; ++j) {
    memcpy(dst, &values, 16);
    dst += BPS;
  }
}

static void VE16(uint8_t* dst) {
  const uint8x16 top = get_16_bytes(dst - BPS);
  int j;
  for (j = 0; j < 16; ++j) {
    memcpy(dst + j * BPS, &top, 16);
  }
}

static void HE16(uint8_t* dst) {  // horizontal
  int j;
  for (j = 16; j > 0; --j) {
    const uint8x16 values = splat_uint8(dst[-1]);
    memcpy(dst, &values, 16);
    dst += BPS;
  }
}

static WEBP_INLINE uint32_t add_horizontal_16(uint8_t* dst) {
  const uint8x16 zero = (uint8x16){0};
  const uint8x16 a = get_16_bytes(dst);
  const uint16x8 _a_lbw = (uint16x8)__builtin_shufflevector(
      a, zero, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16);
  const uint16x8 _a_hbw = (uint16x8)__builtin_shufflevector(
      a, zero, 8, 16, 9, 16, 10, 16, 11, 16, 12, 16, 13, 16, 14, 16, 15, 16);
  const uint16x8 sum_a = _a_lbw + _a_hbw;
  const uint16x8 sum_b =
      (uint16x8)__builtin_shufflevector(sum_a, sum_a, 4, 5, 6, 7, 4, 5, 6, 7);
  const uint16x8 sum_c = sum_a + sum_b;
  const uint16x8 sum_d =
      (uint16x8)__builtin_shufflevector(sum_c, sum_c, 2, 3, 2, 3, 2, 3, 2, 3);
  const uint16x8 sum_e = sum_c + sum_d;
  const uint16x8 sum_f =
      (uint16x8)__builtin_shufflevector(sum_e, sum_e, 1, 1, 1, 1, 1, 1, 1, 1);
  const uint16x8 sum_g = sum_e + sum_f;
  return sum_g[0] & 0xffff;
}

static void DC16(uint8_t* dst) {  // DC
  const uint32_t sum = add_horizontal_16(dst - BPS);
  int left = 0;
  int j;
  for (j = 0; j < 16; ++j) {
    left += dst[-1 + j * BPS];
  }
  {
    const int DC = sum + left + 16;
    Put16(DC >> 5, dst);
  }
}

static void DC16NoTop(uint8_t* dst) {  // DC with top samples not available
  int DC = 8;
  int j;
  for (j = 0; j < 16; ++j) {
    DC += dst[-1 + j * BPS];
  }
  Put16(DC >> 4, dst);
}

static void DC16NoLeft(uint8_t* dst) {  // DC with left samples not available
  const int DC = 8 + add_horizontal_16(dst - BPS);
  Put16(DC >> 4, dst);
}

static void DC16NoTopLeft(uint8_t* dst) {  // DC with no top and left samples
  Put16(0x80, dst);
}

//------------------------------------------------------------------------------
// Chroma

static WEBP_INLINE uint32_t add_horizontal_8(uint8_t* dst) {
  const uint8x16 zero = (uint8x16){0};
  const uint8x16 a = get_8_bytes(dst);
  const uint16x8 _a_lbw = (uint16x8)__builtin_shufflevector(
      a, zero, 0, 16, 1, 16, 2, 16, 3, 16, 16, 16, 16, 16, 16, 16, 16, 16);
  const uint16x8 _a_hbw = (uint16x8)__builtin_shufflevector(
      a, zero, 4, 16, 5, 16, 6, 16, 7, 16, 16, 16, 16, 16, 16, 16, 16, 16);
  const uint16x8 sum_a = _a_lbw + _a_hbw;
  const uint16x8 sum_b =
      (uint16x8)__builtin_shufflevector(sum_a, sum_a, 2, 3, 2, 3, 2, 3, 2, 3);
  const uint16x8 sum_c = sum_a + sum_b;
  const uint16x8 sum_d =
      (uint16x8)__builtin_shufflevector(sum_c, sum_c, 1, 1, 1, 1, 1, 1, 1, 1);
  const uint16x8 sum_e = sum_c + sum_d;
  return sum_e[0] & 0xffff;
}

static void VE8uv(uint8_t* dst) {  // vertical
  const uint8x16 top = get_8_bytes(dst - BPS);
  int j;
  for (j = 0; j < 8; ++j) {
    memcpy(dst + j * BPS, &top, 8);
  }
}

static void HE8uv(uint8_t* dst) {  // horizontal
  int j;
  for (j = 8; j > 0; --j) {
    const uint8x16 values = splat_uint8(dst[-1]);
    memcpy(dst, &values, 8);
    dst += BPS;
  }
}

// helper for chroma-DC predictions
static WEBP_INLINE void Put8x8uv(uint8_t v, uint8_t* dst) {
  int j;
  const uint8x16 values = splat_uint8(v);
  for (j = 0; j < 8; ++j) {
    memcpy(dst + j * BPS, &values, 8);
  }
}

static void DC8uv(uint8_t* dst) {  // DC
  int left = 0;
  int j;
  const uint32_t sum = add_horizontal_8(dst - BPS);
  for (j = 0; j < 8; ++j) {
    left += dst[-1 + j * BPS];
  }
  {
    const int DC = sum + left + 8;
    Put8x8uv(DC >> 4, dst);
  }
}

static void DC8uvNoLeft(uint8_t* dst) {  // DC with no left samples
  const uint32_t DC = 4 + add_horizontal_8(dst - BPS);
  Put8x8uv(DC >> 3, dst);
}

static void DC8uvNoTop(uint8_t* dst) {  // DC with no top samples
  int dc0 = 4;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[-1 + i * BPS];
  }
  Put8x8uv(dc0 >> 3, dst);
}

static void DC8uvNoTopLeft(uint8_t* dst) {  // DC with nothing
  Put8x8uv(0x80, dst);
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitWASM(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitWASM(void) {
  VP8PredLuma4[2] = VE4;
  VP8PredLuma4[4] = RD4;

  VP8PredLuma16[0] = DC16;
  VP8PredLuma16[2] = VE16;
  VP8PredLuma16[3] = HE16;
  VP8PredLuma16[4] = DC16NoTop;
  VP8PredLuma16[5] = DC16NoLeft;
  VP8PredLuma16[6] = DC16NoTopLeft;

  VP8PredChroma8[0] = DC8uv;
  VP8PredChroma8[2] = VE8uv;
  VP8PredChroma8[3] = HE8uv;
  VP8PredChroma8[4] = DC8uvNoTop;
  VP8PredChroma8[5] = DC8uvNoLeft;
  VP8PredChroma8[6] = DC8uvNoTopLeft;
}

#else  // !WEBP_USE_WASM

WEBP_DSP_INIT_STUB(VP8DspInitWASM)

#endif  // WEBP_USE_WASM
