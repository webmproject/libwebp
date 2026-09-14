// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Speed-critical functions for Sharp YUV.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "sharpyuv/sharpyuv_dsp.h"
#include "sharpyuv/sharpyuv_gamma.h"

#if defined(WEBP_USE_NEON)
#include <arm_neon.h>
#include <assert.h>
#include <stdlib.h>

static uint64_t SharpYuvUpdateY_NEON(const uint16_t* ref, const uint16_t* src,
                                     uint16_t* dst, int len, int bit_depth) {
  const int max_y = (1 << bit_depth) - 1;
  int i;
  const int16x8_t zero = vdupq_n_s16(0);
  const int16x8_t max = vdupq_n_s16(max_y);
  uint64x2_t sum = vdupq_n_u64(0);
  uint64_t diff;

  for (i = 0; i + 8 <= len; i += 8) {
    const int16x8_t A = vreinterpretq_s16_u16(vld1q_u16(ref + i));
    const int16x8_t B = vreinterpretq_s16_u16(vld1q_u16(src + i));
    const int16x8_t C = vreinterpretq_s16_u16(vld1q_u16(dst + i));
    const int16x8_t D = vsubq_s16(A, B);  // diff_y
    const int16x8_t F = vaddq_s16(C, D);  // new_y
    const uint16x8_t H =
        vreinterpretq_u16_s16(vmaxq_s16(vminq_s16(F, max), zero));
    const int16x8_t I = vabsq_s16(D);  // abs(diff_y)
    vst1q_u16(dst + i, H);
    sum = vpadalq_u32(sum, vpaddlq_u16(vreinterpretq_u16_s16(I)));
  }
  diff = vgetq_lane_u64(sum, 0) + vgetq_lane_u64(sum, 1);
  for (; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)(dst[i]) + diff_y;
    dst[i] = SharpYuvClip16(new_y, max_y);
    diff += (uint64_t)(abs(diff_y));
  }
  return diff;
}

static void SharpYuvUpdateRGB_NEON(const int16_t* ref, const int16_t* src,
                                   int16_t* dst, int len) {
  int i;
  for (i = 0; i + 8 <= len; i += 8) {
    const int16x8_t A = vld1q_s16(ref + i);
    const int16x8_t B = vld1q_s16(src + i);
    const int16x8_t C = vld1q_s16(dst + i);
    const int16x8_t D = vsubq_s16(A, B);  // diff_uv
    const int16x8_t E = vaddq_s16(C, D);  // new_uv
    vst1q_s16(dst + i, E);
  }
  for (; i < len; ++i) {
    const int diff_uv = ref[i] - src[i];
    dst[i] += diff_uv;
  }
}

