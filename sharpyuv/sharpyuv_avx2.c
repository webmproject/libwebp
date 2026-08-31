// Copyright 2026 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// AVX2 speed-critical functions for Sharp YUV.

#include "./sharpyuv_dsp.h"
#include "./sharpyuv_gamma.h"

#if defined(WEBP_USE_AVX2)
#include <assert.h>
#include <immintrin.h>
#include <stdlib.h>

#include "src/dsp/cpu.h"
#include "webp/types.h"

#define LOAD_256(P) _mm256_loadu_si256((const __m256i*)(P))
#define STORE_256(P, V) _mm256_storeu_si256((__m256i*)(P), (V))
#define LOAD_128(P) _mm_loadu_si128((const __m128i*)(P))
#define STORE_128(P, V) _mm_storeu_si128((__m128i*)(P), (V))
#define LOAD_64(P) _mm_loadl_epi64((const __m128i*)(P))
#define STORE_64(P, V) _mm_storel_epi64((__m128i*)(P), (V))

// Clamps 16x (resp. 8x) int16 lanes to [0, max].
static WEBP_INLINE __m256i Clip16_AVX2(__m256i v, __m256i max) {
  return _mm256_max_epi16(_mm256_min_epi16(v, max), _mm256_setzero_si256());
}
static WEBP_INLINE __m128i Clip16_128_AVX2(__m128i v, __m128i max) {
  return _mm_max_epi16(_mm_min_epi16(v, max), _mm_setzero_si128());
}

// Widens 8x int16 lanes into low/high 4x32 halves (sign-extending).
static WEBP_INLINE void WidenLoHi16To32_AVX2(__m128i v, __m128i* lo,
                                             __m128i* hi) {
  *lo = _mm_cvtepi16_epi32(v);
  *hi = _mm_cvtepi16_epi32(_mm_srli_si128(v, 8));
}

// e0 = (9*a0+3*a1+3*b0+b1+8)>>4, e1 symmetric. SUFFIX is epi16 or epi32.
#define SHARP_FILTER933_AVX2(SUFFIX, a0, a1, b0, b1, kCst8, e0_out, e1_out) \
  do {                                                                      \
    const __m256i a0b1 = _mm256_add_##SUFFIX(a0, b1);                       \
    const __m256i a1b0 = _mm256_add_##SUFFIX(a1, b0);                       \
    const __m256i a0a1b0b1 = _mm256_add_##SUFFIX(a0b1, a1b0);               \
    const __m256i a0a1b0b1_8 = _mm256_add_##SUFFIX(a0a1b0b1, kCst8);        \
    const __m256i a0b1_2 = _mm256_add_##SUFFIX(a0b1, a0b1);                 \
    const __m256i a1b0_2 = _mm256_add_##SUFFIX(a1b0, a1b0);                 \
    const __m256i c0_ =                                                     \
        _mm256_srai_##SUFFIX(_mm256_add_##SUFFIX(a0b1_2, a0a1b0b1_8), 3);   \
    const __m256i c1_ =                                                     \
        _mm256_srai_##SUFFIX(_mm256_add_##SUFFIX(a1b0_2, a0a1b0b1_8), 3);   \
    const __m256i d0_ = _mm256_add_##SUFFIX(c1_, a0);                       \
    const __m256i d1_ = _mm256_add_##SUFFIX(c0_, a1);                       \
    e0_out = _mm256_srai_##SUFFIX(d0_, 1);                                  \
    e1_out = _mm256_srai_##SUFFIX(d1_, 1);                                  \
  } while (0)

