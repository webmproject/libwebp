// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Sharp RGB to YUV conversion.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./sharpyuv.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "src/webp/types.h"
#include "src/dsp/cpu.h"
#include "sharpyuv/sharpyuv_dsp.h"

//------------------------------------------------------------------------------
// Code for gamma correction

// gamma-compensates loss of resolution during chroma subsampling
#define kGamma 0.80      // for now we use a different gamma value than kGammaF
#define kGammaFix 12     // fixed-point precision for linear values
#define kGammaScale ((1 << kGammaFix) - 1)
#define kGammaTabFix 7   // fixed-point fractional bits precision
#define kGammaTabScale (1 << kGammaTabFix)
#define kGammaTabRounder (kGammaTabScale >> 1)
#define kGammaTabSize (1 << (kGammaFix - kGammaTabFix))

enum {
  YUV_FIX = 16,                    // fixed-point precision for RGB->YUV
  YUV_HALF = 1 << (YUV_FIX - 1),
};

//------------------------------------------------------------------------------
// Sharp RGB->YUV conversion

static const int kNumIterations = 4;
static const int kMinDimensionIterativeConversion = 4;

// We could use SFIX=0 and only uint8_t for fixed_y_t, but it produces some
// banding sometimes. Better use extra precision.
#define SFIX 2                // fixed-point precision of RGB and Y/W
typedef int16_t fixed_t;      // signed type with extra SFIX precision for UV
typedef uint16_t fixed_y_t;   // unsigned type with extra SFIX precision for W

#define SHALF (1 << SFIX >> 1)
#define MAX_Y_T ((256 << SFIX) - 1)
#define SROUNDER (1 << (YUV_FIX + SFIX - 1))

// We use tables of different size and precision for the Rec709 / BT2020
// transfer function.
#define kGammaF (1./0.45)
static uint32_t kLinearToGammaTabS[kGammaTabSize + 2];
#define GAMMA_TO_LINEAR_BITS 14
static uint32_t kGammaToLinearTabS[MAX_Y_T + 1];   // size scales with Y_FIX
static volatile int kGammaTablesSOk = 0;
static void InitGammaTablesS(void);

WEBP_DSP_INIT_FUNC(InitGammaTablesS) {
  assert(2 * GAMMA_TO_LINEAR_BITS < 32);  // we use uint32_t intermediate values
  if (!kGammaTablesSOk) {
    int v;
    const double norm = 1. / MAX_Y_T;
    const double scale = 1. / kGammaTabSize;
    const double a = 0.09929682680944;
    const double thresh = 0.018053968510807;
    const double final_scale = 1 << GAMMA_TO_LINEAR_BITS;
    for (v = 0; v <= MAX_Y_T; ++v) {
      const double g = norm * v;
      double value;
      if (g <= thresh * 4.5) {
        value = g / 4.5;
      } else {
        const double a_rec = 1. / (1. + a);
        value = pow(a_rec * (g + a), kGammaF);
      }
      kGammaToLinearTabS[v] = (uint32_t)(value * final_scale + .5);
    }
    for (v = 0; v <= kGammaTabSize; ++v) {
      const double g = scale * v;
      double value;
      if (g <= thresh) {
        value = 4.5 * g;
      } else {
        value = (1. + a) * pow(g, 1. / kGammaF) - a;
      }
      // we already incorporate the 1/2 rounding constant here
      kLinearToGammaTabS[v] =
          (uint32_t)(MAX_Y_T * value) + (1 << GAMMA_TO_LINEAR_BITS >> 1);
    }
    // to prevent small rounding errors to cause read-overflow:
    kLinearToGammaTabS[kGammaTabSize + 1] = kLinearToGammaTabS[kGammaTabSize];
    kGammaTablesSOk = 1;
  }
}

// return value has a fixed-point precision of GAMMA_TO_LINEAR_BITS
static WEBP_INLINE uint32_t GammaToLinearS(int v) {
  return kGammaToLinearTabS[v];
}

static WEBP_INLINE uint32_t LinearToGammaS(uint32_t value) {
  // 'value' is in GAMMA_TO_LINEAR_BITS fractional precision
  const uint32_t v = value * kGammaTabSize;
  const uint32_t tab_pos = v >> GAMMA_TO_LINEAR_BITS;
  // fractional part, in GAMMA_TO_LINEAR_BITS fixed-point precision
  const uint32_t x = v - (tab_pos << GAMMA_TO_LINEAR_BITS);  // fractional part
  // v0 / v1 are in GAMMA_TO_LINEAR_BITS fixed-point precision (range [0..1])
  const uint32_t v0 = kLinearToGammaTabS[tab_pos + 0];
  const uint32_t v1 = kLinearToGammaTabS[tab_pos + 1];
  // Final interpolation. Note that rounding is already included.
  const uint32_t v2 = (v1 - v0) * x;    // note: v1 >= v0.
  const uint32_t result = v0 + (v2 >> GAMMA_TO_LINEAR_BITS);
  return result;
}

