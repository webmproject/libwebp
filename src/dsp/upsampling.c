// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// YUV to RGB upsampling functions.
//
// Author: somnath@google.com (Somnath Banerjee)

#include "./dsp.h"
#include "./yuv.h"
#include "../dec/webpi.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Fancy upsampler

#ifdef FANCY_UPSAMPLING

// Fancy upsampling functions to convert YUV to RGB
WebPUpsampleLinePairFunc WebPUpsamplers[MODE_LAST];
WebPUpsampleLinePairFunc WebPUpsamplersKeepAlpha[MODE_LAST];

// Given samples laid out in a square as:
//  [a b]
//  [c d]
// we interpolate u/v as:
//  ([9*a + 3*b + 3*c +   d    3*a + 9*b + 3*c +   d] + [8 8]) / 16
//  ([3*a +   b + 9*c + 3*d      a + 3*b + 3*c + 9*d]   [8 8]) / 16

// We process u and v together stashed into 32bit (16bit each).
#define LOAD_UV(u,v) ((u) | ((v) << 16))

#define UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                                  \
static void FUNC_NAME(const uint8_t* top_y, const uint8_t* bottom_y,           \
                      const uint8_t* top_u, const uint8_t* top_v,              \
                      const uint8_t* cur_u, const uint8_t* cur_v,              \
                      uint8_t* top_dst, uint8_t* bottom_dst, int len) {        \
  int x;                                                                       \
  const int last_pixel_pair = (len - 1) >> 1;                                  \
  uint32_t tl_uv = LOAD_UV(top_u[0], top_v[0]);   /* top-left sample */        \
  uint32_t l_uv  = LOAD_UV(cur_u[0], cur_v[0]);   /* left-sample */            \
  if (top_y) {                                                                 \
    const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;                \
    FUNC(top_y[0], uv0 & 0xff, (uv0 >> 16), top_dst);                          \
  }                                                                            \
  if (bottom_y) {                                                              \
    const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;                \
    FUNC(bottom_y[0], uv0 & 0xff, (uv0 >> 16), bottom_dst);                    \
  }                                                                            \
  for (x = 1; x <= last_pixel_pair; ++x) {                                     \
    const uint32_t t_uv = LOAD_UV(top_u[x], top_v[x]);  /* top sample */       \
    const uint32_t uv   = LOAD_UV(cur_u[x], cur_v[x]);  /* sample */           \
    /* precompute invariant values associated with first and second diagonals*/\
    const uint32_t avg = tl_uv + t_uv + l_uv + uv + 0x00080008u;               \
    const uint32_t diag_12 = (avg + 2 * (t_uv + l_uv)) >> 3;                   \
    const uint32_t diag_03 = (avg + 2 * (tl_uv + uv)) >> 3;                    \
    if (top_y) {                                                               \
      const uint32_t uv0 = (diag_12 + tl_uv) >> 1;                             \
      const uint32_t uv1 = (diag_03 + t_uv) >> 1;                              \
      FUNC(top_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                          \
           top_dst + (2 * x - 1) * XSTEP);                                     \
      FUNC(top_y[2 * x - 0], uv1 & 0xff, (uv1 >> 16),                          \
           top_dst + (2 * x - 0) * XSTEP);                                     \
    }                                                                          \
    if (bottom_y) {                                                            \
      const uint32_t uv0 = (diag_03 + l_uv) >> 1;                              \
      const uint32_t uv1 = (diag_12 + uv) >> 1;                                \
      FUNC(bottom_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                       \
           bottom_dst + (2 * x - 1) * XSTEP);                                  \
      FUNC(bottom_y[2 * x + 0], uv1 & 0xff, (uv1 >> 16),                       \
           bottom_dst + (2 * x + 0) * XSTEP);                                  \
    }                                                                          \
    tl_uv = t_uv;                                                              \
    l_uv = uv;                                                                 \
  }                                                                            \
  if (!(len & 1)) {                                                            \
    if (top_y) {                                                               \
      const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;              \
      FUNC(top_y[len - 1], uv0 & 0xff, (uv0 >> 16),                            \
           top_dst + (len - 1) * XSTEP);                                       \
    }                                                                          \
    if (bottom_y) {                                                            \
      const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;              \
      FUNC(bottom_y[len - 1], uv0 & 0xff, (uv0 >> 16),                         \
           bottom_dst + (len - 1) * XSTEP);                                    \
    }                                                                          \
  }                                                                            \
}