static uint64_t SharpYuvUpdateY_AVX2(const uint16_t* ref, const uint16_t* src,
                                     uint16_t* dst, int len, int bit_depth) {
  const int max_y = (1 << bit_depth) - 1;
  int i;
  const __m256i max = _mm256_set1_epi16(max_y);
  const __m256i one = _mm256_set1_epi16(1);
  __m256i sum = _mm256_setzero_si256();
  uint64_t diff;

  for (i = 0; i + 16 <= len; i += 16) {
    const __m256i A = LOAD_256(ref + i);
    const __m256i B = LOAD_256(src + i);
    const __m256i C = LOAD_256(dst + i);
    const __m256i D = _mm256_sub_epi16(A, B);        // diff_y
    const __m256i F = _mm256_add_epi16(C, D);        // new_y
    const __m256i AbsD = _mm256_abs_epi16(D);        // abs(diff_y)
    const __m256i I = _mm256_madd_epi16(AbsD, one);  // 32-bit pairs
    STORE_256(dst + i, Clip16_AVX2(F, max));
    sum = _mm256_add_epi32(sum, I);
  }
  {
    uint32_t tmp[8];
    STORE_256(tmp, sum);
    diff = (uint64_t)tmp[0] + tmp[1] + tmp[2] + tmp[3] + tmp[4] + tmp[5] +
           tmp[6] + tmp[7];
  }
  for (; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)dst[i] + diff_y;
    dst[i] = SharpYuvClip16(new_y, max_y);
    diff += (uint64_t)abs(diff_y);
  }
  return diff;
}

static void SharpYuvUpdateRGB_AVX2(const int16_t* ref, const int16_t* src,
                                   int16_t* dst, int len) {
  int i;
  for (i = 0; i + 16 <= len; i += 16) {
    const __m256i A = LOAD_256(ref + i);
    const __m256i B = LOAD_256(src + i);
    const __m256i C = LOAD_256(dst + i);
    const __m256i D = _mm256_sub_epi16(A, B);  // diff_uv
    const __m256i E = _mm256_add_epi16(C, D);  // new_uv
    STORE_256(dst + i, E);
  }
  for (; i < len; ++i) {
    const int diff_uv = ref[i] - src[i];
    dst[i] += diff_uv;
  }
}

static void SharpYuvFilterRow16_AVX2(const int16_t* A, const int16_t* B,
                                     int len, const uint16_t* best_y,
                                     uint16_t* out, int bit_depth) {
  const int max_y = (1 << bit_depth) - 1;
  int i;
  const __m256i kCst8 = _mm256_set1_epi16(8);
  const __m256i max = _mm256_set1_epi16(max_y);
  for (i = 0; i + 16 <= len; i += 16) {
    const __m256i a0 = LOAD_256(A + i + 0);
    const __m256i a1 = LOAD_256(A + i + 1);
    const __m256i b0 = LOAD_256(B + i + 0);
    const __m256i b1 = LOAD_256(B + i + 1);
    __m256i e0, e1;
    SHARP_FILTER933_AVX2(epi16, a0, a1, b0, b1, kCst8, e0, e1);
    {
      const __m256i lo = _mm256_unpacklo_epi16(e0, e1);
      const __m256i hi = _mm256_unpackhi_epi16(e0, e1);
      // unpacklo/hi interleave within 128-bit lanes; permute to restore the
      // sequential (i, i+8) ordering matching best_y/out layout.
      const __m256i f0 = _mm256_permute2x128_si256(lo, hi, 0x20);
      const __m256i f1 = _mm256_permute2x128_si256(lo, hi, 0x31);
      const __m256i g0 = LOAD_256(best_y + 2 * i + 0);
      const __m256i g1 = LOAD_256(best_y + 2 * i + 16);
      STORE_256(out + 2 * i + 0, Clip16_AVX2(_mm256_add_epi16(g0, f0), max));
      STORE_256(out + 2 * i + 16, Clip16_AVX2(_mm256_add_epi16(g1, f1), max));
    }
  }
  for (; i < len; ++i) {
    const int v0 =
        (A[i + 0] * 9 + A[i + 1] * 3 + B[i + 0] * 3 + B[i + 1] + 8) >> 4;
    const int v1 =
        (A[i + 1] * 9 + A[i + 0] * 3 + B[i + 1] * 3 + B[i + 0] + 8) >> 4;
    out[2 * i + 0] = SharpYuvClip16(best_y[2 * i + 0] + v0, max_y);
    out[2 * i + 1] = SharpYuvClip16(best_y[2 * i + 1] + v1, max_y);
  }
}

