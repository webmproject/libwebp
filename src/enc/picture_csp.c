// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebPPicture utils for colorspace conversion
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "./vp8enci.h"
#include "../utils/random.h"
#include "../utils/utils.h"
#include "../dsp/yuv.h"

// Uncomment to disable gamma-compression during RGB->U/V averaging
#define USE_GAMMA_COMPRESSION

static const union {
  uint32_t argb;
  uint8_t  bytes[4];
} test_endian = { 0xff000000u };
#define ALPHA_IS_LAST (test_endian.bytes[3] == 0xff)

static WEBP_INLINE uint32_t MakeARGB32(int a, int r, int g, int b) {
  return (((uint32_t)a << 24) | (r << 16) | (g << 8) | b);
}

//------------------------------------------------------------------------------
// Detection of non-trivial transparency

// Returns true if alpha[] has non-0xff values.
static int CheckNonOpaque(const uint8_t* alpha, int width, int height,
                          int x_step, int y_step) {
  if (alpha == NULL) return 0;
  while (height-- > 0) {
    int x;
    for (x = 0; x < width * x_step; x += x_step) {
      if (alpha[x] != 0xff) return 1;  // TODO(skal): check 4/8 bytes at a time.
    }
    alpha += y_step;
  }
  return 0;
}

// Checking for the presence of non-opaque alpha.
int WebPPictureHasTransparency(const WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (!picture->use_argb) {
    return CheckNonOpaque(picture->a, picture->width, picture->height,
                          1, picture->a_stride);
  } else {
    int x, y;
    const uint32_t* argb = picture->argb;
    if (argb == NULL) return 0;
    for (y = 0; y < picture->height; ++y) {
      for (x = 0; x < picture->width; ++x) {
        if (argb[x] < 0xff000000u) return 1;   // test any alpha values != 0xff
      }
      argb += picture->argb_stride;
    }
  }
  return 0;
}

//------------------------------------------------------------------------------
// Code for gamma correction

#if defined(USE_GAMMA_COMPRESSION)

// gamma-compensates loss of resolution during chroma subsampling
#define kGamma 0.80      // for now we use a different gamma value than kGammaF
#define kGammaFix 12     // fixed-point precision for linear values
#define kGammaScale ((1 << kGammaFix) - 1)
#define kGammaTabFix 7   // fixed-point fractional bits precision
#define kGammaTabScale (1 << kGammaTabFix)
#define kGammaTabRounder (kGammaTabScale >> 1)
#define kGammaTabSize (1 << (kGammaFix - kGammaTabFix))

static int kLinearToGammaTab[kGammaTabSize + 1];
static uint16_t kGammaToLinearTab[256];
static int kGammaTablesOk = 0;

static void InitGammaTables(void) {
  if (!kGammaTablesOk) {
    int v;
    const double scale = (double)(1 << kGammaTabFix) / kGammaScale;
    const double norm = 1. / 255.;
    for (v = 0; v <= 255; ++v) {
      kGammaToLinearTab[v] =
          (uint16_t)(pow(norm * v, kGamma) * kGammaScale + .5);
    }
    for (v = 0; v <= kGammaTabSize; ++v) {
      kLinearToGammaTab[v] = (int)(255. * pow(scale * v, 1. / kGamma) + .5);
    }
    kGammaTablesOk = 1;
  }
}

static WEBP_INLINE uint32_t GammaToLinear(uint8_t v) {
  return kGammaToLinearTab[v];
}

static WEBP_INLINE int Interpolate(int v) {
  const int tab_pos = v >> (kGammaTabFix + 2);    // integer part
  const int x = v & ((kGammaTabScale << 2) - 1);  // fractional part
  const int v0 = kLinearToGammaTab[tab_pos];
  const int v1 = kLinearToGammaTab[tab_pos + 1];
  const int y = v1 * x + v0 * ((kGammaTabScale << 2) - x);   // interpolate
  return y;
}

// Convert a linear value 'v' to YUV_FIX+2 fixed-point precision
// U/V value, suitable for RGBToU/V calls.
static WEBP_INLINE int LinearToGamma(uint32_t base_value, int shift) {
  const int y = Interpolate(base_value << shift);   // final uplifted value
  return (y + kGammaTabRounder) >> kGammaTabFix;    // descale
}