// All variants implemented.
UPSAMPLE_FUNC(UpsampleRgbLinePair,  VP8YuvToRgb,  3)
UPSAMPLE_FUNC(UpsampleBgrLinePair,  VP8YuvToBgr,  3)
UPSAMPLE_FUNC(UpsampleRgbaLinePair, VP8YuvToRgba, 4)
UPSAMPLE_FUNC(UpsampleBgraLinePair, VP8YuvToBgra, 4)
UPSAMPLE_FUNC(UpsampleArgbLinePair, VP8YuvToArgb, 4)
UPSAMPLE_FUNC(UpsampleRgba4444LinePair, VP8YuvToRgba4444, 2)
UPSAMPLE_FUNC(UpsampleRgb565LinePair,  VP8YuvToRgb565,  2)
// These two don't erase the alpha value
UPSAMPLE_FUNC(UpsampleRgbKeepAlphaLinePair, VP8YuvToRgb, 4)
UPSAMPLE_FUNC(UpsampleBgrKeepAlphaLinePair, VP8YuvToBgr, 4)
UPSAMPLE_FUNC(UpsampleArgbKeepAlphaLinePair, VP8YuvToArgbKeepA, 4)
UPSAMPLE_FUNC(UpsampleRgba4444KeepAlphaLinePair, VP8YuvToRgba4444KeepA, 2)

#undef LOAD_UV
#undef UPSAMPLE_FUNC

#endif  // FANCY_UPSAMPLING

//------------------------------------------------------------------------------
// simple point-sampling

#define SAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                                    \
static void FUNC_NAME(const uint8_t* top_y, const uint8_t* bottom_y,           \
                      const uint8_t* u, const uint8_t* v,                      \
                      uint8_t* top_dst, uint8_t* bottom_dst, int len) {        \
  int i;                                                                       \
  for (i = 0; i < len - 1; i += 2) {                                           \
    FUNC(top_y[0], u[0], v[0], top_dst);                                       \
    FUNC(top_y[1], u[0], v[0], top_dst + XSTEP);                               \
    FUNC(bottom_y[0], u[0], v[0], bottom_dst);                                 \
    FUNC(bottom_y[1], u[0], v[0], bottom_dst + XSTEP);                         \
    top_y += 2;                                                                \
    bottom_y += 2;                                                             \
    u++;                                                                       \
    v++;                                                                       \
    top_dst += 2 * XSTEP;                                                      \
    bottom_dst += 2 * XSTEP;                                                   \
  }                                                                            \
  if (i == len - 1) {    /* last one */                                        \
    FUNC(top_y[0], u[0], v[0], top_dst);                                       \
    FUNC(bottom_y[0], u[0], v[0], bottom_dst);                                 \
  }                                                                            \
}

// All variants implemented.
SAMPLE_FUNC(SampleRgbLinePair,      VP8YuvToRgb,  3)
SAMPLE_FUNC(SampleBgrLinePair,      VP8YuvToBgr,  3)
SAMPLE_FUNC(SampleRgbaLinePair,     VP8YuvToRgba, 4)
SAMPLE_FUNC(SampleBgraLinePair,     VP8YuvToBgra, 4)
SAMPLE_FUNC(SampleArgbLinePair,     VP8YuvToArgb, 4)
SAMPLE_FUNC(SampleRgba4444LinePair, VP8YuvToRgba4444, 2)
SAMPLE_FUNC(SampleRgb565LinePair,   VP8YuvToRgb565, 2)