// SharpYuvFilterRow16_AVX2's intermediate sums (e.g. a0a1b0b1_8) can exceed
// int16 range once bit_depth > 10 (chroma deltas get large enough that
// 2*(A+B) overflows signed 16 bits and wraps). This variant does the same
// computation in 32-bit lanes to stay safe, matching
// SharpYuvFilterRow32_SSE2/NEON.
static void SharpYuvFilterRow32_AVX2(const int16_t* A, const int16_t* B,
                                     int len, const uint16_t* best_y,
                                     uint16_t* out, int bit_depth) {
  const int max_y = (1 << bit_depth) - 1;
  int i;
  const __m256i kCst8 = _mm256_set1_epi32(8);
  const __m128i max = _mm_set1_epi16(max_y);
  for (i = 0; i + 8 <= len; i += 8) {
    const __m256i a0 = _mm256_cvtepi16_epi32(LOAD_128(A + i + 0));
    const __m256i a1 = _mm256_cvtepi16_epi32(LOAD_128(A + i + 1));
    const __m256i b0 = _mm256_cvtepi16_epi32(LOAD_128(B + i + 0));
    const __m256i b1 = _mm256_cvtepi16_epi32(LOAD_128(B + i + 1));
    __m256i e0, e1;
    SHARP_FILTER933_AVX2(epi32, a0, a1, b0, b1, kCst8, e0, e1);
    // Split into 128-bit halves and interleave/pack each independently (pure
    // SSE-style ops): avoids AVX2's cross-128-lane pack/unpack pitfalls.
    {
      const __m128i e0_lo = _mm256_castsi256_si128(e0);
      const __m128i e0_hi = _mm256_extracti128_si256(e0, 1);
      const __m128i e1_lo = _mm256_castsi256_si128(e1);
      const __m128i e1_hi = _mm256_extracti128_si256(e1, 1);
      const __m128i out_lo = _mm_packs_epi32(_mm_unpacklo_epi32(e0_lo, e1_lo),
                                             _mm_unpackhi_epi32(e0_lo, e1_lo));
      const __m128i out_hi = _mm_packs_epi32(_mm_unpacklo_epi32(e0_hi, e1_hi),
                                             _mm_unpackhi_epi32(e0_hi, e1_hi));
      const __m128i g_lo = LOAD_128(best_y + 2 * i + 0);
      const __m128i g_hi = LOAD_128(best_y + 2 * i + 8);
      STORE_128(out + 2 * i + 0,
                Clip16_128_AVX2(_mm_add_epi16(g_lo, out_lo), max));
      STORE_128(out + 2 * i + 8,
                Clip16_128_AVX2(_mm_add_epi16(g_hi, out_hi), max));
    }
  }
  for (; i < len; ++i) {
    const int a0b1 = A[i + 0] + B[i + 1];
    const int a1b0 = A[i + 1] + B[i + 0];
    const int a0a1b0b1 = a0b1 + a1b0 + 8;
    const int v0 = (8 * A[i + 0] + 2 * a1b0 + a0a1b0b1) >> 4;
    const int v1 = (8 * A[i + 1] + 2 * a0b1 + a0a1b0b1) >> 4;
    out[2 * i + 0] = SharpYuvClip16(best_y[2 * i + 0] + v0, max_y);
    out[2 * i + 1] = SharpYuvClip16(best_y[2 * i + 1] + v1, max_y);
  }
}

static void SharpYuvFilterRow_AVX2(const int16_t* A, const int16_t* B, int len,
                                   const uint16_t* best_y, uint16_t* out,
                                   int bit_depth) {
  if (bit_depth <= 10) {
    SharpYuvFilterRow16_AVX2(A, B, len, best_y, out, bit_depth);
  } else {
    SharpYuvFilterRow32_AVX2(A, B, len, best_y, out, bit_depth);
  }
}

//------------------------------------------------------------------------------
// Final W/RGB -> YUV reconstruction. Must stay numerically equivalent to
// RGBToYUVComponent() in sharpyuv.c, including its int64_t precision.

// Arithmetic right shift by a runtime amount n (1 <= n <= 63): AVX2 has no
// psraq, so build it from the logical shift plus a sign-extended top.
static WEBP_INLINE __m256i Sra64_AVX2(__m256i a, int n) {
  const __m128i cnt = _mm_cvtsi32_si128(n);
  const __m128i cnt2 = _mm_cvtsi32_si128(64 - n);
  const __m256i sign = _mm256_cmpgt_epi64(_mm256_setzero_si256(), a);
  const __m256i shifted = _mm256_srl_epi64(a, cnt);
  const __m256i topbits = _mm256_sll_epi64(sign, cnt2);
  return _mm256_or_si256(shifted, topbits);
}

