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

static WEBP_INLINE int16x8 _unpacklo_epi8(const int8x16 a, const int8x16 b) {
  return __builtin_shufflevector(a, b, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21,
                                 6, 22, 7, 23);
}

static WEBP_INLINE int32x4 _unpacklo_epi16(const int16x8 a, const int16x8 b) {
  return __builtin_shufflevector(a, b, 0, 8, 1, 9, 2, 10, 3, 11);
}

static WEBP_INLINE int32x4 _unpackhi_epi16(const int16x8 a, const int16x8 b) {
  return __builtin_shufflevector(a, b, 4, 12, 5, 13, 6, 14, 7, 15);
}

static WEBP_INLINE int32x4 _unpacklo_epi32(const int32x4 a, const int32x4 b) {
  return __builtin_shufflevector(a, b, 0, 4, 1, 5);
}

static WEBP_INLINE int32x4 _unpackhi_epi32(const int32x4 a, const int32x4 b) {
  return __builtin_shufflevector(a, b, 2, 6, 3, 7);
}

static WEBP_INLINE int32x4 _unpacklo_epi64(const int32x4 a, const int32x4 b) {
  return __builtin_shufflevector(a, b, 0, 1, 4, 5);
}

static WEBP_INLINE int32x4 _unpackhi_epi64(const int32x4 a, const int32x4 b) {
  return __builtin_shufflevector(a, b, 2, 3, 6, 7);
}

static WEBP_INLINE int16x8 _mulhi_int16x8(int16x8 in, int32x4 k) {
  const int16x8 zero = (int16x8){0, 0, 0, 0, 0, 0, 0, 0};
  const int32x4 sixteen = (int32x4){16, 16, 16, 16};
  // Put in upper 16 bits so we can preserve the sign
  const int32x4 in_lo =
      (int32x4)__builtin_shufflevector(in, zero, 8, 0, 8, 1, 8, 2, 8, 3);
  const int32x4 in_hi =
      (int32x4)__builtin_shufflevector(in, zero, 8, 4, 8, 5, 8, 6, 8, 7);
  const int32x4 _lo = (in_lo >> sixteen) * k;
  const int32x4 _hi = (in_hi >> sixteen) * k;
  // only keep the upper 16 bits
  const int16x8 res = (int16x8)__builtin_shufflevector(
      (int16x8)_lo, (int16x8)_hi, 1, 3, 5, 7, 9, 11, 13, 15);
  return res;
}

static WEBP_INLINE uint8x16 int16x8_to_uint8x16_sat(int16x8 x) {
  const uint8x16 k00ff00ff =
      (uint8x16){-1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0};
  const int16x8 fifteen = (int16x8){15, 15, 15, 15, 15, 15, 15, 15};
  const int16x8 a = (uint16x8)x > (uint16x8)k00ff00ff;
  const int16x8 b = x & ~a;
  const int16x8 c = (x & a) >> fifteen;
  const int16x8 d = ~c & a;
  const int16x8 e = b | d;
  const uint8x16 final = (uint8x16)__builtin_shufflevector(
      (int8x16)e, (int8x16)e, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26,
      28, 30);
  return final;
}
//------------------------------------------------------------------------------
// Transforms (Paragraph 14.4)