#else

static void InitGammaTables(void) {}
static WEBP_INLINE uint32_t GammaToLinear(uint8_t v) { return v; }
static WEBP_INLINE int LinearToGamma(uint32_t base_value, int shift) {
  return (int)(base_value << shift);
}

#endif    // USE_GAMMA_COMPRESSION

//------------------------------------------------------------------------------
// RGB -> YUV conversion

static int RGBToY(int r, int g, int b, VP8Random* const rg) {
  return (rg == NULL) ? VP8RGBToY(r, g, b, YUV_HALF)
                      : VP8RGBToY(r, g, b, VP8RandomBits(rg, YUV_FIX));
}

static int RGBToU(int r, int g, int b, VP8Random* const rg) {
  return (rg == NULL) ? VP8RGBToU(r, g, b, YUV_HALF << 2)
                      : VP8RGBToU(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
}

static int RGBToV(int r, int g, int b, VP8Random* const rg) {
  return (rg == NULL) ? VP8RGBToV(r, g, b, YUV_HALF << 2)
                      : VP8RGBToV(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
}

//------------------------------------------------------------------------------
// Smart RGB->YUV conversion

static const int kNumIterations = 6;
static const int kMinDimensionIterativeConversion = 4;

// We use a-priori a different precision for storing RGB and Y/W components
// We could use YFIX=0 and only uint8_t for fixed_y_t, but it produces some
// banding sometimes. Better use extra precision.
// TODO(skal): cleanup once TFIX/YFIX values are fixed.

typedef int16_t fixed_t;      // signed type with extra TFIX precision for UV
typedef uint16_t fixed_y_t;   // unsigned type with extra YFIX precision for W
#define TFIX 6   // fixed-point precision of RGB
#define YFIX 2   // fixed point precision for Y/W

#define THALF ((1 << TFIX) >> 1)
#define MAX_Y_T ((256 << YFIX) - 1)
#define TROUNDER (1 << (YUV_FIX + TFIX - 1))

#if defined(USE_GAMMA_COMPRESSION)

// float variant of gamma-correction
// We use tables of different size and precision, along with a 'real-world'
// Gamma value close to ~2.
#define kGammaF 2.2
static float kGammaToLinearTabF[MAX_Y_T + 1];   // size scales with Y_FIX
static float kLinearToGammaTabF[kGammaTabSize + 2];
static int kGammaTablesFOk = 0;

static void InitGammaTablesF(void) {
  if (!kGammaTablesFOk) {
    int v;
    const double norm = 1. / MAX_Y_T;
    const double scale = 1. / kGammaTabSize;
    for (v = 0; v <= MAX_Y_T; ++v) {
      kGammaToLinearTabF[v] = (float)pow(norm * v, kGammaF);
    }
    for (v = 0; v <= kGammaTabSize; ++v) {
      kLinearToGammaTabF[v] = (float)(MAX_Y_T * pow(scale * v, 1. / kGammaF));
    }
    // to prevent small rounding errors to cause read-overflow:
    kLinearToGammaTabF[kGammaTabSize + 1] = kLinearToGammaTabF[kGammaTabSize];
    kGammaTablesFOk = 1;
  }
}

static WEBP_INLINE float GammaToLinearF(int v) {
  return kGammaToLinearTabF[v];
}

static WEBP_INLINE float LinearToGammaF(float value) {
  const float v = value * kGammaTabSize;
  const int tab_pos = (int)v;
  const float x = v - (float)tab_pos;      // fractional part
  const float v0 = kLinearToGammaTabF[tab_pos + 0];
  const float v1 = kLinearToGammaTabF[tab_pos + 1];
  const float y = v1 * x + v0 * (1.f - x);  // interpolate
  return y;
}

#else

static void InitGammaTablesF(void) {}
static WEBP_INLINE float GammaToLinearF(int v) {
  const float norm = 1.f / MAX_Y_T;
  return norm * v;
}
static WEBP_INLINE float LinearToGammaF(float value) {
  return MAX_Y_T * value;
}

#endif    // USE_GAMMA_COMPRESSION

//------------------------------------------------------------------------------

// precision: YFIX -> TFIX
static WEBP_INLINE int FixedYToW(int v) {
#if TFIX == YFIX
  return v;
#elif TFIX >= YFIX
  return v << (TFIX - YFIX);
#else
  return v >> (YFIX - TFIX);
#endif
}

static WEBP_INLINE int FixedWToY(int v) {
#if TFIX == YFIX
  return v;
#elif YFIX >= TFIX
  return v << (YFIX - TFIX);
#else
  return v >> (TFIX - YFIX);
#endif
}

static uint8_t clip_8b(fixed_t v) {
  return (!(v & ~0xff)) ? (uint8_t)v : (v < 0) ? 0u : 255u;
}

static fixed_y_t clip_y(int y) {
  return (!(y & ~MAX_Y_T)) ? (fixed_y_t)y : (y < 0) ? 0 : MAX_Y_T;
}

// precision: TFIX -> YFIX
static fixed_y_t clip_fixed_t(fixed_t v) {
  const int y = FixedWToY(v);
  const fixed_y_t w = clip_y(y);
  return w;
}

//------------------------------------------------------------------------------

static int RGBToGray(int r, int g, int b) {
  const int luma = 19595 * r + 38470 * g + 7471 * b + YUV_HALF;
  return (luma >> YUV_FIX);
}

static float RGBToGrayF(float r, float g, float b) {
  return 0.299f * r + 0.587f * g + 0.114f * b;
}

static float ScaleDown(int a, int b, int c, int d) {
  const float A = GammaToLinearF(a);
  const float B = GammaToLinearF(b);
  const float C = GammaToLinearF(c);
  const float D = GammaToLinearF(d);
  return LinearToGammaF(0.25f * (A + B + C + D));
}

static WEBP_INLINE void UpdateW(const fixed_y_t* src, fixed_y_t* dst, int len) {
  while (len-- > 0) {
    const float R = GammaToLinearF(src[0]);
    const float G = GammaToLinearF(src[1]);
    const float B = GammaToLinearF(src[2]);
    const float Y = RGBToGrayF(R, G, B);
    *dst++ = (fixed_y_t)(LinearToGammaF(Y) + .5);
    src += 3;
  }
}

static WEBP_INLINE void UpdateChroma(const fixed_y_t* src1,
                                     const fixed_y_t* src2,
                                     fixed_t* dst, fixed_y_t* tmp, int len) {
  while (len--> 0) {
    const float r = ScaleDown(src1[0], src1[3], src2[0], src2[3]);
    const float g = ScaleDown(src1[1], src1[4], src2[1], src2[4]);
    const float b = ScaleDown(src1[2], src1[5], src2[2], src2[5]);
    const float W = RGBToGrayF(r, g, b);
    dst[0] = (fixed_t)FixedYToW((int)(r - W));
    dst[1] = (fixed_t)FixedYToW((int)(g - W));
    dst[2] = (fixed_t)FixedYToW((int)(b - W));
    dst += 3;
    src1 += 6;
    src2 += 6;
    if (tmp != NULL) {
      tmp[0] = tmp[1] = clip_y((int)(W + .5));
      tmp += 2;
    }
  }
}

//------------------------------------------------------------------------------

static WEBP_INLINE int Filter(const fixed_t* const A, const fixed_t* const B,
                              int rightwise) {
  int v;
  if (!rightwise) {
    v = (A[0] * 9 + A[-3] * 3 + B[0] * 3 + B[-3]);
  } else {
    v = (A[0] * 9 + A[+3] * 3 + B[0] * 3 + B[+3]);
  }
  return (v + 8) >> 4;
}

static WEBP_INLINE int Filter2(int A, int B) { return (A * 3 + B + 2) >> 2; }

//------------------------------------------------------------------------------

// 8bit -> YFIX
static WEBP_INLINE fixed_y_t UpLift(uint8_t a) {
  return ((fixed_y_t)a << YFIX) | (1 << (YFIX - 1));
}

static void ImportOneRow(const uint8_t* const r_ptr,
                         const uint8_t* const g_ptr,
                         const uint8_t* const b_ptr,
                         int step,
                         int pic_width,
                         fixed_y_t* const dst) {
  int i;
  for (i = 0; i < pic_width; ++i) {
    const int off = i * step;
    dst[3 * i + 0] = UpLift(r_ptr[off]);
    dst[3 * i + 1] = UpLift(g_ptr[off]);
    dst[3 * i + 2] = UpLift(b_ptr[off]);
  }
  if (pic_width & 1) {  // replicate rightmost pixel
    memcpy(dst + 3 * pic_width, dst + 3 * (pic_width - 1), 3 * sizeof(*dst));
  }
}

static void InterpolateTwoRows(const fixed_y_t* const best_y,
                               const fixed_t* const prev_uv,
                               const fixed_t* const cur_uv,
                               const fixed_t* const next_uv,
                               int w,
                               fixed_y_t* const out1,
                               fixed_y_t* const out2) {
  int i, k;
  {  // special boundary case for i==0
    const int W0 = FixedYToW(best_y[0]);
    const int W1 = FixedYToW(best_y[w]);
    for (k = 0; k <= 2; ++k) {
      out1[k] = clip_fixed_t(Filter2(cur_uv[k], prev_uv[k]) + W0);
      out2[k] = clip_fixed_t(Filter2(cur_uv[k], next_uv[k]) + W1);
    }
  }
  for (i = 1; i < w - 1; ++i) {
    const int W0 = FixedYToW(best_y[i + 0]);
    const int W1 = FixedYToW(best_y[i + w]);
    const int off = 3 * (i >> 1);
    for (k = 0; k <= 2; ++k) {
      const int tmp0 = Filter(cur_uv + off + k, prev_uv + off + k, i & 1);
      const int tmp1 = Filter(cur_uv + off + k, next_uv + off + k, i & 1);
      out1[3 * i + k] = clip_fixed_t(tmp0 + W0);
      out2[3 * i + k] = clip_fixed_t(tmp1 + W1);
    }
  }
  {  // special boundary case for i == w - 1
    const int W0 = FixedYToW(best_y[i + 0]);
    const int W1 = FixedYToW(best_y[i + w]);
    const int off = 3 * (i >> 1);
    for (k = 0; k <= 2; ++k) {
      out1[3 * i + k] =
          clip_fixed_t(Filter2(cur_uv[off + k], prev_uv[off + k]) + W0);
      out2[3 * i + k] =
          clip_fixed_t(Filter2(cur_uv[off + k], next_uv[off + k]) + W1);
    }
  }
}

static WEBP_INLINE uint8_t ConvertRGBToY(int r, int g, int b) {
  const int luma = 16839 * r + 33059 * g + 6420 * b + TROUNDER;
  return clip_8b(16 + (luma >> (YUV_FIX + TFIX)));
}

static WEBP_INLINE uint8_t ConvertRGBToU(int r, int g, int b) {
  const int u =  -9719 * r - 19081 * g + 28800 * b + TROUNDER;
  return clip_8b(128 + (u >> (YUV_FIX + TFIX)));
}

static WEBP_INLINE uint8_t ConvertRGBToV(int r, int g, int b) {
  const int v = +28800 * r - 24116 * g -  4684 * b + TROUNDER;
  return clip_8b(128 + (v >> (YUV_FIX + TFIX)));
}

static int ConvertWRGBToYUV(const fixed_y_t* const best_y,
                            const fixed_t* const best_uv,
                            WebPPicture* const picture) {
  int i, j;
  const int w = (picture->width + 1) & ~1;
  const int h = (picture->height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  for (j = 0; j < picture->height; ++j) {
    for (i = 0; i < picture->width; ++i) {
      const int off = 3 * ((i >> 1) + (j >> 1) * uv_w);
      const int off2 = i + j * picture->y_stride;
      const int W = FixedYToW(best_y[i + j * w]);
      const int r = best_uv[off + 0] + W;
      const int g = best_uv[off + 1] + W;
      const int b = best_uv[off + 2] + W;
      picture->y[off2] = ConvertRGBToY(r, g, b);
    }
  }
  for (j = 0; j < uv_h; ++j) {
    uint8_t* const dst_u = picture->u + j * picture->uv_stride;
    uint8_t* const dst_v = picture->v + j * picture->uv_stride;
    for (i = 0; i < uv_w; ++i) {
      const int off = 3 * (i + j * uv_w);
      const int r = best_uv[off + 0];
      const int g = best_uv[off + 1];
      const int b = best_uv[off + 2];
      dst_u[i] = ConvertRGBToU(r, g, b);
      dst_v[i] = ConvertRGBToV(r, g, b);
    }
  }
  return 1;
}


//------------------------------------------------------------------------------
// Main function

#define SAFE_ALLOC(W, H, T) ((T*)WebPSafeMalloc((W) * (H), sizeof(T)))

static int PreprocessARGB(const uint8_t* const r_ptr,
                          const uint8_t* const g_ptr,
                          const uint8_t* const b_ptr,
                          int step, int rgb_stride,
                          WebPPicture* const picture) {
  // we expand the right/bottom border if needed
  const int w = (picture->width + 1) & ~1;
  const int h = (picture->height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  int i, j, iter;

  // TODO(skal): allocate one big memory chunk. But for now, it's easier
  // for valgrind debugging to have several chunks.
  fixed_y_t* const tmp_buffer = SAFE_ALLOC(w * 3, 2, fixed_y_t);   // scratch
  fixed_y_t* const best_y = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const target_y = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const best_rgb_y = SAFE_ALLOC(w, 2, fixed_y_t);
  fixed_t* const best_uv = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const target_uv = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const best_rgb_uv = SAFE_ALLOC(uv_w * 3, 1, fixed_t);
  int ok;

  if (best_y == NULL || best_uv == NULL ||
      target_y == NULL || target_uv == NULL ||
      best_rgb_y == NULL || best_rgb_uv == NULL ||
      tmp_buffer == NULL) {
    ok = WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    goto End;
  }
  assert(picture->width >= kMinDimensionIterativeConversion);
  assert(picture->height >= kMinDimensionIterativeConversion);

  // Import RGB samples to W/RGB representation.
  for (j = 0; j < picture->height; j += 2) {
    const int is_last_row = (j == picture->height - 1);
    fixed_y_t* const src1 = tmp_buffer;
    fixed_y_t* const src2 = tmp_buffer + 3 * w;
    const int off1 = j * rgb_stride;
    const int off2 = off1 + rgb_stride;
    const int uv_off = (j >> 1) * 3 * uv_w;
    fixed_y_t* const dst_y = best_y + j * w;

    // prepare two rows of input
    ImportOneRow(r_ptr + off1, g_ptr + off1, b_ptr + off1,
                 step, picture->width, src1);
    if (!is_last_row) {
      ImportOneRow(r_ptr + off2, g_ptr + off2, b_ptr + off2,
                   step, picture->width, src2);
    } else {
      memcpy(src2, src1, 3 * w * sizeof(*src2));
    }
    UpdateW(src1, target_y + (j + 0) * w, w);
    UpdateW(src2, target_y + (j + 1) * w, w);
    UpdateChroma(src1, src2, target_uv + uv_off, dst_y, uv_w);
    memcpy(best_uv + uv_off, target_uv + uv_off, 3 * uv_w * sizeof(*best_uv));
    memcpy(dst_y + w, dst_y, w * sizeof(*dst_y));
  }

  // Iterate and resolve clipping conflicts.
  for (iter = 0; iter < kNumIterations; ++iter) {
    int k;
    const fixed_t* cur_uv = best_uv;
    const fixed_t* prev_uv = best_uv;
    for (j = 0; j < h; j += 2) {
      fixed_y_t* const src1 = tmp_buffer;
      fixed_y_t* const src2 = tmp_buffer + 3 * w;

      {
        const fixed_t* const next_uv = cur_uv + ((j < h - 2) ? 3 * uv_w : 0);
        InterpolateTwoRows(best_y + j * w, prev_uv, cur_uv, next_uv,
                           w, src1, src2);
        prev_uv = cur_uv;
        cur_uv = next_uv;
      }

      UpdateW(src1, best_rgb_y + 0 * w, w);
      UpdateW(src2, best_rgb_y + 1 * w, w);
      UpdateChroma(src1, src2, best_rgb_uv, NULL, uv_w);

      // update two rows of Y and one row of RGB
      for (i = 0; i < 2 * w; ++i) {
        const int off = i + j * w;
        const int diff_y = target_y[off] - best_rgb_y[i];
        const int new_y = (int)best_y[off] + diff_y;
        best_y[off] = clip_y(new_y);
      }
      for (i = 0; i < uv_w; ++i) {
        const int off = 3 * (i + (j >> 1) * uv_w);
        int W;
        for (k = 0; k <= 2; ++k) {
          const int diff_uv = (int)target_uv[off + k] - best_rgb_uv[3 * i + k];
          best_uv[off + k] += diff_uv;
        }
        W = RGBToGray(best_uv[off + 0], best_uv[off + 1], best_uv[off + 2]);
        for (k = 0; k <= 2; ++k) {
          best_uv[off + k] -= W;
        }
      }
    }
    // TODO(skal): add early-termination criterion
  }

  // final reconstruction
  ok = ConvertWRGBToYUV(best_y, best_uv, picture);

 End:
  WebPSafeFree(best_y);
  WebPSafeFree(best_uv);
  WebPSafeFree(target_y);
  WebPSafeFree(target_uv);
  WebPSafeFree(best_rgb_y);
  WebPSafeFree(best_rgb_uv);
  WebPSafeFree(tmp_buffer);
  return ok;
}
#undef SAFE_ALLOC

//------------------------------------------------------------------------------
// "Fast" regular RGB->YUV

#define SUM4(ptr) LinearToGamma(                           \
    GammaToLinear((ptr)[0]) +                              \
    GammaToLinear((ptr)[step]) +                           \
    GammaToLinear((ptr)[rgb_stride]) +                     \
    GammaToLinear((ptr)[rgb_stride + step]), 0)            \

#define SUM2V(ptr) \
    LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[rgb_stride]), 1)

static WEBP_INLINE void ConvertRowToY(const uint8_t* const r_ptr,
                                      const uint8_t* const g_ptr,
                                      const uint8_t* const b_ptr,
                                      int step,
                                      uint8_t* const dst_y,
                                      int width,
                                      VP8Random* const rg) {
  int i, j;
  for (i = 0, j = 0; i < width; ++i, j += step) {
    dst_y[i] = RGBToY(r_ptr[j], g_ptr[j], b_ptr[j], rg);
  }
}

static WEBP_INLINE void ConvertRowsToUV(const uint8_t* const r_ptr,
                                        const uint8_t* const g_ptr,
                                        const uint8_t* const b_ptr,
                                        int step, int rgb_stride,
                                        uint8_t* const dst_u,
                                        uint8_t* const dst_v,
                                        int width,
                                        VP8Random* const rg) {
  int i, j;
  for (i = 0, j = 0; i < (width >> 1); ++i, j += 2 * step) {
    const int r = SUM4(r_ptr + j);
    const int g = SUM4(g_ptr + j);
    const int b = SUM4(b_ptr + j);
    dst_u[i] = RGBToU(r, g, b, rg);
    dst_v[i] = RGBToV(r, g, b, rg);
  }
  if (width & 1) {
    const int r = SUM2V(r_ptr + j);
    const int g = SUM2V(g_ptr + j);
    const int b = SUM2V(b_ptr + j);
    dst_u[i] = RGBToU(r, g, b, rg);
    dst_v[i] = RGBToV(r, g, b, rg);
  }
}

static int ImportYUVAFromRGBA(const uint8_t* const r_ptr,
                              const uint8_t* const g_ptr,
                              const uint8_t* const b_ptr,
                              const uint8_t* const a_ptr,
                              int step,         // bytes per pixel
                              int rgb_stride,   // bytes per scanline
                              float dithering,
                              int use_iterative_conversion,
                              WebPPicture* const picture) {
  int y;
  const int width = picture->width;
  const int height = picture->height;
  const int has_alpha = CheckNonOpaque(a_ptr, width, height, step, rgb_stride);

  picture->colorspace = has_alpha ? WEBP_YUV420A : WEBP_YUV420;
  picture->use_argb = 0;

  // disable smart conversion if source is too small (overkill).
  if (width < kMinDimensionIterativeConversion ||
      height < kMinDimensionIterativeConversion) {
    use_iterative_conversion = 0;
  }

  if (!WebPPictureAllocYUVA(picture, width, height)) {
    return 0;
  }

  if (use_iterative_conversion) {
    InitGammaTablesF();
    if (!PreprocessARGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, picture)) {
      return 0;
    }
  } else {
    uint8_t* dst_y = picture->y;
    uint8_t* dst_u = picture->u;
    uint8_t* dst_v = picture->v;

    VP8Random base_rg;
    VP8Random* rg = NULL;
    if (dithering > 0.) {
      VP8InitRandom(&base_rg, dithering);
      rg = &base_rg;
    }

    InitGammaTables();

    // Downsample Y/U/V planes, two rows at a time
    for (y = 0; y < (height >> 1); ++y) {
      const int off1 = (2 * y + 0) * rgb_stride;
      const int off2 = (2 * y + 1) * rgb_stride;
      ConvertRowToY(r_ptr + off1, g_ptr + off1, b_ptr + off1, step,
                    dst_y, width, rg);
      ConvertRowToY(r_ptr + off2, g_ptr + off2, b_ptr + off2, step,
                    dst_y + picture->y_stride, width, rg);
      dst_y += 2 * picture->y_stride;
      ConvertRowsToUV(r_ptr + off1, g_ptr + off1, b_ptr + off1,
                      step, rgb_stride, dst_u, dst_v, width, rg);
      dst_u += picture->uv_stride;
      dst_v += picture->uv_stride;
    }
    if (height & 1) {    // extra last row
      const int off = 2 * y * rgb_stride;
      ConvertRowToY(r_ptr + off, g_ptr + off, b_ptr + off, step,
                    dst_y, width, rg);
      ConvertRowsToUV(r_ptr + off, g_ptr + off, b_ptr + off,
                      step, 0, dst_u, dst_v, width, rg);
    }
  }

  if (has_alpha) {
    assert(step >= 4);
    assert(picture->a != NULL);
    for (y = 0; y < height; ++y) {
      int x;
      for (x = 0; x < width; ++x) {
        picture->a[x + y * picture->a_stride] =
            a_ptr[step * x + y * rgb_stride];
      }
    }
  }
  return 1;
}