// Truncating (non-saturating) narrow of 8x int32 lanes to 8x int16: matches
// a plain C (int16_t)/(uint16_t) cast, unlike _mm256_packs/packus_epi32.
static WEBP_INLINE __m128i TruncateNarrow32To16_AVX2(__m256i v) {
  const __m256i masked = _mm256_and_si256(v, _mm256_set1_epi32(0xffff));
  const __m256i packed = _mm256_packus_epi32(masked, masked);
  const __m128i lo = _mm256_castsi256_si128(packed);
  const __m128i hi = _mm256_extracti128_si256(packed, 1);
  return _mm_unpacklo_epi64(lo, hi);
}

// Narrow 4x64-bit lanes (2 groups of 2, from the mul_epi32 lane layout) to
// 4x32-bit, keeping only the low 32 bits of each 64-bit lane.
static WEBP_INLINE __m128i Narrow64To32_AVX2(__m256i v) {
  const __m256i shuffled = _mm256_shuffle_epi32(v, _MM_SHUFFLE(2, 0, 2, 0));
  const __m128i lo = _mm256_castsi256_si128(shuffled);
  const __m128i hi = _mm256_extracti128_si256(shuffled, 1);
  return _mm_unpacklo_epi64(lo, hi);
}

// round((R*coeffs[0] + G*coeffs[1] + B*coeffs[2] + bias) >> shift) for 4
// lanes, in exact 64-bit precision (matches the int64_t path in
// RGBToYUVComponent / RGBToGray).
static WEBP_INLINE __m128i Convert4_AVX2(__m128i R, __m128i G, __m128i B,
                                         const int coeffs[4], __m256i bias,
                                         int shift) {
  const __m256i R64 = _mm256_cvtepi32_epi64(R);
  const __m256i G64 = _mm256_cvtepi32_epi64(G);
  const __m256i B64 = _mm256_cvtepi32_epi64(B);
  __m256i acc = _mm256_add_epi64(
      bias, _mm256_mul_epi32(R64, _mm256_set1_epi64x(coeffs[0])));
  acc = _mm256_add_epi64(acc,
                         _mm256_mul_epi32(G64, _mm256_set1_epi64x(coeffs[1])));
  acc = _mm256_add_epi64(acc,
                         _mm256_mul_epi32(B64, _mm256_set1_epi64x(coeffs[2])));
  acc = Sra64_AVX2(acc, shift);
  return Narrow64To32_AVX2(acc);
}

static void SharpYuvConvertRowY_AVX2(const uint16_t* best_y,
                                     const int16_t* best_uv, int width,
                                     int uv_w, const int coeffs[4], int sfix,
                                     int yuv_bit_depth, void* y_out) {
  const int shift = SHARPYUV_YUV_FIX + sfix;
  const int64_t rounder = 1LL << (shift - 1);
  const __m256i bias = _mm256_set1_epi64x(coeffs[3] + rounder);
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  int i = 0;
  for (; i + 8 <= width; i += 8) {
    const int off = i >> 1;
    const __m128i W = LOAD_128(best_y + i);
    const __m128i Ro = LOAD_64(best_uv + off + 0 * uv_w);
    const __m128i Go = LOAD_64(best_uv + off + 1 * uv_w);
    const __m128i Bo = LOAD_64(best_uv + off + 2 * uv_w);
    const __m128i Rv = _mm_add_epi16(_mm_unpacklo_epi16(Ro, Ro), W);
    const __m128i Gv = _mm_add_epi16(_mm_unpacklo_epi16(Go, Go), W);
    const __m128i Bv = _mm_add_epi16(_mm_unpacklo_epi16(Bo, Bo), W);
    __m128i R_lo, R_hi, G_lo, G_hi, B_lo, B_hi;
    WidenLoHi16To32_AVX2(Rv, &R_lo, &R_hi);
    WidenLoHi16To32_AVX2(Gv, &G_lo, &G_hi);
    WidenLoHi16To32_AVX2(Bv, &B_lo, &B_hi);
    {
      const __m128i y_lo = Convert4_AVX2(R_lo, G_lo, B_lo, coeffs, bias, shift);
      const __m128i y_hi = Convert4_AVX2(R_hi, G_hi, B_hi, coeffs, bias, shift);
      const __m128i y16 = _mm_packs_epi32(y_lo, y_hi);  // saturating:
                                                        // matches
                                                        // clip()/clip_8b().
      if (yuv_bit_depth <= 8) {
        STORE_64((uint8_t*)y_out + i, _mm_packus_epi16(y16, y16));
      } else {
        const __m128i maxv = _mm_set1_epi16((int16_t)yuv_max);
        STORE_128((uint16_t*)y_out + i, Clip16_128_AVX2(y16, maxv));
      }
    }
  }
  for (; i < width; ++i) {
    const int off = i >> 1;
    const int W = best_y[i];
    const int r = best_uv[off + 0 * uv_w] + W;
    const int g = best_uv[off + 1 * uv_w] + W;
    const int b = best_uv[off + 2 * uv_w] + W;
    const int y = SharpYuvConvertComponent(r, g, b, coeffs, sfix);
    if (yuv_bit_depth <= 8) {
      ((uint8_t*)y_out)[i] = (uint8_t)SharpYuvClip16(y, 255);
    } else {
      ((uint16_t*)y_out)[i] = SharpYuvClip16(y, yuv_max);
    }
  }
}