//------------------------------------------------------------------------------

static uint8_t clip_8b(fixed_t v) {
  return (!(v & ~0xff)) ? (uint8_t)v : (v < 0) ? 0u : 255u;
}

static fixed_y_t clip_y(int y) {
  return (!(y & ~MAX_Y_T)) ? (fixed_y_t)y : (y < 0) ? 0 : MAX_Y_T;
}

//------------------------------------------------------------------------------

static int RGBToGray(int r, int g, int b) {
  const int luma = 13933 * r + 46871 * g + 4732 * b + YUV_HALF;
  return (luma >> YUV_FIX);
}

static uint32_t ScaleDown(int a, int b, int c, int d) {
  const uint32_t A = GammaToLinearS(a);
  const uint32_t B = GammaToLinearS(b);
  const uint32_t C = GammaToLinearS(c);
  const uint32_t D = GammaToLinearS(d);
  return LinearToGammaS((A + B + C + D + 2) >> 2);
}

static WEBP_INLINE void UpdateW(const fixed_y_t* src, fixed_y_t* dst, int w) {
  int i;
  for (i = 0; i < w; ++i) {
    const uint32_t R = GammaToLinearS(src[0 * w + i]);
    const uint32_t G = GammaToLinearS(src[1 * w + i]);
    const uint32_t B = GammaToLinearS(src[2 * w + i]);
    const uint32_t Y = RGBToGray(R, G, B);
    dst[i] = (fixed_y_t)LinearToGammaS(Y);
  }
}

static void UpdateChroma(const fixed_y_t* src1, const fixed_y_t* src2,
                         fixed_t* dst, int uv_w) {
  int i;
  for (i = 0; i < uv_w; ++i) {
    const int r = ScaleDown(src1[0 * uv_w + 0], src1[0 * uv_w + 1],
                            src2[0 * uv_w + 0], src2[0 * uv_w + 1]);
    const int g = ScaleDown(src1[2 * uv_w + 0], src1[2 * uv_w + 1],
                            src2[2 * uv_w + 0], src2[2 * uv_w + 1]);
    const int b = ScaleDown(src1[4 * uv_w + 0], src1[4 * uv_w + 1],
                            src2[4 * uv_w + 0], src2[4 * uv_w + 1]);
    const int W = RGBToGray(r, g, b);
    dst[0 * uv_w] = (fixed_t)(r - W);
    dst[1 * uv_w] = (fixed_t)(g - W);
    dst[2 * uv_w] = (fixed_t)(b - W);
    dst  += 1;
    src1 += 2;
    src2 += 2;
  }
}

static void StoreGray(const fixed_y_t* rgb, fixed_y_t* y, int w) {
  int i;
  for (i = 0; i < w; ++i) {
    y[i] = RGBToGray(rgb[0 * w + i], rgb[1 * w + i], rgb[2 * w + i]);
  }
}

//------------------------------------------------------------------------------

static WEBP_INLINE fixed_y_t Filter2(int A, int B, int W0) {
  const int v0 = (A * 3 + B + 2) >> 2;
  return clip_y(v0 + W0);
}

//------------------------------------------------------------------------------

static WEBP_INLINE fixed_y_t UpLift(uint8_t a) {  // 8bit -> SFIX
  return ((fixed_y_t)a << SFIX) | SHALF;
}

static void ImportOneRow(const uint8_t* const r_ptr,
                         const uint8_t* const g_ptr,
                         const uint8_t* const b_ptr,
                         int step,
                         int pic_width,
                         fixed_y_t* const dst) {
  int i;
  const int w = (pic_width + 1) & ~1;
  for (i = 0; i < pic_width; ++i) {
    const int off = i * step;
    dst[i + 0 * w] = UpLift(r_ptr[off]);
    dst[i + 1 * w] = UpLift(g_ptr[off]);
    dst[i + 2 * w] = UpLift(b_ptr[off]);
  }
  if (pic_width & 1) {  // replicate rightmost pixel
    dst[pic_width + 0 * w] = dst[pic_width + 0 * w - 1];
    dst[pic_width + 1 * w] = dst[pic_width + 1 * w - 1];
    dst[pic_width + 2 * w] = dst[pic_width + 2 * w - 1];
  }
}