static void SharpYuvFilterRow16_NEON(const int16_t* A, const int16_t* B,
                                     int len, const uint16_t* best_y,
                                     uint16_t* out, int bit_depth) {
  const int max_y = (1 << bit_depth) - 1;
  int i;
  const int16x8_t max = vdupq_n_s16(max_y);
  const int16x8_t zero = vdupq_n_s16(0);
  for (i = 0; i + 8 <= len; i += 8) {
    const int16x8_t a0 = vld1q_s16(A + i + 0);
    const int16x8_t a1 = vld1q_s16(A + i + 1);
    const int16x8_t b0 = vld1q_s16(B + i + 0);
    const int16x8_t b1 = vld1q_s16(B + i + 1);
    const int16x8_t a0b1 = vaddq_s16(a0, b1);
    const int16x8_t a1b0 = vaddq_s16(a1, b0);
    const int16x8_t a0a1b0b1 = vaddq_s16(a0b1, a1b0);  // A0+A1+B0+B1
    const int16x8_t a0b1_2 = vaddq_s16(a0b1, a0b1);    // 2*(A0+B1)
    const int16x8_t a1b0_2 = vaddq_s16(a1b0, a1b0);    // 2*(A1+B0)
    const int16x8_t c0 = vshrq_n_s16(vaddq_s16(a0b1_2, a0a1b0b1), 3);
    const int16x8_t c1 = vshrq_n_s16(vaddq_s16(a1b0_2, a0a1b0b1), 3);
    const int16x8_t e0 = vrhaddq_s16(c1, a0);
    const int16x8_t e1 = vrhaddq_s16(c0, a1);
    const int16x8x2_t f = vzipq_s16(e0, e1);
    const int16x8_t g0 = vreinterpretq_s16_u16(vld1q_u16(best_y + 2 * i + 0));
    const int16x8_t g1 = vreinterpretq_s16_u16(vld1q_u16(best_y + 2 * i + 8));
    const int16x8_t h0 = vaddq_s16(g0, f.val[0]);
    const int16x8_t h1 = vaddq_s16(g1, f.val[1]);
    const int16x8_t i0 = vmaxq_s16(vminq_s16(h0, max), zero);
    const int16x8_t i1 = vmaxq_s16(vminq_s16(h1, max), zero);
    vst1q_u16(out + 2 * i + 0, vreinterpretq_u16_s16(i0));
    vst1q_u16(out + 2 * i + 8, vreinterpretq_u16_s16(i1));
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

static void SharpYuvFilterRow32_NEON(const int16_t* A, const int16_t* B,
                                     int len, const uint16_t* best_y,
                                     uint16_t* out, int bit_depth) {
  const int max_y = (1 << bit_depth) - 1;
  int i;
  const uint16x8_t max = vdupq_n_u16(max_y);
  for (i = 0; i + 4 <= len; i += 4) {
    const int16x4_t a0 = vld1_s16(A + i + 0);
    const int16x4_t a1 = vld1_s16(A + i + 1);
    const int16x4_t b0 = vld1_s16(B + i + 0);
    const int16x4_t b1 = vld1_s16(B + i + 1);
    const int32x4_t a0b1 = vaddl_s16(a0, b1);
    const int32x4_t a1b0 = vaddl_s16(a1, b0);
    const int32x4_t a0a1b0b1 = vaddq_s32(a0b1, a1b0);  // A0+A1+B0+B1
    const int32x4_t a0b1_2 = vaddq_s32(a0b1, a0b1);    // 2*(A0+B1)
    const int32x4_t a1b0_2 = vaddq_s32(a1b0, a1b0);    // 2*(A1+B0)
    const int32x4_t c0 = vshrq_n_s32(vaddq_s32(a0b1_2, a0a1b0b1), 3);
    const int32x4_t c1 = vshrq_n_s32(vaddq_s32(a1b0_2, a0a1b0b1), 3);
    const int32x4_t e0 = vrhaddq_s32(c1, vmovl_s16(a0));
    const int32x4_t e1 = vrhaddq_s32(c0, vmovl_s16(a1));
    const int32x4x2_t f = vzipq_s32(e0, e1);

    const int16x8_t g = vreinterpretq_s16_u16(vld1q_u16(best_y + 2 * i));
    const int32x4_t h0 = vaddw_s16(f.val[0], vget_low_s16(g));
    const int32x4_t h1 = vaddw_s16(f.val[1], vget_high_s16(g));
    const uint16x8_t i_16 = vcombine_u16(vqmovun_s32(h0), vqmovun_s32(h1));
    const uint16x8_t i_clamped = vminq_u16(i_16, max);
    vst1q_u16(out + 2 * i + 0, i_clamped);
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

static void SharpYuvFilterRow_NEON(const int16_t* A, const int16_t* B, int len,
                                   const uint16_t* best_y, uint16_t* out,
                                   int bit_depth) {
  if (bit_depth <= 10) {
    SharpYuvFilterRow16_NEON(A, B, len, best_y, out, bit_depth);
  } else {
    SharpYuvFilterRow32_NEON(A, B, len, best_y, out, bit_depth);
  }
}

//------------------------------------------------------------------------------
// Final W/RGB -> YUV reconstruction, fits (tightly) in int64_t precision.

// {v0,v1,v2,v3} -> {v0,v0,v1,v1,v2,v2,v3,v3}
static WEBP_INLINE int16x8_t DupEach_NEON(int16x4_t v) {
  const int16x4x2_t zipped = vzip_s16(v, v);
  return vcombine_s16(zipped.val[0], zipped.val[1]);
}

// Returns round((R*coeffs[0] + G*coeffs[1] + B*coeffs[2] + bias) >> shift)
// for 4 lanes (exact 64-bit precision)
static WEBP_INLINE int32x4_t Convert4_NEON(int32x4_t R, int32x4_t G,
                                           int32x4_t B, const int coeffs[4],
                                           int64x2_t bias,
                                           int64x2_t neg_shift) {
  const int32x2_t c0 = vdup_n_s32(coeffs[0]);
  const int32x2_t c1 = vdup_n_s32(coeffs[1]);
  const int32x2_t c2 = vdup_n_s32(coeffs[2]);
  int64x2_t lo = vmlal_s32(bias, vget_low_s32(R), c0);
  int64x2_t hi = vmlal_s32(bias, vget_high_s32(R), c0);
  lo = vmlal_s32(lo, vget_low_s32(G), c1);
  hi = vmlal_s32(hi, vget_high_s32(G), c1);
  lo = vmlal_s32(lo, vget_low_s32(B), c2);
  hi = vmlal_s32(hi, vget_high_s32(B), c2);
  lo = vshlq_s64(lo, neg_shift);
  hi = vshlq_s64(hi, neg_shift);
  return vcombine_s32(vmovn_s64(lo), vmovn_s64(hi));
}

static void SharpYuvConvertRowY_NEON(const uint16_t* best_y,
                                     const int16_t* best_uv, int width,
                                     int uv_w, const int coeffs[4], int sfix,
                                     int yuv_bit_depth, void* y_out) {
  const int shift = SHARPYUV_YUV_FIX + sfix;
  const int64_t rounder = 1LL << (shift - 1);
  const int64x2_t bias = vdupq_n_s64((int64_t)coeffs[3] + rounder);
  const int64x2_t neg_shift = vdupq_n_s64(-(int64_t)shift);
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  int i = 0;
  for (; i + 8 <= width; i += 8) {
    const int off = i >> 1;
    const int16x8_t W = vreinterpretq_s16_u16(vld1q_u16(best_y + i));
    const int16x4_t Ro = vld1_s16(best_uv + off + 0 * uv_w);
    const int16x4_t Go = vld1_s16(best_uv + off + 1 * uv_w);
    const int16x4_t Bo = vld1_s16(best_uv + off + 2 * uv_w);
    const int16x8_t R16 = vaddq_s16(DupEach_NEON(Ro), W);
    const int16x8_t G16 = vaddq_s16(DupEach_NEON(Go), W);
    const int16x8_t B16 = vaddq_s16(DupEach_NEON(Bo), W);

    const int32x4_t y_lo = Convert4_NEON(
        vmovl_s16(vget_low_s16(R16)), vmovl_s16(vget_low_s16(G16)),
        vmovl_s16(vget_low_s16(B16)), coeffs, bias, neg_shift);
    const int32x4_t y_hi = Convert4_NEON(
        vmovl_s16(vget_high_s16(R16)), vmovl_s16(vget_high_s16(G16)),
        vmovl_s16(vget_high_s16(B16)), coeffs, bias, neg_shift);
    const int16x8_t y16 = vcombine_s16(vqmovn_s32(y_lo), vqmovn_s32(y_hi));

    if (yuv_bit_depth <= 8) {
      vst1_u8((uint8_t*)y_out + i, vqmovun_s16(y16));
    } else {
      const int16x8_t zero = vdupq_n_s16(0);
      const int16x8_t maxv = vdupq_n_s16((int16_t)yuv_max);
      const int16x8_t clamped = vmaxq_s16(vminq_s16(y16, maxv), zero);
      vst1q_u16((uint16_t*)y_out + i, vreinterpretq_u16_s16(clamped));
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

static void SharpYuvConvertRowUV_NEON(const int16_t* best_uv, int uv_w,
                                      const int coeffs_u[4],
                                      const int coeffs_v[4], int sfix,
                                      int yuv_bit_depth, void* u_out,
                                      void* v_out) {
  const int shift = SHARPYUV_YUV_FIX + sfix;
  const int64_t rounder = 1LL << (shift - 1);
  const int64x2_t bias_u = vdupq_n_s64((int64_t)coeffs_u[3] + rounder);
  const int64x2_t bias_v = vdupq_n_s64((int64_t)coeffs_v[3] + rounder);
  const int64x2_t neg_shift = vdupq_n_s64(-(int64_t)shift);
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  int i = 0;
  for (; i + 8 <= uv_w; i += 8) {
    const int16x8_t R16 = vld1q_s16(best_uv + i + 0 * uv_w);
    const int16x8_t G16 = vld1q_s16(best_uv + i + 1 * uv_w);
    const int16x8_t B16 = vld1q_s16(best_uv + i + 2 * uv_w);
    const int32x4_t R_lo = vmovl_s16(vget_low_s16(R16));
    const int32x4_t R_hi = vmovl_s16(vget_high_s16(R16));
    const int32x4_t G_lo = vmovl_s16(vget_low_s16(G16));
    const int32x4_t G_hi = vmovl_s16(vget_high_s16(G16));
    const int32x4_t B_lo = vmovl_s16(vget_low_s16(B16));
    const int32x4_t B_hi = vmovl_s16(vget_high_s16(B16));

    const int32x4_t u_lo =
        Convert4_NEON(R_lo, G_lo, B_lo, coeffs_u, bias_u, neg_shift);
    const int32x4_t u_hi =
        Convert4_NEON(R_hi, G_hi, B_hi, coeffs_u, bias_u, neg_shift);
    const int32x4_t v_lo =
        Convert4_NEON(R_lo, G_lo, B_lo, coeffs_v, bias_v, neg_shift);
    const int32x4_t v_hi =
        Convert4_NEON(R_hi, G_hi, B_hi, coeffs_v, bias_v, neg_shift);
    const int16x8_t u16 = vcombine_s16(vqmovn_s32(u_lo), vqmovn_s32(u_hi));
    const int16x8_t v16 = vcombine_s16(vqmovn_s32(v_lo), vqmovn_s32(v_hi));

    if (yuv_bit_depth <= 8) {
      vst1_u8((uint8_t*)u_out + i, vqmovun_s16(u16));
      vst1_u8((uint8_t*)v_out + i, vqmovun_s16(v16));
    } else {
      const int16x8_t zero = vdupq_n_s16(0);
      const int16x8_t maxv = vdupq_n_s16((int16_t)yuv_max);
      vst1q_u16((uint16_t*)u_out + i,
                vreinterpretq_u16_s16(vmaxq_s16(vminq_s16(u16, maxv), zero)));
      vst1q_u16((uint16_t*)v_out + i,
                vreinterpretq_u16_s16(vmaxq_s16(vminq_s16(v16, maxv), zero)));
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
// sRGB gamma<->linear fast paths for UpdateW/UpdateChroma (the dominant cost
// in the main convergence loop). NEON has no gather instruction, unfortunately.

// Widens 8 gamma samples to 16.16 fixed-point linear values.
static WEBP_INLINE void GammaToLinear8_NEON(uint16x8_t v16, int shift_right,
                                            uint32x4_t* out_lo,
                                            uint32x4_t* out_hi) {
  const int32x4_t neg_shift = vdupq_n_s32(-shift_right);
  const int32x4_t half =
      vdupq_n_s32(shift_right > 0 ? (1 << (shift_right - 1)) : 0);
  const int32x4_t v_lo = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(v16)));
  const int32x4_t v_hi = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(v16)));
  const int32x4_t pos_lo = vshlq_s32(v_lo, neg_shift);  // tab_pos
  const int32x4_t pos_hi = vshlq_s32(v_hi, neg_shift);
  int32_t idx[8];
  uint32_t v0v[8], v1v[8];
  int lane;
  vst1q_s32(idx + 0, pos_lo);
  vst1q_s32(idx + 4, pos_hi);
  for (lane = 0; lane < 8; ++lane) {
    v0v[lane] = kSharpYuvGammaToLinearTabS[idx[lane]];
    v1v[lane] = kSharpYuvGammaToLinearTabS[idx[lane] + 1];
  }
  {
    const int32x4_t shift_amt = vdupq_n_s32(shift_right);
    const int32x4_t x_lo = vsubq_s32(v_lo, vshlq_s32(pos_lo, shift_amt));
    const int32x4_t x_hi = vsubq_s32(v_hi, vshlq_s32(pos_hi, shift_amt));
    const int32x4_t v0_lo = vreinterpretq_s32_u32(vld1q_u32(v0v + 0));
    const int32x4_t v0_hi = vreinterpretq_s32_u32(vld1q_u32(v0v + 4));
    const int32x4_t v1_lo = vreinterpretq_s32_u32(vld1q_u32(v1v + 0));
    const int32x4_t v1_hi = vreinterpretq_s32_u32(vld1q_u32(v1v + 4));
    // d (<=65536) * x (< 2^shift_right <= 16): fits comfortably in int32.
    const int32x4_t prod_lo = vmulq_s32(vsubq_s32(v1_lo, v0_lo), x_lo);
    const int32x4_t prod_hi = vmulq_s32(vsubq_s32(v1_hi, v0_hi), x_hi);
    const int32x4_t shifted_lo = vshlq_s32(vaddq_s32(prod_lo, half), neg_shift);
    const int32x4_t shifted_hi = vshlq_s32(vaddq_s32(prod_hi, half), neg_shift);
    *out_lo = vreinterpretq_u32_s32(vaddq_s32(v0_lo, shifted_lo));
    *out_hi = vreinterpretq_u32_s32(vaddq_s32(v0_hi, shifted_hi));
  }
}

// Reverse direction: 8 16.16 fixed-point linear values -> 8 gamma samples
static WEBP_INLINE uint16x8_t LinearToGamma8_NEON(uint32x4_t Y_lo,
                                                  uint32x4_t Y_hi,
                                                  int out_shift) {
  const uint32x4_t tab_pos_lo = vshrq_n_u32(Y_lo, 7);  // constant shift: the
  const uint32x4_t tab_pos_hi = vshrq_n_u32(Y_hi, 7);  // table bits are fixed
  const uint32x4_t x_lo = vsubq_u32(Y_lo, vshlq_n_u32(tab_pos_lo, 7));
  const uint32x4_t x_hi = vsubq_u32(Y_hi, vshlq_n_u32(tab_pos_hi, 7));
  int32_t idx[8];
  uint32_t v0v[8], v1v[8];
  int lane;
  vst1q_u32((uint32_t*)(idx + 0), tab_pos_lo);
  vst1q_u32((uint32_t*)(idx + 4), tab_pos_hi);
  for (lane = 0; lane < 8; ++lane) {
    v0v[lane] = kSharpYuvLinearToGammaTabS[idx[lane]] >> out_shift;
    v1v[lane] = kSharpYuvLinearToGammaTabS[idx[lane] + 1] >> out_shift;
  }
  {
    const int32x4_t v0_lo = vreinterpretq_s32_u32(vld1q_u32(v0v + 0));
    const int32x4_t v0_hi = vreinterpretq_s32_u32(vld1q_u32(v0v + 4));
    const int32x4_t v1_lo = vreinterpretq_s32_u32(vld1q_u32(v1v + 0));
    const int32x4_t v1_hi = vreinterpretq_s32_u32(vld1q_u32(v1v + 4));
    // d (<=65536) * x (< 128): fits comfortably in int32.
    const int32x4_t prod_lo =
        vmulq_s32(vsubq_s32(v1_lo, v0_lo), vreinterpretq_s32_u32(x_lo));
    const int32x4_t prod_hi =
        vmulq_s32(vsubq_s32(v1_hi, v0_hi), vreinterpretq_s32_u32(x_hi));
    const int32x4_t sum_lo = vaddq_s32(prod_lo, vdupq_n_s32(64));
    const int32x4_t sum_hi = vaddq_s32(prod_hi, vdupq_n_s32(64));
    const int32x4_t out_lo = vaddq_s32(v0_lo, vshrq_n_s32(sum_lo, 7));
    const int32x4_t out_hi = vaddq_s32(v0_hi, vshrq_n_s32(sum_hi, 7));
    // Truncating narrow (not saturating): matches the plain uint16_t cast in
    // FromLinearSrgb().
    return vreinterpretq_u16_s16(
        vcombine_s16(vmovn_s32(out_lo), vmovn_s32(out_hi)));
  }
}

// (13933*R + 46871*G + 4732*B + 32768) >> 16, in exact uint64_t precision.
static WEBP_INLINE uint32x4_t SharpYuvRGBToGray4_NEON(uint32x4_t R,
                                                      uint32x4_t G,
                                                      uint32x4_t B) {
  const uint64x2_t bias = vdupq_n_u64(1 << (SHARPYUV_YUV_FIX - 1));
  uint64x2_t lo = vmlal_u32(bias, vget_low_u32(R), vdup_n_u32(13933));
  uint64x2_t hi = vmlal_u32(bias, vget_high_u32(R), vdup_n_u32(13933));
  lo = vmlal_u32(lo, vget_low_u32(G), vdup_n_u32(46871));
  hi = vmlal_u32(hi, vget_high_u32(G), vdup_n_u32(46871));
  lo = vmlal_u32(lo, vget_low_u32(B), vdup_n_u32(4732));
  hi = vmlal_u32(hi, vget_high_u32(B), vdup_n_u32(4732));
  lo = vshrq_n_u64(lo, SHARPYUV_YUV_FIX);
  hi = vshrq_n_u64(hi, SHARPYUV_YUV_FIX);
  return vcombine_u32(vmovn_u64(lo), vmovn_u64(hi));
}

static void SharpYuvUpdateWSrgb_NEON(const uint16_t* src, uint16_t* dst, int w,
                                     int bit_depth) {
  const int shift_right = bit_depth - SHARPYUV_GAMMA_TO_LINEAR_TAB_BITS;
  const int out_shift = SHARPYUV_GAMMA_TO_LINEAR_BITS - bit_depth;
  int i = 0;
  for (; i + 8 <= w; i += 8) {
    uint32x4_t R_lo, R_hi, G_lo, G_hi, B_lo, B_hi, Y_lo, Y_hi;
    GammaToLinear8_NEON(vld1q_u16(src + 0 * w + i), shift_right, &R_lo, &R_hi);
    GammaToLinear8_NEON(vld1q_u16(src + 1 * w + i), shift_right, &G_lo, &G_hi);
    GammaToLinear8_NEON(vld1q_u16(src + 2 * w + i), shift_right, &B_lo, &B_hi);
    Y_lo = SharpYuvRGBToGray4_NEON(R_lo, G_lo, B_lo);
    Y_hi = SharpYuvRGBToGray4_NEON(R_hi, G_hi, B_hi);
    vst1q_u16(dst + i, LinearToGamma8_NEON(Y_lo, Y_hi, out_shift));
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

// Box-downsamples 8 consecutive uv-position 2x2 groups (read from 2 rows,
// deinterleaved via vld2q). cf. ScaleDown().
static WEBP_INLINE uint16x8_t ScaleDown8_NEON(const uint16_t* p1,
                                              const uint16_t* p2,
                                              int shift_right, int out_shift) {
  const uint16x8x2_t ab = vld2q_u16(p1);
  const uint16x8x2_t cd = vld2q_u16(p2);
  uint32x4_t A_lo, A_hi, B_lo, B_hi, C_lo, C_hi, D_lo, D_hi;
  GammaToLinear8_NEON(ab.val[0], shift_right, &A_lo, &A_hi);
  GammaToLinear8_NEON(ab.val[1], shift_right, &B_lo, &B_hi);
  GammaToLinear8_NEON(cd.val[0], shift_right, &C_lo, &C_hi);
  GammaToLinear8_NEON(cd.val[1], shift_right, &D_lo, &D_hi);
  {
    const uint32x4_t two = vdupq_n_u32(2);
    const uint32x4_t sum_lo =
        vaddq_u32(vaddq_u32(vaddq_u32(A_lo, B_lo), vaddq_u32(C_lo, D_lo)), two);
    const uint32x4_t sum_hi =
        vaddq_u32(vaddq_u32(vaddq_u32(A_hi, B_hi), vaddq_u32(C_hi, D_hi)), two);
    return LinearToGamma8_NEON(vshrq_n_u32(sum_lo, 2), vshrq_n_u32(sum_hi, 2),
                               out_shift);
  }
}

static void SharpYuvUpdateChromaSrgb_NEON(const uint16_t* src1,
                                          const uint16_t* src2, int16_t* dst,
                                          int uv_w, int bit_depth) {
  const int shift_right = bit_depth - SHARPYUV_GAMMA_TO_LINEAR_TAB_BITS;
  const int out_shift = SHARPYUV_GAMMA_TO_LINEAR_BITS - bit_depth;
  int i = 0;
  for (; i + 8 <= uv_w; i += 8) {
    const int off = 2 * i;
    const uint16x8_t r16 = ScaleDown8_NEON(
        src1 + off + 0 * uv_w, src2 + off + 0 * uv_w, shift_right, out_shift);
    const uint16x8_t g16 = ScaleDown8_NEON(
        src1 + off + 2 * uv_w, src2 + off + 2 * uv_w, shift_right, out_shift);
    const uint16x8_t b16 = ScaleDown8_NEON(
        src1 + off + 4 * uv_w, src2 + off + 4 * uv_w, shift_right, out_shift);
    const uint32x4_t r_lo = vmovl_u16(vget_low_u16(r16));
    const uint32x4_t r_hi = vmovl_u16(vget_high_u16(r16));
    const uint32x4_t g_lo = vmovl_u16(vget_low_u16(g16));
    const uint32x4_t g_hi = vmovl_u16(vget_high_u16(g16));
    const uint32x4_t b_lo = vmovl_u16(vget_low_u16(b16));
    const uint32x4_t b_hi = vmovl_u16(vget_high_u16(b16));
    const uint32x4_t w_lo = SharpYuvRGBToGray4_NEON(r_lo, g_lo, b_lo);
    const uint32x4_t w_hi = SharpYuvRGBToGray4_NEON(r_hi, g_hi, b_hi);
    const int16x8_t dr =
        vcombine_s16(vmovn_s32(vsubq_s32(vreinterpretq_s32_u32(r_lo),
                                         vreinterpretq_s32_u32(w_lo))),
                     vmovn_s32(vsubq_s32(vreinterpretq_s32_u32(r_hi),
                                         vreinterpretq_s32_u32(w_hi))));
    const int16x8_t dg =
        vcombine_s16(vmovn_s32(vsubq_s32(vreinterpretq_s32_u32(g_lo),
                                         vreinterpretq_s32_u32(w_lo))),
                     vmovn_s32(vsubq_s32(vreinterpretq_s32_u32(g_hi),
                                         vreinterpretq_s32_u32(w_hi))));
    const int16x8_t db =
        vcombine_s16(vmovn_s32(vsubq_s32(vreinterpretq_s32_u32(b_lo),
                                         vreinterpretq_s32_u32(w_lo))),
                     vmovn_s32(vsubq_s32(vreinterpretq_s32_u32(b_hi),
                                         vreinterpretq_s32_u32(w_hi))));
    vst1q_s16(dst + i + 0 * uv_w, dr);
    vst1q_s16(dst + i + 1 * uv_w, dg);
    vst1q_s16(dst + i + 2 * uv_w, db);
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

extern void InitSharpYuvNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void InitSharpYuvNEON(void) {
  SharpYuvUpdateY = SharpYuvUpdateY_NEON;
  SharpYuvUpdateRGB = SharpYuvUpdateRGB_NEON;
  SharpYuvFilterRow = SharpYuvFilterRow_NEON;
  SharpYuvUpdateWSrgb = SharpYuvUpdateWSrgb_NEON;
  SharpYuvUpdateChromaSrgb = SharpYuvUpdateChromaSrgb_NEON;
  SharpYuvConvertRowY = SharpYuvConvertRowY_NEON;
  SharpYuvConvertRowUV = SharpYuvConvertRowUV_NEON;
}

#else  // !WEBP_USE_NEON

extern void InitSharpYuvNEON(void);

void InitSharpYuvNEON(void) {}

#endif  // WEBP_USE_NEON