static void SharpYuvConvertRowUV_AVX2(const int16_t* best_uv, int uv_w,
                                      const int coeffs_u[4],
                                      const int coeffs_v[4], int sfix,
                                      int yuv_bit_depth, void* u_out,
                                      void* v_out) {
  const int shift = SHARPYUV_YUV_FIX + sfix;
  const int64_t rounder = 1LL << (shift - 1);
  const __m256i bias_u = _mm256_set1_epi64x(coeffs_u[3] + rounder);
  const __m256i bias_v = _mm256_set1_epi64x(coeffs_v[3] + rounder);
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  int i = 0;
  for (; i + 8 <= uv_w; i += 8) {
    const __m128i R16 = LOAD_128(best_uv + i + 0 * uv_w);
    const __m128i G16 = LOAD_128(best_uv + i + 1 * uv_w);
    const __m128i B16 = LOAD_128(best_uv + i + 2 * uv_w);
    __m128i R_lo, R_hi, G_lo, G_hi, B_lo, B_hi;
    WidenLoHi16To32_AVX2(R16, &R_lo, &R_hi);
    WidenLoHi16To32_AVX2(G16, &G_lo, &G_hi);
    WidenLoHi16To32_AVX2(B16, &B_lo, &B_hi);
    {
      const __m128i u_lo =
          Convert4_AVX2(R_lo, G_lo, B_lo, coeffs_u, bias_u, shift);
      const __m128i u_hi =
          Convert4_AVX2(R_hi, G_hi, B_hi, coeffs_u, bias_u, shift);
      const __m128i v_lo =
          Convert4_AVX2(R_lo, G_lo, B_lo, coeffs_v, bias_v, shift);
      const __m128i v_hi =
          Convert4_AVX2(R_hi, G_hi, B_hi, coeffs_v, bias_v, shift);
      const __m128i u16 = _mm_packs_epi32(u_lo, u_hi);
      const __m128i v16 = _mm_packs_epi32(v_lo, v_hi);
      if (yuv_bit_depth <= 8) {
        STORE_64((uint8_t*)u_out + i, _mm_packus_epi16(u16, u16));
        STORE_64((uint8_t*)v_out + i, _mm_packus_epi16(v16, v16));
      } else {
        const __m128i maxv = _mm_set1_epi16((int16_t)yuv_max);
        STORE_128((uint16_t*)u_out + i, Clip16_128_AVX2(u16, maxv));
        STORE_128((uint16_t*)v_out + i, Clip16_128_AVX2(v16, maxv));
      }
    }
  }
  for (; i < uv_w; ++i) {
    const int r = best_uv[i + 0 * uv_w];
    const int g = best_uv[i + 1 * uv_w];
    const int b = best_uv[i + 2 * uv_w];
    const int u = SharpYuvConvertComponent(r, g, b, coeffs_u, sfix);
    const int v = SharpYuvConvertComponent(r, g, b, coeffs_v, sfix);
    if (yuv_bit_depth <= 8) {
      ((uint8_t*)u_out)[i] = (uint8_t)SharpYuvClip16(u, 255);
      ((uint8_t*)v_out)[i] = (uint8_t)SharpYuvClip16(v, 255);
    } else {
      ((uint16_t*)u_out)[i] = SharpYuvClip16(u, yuv_max);
      ((uint16_t*)v_out)[i] = SharpYuvClip16(v, yuv_max);
    }
  }
}