static void InterpolateTwoRows(const fixed_y_t* const best_y,
                               const fixed_t* prev_uv,
                               const fixed_t* cur_uv,
                               const fixed_t* next_uv,
                               int w,
                               fixed_y_t* out1,
                               fixed_y_t* out2) {
  const int uv_w = w >> 1;
  const int len = (w - 1) >> 1;   // length to filter
  int k = 3;
  while (k-- > 0) {   // process each R/G/B segments in turn
    // special boundary case for i==0
    out1[0] = Filter2(cur_uv[0], prev_uv[0], best_y[0]);
    out2[0] = Filter2(cur_uv[0], next_uv[0], best_y[w]);

    SharpYUVFilterRow(cur_uv, prev_uv, len, best_y + 0 + 1, out1 + 1);
    SharpYUVFilterRow(cur_uv, next_uv, len, best_y + w + 1, out2 + 1);

    // special boundary case for i == w - 1 when w is even
    if (!(w & 1)) {
      out1[w - 1] = Filter2(cur_uv[uv_w - 1], prev_uv[uv_w - 1],
                            best_y[w - 1 + 0]);
      out2[w - 1] = Filter2(cur_uv[uv_w - 1], next_uv[uv_w - 1],
                            best_y[w - 1 + w]);
    }
    out1 += w;
    out2 += w;
    prev_uv += uv_w;
    cur_uv  += uv_w;
    next_uv += uv_w;
  }
}

static WEBP_INLINE uint8_t ConvertRGBToY(int r, int g, int b) {
  const int luma = 16839 * r + 33059 * g + 6420 * b + SROUNDER;
  return clip_8b(16 + (luma >> (YUV_FIX + SFIX)));
}

static WEBP_INLINE uint8_t ConvertRGBToU(int r, int g, int b) {
  const int u =  -9719 * r - 19081 * g + 28800 * b + SROUNDER;
  return clip_8b(128 + (u >> (YUV_FIX + SFIX)));
}

static WEBP_INLINE uint8_t ConvertRGBToV(int r, int g, int b) {
  const int v = +28800 * r - 24116 * g - 4684 * b + SROUNDER;
  return clip_8b(128 + (v >> (YUV_FIX + SFIX)));
}