#undef SAMPLE_FUNC

const WebPSampleLinePairFunc WebPSamplers[MODE_LAST] = {
  SampleRgbLinePair,       // MODE_RGB
  SampleRgbaLinePair,      // MODE_RGBA
  SampleBgrLinePair,       // MODE_BGR
  SampleBgraLinePair,      // MODE_BGRA
  SampleArgbLinePair,      // MODE_ARGB
  SampleRgba4444LinePair,  // MODE_RGBA_4444
  SampleRgb565LinePair     // MODE_RGB_565
};

//------------------------------------------------------------------------------
// YUV444 converter

#define YUV444_FUNC(FUNC_NAME, FUNC, XSTEP)                                    \
static void FUNC_NAME(const uint8_t* y, const uint8_t* u, const uint8_t* v,    \
                      uint8_t* dst, int len) {                                 \
  int i;                                                                       \
  for (i = 0; i < len; ++i) FUNC(y[i], u[i], v[i], &dst[i * XSTEP]);           \
}

YUV444_FUNC(Yuv444ToRgb,      VP8YuvToRgb,  3)
YUV444_FUNC(Yuv444ToBgr,      VP8YuvToBgr,  3)
YUV444_FUNC(Yuv444ToRgba,     VP8YuvToRgba, 4)
YUV444_FUNC(Yuv444ToBgra,     VP8YuvToBgra, 4)
YUV444_FUNC(Yuv444ToArgb,     VP8YuvToArgb, 4)
YUV444_FUNC(Yuv444ToRgba4444, VP8YuvToRgba4444, 2)
YUV444_FUNC(Yuv444ToRgb565,   VP8YuvToRgb565, 2)

#undef YUV444_FUNC

const WebPYUV444Converter WebPYUV444Converters[MODE_LAST] = {
  Yuv444ToRgb,       // MODE_RGB
  Yuv444ToRgba,      // MODE_RGBA
  Yuv444ToBgr,       // MODE_BGR
  Yuv444ToBgra,      // MODE_BGRA
  Yuv444ToArgb,      // MODE_ARGB
  Yuv444ToRgba4444,  // MODE_RGBA_4444
  Yuv444ToRgb565     // MODE_RGB_565
};

//------------------------------------------------------------------------------
// Main call

void WebPInitUpsamplers(void) {
#ifdef FANCY_UPSAMPLING
  WebPUpsamplers[MODE_RGB]       = UpsampleRgbLinePair;
  WebPUpsamplers[MODE_RGBA]      = UpsampleRgbaLinePair;
  WebPUpsamplers[MODE_BGR]       = UpsampleBgrLinePair;
  WebPUpsamplers[MODE_BGRA]      = UpsampleBgraLinePair;
  WebPUpsamplers[MODE_ARGB]      = UpsampleArgbLinePair;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair;
  WebPUpsamplers[MODE_RGB_565]   = UpsampleRgb565LinePair;

  WebPUpsamplersKeepAlpha[MODE_RGB]       = UpsampleRgbLinePair;
  WebPUpsamplersKeepAlpha[MODE_RGBA]      = UpsampleRgbKeepAlphaLinePair;
  WebPUpsamplersKeepAlpha[MODE_BGR]       = UpsampleBgrLinePair;
  WebPUpsamplersKeepAlpha[MODE_BGRA]      = UpsampleBgrKeepAlphaLinePair;
  WebPUpsamplersKeepAlpha[MODE_ARGB]      = UpsampleArgbKeepAlphaLinePair;
  WebPUpsamplersKeepAlpha[MODE_RGBA_4444] = UpsampleRgba4444KeepAlphaLinePair;
  WebPUpsamplersKeepAlpha[MODE_RGB_565]   = UpsampleRgb565LinePair;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo) {
#if defined(__SSE2__) || defined(_MSC_VER)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitUpsamplersSSE2();
    }
#endif
  }
#endif  // FANCY_UPSAMPLING
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