//------------------------------------------------------------------------------
// sRGB gamma<->linear fast paths for UpdateW/UpdateChroma.
// Leverage vpgatherdd gather instruction!

// Widens 8 samples to 16.16 fixed-point linear values using the
// sRGB gamma->linear table (shift_right = bit_depth - table_bits, >= 0).
static WEBP_INLINE __m256i GammaToLinear8_AVX2(__m128i v16, int shift_right) {
  const __m256i v32 = _mm256_cvtepu16_epi32(v16);
  const __m128i cnt = _mm_cvtsi32_si128(shift_right);
  const __m256i pos = _mm256_srl_epi32(v32, cnt);
  const __m256i x = _mm256_sub_epi32(v32, _mm256_sll_epi32(pos, cnt));
  const __m256i v0 =
      _mm256_i32gather_epi32((const int*)kSharpYuvGammaToLinearTabS, pos, 4);
  const __m256i v1 =
      _mm256_i32gather_epi32((const int*)kSharpYuvGammaToLinearTabS,
                             _mm256_add_epi32(pos, _mm256_set1_epi32(1)), 4);
  // d * x <= 65536 * 2^shift_right: fits in int32
  const __m256i prod = _mm256_mullo_epi32(_mm256_sub_epi32(v1, v0), x);
  const int half = (shift_right > 0) ? (1 << (shift_right - 1)) : 0;
  const __m256i sum = _mm256_add_epi32(prod, _mm256_set1_epi32(half));
  const __m256i shifted = _mm256_srl_epi32(sum, cnt);
  return _mm256_add_epi32(v0, shifted);
}

// Reverse direction: 8 16.16 fixed-point values -> 8 gamma samples.
static WEBP_INLINE __m256i LinearToGamma8_AVX2(__m256i v, int out_shift) {
  const __m256i pos = _mm256_srli_epi32(v, 7);  // table bits are fixed.
  const __m256i x = _mm256_sub_epi32(v, _mm256_slli_epi32(pos, 7));
  const __m256i v0_raw =
      _mm256_i32gather_epi32((const int*)kSharpYuvLinearToGammaTabS, pos, 4);
  const __m256i v1_raw =
      _mm256_i32gather_epi32((const int*)kSharpYuvLinearToGammaTabS,
                             _mm256_add_epi32(pos, _mm256_set1_epi32(1)), 4);
  const __m128i cnt = _mm_cvtsi32_si128(out_shift);
  const __m256i v0 = _mm256_srl_epi32(v0_raw, cnt);
  const __m256i v1 = _mm256_srl_epi32(v1_raw, cnt);
  // d * x <= 65536 * 128: fits in int32
  const __m256i prod = _mm256_mullo_epi32(_mm256_sub_epi32(v1, v0), x);
  const __m256i sum = _mm256_add_epi32(prod, _mm256_set1_epi32(64));
  const __m256i shifted = _mm256_srai_epi32(sum, 7);
  return _mm256_add_epi32(v0, shifted);
}

// (13933*R + 46871*G + 4732*B + 32768) >> 16, fits tightly in 32bits!
static WEBP_INLINE __m256i SharpYuvRGBToGray8_AVX2(__m256i R, __m256i G,
                                                   __m256i B) {
  static const int kGrayCoeffs[4] = {13933, 46871, 4732, 0};
  const __m256i bias = _mm256_set1_epi64x(1LL << (SHARPYUV_YUV_FIX - 1));
  const __m128i y_lo = Convert4_AVX2(
      _mm256_castsi256_si128(R), _mm256_castsi256_si128(G),
      _mm256_castsi256_si128(B), kGrayCoeffs, bias, SHARPYUV_YUV_FIX);
  const __m128i y_hi = Convert4_AVX2(
      _mm256_extracti128_si256(R, 1), _mm256_extracti128_si256(G, 1),
      _mm256_extracti128_si256(B, 1), kGrayCoeffs, bias, SHARPYUV_YUV_FIX);
  return _mm256_insertf128_si256(_mm256_castsi128_si256(y_lo), y_hi, 1);
}