// Transpose two 4x4 16b matrices horizontally stored in registers.
static WEBP_INLINE void VP8Transpose_2_4x4_16b(
    const int16x8* const in0, const int16x8* const in1,
    const int16x8* const in2, const int16x8* const in3, int16x8* const out0,
    int16x8* const out1, int16x8* const out2, int16x8* const out3) {
  // Transpose the two 4x4.
  // a00 a01 a02 a03   b00 b01 b02 b03
  // a10 a11 a12 a13   b10 b11 b12 b13
  // a20 a21 a22 a23   b20 b21 b22 b23
  // a30 a31 a32 a33   b30 b31 b32 b33
  const int32x4 transpose0_0 = _unpacklo_epi16(*in0, *in1);
  const int32x4 transpose0_1 = _unpacklo_epi16(*in2, *in3);
  const int32x4 transpose0_2 = _unpackhi_epi16(*in0, *in1);
  const int32x4 transpose0_3 = _unpackhi_epi16(*in2, *in3);
  // a00 a10 a01 a11   a02 a12 a03 a13
  // a20 a30 a21 a31   a22 a32 a23 a33
  // b00 b10 b01 b11   b02 b12 b03 b13
  // b20 b30 b21 b31   b22 b32 b23 b33
  const int32x4 transpose1_0 = _unpacklo_epi32(transpose0_0, transpose0_1);
  const int32x4 transpose1_1 = _unpacklo_epi32(transpose0_2, transpose0_3);
  const int32x4 transpose1_2 = _unpackhi_epi32(transpose0_0, transpose0_1);
  const int32x4 transpose1_3 = _unpackhi_epi32(transpose0_2, transpose0_3);
  // a00 a10 a20 a30 a01 a11 a21 a31
  // b00 b10 b20 b30 b01 b11 b21 b31
  // a02 a12 a22 a32 a03 a13 a23 a33
  // b02 b12 a22 b32 b03 b13 b23 b33
  *out0 = _unpacklo_epi64(transpose1_0, transpose1_1);
  *out1 = _unpackhi_epi64(transpose1_0, transpose1_1);
  *out2 = _unpacklo_epi64(transpose1_2, transpose1_3);
  *out3 = _unpackhi_epi64(transpose1_2, transpose1_3);
  // a00 a10 a20 a30   b00 b10 b20 b30
  // a01 a11 a21 a31   b01 b11 b21 b31
  // a02 a12 a22 a32   b02 b12 b22 b32
  // a03 a13 a23 a33   b03 b13 b23 b33
}