#undef SUM4
#undef SUM2V
#undef SUM2H
#undef SUM1
#undef RGB_TO_UV

//------------------------------------------------------------------------------
// call for ARGB->YUVA conversion

static int PictureARGBToYUVA(WebPPicture* picture, WebPEncCSP colorspace,
                             float dithering, int use_iterative_conversion) {
  if (picture == NULL) return 0;
  if (picture->argb == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  } else if ((colorspace & WEBP_CSP_UV_MASK) != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  } else {
    const uint8_t* const argb = (const uint8_t*)picture->argb;
    const uint8_t* const r = ALPHA_IS_LAST ? argb + 2 : argb + 1;
    const uint8_t* const g = ALPHA_IS_LAST ? argb + 1 : argb + 2;
    const uint8_t* const b = ALPHA_IS_LAST ? argb + 0 : argb + 3;
    const uint8_t* const a = ALPHA_IS_LAST ? argb + 3 : argb + 0;

    picture->colorspace = WEBP_YUV420;
    return ImportYUVAFromRGBA(r, g, b, a, 4, 4 * picture->argb_stride,
                              dithering, use_iterative_conversion, picture);
  }
}

int WebPPictureARGBToYUVADithered(WebPPicture* picture, WebPEncCSP colorspace,
                                  float dithering) {
  return PictureARGBToYUVA(picture, colorspace, dithering, 0);
}