static int ConvertWRGBToYUV(const fixed_y_t* best_y, const fixed_t* best_uv,
                            uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
                            int dst_stride_u, uint8_t* dst_v, int dst_stride_v,
                            int width, int height) {
  int i, j;
  const fixed_t* const best_uv_base = best_uv;
  const int w = (width + 1) & ~1;
  const int h = (height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  for (best_uv = best_uv_base, j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      const int off = (i >> 1);
      const int W = best_y[i];
      const int r = best_uv[off + 0 * uv_w] + W;
      const int g = best_uv[off + 1 * uv_w] + W;
      const int b = best_uv[off + 2 * uv_w] + W;
      dst_y[i] = ConvertRGBToY(r, g, b);
    }
    best_y += w;
    best_uv += (j & 1) * 3 * uv_w;
    dst_y += dst_stride_y;
  }
  for (best_uv = best_uv_base, j = 0; j < uv_h; ++j) {
    for (i = 0; i < uv_w; ++i) {
      const int off = i;
      const int r = best_uv[off + 0 * uv_w];
      const int g = best_uv[off + 1 * uv_w];
      const int b = best_uv[off + 2 * uv_w];
      dst_u[i] = ConvertRGBToU(r, g, b);
      dst_v[i] = ConvertRGBToV(r, g, b);
    }
    best_uv += 3 * uv_w;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  return 1;
}

//------------------------------------------------------------------------------
// Main function

static void* SafeMalloc(uint64_t nmemb, size_t size) {
  const uint64_t total_size = nmemb * (uint64_t)size;
  if (total_size != (size_t)total_size) return NULL;
  return malloc((size_t)total_size);
}

#define SAFE_ALLOC(W, H, T) ((T*)SafeMalloc((W) * (H), sizeof(T)))

static int DoSharpArgbToYuv(const uint8_t* r_ptr, const uint8_t* g_ptr,
                            const uint8_t* b_ptr, int step, int rgb_stride,
                            uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
                            int dst_stride_u, uint8_t* dst_v, int dst_stride_v,
                            int width, int height) {
  // we expand the right/bottom border if needed
  const int w = (width + 1) & ~1;
  const int h = (height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  uint64_t prev_diff_y_sum = ~0;
  int j, iter;

  // TODO(skal): allocate one big memory chunk. But for now, it's easier
  // for valgrind debugging to have several chunks.
  fixed_y_t* const tmp_buffer = SAFE_ALLOC(w * 3, 2, fixed_y_t);   // scratch
  fixed_y_t* const best_y_base = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const target_y_base = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const best_rgb_y = SAFE_ALLOC(w, 2, fixed_y_t);
  fixed_t* const best_uv_base = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const target_uv_base = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const best_rgb_uv = SAFE_ALLOC(uv_w * 3, 1, fixed_t);
  fixed_y_t* best_y = best_y_base;
  fixed_y_t* target_y = target_y_base;
  fixed_t* best_uv = best_uv_base;
  fixed_t* target_uv = target_uv_base;
  const uint64_t diff_y_threshold = (uint64_t)(3.0 * w * h);
  int ok;

  if (best_y_base == NULL || best_uv_base == NULL ||
      target_y_base == NULL || target_uv_base == NULL ||
      best_rgb_y == NULL || best_rgb_uv == NULL ||
      tmp_buffer == NULL) {
    ok = 0;
    goto End;
  }

  InitGammaTablesS();
  InitSharpYuv();

  // Import RGB samples to W/RGB representation.
  for (j = 0; j < height; j += 2) {
    const int is_last_row = (j == height - 1);
    fixed_y_t* const src1 = tmp_buffer + 0 * w;
    fixed_y_t* const src2 = tmp_buffer + 3 * w;

    // prepare two rows of input
    ImportOneRow(r_ptr, g_ptr, b_ptr, step, width, src1);
    if (!is_last_row) {
      ImportOneRow(r_ptr + rgb_stride, g_ptr + rgb_stride, b_ptr + rgb_stride,
                   step, width, src2);
    } else {
      memcpy(src2, src1, 3 * w * sizeof(*src2));
    }
    StoreGray(src1, best_y + 0, w);
    StoreGray(src2, best_y + w, w);

    UpdateW(src1, target_y, w);
    UpdateW(src2, target_y + w, w);
    UpdateChroma(src1, src2, target_uv, uv_w);
    memcpy(best_uv, target_uv, 3 * uv_w * sizeof(*best_uv));
    best_y += 2 * w;
    best_uv += 3 * uv_w;
    target_y += 2 * w;
    target_uv += 3 * uv_w;
    r_ptr += 2 * rgb_stride;
    g_ptr += 2 * rgb_stride;
    b_ptr += 2 * rgb_stride;
  }

  // Iterate and resolve clipping conflicts.
  for (iter = 0; iter < kNumIterations; ++iter) {
    const fixed_t* cur_uv = best_uv_base;
    const fixed_t* prev_uv = best_uv_base;
    uint64_t diff_y_sum = 0;

    best_y = best_y_base;
    best_uv = best_uv_base;
    target_y = target_y_base;
    target_uv = target_uv_base;
    for (j = 0; j < h; j += 2) {
      fixed_y_t* const src1 = tmp_buffer + 0 * w;
      fixed_y_t* const src2 = tmp_buffer + 3 * w;
      {
        const fixed_t* const next_uv = cur_uv + ((j < h - 2) ? 3 * uv_w : 0);
        InterpolateTwoRows(best_y, prev_uv, cur_uv, next_uv, w, src1, src2);
        prev_uv = cur_uv;
        cur_uv = next_uv;
      }

      UpdateW(src1, best_rgb_y + 0 * w, w);
      UpdateW(src2, best_rgb_y + 1 * w, w);
      UpdateChroma(src1, src2, best_rgb_uv, uv_w);

      // update two rows of Y and one row of RGB
      diff_y_sum += SharpYUVUpdateY(target_y, best_rgb_y, best_y, 2 * w);
      SharpYUVUpdateRGB(target_uv, best_rgb_uv, best_uv, 3 * uv_w);

      best_y += 2 * w;
      best_uv += 3 * uv_w;
      target_y += 2 * w;
      target_uv += 3 * uv_w;
    }
    // test exit condition
    if (iter > 0) {
      if (diff_y_sum < diff_y_threshold) break;
      if (diff_y_sum > prev_diff_y_sum) break;
    }
    prev_diff_y_sum = diff_y_sum;
  }
  // final reconstruction
  ok = ConvertWRGBToYUV(best_y_base, best_uv_base, dst_y, dst_stride_y, dst_u,
                        dst_stride_u, dst_v, dst_stride_v, width, height);

 End:
  free(best_y_base);
  free(best_uv_base);
  free(target_y_base);
  free(target_uv_base);
  free(best_rgb_y);
  free(best_rgb_uv);
  free(tmp_buffer);
  return ok;
 }
#undef SAFE_ALLOC

int SharpArgbToYuv(const uint8_t* r_ptr, const uint8_t* g_ptr,
                   const uint8_t* b_ptr, int step, int rgb_stride,
                   uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
                   int dst_stride_u, uint8_t* dst_v, int dst_stride_v,
                   int width, int height) {
  if (width < kMinDimensionIterativeConversion ||
      height < kMinDimensionIterativeConversion) {
    return 0;
  }
  return DoSharpArgbToYuv(
      r_ptr, g_ptr, b_ptr, step, rgb_stride, dst_y, dst_stride_y, dst_u,
      dst_stride_u, dst_v, dst_stride_v, width, height);
}

//------------------------------------------------------------------------------