static void SharpYuvUpdateWSrgb_AVX2(const uint16_t* src, uint16_t* dst, int w,
                                     int bit_depth) {
  const int shift_right = bit_depth - SHARPYUV_GAMMA_TO_LINEAR_TAB_BITS;
  const int out_shift = SHARPYUV_GAMMA_TO_LINEAR_BITS - bit_depth;
  int i = 0;
  assert(shift_right >= 0);
  for (; i + 8 <= w; i += 8) {
    const __m256i R =
        GammaToLinear8_AVX2(LOAD_128(src + 0 * w + i), shift_right);
    const __m256i G =
        GammaToLinear8_AVX2(LOAD_128(src + 1 * w + i), shift_right);
    const __m256i B =
        GammaToLinear8_AVX2(LOAD_128(src + 2 * w + i), shift_right);
    const __m256i Y = SharpYuvRGBToGray8_AVX2(R, G, B);
    STORE_128(dst + i,
              TruncateNarrow32To16_AVX2(LinearToGamma8_AVX2(Y, out_shift)));
  }
  for (; i < w; ++i) {
    const uint32_t R = SharpYuvGammaToLinear(src[0 * w + i], bit_depth,
                                             kSharpYuvTransferFunctionSrgb);
    const uint32_t G = SharpYuvGammaToLinear(src[1 * w + i], bit_depth,
                                             kSharpYuvTransferFunctionSrgb);
    const uint32_t B = SharpYuvGammaToLinear(src[2 * w + i], bit_depth,
                                             kSharpYuvTransferFunctionSrgb);
    const int Y = SharpYuvRGBToGray(R, G, B);
    dst[i] = SharpYuvLinearToGamma((uint32_t)Y, bit_depth,
                                   kSharpYuvTransferFunctionSrgb);
  }
}

// Deinterleaves 8 (a,b) pairs into a=[a0..a7], b=[b0..b7].
static WEBP_INLINE void Deinterleave8_AVX2(const uint16_t* p, __m128i* a,
                                           __m128i* b) {
  static const uint8_t kM[16] = {0, 1, 4, 5, 8,  9,  12, 13,
                                 2, 3, 6, 7, 10, 11, 14, 15};
  const __m256i mask = _mm256_broadcastsi128_si256(LOAD_128(kM));
  const __m256i v = LOAD_256(p);
  const __m256i shuffled = _mm256_shuffle_epi8(v, mask);
  const __m128i lane0 = _mm256_castsi256_si128(shuffled);
  const __m128i lane1 = _mm256_extracti128_si256(shuffled, 1);
  *a = _mm_unpacklo_epi64(lane0, lane1);
  *b = _mm_unpackhi_epi64(lane0, lane1);
}

// Box-downsamples 8 consecutive 2x2 uv-groups, matching ScaleDown().
static WEBP_INLINE __m128i ScaleDown8_AVX2(const uint16_t* p1,
                                           const uint16_t* p2, int shift_right,
                                           int out_shift) {
  __m128i a, b, c, d;
  Deinterleave8_AVX2(p1, &a, &b);
  Deinterleave8_AVX2(p2, &c, &d);
  {
    const __m256i A = GammaToLinear8_AVX2(a, shift_right);
    const __m256i B = GammaToLinear8_AVX2(b, shift_right);
    const __m256i C = GammaToLinear8_AVX2(c, shift_right);
    const __m256i D = GammaToLinear8_AVX2(d, shift_right);
    const __m256i sum =
        _mm256_add_epi32(_mm256_add_epi32(A, B), _mm256_add_epi32(C, D));
    const __m256i avg =
        _mm256_srli_epi32(_mm256_add_epi32(sum, _mm256_set1_epi32(2)), 2);
    return TruncateNarrow32To16_AVX2(LinearToGamma8_AVX2(avg, out_shift));
  }
}