int WebPPictureARGBToYUVA(WebPPicture* picture, WebPEncCSP colorspace) {
  return PictureARGBToYUVA(picture, colorspace, 0.f, 0);
}

int WebPPictureSmartARGBToYUVA(WebPPicture* picture) {
  return PictureARGBToYUVA(picture, WEBP_YUV420, 0.f, 1);
}

//------------------------------------------------------------------------------
// call for YUVA -> ARGB conversion

int WebPPictureYUVAToARGB(WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (picture->y == NULL || picture->u == NULL || picture->v == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_ALPHA_BIT) && picture->a == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_UV_MASK) != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }
  // Allocate a new argb buffer (discarding the previous one).
  if (!WebPPictureAllocARGB(picture, picture->width, picture->height)) return 0;
  picture->use_argb = 1;

  // Convert
  {
    int y;
    const int width = picture->width;
    const int height = picture->height;
    const int argb_stride = 4 * picture->argb_stride;
    uint8_t* dst = (uint8_t*)picture->argb;
    const uint8_t *cur_u = picture->u, *cur_v = picture->v, *cur_y = picture->y;
    WebPUpsampleLinePairFunc upsample = WebPGetLinePairConverter(ALPHA_IS_LAST);

    // First row, with replicated top samples.
    upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    cur_y += picture->y_stride;
    dst += argb_stride;
    // Center rows.
    for (y = 1; y + 1 < height; y += 2) {
      const uint8_t* const top_u = cur_u;
      const uint8_t* const top_v = cur_v;
      cur_u += picture->uv_stride;
      cur_v += picture->uv_stride;
      upsample(cur_y, cur_y + picture->y_stride, top_u, top_v, cur_u, cur_v,
               dst, dst + argb_stride, width);
      cur_y += 2 * picture->y_stride;
      dst += 2 * argb_stride;
    }
    // Last row (if needed), with replicated bottom samples.
    if (height > 1 && !(height & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    }
    // Insert alpha values if needed, in replacement for the default 0xff ones.
    if (picture->colorspace & WEBP_CSP_ALPHA_BIT) {
      for (y = 0; y < height; ++y) {
        uint32_t* const argb_dst = picture->argb + y * picture->argb_stride;
        const uint8_t* const src = picture->a + y * picture->a_stride;
        int x;
        for (x = 0; x < width; ++x) {
          argb_dst[x] = (argb_dst[x] & 0x00ffffffu) | ((uint32_t)src[x] << 24);
        }
      }
    }
  }
  return 1;
}

