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

static WEBP_INLINE uint8x16 get_16_bytes(const uint8_t* src) {
  uint8x16 a;
  memcpy(&a, src, 16);
  return a;
}

static WEBP_INLINE uint8x16 get_8_bytes(const uint8_t* src) {
  uint8x16 a;
  memcpy(&a, src, 8);
  return a;
}

static WEBP_INLINE uint8x16 get_4_bytes(const uint8_t* src) {
  uint8x16 a;
  memcpy(&a, src, 4);
  return a;
}

static WEBP_INLINE int16x8 splat_int16(int val) {
  int16x8 a;
  a[0] = val;
  a = (int16x8)__builtin_shufflevector(a, a, 0, 0, 0, 0, 0, 0, 0, 0);
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

static WEBP_INLINE int16x8 _unpackhi_epi8(const int8x16 a, const int8x16 b) {
  return __builtin_shufflevector(a, b, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13,
                                 29, 14, 30, 15, 31);
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

/*
  Replace _mulhi_int16x8() with builtins. For performance testing.
*/

#if defined(__i386__) || defined(__x86_64__)
//  #define ENABLE_X86_BUILTIN_MULHI_INT16X8
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
//  #define ENABLE_NEON_BUILTIN_MULHI_INT16X8
#endif

static WEBP_INLINE int16x8 _mulhi_int16x8(const int16x8 in, const int32x4 k) {
#if defined(ENABLE_X86_BUILTIN_MULHI_INT16X8)
  const int16x8 k_16bit = splat_int16(k[0]);
  return (int16x8)__builtin_ia32_pmulhw128(in, k_16bit);
#elif defined(ENABLE_NEON_BUILTIN_MULHI_INT16X8)
  const int16x8 k_16bit = splat_int16(k[0]);
  const int16x8 one = (int16x8){1, 1, 1, 1, 1, 1, 1, 1};
  return ((int16x8)__builtin_neon_vqdmulhq_v((int8x16)in, (int8x16)k_16bit,
                                             33)) >> one;
#else
  const int16x8 zero = (int16x8){0, 0, 0, 0, 0, 0, 0, 0};
  const int32x4 sixteen = (int32x4){16, 16, 16, 16};
  // Put in upper 16 bits so we can preserve the sign
  const int32x4 in_lo = (int32x4)_unpacklo_epi16(zero, in);
  const int32x4 in_hi = (int32x4)_unpackhi_epi16(zero, in);
  const int32x4 _lo = (in_lo >> sixteen) * k;
  const int32x4 _hi = (in_hi >> sixteen) * k;
  // only keep the upper 16 bits
  const int16x8 res = (int16x8)__builtin_shufflevector(
      (int16x8)_lo, (int16x8)_hi, 1, 3, 5, 7, 9, 11, 13, 15);
  return res;
#endif
}

static WEBP_INLINE uint8x16 int16x8_to_uint8x16_sat(const int16x8 x) {
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

// int16 to int8 with saturation.
static WEBP_INLINE int8x16 int16x8_to_int8x16_sat(const int16x8 x) {
  const int16x8 k7f = splat_int16(0x007f);
  const int16x8 kff80 = splat_int16(0xff80);
  const int16x8 s1 = (x < k7f);
  const int16x8 a = (s1 & x) | (~s1 & k7f);
  const int16x8 s2 = (a > kff80);
  const int16x8 a2 = (s2 & a) | (~s2 & kff80);
  const int8x16 final = (int8x16)__builtin_shufflevector(
      (int8x16)a2, (int8x16)a2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24,
      26, 28, 30);
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
#if defined(ENABLE_X86_BUILTIN_MULHI_INT16X8)
  // This implementation makes use of 16-bit fixed point versions of two
  // multiply constants:
  //    K1 = sqrt(2) * cos (pi/8) ~= 85627 / 2^16
  //    K2 = sqrt(2) * sin (pi/8) ~= 35468 / 2^16
  //
  // To be able to use signed 16-bit integers, we use the following trick to
  // have constants within range:
  // - Associated constants are obtained by subtracting the 16-bit fixed point
  //   version of one:
  //      k = K - (1 << 16)  =>  K = k + (1 << 16)
  //      K1 = 85267  =>  k1 =  20091
  //      K2 = 35468  =>  k2 = -30068
  // - The multiplication of a variable by a constant become the sum of the
  //   variable and the multiplication of that variable by the associated
  //   constant:
  //      (x * K) >> 16 = (x * (k + (1 << 16))) >> 16 = ((x * k ) >> 16) + x
  const int32x4 k1 = {20091, 20091, 20091, 20091};
  const int32x4 k2 = {-30068, -30068, -30068, -30068};
#else
  const int32x4 k1 = {20091, 20091, 20091, 20091};
  const int32x4 k2 = {35468, 35468, 35468, 35468};
#endif
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

#if defined(ENABLE_X86_BUILTIN_MULHI_INT16X8) || \
    defined(ENABLE_NEON_BUILTIN_MULHI_INT16X8)
    // c = MUL(in1, K2) - MUL(in3, K1) = MUL(in1, k2) - MUL(in3, k1) + in1 - in3
    const int16x8 c1 = _mulhi_int16x8(in1, k2);
    const int16x8 c2 = _mulhi_int16x8(in3, k1);
    const int16x8 c3 = in1 - in3;
    const int16x8 c4 = c1 - c2;
    const int16x8 c = c3 + c4;
    // d = MUL(in1, K1) + MUL(in3, K2) = MUL(in1, k1) + MUL(in3, k2) + in1 + in3
    const int16x8 d1 = _mulhi_int16x8(in1, k1);
    const int16x8 d2 = _mulhi_int16x8(in3, k2);
    const int16x8 d3 = in1 + in3;
    const int16x8 d4 = d1 + d2;
    const int16x8 d = d3 + d4;
#else
    const int16x8 c1 = _mulhi_int16x8(in1, k2);
    const int16x8 c2 = _mulhi_int16x8(in3, k1) + in3;
    const int16x8 c = c1 - c2;
    const int16x8 d1 = _mulhi_int16x8(in1, k1) + in1;
    const int16x8 d2 = _mulhi_int16x8(in3, k2);
    const int16x8 d = d1 + d2;
#endif

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
#if defined(ENABLE_X86_BUILTIN_MULHI_INT16X8) || \
    defined(ENABLE_NEON_BUILTIN_MULHI_INT16X8)
    // c = MUL(T1, K2) - MUL(T3, K1) = MUL(T1, k2) - MUL(T3, k1) + T1 - T3
    const int16x8 c1 = _mulhi_int16x8(T1, k2);
    const int16x8 c2 = _mulhi_int16x8(T3, k1);
    const int16x8 c3 = T1 - T3;
    const int16x8 c4 = c1 - c2;
    const int16x8 c = c3 + c4;
    // d = MUL(T1, K1) + MUL(T3, K2) = MUL(T1, k1) + MUL(T3, k2) + T1 + T3
    const int16x8 d1 = _mulhi_int16x8(T1, k1);
    const int16x8 d2 = _mulhi_int16x8(T3, k2);
    const int16x8 d3 = T1 + T3;
    const int16x8 d4 = d1 + d2;
    const int16x8 d = d3 + d4;
#else
    const int16x8 c1 = _mulhi_int16x8(T1, k2);
    const int16x8 c2 = _mulhi_int16x8(T3, k1) + T3;
    const int16x8 c = c1 - c2;
    const int16x8 d1 = _mulhi_int16x8(T1, k1) + T1;
    const int16x8 d2 = _mulhi_int16x8(T3, k2);
    const int16x8 d = d1 + d2;
#endif

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
// Loop Filter (Paragraph 15)

/*
  Currently, the add/sub sat instructions are not supported, however in the
  future, they will be.  So for now, we will cheat and use the builtins.

  See https://github.com/WebAssembly/meetings/blob/master/2017/CG-05.md
    Poll: Adopt the saturating integer arithmetic operations
    {i8x16,i16x8}.{add,sub}_saturate_[su].
*/

#if defined(__i386__) || defined(__x86_64__)
#define ENABLE_X86_BUILTIN_ADDSUB_SAT
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
#define ENABLE_NEON_BUILTIN_ADDSUB_SAT
#endif

static WEBP_INLINE uint8x16 uint8x16_add_sat(const uint8x16 a,
                                             const uint8x16 b) {
#if defined(ENABLE_X86_BUILTIN_ADDSUB_SAT)
  return (uint8x16)__builtin_ia32_paddusb128(a, b);
#elif defined(ENABLE_NEON_BUILTIN_ADDSUB_SAT)
  return (uint8x16)__builtin_neon_vqaddq_v(a, b, 48);
#else
  // Generic implementation for non-x86
  const uint8x16 zero = splat_uint8(0);
  const uint16x8 a_lo = _unpacklo_epi8(a, zero);
  const uint16x8 a_hi = _unpackhi_epi8(a, zero);
  const uint16x8 b_lo = _unpacklo_epi8(b, zero);
  const uint16x8 b_hi = _unpackhi_epi8(b, zero);
  const uint16x8 sum_lo = a_lo + b_lo;
  const uint16x8 sum_hi = a_hi + b_hi;
  const uint8x16 usat_lo = int16x8_to_uint8x16_sat(sum_lo);
  const uint8x16 usat_hi = int16x8_to_uint8x16_sat(sum_hi);
  return _unpacklo_epi64(usat_lo, usat_hi);
#endif
}

static WEBP_INLINE int8x16 int8x16_add_sat(const int8x16 a, const int8x16 b) {
#if defined(ENABLE_X86_BUILTIN_ADDSUB_SAT)
  return (int8x16)__builtin_ia32_paddsb128(a, b);
#elif defined(ENABLE_NEON_BUILTIN_ADDSUB_SAT)
  return (int8x16)__builtin_neon_vqaddq_v(a, b, 32);
#else
  // Generic implementation for non-x86
  const int8x16 zero = splat_uint8(0);
  const int16x8 eight = splat_int16(8);
  const int16x8 a_lo = _unpacklo_epi8(zero, a) >> eight;
  const int16x8 a_hi = _unpackhi_epi8(zero, a) >> eight;
  const int16x8 b_lo = _unpacklo_epi8(zero, b) >> eight;
  const int16x8 b_hi = _unpackhi_epi8(zero, b) >> eight;
  const int16x8 sum_lo = a_lo + b_lo;
  const int16x8 sum_hi = a_hi + b_hi;
  const int8x16 sat_lo = int16x8_to_int8x16_sat(sum_lo);
  const int8x16 sat_hi = int16x8_to_int8x16_sat(sum_hi);
  return _unpacklo_epi64(sat_lo, sat_hi);
#endif
}

static WEBP_INLINE uint8x16 uint8x16_sub_sat(const uint8x16 a,
                                             const uint8x16 b) {
#if defined(ENABLE_X86_BUILTIN_ADDSUB_SAT)
  return (uint8x16)__builtin_ia32_psubusb128(a, b);
#elif defined(ENABLE_NEON_BUILTIN_ADDSUB_SAT)
  return (int8x16)__builtin_neon_vqsubq_v(a, b, 48);
#else
  // Generic implementation for non-x86
  const uint8x16 zero = splat_uint8(0);
  const uint16x8 a_lo = _unpacklo_epi8(a, zero);
  const uint16x8 a_hi = _unpackhi_epi8(a, zero);
  const uint16x8 b_lo = _unpacklo_epi8(b, zero);
  const uint16x8 b_hi = _unpackhi_epi8(b, zero);
  const uint16x8 diff_lo = a_lo - b_lo;
  const uint16x8 diff_hi = a_hi - b_hi;
  const uint8x16 usat_lo = int16x8_to_uint8x16_sat(diff_lo);
  const uint8x16 usat_hi = int16x8_to_uint8x16_sat(diff_hi);
  return _unpacklo_epi64(usat_lo, usat_hi);
#endif
}

static WEBP_INLINE int8x16 int8x16_sub_sat(const int8x16 a, const int8x16 b) {
#if defined(ENABLE_X86_BUILTIN_ADDSUB_SAT)
  return (int8x16)__builtin_ia32_psubsb128(a, b);
#elif defined(ENABLE_NEON_BUILTIN_ADDSUB_SAT)
  return (int8x16)__builtin_neon_vqsubq_v(a, b, 32);
#else
  // Generic implementation for non-x86
  const int8x16 zero = splat_uint8(0);
  const int16x8 eight = splat_int16(8);
  const int16x8 a_lo = _unpacklo_epi8(zero, a) >> eight;
  const int16x8 a_hi = _unpackhi_epi8(zero, a) >> eight;
  const int16x8 b_lo = _unpacklo_epi8(zero, b) >> eight;
  const int16x8 b_hi = _unpackhi_epi8(zero, b) >> eight;
  const int16x8 diff_lo = a_lo - b_lo;
  const int16x8 diff_hi = a_hi - b_hi;
  const int8x16 sat_lo = int16x8_to_int8x16_sat(diff_lo);
  const int8x16 sat_hi = int16x8_to_int8x16_sat(diff_hi);
  return _unpacklo_epi64(sat_lo, sat_hi);
#endif
}

static WEBP_INLINE uint8x16 _max_u8x16(const uint8x16 a, const uint8x16 b) {
  const uint8x16 s1 = (a > b);
  return (s1 & a) | (~s1 & b);
}

// Compute abs(p - q) = subs(p - q) OR subs(q - p)
static WEBP_INLINE int8x16 abs_diff(int8x16 p, int8x16 q) {
  const int8x16 a = uint8x16_sub_sat(p, q);
  const int8x16 b = uint8x16_sub_sat(q, p);
  return a | b;
}

// int16 to int8 with saturation.
static WEBP_INLINE int8x16 _pack_epi16_to_epi8(const int16x8 lo,
                                               const int16x8 hi) {
  const int8x16 sat_lo = int16x8_to_int8x16_sat(lo);
  const int8x16 sat_hi = int16x8_to_int8x16_sat(hi);
  return _unpacklo_epi64(sat_lo, sat_hi);
}

// Shift each byte of "x" by 3 bits while preserving by the sign bit.
static WEBP_INLINE void SignedShift8b(int8x16* const x) {
  const int8x16 zero = {0};
  const int16x8 eleven = splat_int16(3 + 8);
  const int16x8 lo_0 = _unpacklo_epi8(zero, *x);
  const int16x8 hi_0 = _unpackhi_epi8(zero, *x);
  const int16x8 lo_1 = lo_0 >> eleven;
  const int16x8 hi_1 = hi_0 >> eleven;
  *x = _pack_epi16_to_epi8(lo_1, hi_1);
}

#define FLIP_SIGN_BIT2(a, b) \
  {                          \
    a = a ^ sign_bit;        \
    b = b ^ sign_bit;        \
  }

#define FLIP_SIGN_BIT4(a, b, c, d) \
  {                                \
    FLIP_SIGN_BIT2(a, b);          \
    FLIP_SIGN_BIT2(c, d);          \
  }

// input/output is uint8_t
static WEBP_INLINE void GetNotHEV(const int8x16* const p1,
                                  const int8x16* const p0,
                                  const int8x16* const q0,
                                  const int8x16* const q1, int hev_thresh,
                                  int8x16* const not_hev) {
  const int8x16 zero = {0};
  const int8x16 t_1 = abs_diff(*p1, *p0);
  const int8x16 t_2 = abs_diff(*q1, *q0);
  const int8x16 h = splat_uint8(hev_thresh);
  const int8x16 t_max = _max_u8x16(t_1, t_2);
  const int8x16 t_max_h = uint8x16_sub_sat(t_max, h);
  *not_hev = (t_max_h == zero);  // not_hev <= t1 && not_hev <= t2
}

// input pixels are int8_t
static WEBP_INLINE void GetBaseDelta(const int8x16* const p1,
                                     const int8x16* const p0,
                                     const int8x16* const q0,
                                     const int8x16* const q1,
                                     int8x16* const delta) {
  // beware of addition order, for saturation!
  const int8x16 p1_q1 = int8x16_sub_sat(*p1, *q1);   // p1 - q1
  const int8x16 q0_p0 = int8x16_sub_sat(*q0, *p0);   // q0 - p0
  const int8x16 s1 = int8x16_add_sat(p1_q1, q0_p0);  // p1 - q1 + 1 * (q0 - p0)
  const int8x16 s2 = int8x16_add_sat(q0_p0, s1);     // p1 - q1 + 2 * (q0 - p0)
  const int8x16 s3 = int8x16_add_sat(q0_p0, s2);     // p1 - q1 + 3 * (q0 - p0)
  *delta = s3;
}

// input and output are int8_t
static WEBP_INLINE void DoSimpleFilter(int8x16* const p0, int8x16* const q0,
                                       const int8x16* const fl) {
  const int8x16 k3 = splat_uint8(3);
  const int8x16 k4 = splat_uint8(4);
  int8x16 v3 = int8x16_add_sat(*fl, k3);
  int8x16 v4 = int8x16_add_sat(*fl, k4);
  SignedShift8b(&v4);              // v4 >> 3
  SignedShift8b(&v3);              // v3 >> 3
  *q0 = int8x16_sub_sat(*q0, v4);  // q0 -= v4
  *p0 = int8x16_add_sat(*p0, v3);  // p0 += v3
}

// Updates values of 2 pixels at MB edge during complex filtering.
// Update operations:
// q = q - delta and p = p + delta; where delta = [(a_hi >> 7), (a_lo >> 7)]
// Pixels 'pi' and 'qi' are int8_t on input, uint8_t on output (sign flip).
static WEBP_INLINE void Update2Pixels(int8x16* const pi, int8x16* const qi,
                                      const int16x8* const a0_lo,
                                      const int16x8* const a0_hi) {
  const int16x8 _7 = splat_int16(7);
  const int16x8 a1_lo = *a0_lo >> _7;
  const int16x8 a1_hi = *a0_hi >> _7;
  const int8x16 delta = _pack_epi16_to_epi8(a1_lo, a1_hi);
  const int8x16 sign_bit = (int8x16)splat_uint8(0x80);
  *pi = int8x16_add_sat(*pi, delta);
  *qi = int8x16_sub_sat(*qi, delta);
  FLIP_SIGN_BIT2(*pi, *qi);
}

// input pixels are uint8_t
static WEBP_INLINE void NeedsFilter(const int8x16* const p1,
                                    const int8x16* const p0,
                                    const int8x16* const q0,
                                    const int8x16* const q1, int thresh,
                                    int8x16* const mask) {
  const int8x16 zero = {0};
  const int16x8 one = {1, 1, 1, 1, 1, 1, 1, 1};
  const int8x16 m_thresh = splat_uint8(thresh);
  const int8x16 t1 = abs_diff(*p1, *q1);  // abs(p1 - q1)
  const uint8x16 kFE = splat_uint8(0xFE);
  const uint16x8 t2 = t1 & kFE;                 // set lsb of each byte to zero
  const uint16x8 t3 = t2 >> one;                // abs(p1 - q1) / 2
  const int8x16 t4 = abs_diff(*p0, *q0);        // abs(p0 - q0)
  const int8x16 t5 = uint8x16_add_sat(t4, t4);  // abs(p0 - q0) * 2
  const int8x16 t6 = uint8x16_add_sat(t5, t3);  // abs(p0-q0)*2 + abs(p1-q1)/2
  const int8x16 t7 = uint8x16_sub_sat(t6, m_thresh);  // mask <= m_thresh
  *mask = (t7 == zero);
}

//------------------------------------------------------------------------------
// Edge filtering functions

// Applies filter on 2 pixels (p0 and q0)
static WEBP_INLINE void DoFilter2(int8x16* const p1, int8x16* const p0,
                                  int8x16* const q0, int8x16* const q1,
                                  int thresh) {
  int8x16 a, mask;
  const int8x16 sign_bit = splat_uint8(0x80);
  // convert p1/q1 to int8_t (for GetBaseDelta)
  const int8x16 p1s = *p1 ^ sign_bit;
  const int8x16 q1s = *q1 ^ sign_bit;

  NeedsFilter(p1, p0, q0, q1, thresh, &mask);

  FLIP_SIGN_BIT2(*p0, *q0);
  GetBaseDelta(&p1s, p0, q0, &q1s, &a);
  a = a & mask;  // mask filter values we don't care about
  DoSimpleFilter(p0, q0, &a);
  FLIP_SIGN_BIT2(*p0, *q0);
}

// Applies filter on 4 pixels (p1, p0, q0 and q1)
static WEBP_INLINE void DoFilter4(int8x16* const p1, int8x16* const p0,
                                  int8x16* const q0, int8x16* const q1,
                                  const int8x16* const mask, int hev_thresh) {
  const uint8x16 zero = {0};
  const uint8x16 sign_bit = splat_uint8(0x80);
  const uint8x16 k64 = splat_uint8(64);
  const uint8x16 k3 = splat_uint8(3);
  const uint8x16 k4 = splat_uint8(4);
  int8x16 not_hev;
  int8x16 t1, t2, t3;

  // compute hev mask
  GetNotHEV(p1, p0, q0, q1, hev_thresh, &not_hev);

  // convert to signed values
  FLIP_SIGN_BIT4(*p1, *p0, *q0, *q1);

  t1 = int8x16_sub_sat(*p1, *q1);  // p1 - q1
  t1 = ~not_hev & t1;              // hev(p1 - q1)
  t2 = int8x16_sub_sat(*q0, *p0);  // q0 - p0
  t1 = int8x16_add_sat(t1, t2);    // hev(p1 - q1) + 1 * (q0 - p0)
  t1 = int8x16_add_sat(t1, t2);    // hev(p1 - q1) + 2 * (q0 - p0)
  t1 = int8x16_add_sat(t1, t2);    // hev(p1 - q1) + 3 * (q0 - p0)
  t1 = t1 & *mask;                 // mask filter values we don't care about

  t2 = int8x16_add_sat(t1, k3);    // 3 * (q0 - p0) + hev(p1 - q1) + 3
  t3 = int8x16_add_sat(t1, k4);    // 3 * (q0 - p0) + hev(p1 - q1) + 4
  SignedShift8b(&t2);              // (3 * (q0 - p0) + hev(p1 - q1) + 3) >> 3
  SignedShift8b(&t3);              // (3 * (q0 - p0) + hev(p1 - q1) + 4) >> 3
  *p0 = int8x16_add_sat(*p0, t2);  // p0 += t2
  *q0 = int8x16_sub_sat(*q0, t3);  // q0 -= t3
  FLIP_SIGN_BIT2(*p0, *q0);

  // this is equivalent to signed (a + 1) >> 1 calculation
  t2 = t3 + sign_bit;
#if 0
  t3 = _mm_avg_epu8(t2, zero);
#else
  // This code will be eliminated if the above avg instruction is supported.
  {
    const int16x8 one = {1, 1, 1, 1, 1, 1, 1, 1};
    const int16x8 t2_lo = (int16x8)_unpacklo_epi8(t2, zero);
    const int16x8 t2_hi = (int16x8)_unpackhi_epi8(t2, zero);
    const int16x8 a = (t2_lo + one) >> one;
    const int16x8 b = (t2_hi + one) >> one;
    t3 = __builtin_shufflevector((int8x16)a, (int8x16)b, 0, 2, 4, 6, 8, 10, 12,
                                 14, 16, 18, 20, 22, 24, 26, 28, 30);
  }
#endif
  t3 = t3 - k64;

  t3 = not_hev & t3;               // if !hev
  *q1 = int8x16_sub_sat(*q1, t3);  // q1 -= t3
  *p1 = int8x16_add_sat(*p1, t3);  // p1 += t3
  FLIP_SIGN_BIT2(*p1, *q1);
}

// Applies filter on 6 pixels (p2, p1, p0, q0, q1 and q2)
static WEBP_INLINE void DoFilter6(int8x16* const p2, int8x16* const p1,
                                  int8x16* const p0, int8x16* const q0,
                                  int8x16* const q1, int8x16* const q2,
                                  const int8x16* const mask, int hev_thresh) {
  const int8x16 zero = {0};
  const int8x16 sign_bit = splat_uint8(0x80);
  int8x16 a, not_hev;

  // compute hev mask
  GetNotHEV(p1, p0, q0, q1, hev_thresh, &not_hev);

  FLIP_SIGN_BIT4(*p1, *p0, *q0, *q1);
  FLIP_SIGN_BIT2(*p2, *q2);
  GetBaseDelta(p1, p0, q0, q1, &a);

  {  // do simple filter on pixels with hev
    const int8x16 m = (~not_hev) & *mask;
    const int8x16 f = a & m;
    DoSimpleFilter(p0, q0, &f);
  }

  {  // do strong filter on pixels with not hev
    const int32x4 k9 = {0x0900, 0x0900, 0x0900, 0x0900};
    const int16x8 k63 = splat_int16(63);

    const int16x8 m = not_hev & *mask;
    const int16x8 f = a & m;
    const int16x8 f_lo = (int16x8)_unpacklo_epi8(zero, f);
    const int16x8 f_hi = (int16x8)_unpackhi_epi8(zero, f);

    const int16x8 f9_lo = _mulhi_int16x8(f_lo, k9);  // Filter (lo) * 9
    const int16x8 f9_hi = _mulhi_int16x8(f_hi, k9);  // Filter (hi) * 9

    const int16x8 a2_lo = f9_lo + k63;  // Filter * 9 + 63
    const int16x8 a2_hi = f9_hi + k63;  // Filter * 9 + 63

    const int16x8 a1_lo = a2_lo + f9_lo;  // Filter * 18 + 63
    const int16x8 a1_hi = a2_hi + f9_hi;  // Filter * 18 + 63

    const int16x8 a0_lo = a1_lo + f9_lo;  // Filter * 27 + 63
    const int16x8 a0_hi = a1_hi + f9_hi;  // Filter * 27 + 63

    Update2Pixels(p2, q2, &a2_lo, &a2_hi);
    Update2Pixels(p1, q1, &a1_lo, &a1_hi);
    Update2Pixels(p0, q0, &a0_lo, &a0_hi);
  }
}

static WEBP_INLINE uint32x4 _set_int32x4(uint32_t v3, uint32_t v2, uint32_t v1,
                                         uint32_t v0) {
  uint32x4 x;
  x[3] = v3;
  x[2] = v2;
  x[1] = v1;
  x[0] = v0;
  return x;
}

// reads 8 rows across a vertical edge.
static WEBP_INLINE void Load8x4(const uint8_t* const b, int stride,
                                int8x16* const p, int8x16* const q) {
  // A0 = 63 62 61 60 23 22 21 20 43 42 41 40 03 02 01 00
  // A1 = 73 72 71 70 33 32 31 30 53 52 51 50 13 12 11 10
  const int32x4 A0 = _set_int32x4(
      WebPMemToUint32(&b[6 * stride]), WebPMemToUint32(&b[2 * stride]),
      WebPMemToUint32(&b[4 * stride]), WebPMemToUint32(&b[0 * stride]));
  const int32x4 A1 = _set_int32x4(
      WebPMemToUint32(&b[7 * stride]), WebPMemToUint32(&b[3 * stride]),
      WebPMemToUint32(&b[5 * stride]), WebPMemToUint32(&b[1 * stride]));

  // B0 = 53 43 52 42 51 41 50 40 13 03 12 02 11 01 10 00
  // B1 = 73 63 72 62 71 61 70 60 33 23 32 22 31 21 30 20
  const int16x8 B0 = _unpacklo_epi8(A0, A1);
  const int16x8 B1 = _unpackhi_epi8(A0, A1);

  // C0 = 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00
  // C1 = 73 63 53 43 72 62 52 42 71 61 51 41 70 60 50 40
  const int32x4 C0 = _unpacklo_epi16(B0, B1);
  const int32x4 C1 = _unpackhi_epi16(B0, B1);

  // *p = 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
  // *q = 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
  *p = _unpacklo_epi32(C0, C1);
  *q = _unpackhi_epi32(C0, C1);
}

static WEBP_INLINE void Load16x4(const uint8_t* const r0,
                                 const uint8_t* const r8, int stride,
                                 int8x16* const p1, int8x16* const p0,
                                 int8x16* const q0, int8x16* const q1) {
  // Assume the pixels around the edge (|) are numbered as follows
  //                00 01 | 02 03
  //                10 11 | 12 13
  //                 ...  |  ...
  //                e0 e1 | e2 e3
  //                f0 f1 | f2 f3
  //
  // r0 is pointing to the 0th row (00)
  // r8 is pointing to the 8th row (80)

  // Load
  // p1 = 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
  // q0 = 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
  // p0 = f1 e1 d1 c1 b1 a1 91 81 f0 e0 d0 c0 b0 a0 90 80
  // q1 = f3 e3 d3 c3 b3 a3 93 83 f2 e2 d2 c2 b2 a2 92 82
  Load8x4(r0, stride, p1, q0);
  Load8x4(r8, stride, p0, q1);

  {
    // p1 = f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00
    // p0 = f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
    // q0 = f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
    // q1 = f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03
    const int8x16 t1 = *p1;
    const int8x16 t2 = *q0;
    *p1 = _unpacklo_epi64(t1, *p0);
    *p0 = _unpackhi_epi64(t1, *p0);
    *q0 = _unpacklo_epi64(t2, *q1);
    *q1 = _unpackhi_epi64(t2, *q1);
  }
}

static WEBP_INLINE void Store4x4(int8x16* const x, uint8_t* dst, int stride) {
  uint32x4 val = (uint32x4)*x;
  int i;
  for (i = 0; i < 4; ++i, dst += stride) {
    WebPUint32ToMem(dst, val[i]);
  }
}

// Transpose back and store
static WEBP_INLINE void Store16x4(const int8x16* const p1,
                                  const int8x16* const p0,
                                  const int8x16* const q0,
                                  const int8x16* const q1, uint8_t* r0,
                                  uint8_t* r8, int stride) {
  int8x16 t1, p1_s, p0_s, q0_s, q1_s;

  // p0 = 71 70 61 60 51 50 41 40 31 30 21 20 11 10 01 00
  // p1 = f1 f0 e1 e0 d1 d0 c1 c0 b1 b0 a1 a0 91 90 81 80
  t1 = *p0;
  p0_s = _unpacklo_epi8(*p1, t1);
  p1_s = _unpackhi_epi8(*p1, t1);

  // q0 = 73 72 63 62 53 52 43 42 33 32 23 22 13 12 03 02
  // q1 = f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82
  t1 = *q0;
  q0_s = _unpacklo_epi8(t1, *q1);
  q1_s = _unpackhi_epi8(t1, *q1);

  // p0 = 33 32 31 30 23 22 21 20 13 12 11 10 03 02 01 00
  // q0 = 73 72 71 70 63 62 61 60 53 52 51 50 43 42 41 40
  t1 = p0_s;
  p0_s = _unpacklo_epi16(t1, q0_s);
  q0_s = _unpackhi_epi16(t1, q0_s);

  // p1 = b3 b2 b1 b0 a3 a2 a1 a0 93 92 91 90 83 82 81 80
  // q1 = f3 f2 f1 f0 e3 e2 e1 e0 d3 d2 d1 d0 c3 c2 c1 c0
  t1 = p1_s;
  p1_s = _unpacklo_epi16(t1, q1_s);
  q1_s = _unpackhi_epi16(t1, q1_s);

  Store4x4(&p0_s, r0, stride);
  r0 += 4 * stride;
  Store4x4(&q0_s, r0, stride);

  Store4x4(&p1_s, r8, stride);
  r8 += 4 * stride;
  Store4x4(&q1_s, r8, stride);
}

//------------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

static void SimpleVFilter16(uint8_t* p, int stride, int thresh) {
  int8x16 p1, p0, q0, q1;

  // Load
  memcpy(&p1, &p[-2 * stride], 16);
  memcpy(&p0, &p[-stride], 16);
  memcpy(&q0, &p[0], 16);
  memcpy(&q1, &p[stride], 16);

  DoFilter2(&p1, &p0, &q0, &q1, thresh);

  // Store
  memcpy(&p[-stride], &p0, 16);
  memcpy(&p[0], &q0, 16);
}

static void SimpleHFilter16(uint8_t* p, int stride, int thresh) {
  int8x16 p1, p0, q0, q1;

  p -= 2;  // beginning of p1

  Load16x4(p, p + 8 * stride, stride, &p1, &p0, &q0, &q1);
  DoFilter2(&p1, &p0, &q0, &q1, thresh);
  Store16x4(&p1, &p0, &q0, &q1, p, p + 8 * stride, stride);
}

static void SimpleVFilter16i(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16(p, stride, thresh);
  }
}

static void SimpleHFilter16i(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16(p, stride, thresh);
  }
}

//------------------------------------------------------------------------------
// Complex In-loop filtering (Paragraph 15.3)
#define MAX_DIFF1(p3, p2, p1, p0, m)     \
  do {                                   \
    m = abs_diff(p1, p0);                \
    m = _max_u8x16(m, abs_diff(p3, p2)); \
    m = _max_u8x16(m, abs_diff(p2, p1)); \
  } while (0)

#define MAX_DIFF2(p3, p2, p1, p0, m)     \
  do {                                   \
    m = _max_u8x16(m, abs_diff(p1, p0)); \
    m = _max_u8x16(m, abs_diff(p3, p2)); \
    m = _max_u8x16(m, abs_diff(p2, p1)); \
  } while (0)

#define LOAD_H_EDGES4(p, stride, e1, e2, e3, e4) \
  {                                              \
    memcpy(&e1, &(p)[0 * stride], 16);           \
    memcpy(&e2, &(p)[1 * stride], 16);           \
    memcpy(&e3, &(p)[2 * stride], 16);           \
    memcpy(&e4, &(p)[3 * stride], 16);           \
  }

#define LOADUV_H_EDGE(p, u, v, stride)           \
  do {                                           \
    int8x16 U;                                   \
    int8x16 V;                                   \
    memcpy(&U, &(u)[(stride)], 8);               \
    memcpy(&V, &(v)[(stride)], 8);               \
    p = _unpacklo_epi64((int32x4)U, (int32x4)V); \
  } while (0)

#define LOADUV_H_EDGES4(u, v, stride, e1, e2, e3, e4) \
  {                                                   \
    LOADUV_H_EDGE(e1, u, v, 0 * stride);              \
    LOADUV_H_EDGE(e2, u, v, 1 * stride);              \
    LOADUV_H_EDGE(e3, u, v, 2 * stride);              \
    LOADUV_H_EDGE(e4, u, v, 3 * stride);              \
  }

#define STOREUV(p, u, v, stride)                 \
  {                                              \
    memcpy(&u[(stride)], &p, 8);                 \
    p = _unpackhi_epi64((int32x4)p, (int32x4)p); \
    memcpy(&v[(stride)], &p, 8);                 \
  }

static WEBP_INLINE void ComplexMask(const int8x16* const p1,
                                    const int8x16* const p0,
                                    const int8x16* const q0,
                                    const int8x16* const q1, int thresh,
                                    int ithresh, int8x16* const mask) {
  const int8x16 zero = {0};
  const uint8x16 it = splat_uint8(ithresh);
  const int8x16 diff = uint8x16_sub_sat(*mask, it);
  const int8x16 thresh_mask = (diff == zero);
  int8x16 filter_mask;
  NeedsFilter(p1, p0, q0, q1, thresh, &filter_mask);
  *mask = thresh_mask & filter_mask;
}

// on macroblock edges
static void VFilter16(uint8_t* p, int stride, int thresh, int ithresh,
                      int hev_thresh) {
  int8x16 t1;
  int8x16 mask;
  int8x16 p2, p1, p0, q0, q1, q2;

  // Load p3, p2, p1, p0
  LOAD_H_EDGES4(p - 4 * stride, stride, t1, p2, p1, p0);
  MAX_DIFF1(t1, p2, p1, p0, mask);

  // Load q0, q1, q2, q3
  LOAD_H_EDGES4(p, stride, q0, q1, q2, t1);
  MAX_DIFF2(t1, q2, q1, q0, mask);

  ComplexMask(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  // Store
  memcpy(&p[-3 * stride], &p2, 16);
  memcpy(&p[-2 * stride], &p1, 16);
  memcpy(&p[-1 * stride], &p0, 16);
  memcpy(&p[+0 * stride], &q0, 16);
  memcpy(&p[+1 * stride], &q1, 16);
  memcpy(&p[+2 * stride], &q2, 16);
}

static void HFilter16(uint8_t* p, int stride, int thresh, int ithresh,
                      int hev_thresh) {
  int8x16 mask;
  int8x16 p3, p2, p1, p0, q0, q1, q2, q3;

  uint8_t* const b = p - 4;
  Load16x4(b, b + 8 * stride, stride, &p3, &p2, &p1, &p0);
  MAX_DIFF1(p3, p2, p1, p0, mask);

  Load16x4(p, p + 8 * stride, stride, &q0, &q1, &q2, &q3);
  MAX_DIFF2(q3, q2, q1, q0, mask);

  ComplexMask(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  Store16x4(&p3, &p2, &p1, &p0, b, b + 8 * stride, stride);
  Store16x4(&q0, &q1, &q2, &q3, p, p + 8 * stride, stride);
}

// on three inner edges
static void VFilter16i(uint8_t* p, int stride, int thresh, int ithresh,
                       int hev_thresh) {
  int k;
  int8x16 p3, p2, p1, p0;  // loop invariants

  LOAD_H_EDGES4(p, stride, p3, p2, p1, p0);  // prologue

  for (k = 3; k > 0; --k) {
    int8x16 mask, tmp1, tmp2;
    uint8_t* const b = p + 2 * stride;  // beginning of p1
    p += 4 * stride;

    MAX_DIFF1(p3, p2, p1, p0, mask);  // compute partial mask
    LOAD_H_EDGES4(p, stride, p3, p2, tmp1, tmp2);
    MAX_DIFF2(p3, p2, tmp1, tmp2, mask);

    // p3 and p2 are not just temporary variables here: they will be
    // re-used for next span. And q2/q3 will become p1/p0 accordingly.
    ComplexMask(&p1, &p0, &p3, &p2, thresh, ithresh, &mask);
    DoFilter4(&p1, &p0, &p3, &p2, &mask, hev_thresh);

    // Store
    memcpy(&b[0 * stride], &p1, 16);
    memcpy(&b[1 * stride], &p0, 16);
    memcpy(&b[2 * stride], &p3, 16);
    memcpy(&b[3 * stride], &p2, 16);

    // rotate samples
    p1 = tmp1;
    p0 = tmp2;
  }
}

static void HFilter16i(uint8_t* p, int stride, int thresh, int ithresh,
                       int hev_thresh) {
  int k;
  int8x16 p3, p2, p1, p0;  // loop invariants

  Load16x4(p, p + 8 * stride, stride, &p3, &p2, &p1, &p0);  // prologue

  for (k = 3; k > 0; --k) {
    int8x16 mask, tmp1, tmp2;
    uint8_t* const b = p + 2;  // beginning of p1

    p += 4;  // beginning of q0 (and next span)

    MAX_DIFF1(p3, p2, p1, p0, mask);  // compute partial mask
    Load16x4(p, p + 8 * stride, stride, &p3, &p2, &tmp1, &tmp2);
    MAX_DIFF2(p3, p2, tmp1, tmp2, mask);

    ComplexMask(&p1, &p0, &p3, &p2, thresh, ithresh, &mask);
    DoFilter4(&p1, &p0, &p3, &p2, &mask, hev_thresh);

    Store16x4(&p1, &p0, &p3, &p2, b, b + 8 * stride, stride);

    // rotate samples
    p1 = tmp1;
    p0 = tmp2;
  }
}

// 8-pixels wide variant, for chroma filtering
static void VFilter8(uint8_t* u, uint8_t* v, int stride, int thresh,
                     int ithresh, int hev_thresh) {
  int8x16 mask;
  int8x16 t1, p2, p1, p0, q0, q1, q2;

  // Load p3, p2, p1, p0
  LOADUV_H_EDGES4(u - 4 * stride, v - 4 * stride, stride, t1, p2, p1, p0);
  MAX_DIFF1(t1, p2, p1, p0, mask);

  // Load q0, q1, q2, q3
  LOADUV_H_EDGES4(u, v, stride, q0, q1, q2, t1);
  MAX_DIFF2(t1, q2, q1, q0, mask);

  ComplexMask(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  // Store
  STOREUV(p2, u, v, -3 * stride);
  STOREUV(p1, u, v, -2 * stride);
  STOREUV(p0, u, v, -1 * stride);
  STOREUV(q0, u, v, 0 * stride);
  STOREUV(q1, u, v, 1 * stride);
  STOREUV(q2, u, v, 2 * stride);
}

static void HFilter8(uint8_t* u, uint8_t* v, int stride, int thresh,
                     int ithresh, int hev_thresh) {
  int8x16 mask;
  int8x16 p3, p2, p1, p0, q0, q1, q2, q3;

  uint8_t* const tu = u - 4;
  uint8_t* const tv = v - 4;
  Load16x4(tu, tv, stride, &p3, &p2, &p1, &p0);
  MAX_DIFF1(p3, p2, p1, p0, mask);

  Load16x4(u, v, stride, &q0, &q1, &q2, &q3);
  MAX_DIFF2(q3, q2, q1, q0, mask);

  ComplexMask(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  Store16x4(&p3, &p2, &p1, &p0, tu, tv, stride);
  Store16x4(&q0, &q1, &q2, &q3, u, v, stride);
}

static void VFilter8i(uint8_t* u, uint8_t* v, int stride, int thresh,
                      int ithresh, int hev_thresh) {
  int8x16 mask;
  int8x16 t1, t2, p1, p0, q0, q1;

  // Load p3, p2, p1, p0
  LOADUV_H_EDGES4(u, v, stride, t2, t1, p1, p0);
  MAX_DIFF1(t2, t1, p1, p0, mask);

  u += 4 * stride;
  v += 4 * stride;

  // Load q0, q1, q2, q3
  LOADUV_H_EDGES4(u, v, stride, q0, q1, t1, t2);
  MAX_DIFF2(t2, t1, q1, q0, mask);

  ComplexMask(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter4(&p1, &p0, &q0, &q1, &mask, hev_thresh);

  // Store
  STOREUV(p1, u, v, -2 * stride);
  STOREUV(p0, u, v, -1 * stride);
  STOREUV(q0, u, v, 0 * stride);
  STOREUV(q1, u, v, 1 * stride);
}

static void HFilter8i(uint8_t* u, uint8_t* v, int stride, int thresh,
                      int ithresh, int hev_thresh) {
  int8x16 mask;
  int8x16 t1, t2, p1, p0, q0, q1;
  Load16x4(u, v, stride, &t2, &t1, &p1, &p0);
  MAX_DIFF1(t2, t1, p1, p0, mask);

  u += 4;  // beginning of q0
  v += 4;
  Load16x4(u, v, stride, &q0, &q1, &t1, &t2);
  MAX_DIFF2(t2, t1, q1, q0, mask);

  ComplexMask(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter4(&p1, &p0, &q0, &q1, &mask, hev_thresh);

  u -= 2;  // beginning of p1
  v -= 2;
  Store16x4(&p1, &p0, &q0, &q1, u, v, stride);
}

//------------------------------------------------------------------------------
// 4x4 predictions

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG2(a, b) (((a) + (b) + 1) >> 1)
#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))

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

static void VR4(uint8_t* dst) {  // Vertical-Right
  const uint32_t I = dst[-1 + 0 * BPS];
  const uint32_t J = dst[-1 + 1 * BPS];
  const uint32_t K = dst[-1 + 2 * BPS];
  const uint32_t X = dst[-1 - BPS];
  const uint8x16 zero = (uint8x16){0};
  const uint16x8 one = (uint16x8){1, 1, 1, 1, 1, 1, 1, 1};
  const uint16x8 two = (uint16x8){2, 2, 2, 2, 2, 2, 2, 2};
  const uint8x16 top = get_8_bytes(dst - BPS - 1);
  const uint16x8 XABCD = (uint16x8)__builtin_shufflevector(
      top, zero, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 16, 16, 16, 16, 16, 16);
  const uint16x8 ABCD0 = (uint16x8)__builtin_shufflevector(
      top, zero, 1, 16, 2, 16, 3, 16, 4, 16, 16, 16, 16, 16, 16, 16, 16, 16);
  const uint16x8 abcd = (XABCD + ABCD0 + one) >> one;
  const uint16x8 IX = (uint16x8)cvt32_to_128(I | (X << 8));
  const uint16x8 IXABCD = (uint16x8)__builtin_shufflevector(
      (uint8x16)XABCD, (uint8x16)IX, 16, 31, 17, 31, 2, 3, 4, 5, 6, 7, 8, 9, 10,
      11, 12, 13);
  const uint16x8 efgh = (IXABCD + XABCD + XABCD + ABCD0 + two) >> two;
  // pack
  const uint32x4 vals0 = (uint32x4)__builtin_shufflevector(
      (uint8x16)abcd, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals1 = (uint32x4)__builtin_shufflevector(
      (uint8x16)efgh, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  // shift left one byte and pack
  const uint32x4 vals2 = (uint32x4)__builtin_shufflevector(
      (uint8x16)abcd, zero, 16, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  // shift left one byte and pack
  const uint32x4 vals3 = (uint32x4)__builtin_shufflevector(
      (uint8x16)efgh, zero, 16, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  WebPUint32ToMem(dst + 0 * BPS, vals0[0]);
  WebPUint32ToMem(dst + 1 * BPS, vals1[0]);
  WebPUint32ToMem(dst + 2 * BPS, vals2[0]);
  WebPUint32ToMem(dst + 3 * BPS, vals3[0]);

  // these two are hard to implement in SSE2, so we keep the C-version:
  DST(0, 2) = AVG3(J, I, X);
  DST(0, 3) = AVG3(K, J, I);
}

static void LD4(uint8_t* dst) {  // Down-Left
  const uint8x16 zero = (uint8x16){0};
  const uint16x8 two = (uint16x8){2, 2, 2, 2, 2, 2, 2, 2};
  const uint8x16 top = get_8_bytes(dst - BPS);
  const uint16x8 ABCDEFGH = (uint16x8)__builtin_shufflevector(
      top, zero, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16);
  const uint16x8 BCDEFGH0 = (uint16x8)__builtin_shufflevector(
      top, zero, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 16, 16);
  const uint16x8 CDEFGHH0 = (uint16x8)__builtin_shufflevector(
      top, zero, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 7, 16, 16, 16);
  const uint16x8 avg3 =
      (ABCDEFGH + BCDEFGH0 + BCDEFGH0 + CDEFGHH0 + two) >> two;
  const uint32x4 vals0 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals1 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 2, 4, 6, 8, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals2 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 4, 6, 8, 10, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals3 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 6, 8, 10, 12, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16);
  WebPUint32ToMem(dst + 0 * BPS, vals0[0]);
  WebPUint32ToMem(dst + 1 * BPS, vals1[0]);
  WebPUint32ToMem(dst + 2 * BPS, vals2[0]);
  WebPUint32ToMem(dst + 3 * BPS, vals3[0]);
}

static void VL4(uint8_t* dst) {  // Vertical-Left
  const uint8x16 zero = (uint8x16){0};
  const uint16x8 one = (uint16x8){1, 1, 1, 1, 1, 1, 1, 1};
  const uint16x8 two = (uint16x8){2, 2, 2, 2, 2, 2, 2, 2};
  const uint8x16 top = get_8_bytes(dst - BPS);
  const uint16x8 ABCDEFGH = (uint16x8)__builtin_shufflevector(
      top, zero, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16);
  const uint16x8 BCDEFGH_ = (uint16x8)__builtin_shufflevector(
      top, zero, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 16, 16);
  const uint16x8 CDEFGH__ = (uint16x8)__builtin_shufflevector(
      top, zero, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16, 16, 16, 16, 16);
  const uint16x8 avg1 = (ABCDEFGH + BCDEFGH_ + one) >> one;
  const uint16x8 avg3 =
      (ABCDEFGH + BCDEFGH_ + BCDEFGH_ + CDEFGH__ + two) >> two;
  const uint32x4 vals0 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg1, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals1 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 0, 2, 4, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals2 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg1, zero, 2, 4, 6, 8, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals3 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 2, 4, 6, 8, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16);
  const uint32x4 vals4 = (uint32x4)__builtin_shufflevector(
      (uint8x16)avg3, zero, 8, 10, 12, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16);
  const uint32_t extra_out = vals4[0];
  WebPUint32ToMem(dst + 0 * BPS, vals0[0]);
  WebPUint32ToMem(dst + 1 * BPS, vals1[0]);
  WebPUint32ToMem(dst + 2 * BPS, vals2[0]);
  WebPUint32ToMem(dst + 3 * BPS, vals3[0]);

  // these two are hard to get and irregular
  DST(3, 2) = (extra_out >> 0) & 0xff;
  DST(3, 3) = (extra_out >> 8) & 0xff;
}

#undef DST
#undef AVG2
#undef AVG3

static void TrueMotion(uint8_t* dst, uint32_t size) {
  const uint8x16 zero = (uint8x16){0};
  uint8_t* top = dst - BPS;
  int y;

  if (size == 4) {
    const uint8x16 top_values = get_4_bytes(top);
    const int16x8 top_base = (int16x8)_unpacklo_epi8(top_values, zero);
    for (y = 0; y < 4; ++y, dst += BPS) {
      const int val = dst[-1] - top[-1];
      const int16x8 base = splat_int16(val);
      const uint32x4 out = (uint32x4)int16x8_to_uint8x16_sat(base + top_base);
      WebPUint32ToMem(dst, out[0]);
    }
  } else if (size == 8) {
    const uint8x16 top_values = get_8_bytes(top);
    const int16x8 top_base = (int16x8)_unpacklo_epi8(top_values, zero);
    for (y = 0; y < 8; ++y, dst += BPS) {
      const int val = dst[-1] - top[-1];
      const int16x8 base = splat_int16(val);
      const uint8x16 out = (uint8x16)int16x8_to_uint8x16_sat(base + top_base);
      memcpy(dst, &out, 8);
    }
  } else {
    const uint8x16 top_values = get_16_bytes(top);
    const int16x8 top_base_0 = (int16x8)_unpacklo_epi8(top_values, zero);
    const int16x8 top_base_1 = (int16x8)_unpackhi_epi8(top_values, zero);
    for (y = 0; y < 16; ++y, dst += BPS) {
      const int val = dst[-1] - top[-1];
      const int16x8 base = splat_int16(val);
      const uint8x16 out_0 =
          (uint8x16)int16x8_to_uint8x16_sat(base + top_base_0);
      const uint8x16 out_1 =
          (uint8x16)int16x8_to_uint8x16_sat(base + top_base_1);
      const uint8x16 out = _unpacklo_epi64(out_0, out_1);
      memcpy(dst, &out, 16);
    }
  }
}

static void TM4(uint8_t* dst) { TrueMotion(dst, 4); }
static void TM8uv(uint8_t* dst) { TrueMotion(dst, 8); }
static void TM16(uint8_t* dst) { TrueMotion(dst, 16); }

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

  VP8VFilter16 = VFilter16;
  VP8HFilter16 = HFilter16;
  VP8VFilter8 = VFilter8;
  VP8HFilter8 = HFilter8;
  VP8VFilter16i = VFilter16i;
  VP8HFilter16i = HFilter16i;
  VP8VFilter8i = VFilter8i;
  VP8HFilter8i = HFilter8i;

  VP8SimpleVFilter16 = SimpleVFilter16;
  VP8SimpleHFilter16 = SimpleHFilter16;
  VP8SimpleVFilter16i = SimpleVFilter16i;
  VP8SimpleHFilter16i = SimpleHFilter16i;

  VP8PredLuma4[1] = TM4;
  VP8PredLuma4[2] = VE4;
  VP8PredLuma4[4] = RD4;
  VP8PredLuma4[5] = VR4;
  VP8PredLuma4[6] = LD4;
  VP8PredLuma4[7] = VL4;

  VP8PredLuma16[0] = DC16;
  VP8PredLuma16[1] = TM16;
  VP8PredLuma16[2] = VE16;
  VP8PredLuma16[3] = HE16;
  VP8PredLuma16[4] = DC16NoTop;
  VP8PredLuma16[5] = DC16NoLeft;
  VP8PredLuma16[6] = DC16NoTopLeft;

  VP8PredChroma8[0] = DC8uv;
  VP8PredChroma8[1] = TM8uv;
  VP8PredChroma8[2] = VE8uv;
  VP8PredChroma8[3] = HE8uv;
  VP8PredChroma8[4] = DC8uvNoTop;
  VP8PredChroma8[5] = DC8uvNoLeft;
  VP8PredChroma8[6] = DC8uvNoTopLeft;
}

#else  // !WEBP_USE_WASM

WEBP_DSP_INIT_STUB(VP8DspInitWASM)

#endif  // WEBP_USE_WASM