static void SharpYuvUpdateChromaSrgb_AVX2(const uint16_t* src1,
                                          const uint16_t* src2, int16_t* dst,
                                          int uv_w, int bit_depth) {
  const int shift_right = bit_depth - SHARPYUV_GAMMA_TO_LINEAR_TAB_BITS;
  const int out_shift = SHARPYUV_GAMMA_TO_LINEAR_BITS - bit_depth;
  int i = 0;
  assert(shift_right >= 0);
  for (; i + 8 <= uv_w; i += 8) {
    const int off = 2 * i;
    const __m128i r16 = ScaleDown8_AVX2(
        src1 + off + 0 * uv_w, src2 + off + 0 * uv_w, shift_right, out_shift);
    const __m128i g16 = ScaleDown8_AVX2(
        src1 + off + 2 * uv_w, src2 + off + 2 * uv_w, shift_right, out_shift);
    const __m128i b16 = ScaleDown8_AVX2(
        src1 + off + 4 * uv_w, src2 + off + 4 * uv_w, shift_right, out_shift);
    const __m256i r32 = _mm256_cvtepu16_epi32(r16);
    const __m256i g32 = _mm256_cvtepu16_epi32(g16);
    const __m256i b32 = _mm256_cvtepu16_epi32(b16);
    const __m256i w32 = SharpYuvRGBToGray8_AVX2(r32, g32, b32);
    STORE_128(dst + i + 0 * uv_w,
              TruncateNarrow32To16_AVX2(_mm256_sub_epi32(r32, w32)));
    STORE_128(dst + i + 1 * uv_w,
              TruncateNarrow32To16_AVX2(_mm256_sub_epi32(g32, w32)));
    STORE_128(dst + i + 2 * uv_w,
              TruncateNarrow32To16_AVX2(_mm256_sub_epi32(b32, w32)));
  }
  for (; i < uv_w; ++i) {
    const int off = 2 * i;
    uint32_t rgb[3];
    int c;
    for (c = 0; c < 3; ++c) {
      const uint16_t* s1 = src1 + off + 2 * c * uv_w;
      const uint16_t* s2 = src2 + off + 2 * c * uv_w;
      const uint32_t A = SharpYuvGammaToLinear(s1[0], bit_depth,
                                               kSharpYuvTransferFunctionSrgb);
      const uint32_t B = SharpYuvGammaToLinear(s1[1], bit_depth,
                                               kSharpYuvTransferFunctionSrgb);
      const uint32_t C = SharpYuvGammaToLinear(s2[0], bit_depth,
                                               kSharpYuvTransferFunctionSrgb);
      const uint32_t D = SharpYuvGammaToLinear(s2[1], bit_depth,
                                               kSharpYuvTransferFunctionSrgb);
      rgb[c] = SharpYuvLinearToGamma((A + B + C + D + 2) >> 2, bit_depth,
                                     kSharpYuvTransferFunctionSrgb);
    }
    {
      const int W = SharpYuvRGBToGray(rgb[0], rgb[1], rgb[2]);
      dst[i + 0 * uv_w] = (int16_t)((int)rgb[0] - W);
      dst[i + 1 * uv_w] = (int16_t)((int)rgb[1] - W);
      dst[i + 2 * uv_w] = (int16_t)((int)rgb[2] - W);
    }
  }
}

//------------------------------------------------------------------------------

extern void InitSharpYuvAVX2(void);

WEBP_TSAN_IGNORE_FUNCTION void InitSharpYuvAVX2(void) {
  SharpYuvUpdateY = SharpYuvUpdateY_AVX2;
  SharpYuvUpdateRGB = SharpYuvUpdateRGB_AVX2;
  SharpYuvFilterRow = SharpYuvFilterRow_AVX2;
  SharpYuvConvertRowY = SharpYuvConvertRowY_AVX2;
  SharpYuvConvertRowUV = SharpYuvConvertRowUV_AVX2;
  SharpYuvUpdateWSrgb = SharpYuvUpdateWSrgb_AVX2;
  SharpYuvUpdateChromaSrgb = SharpYuvUpdateChromaSrgb_AVX2;
}

#else  // !WEBP_USE_AVX2

extern void InitSharpYuvAVX2(void);

void InitSharpYuvAVX2(void) {}

#endif  // WEBP_USE_AVX2