//------------------------------------------------------------------------------
// automatic import / conversion

static int Import(WebPPicture* const picture,
                  const uint8_t* const rgb, int rgb_stride,
                  int step, int swap_rb, int import_alpha) {
  int y;
  const uint8_t* const r_ptr = rgb + (swap_rb ? 2 : 0);
  const uint8_t* const g_ptr = rgb + 1;
  const uint8_t* const b_ptr = rgb + (swap_rb ? 0 : 2);
  const uint8_t* const a_ptr = import_alpha ? rgb + 3 : NULL;
  const int width = picture->width;
  const int height = picture->height;

  if (!picture->use_argb) {
    return ImportYUVAFromRGBA(r_ptr, g_ptr, b_ptr, a_ptr, step, rgb_stride,
                              0.f /* no dithering */, 0, picture);
  }
  if (!WebPPictureAlloc(picture)) return 0;

  assert(step >= (import_alpha ? 4 : 3));
  for (y = 0; y < height; ++y) {
    uint32_t* const dst = &picture->argb[y * picture->argb_stride];
    int x;
    for (x = 0; x < width; ++x) {
      const int offset = step * x + y * rgb_stride;
      dst[x] = MakeARGB32(import_alpha ? a_ptr[offset] : 0xff,
                          r_ptr[offset], g_ptr[offset], b_ptr[offset]);
    }
  }
  return 1;
}

// Public API

int WebPPictureImportRGB(WebPPicture* picture,
                         const uint8_t* rgb, int rgb_stride) {
  return (picture != NULL) ? Import(picture, rgb, rgb_stride, 3, 0, 0) : 0;
}

int WebPPictureImportBGR(WebPPicture* picture,
                         const uint8_t* rgb, int rgb_stride) {
  return (picture != NULL) ? Import(picture, rgb, rgb_stride, 3, 1, 0) : 0;
}

int WebPPictureImportRGBA(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return (picture != NULL) ? Import(picture, rgba, rgba_stride, 4, 0, 1) : 0;
}

int WebPPictureImportBGRA(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return (picture != NULL) ? Import(picture, rgba, rgba_stride, 4, 1, 1) : 0;
}

int WebPPictureImportRGBX(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return (picture != NULL) ? Import(picture, rgba, rgba_stride, 4, 0, 0) : 0;
}

int WebPPictureImportBGRX(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return (picture != NULL) ? Import(picture, rgba, rgba_stride, 4, 1, 0) : 0;
}

//------------------------------------------------------------------------------