static void Transform(const int16_t* in, uint8_t* dst, int do_two) {
  const int32x4 k1 = {20091, 20091, 20091, 20091};
  const int32x4 k2 = {35468, 35468, 35468, 35468};
  int16x8 T0, T1, T2, T3;

  // Load and concatenate the transform coefficients (we'll do two transforms
  // in parallel). In the case of only one transform, the second half of the
  // vectors will just contain random value we'll never use nor store.
  int16x8 in0, in1, in2, in3;
  {
    in0 = get_8_bytes((uint8_t*)&in[0]);
    in1 = get_8_bytes((uint8_t*)&in[4]);
    in2 = get_8_bytes((uint8_t*)&in[8]);
    in3 = get_8_bytes((uint8_t*)&in[12]);
    // a00 a10 a20 a30   x x x x
    // a01 a11 a21 a31   x x x x
    // a02 a12 a22 a32   x x x x
    // a03 a13 a23 a33   x x x x
    if (do_two) {
      const int16x8 inB0 = get_8_bytes((uint8_t*)&in[16]);
      const int16x8 inB1 = get_8_bytes((uint8_t*)&in[20]);
      const int16x8 inB2 = get_8_bytes((uint8_t*)&in[24]);
      const int16x8 inB3 = get_8_bytes((uint8_t*)&in[28]);
      in0 = _unpacklo_epi64(in0, inB0);
      in1 = _unpacklo_epi64(in1, inB1);
      in2 = _unpacklo_epi64(in2, inB2);
      in3 = _unpacklo_epi64(in3, inB3);
      // a00 a10 a20 a30   b00 b10 b20 b30
      // a01 a11 a21 a31   b01 b11 b21 b31
      // a02 a12 a22 a32   b02 b12 b22 b32
      // a03 a13 a23 a33   b03 b13 b23 b33
    }
  }

  // Vertical pass and subsequent transpose.
  {
    const int16x8 a = in0 + in2;
    const int16x8 b = in0 - in2;
    const int16x8 c1 = _mulhi_int16x8(in1, k2);
    const int16x8 c2 = _mulhi_int16x8(in3, k1) + in3;
    const int16x8 c = c1 - c2;
    const int16x8 d1 = _mulhi_int16x8(in1, k1) + in1;
    const int16x8 d2 = _mulhi_int16x8(in3, k2);
    const int16x8 d = d1 + d2;

    // Second pass.
    const int16x8 tmp0 = a + d;
    const int16x8 tmp1 = b + c;
    const int16x8 tmp2 = b - c;
    const int16x8 tmp3 = a - d;

    // Transpose the two 4x4.
    VP8Transpose_2_4x4_16b(&tmp0, &tmp1, &tmp2, &tmp3, &T0, &T1, &T2, &T3);
  }

  // Horizontal pass and subsequent transpose.
  {
    const int16x8 four = {4, 4, 4, 4, 4, 4, 4, 4};
    const int16x8 dc = T0 + four;
    const int16x8 a = dc + T2;
    const int16x8 b = dc - T2;
    const int16x8 c1 = _mulhi_int16x8(T1, k2);
    const int16x8 c2 = _mulhi_int16x8(T3, k1) + T3;
    const int16x8 c = c1 - c2;
    const int16x8 d1 = _mulhi_int16x8(T1, k1) + T1;
    const int16x8 d2 = _mulhi_int16x8(T3, k2);
    const int16x8 d = d1 + d2;

    // Second pass.
    const int16x8 tmp0 = a + d;
    const int16x8 tmp1 = b + c;
    const int16x8 tmp2 = b - c;
    const int16x8 tmp3 = a - d;
    const int16x8 three = {3, 3, 3, 3, 3, 3, 3, 3};
    const int16x8 shifted0 = tmp0 >> three;
    const int16x8 shifted1 = tmp1 >> three;
    const int16x8 shifted2 = tmp2 >> three;
    const int16x8 shifted3 = tmp3 >> three;

    // Transpose the two 4x4.
    VP8Transpose_2_4x4_16b(&shifted0, &shifted1, &shifted2, &shifted3, &T0, &T1,
                           &T2, &T3);
  }

  // Add inverse transform to 'dst' and store.
  {
    const int8x16 zero = {0};
    // Load the reference(s).
    int16x8 dst0, dst1, dst2, dst3;
    if (do_two) {
      // Load eight bytes/pixels per line.
      dst0 = get_8_bytes((uint8_t*)(dst + 0 * BPS));
      dst1 = get_8_bytes((uint8_t*)(dst + 1 * BPS));
      dst2 = get_8_bytes((uint8_t*)(dst + 2 * BPS));
      dst3 = get_8_bytes((uint8_t*)(dst + 3 * BPS));
    } else {
      // Load four bytes/pixels per line.
      memcpy(&dst0, (dst + 0 * BPS), 4);
      memcpy(&dst1, (dst + 1 * BPS), 4);
      memcpy(&dst2, (dst + 2 * BPS), 4);
      memcpy(&dst3, (dst + 3 * BPS), 4);
    }
    // Convert to 16b.
    dst0 = _unpacklo_epi8(dst0, zero);
    dst1 = _unpacklo_epi8(dst1, zero);
    dst2 = _unpacklo_epi8(dst2, zero);
    dst3 = _unpacklo_epi8(dst3, zero);
    // Add the inverse transform(s).
    dst0 = dst0 + T0;
    dst1 = dst1 + T1;
    dst2 = dst2 + T2;
    dst3 = dst3 + T3;
    // Unsigned saturate to 8b.
    dst0 = int16x8_to_uint8x16_sat(dst0);
    dst1 = int16x8_to_uint8x16_sat(dst1);
    dst2 = int16x8_to_uint8x16_sat(dst2);
    dst3 = int16x8_to_uint8x16_sat(dst3);
    // Store the results.
    if (do_two) {
      // Store eight bytes/pixels per line.
      // TODO: use lanes instead ???
      memcpy(dst + 0 * BPS, &dst0, 8);
      memcpy(dst + 1 * BPS, &dst1, 8);
      memcpy(dst + 2 * BPS, &dst2, 8);
      memcpy(dst + 3 * BPS, &dst3, 8);
    } else {
      // Store four bytes/pixels per line.
      memcpy(dst + 0 * BPS, &dst0, 4);
      memcpy(dst + 1 * BPS, &dst1, 4);
      memcpy(dst + 2 * BPS, &dst2, 4);
      memcpy(dst + 3 * BPS, &dst3, 4);
    }
  }
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
  VP8Transform = Transform;

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
