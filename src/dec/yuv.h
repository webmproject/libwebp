// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// inline YUV->RGB conversion function
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_DEC_YUV_H_
#define WEBP_DEC_YUV_H_

#include "webp/decode_vp8.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

enum { YUV_FIX = 16,                // fixed-point precision
       YUV_RANGE_MIN = -227,        // min value of r/g/b output
       YUV_RANGE_MAX = 256 + 226    // max value of r/g/b output
};
extern int16_t VP8kVToR[256], VP8kUToB[256];
extern int32_t VP8kVToG[256], VP8kUToG[256];
extern uint8_t VP8kClip[YUV_RANGE_MAX - YUV_RANGE_MIN];

inline static void VP8YuvToRgb(uint8_t y, uint8_t u, uint8_t v,
                               uint8_t* const rgb) {
  const int r_off = VP8kVToR[v];
  const int g_off = (VP8kVToG[v] + VP8kUToG[u]) >> YUV_FIX;
  const int b_off = VP8kUToB[u];
  rgb[0] = VP8kClip[y + r_off - YUV_RANGE_MIN];
  rgb[1] = VP8kClip[y + g_off - YUV_RANGE_MIN];
  rgb[2] = VP8kClip[y + b_off - YUV_RANGE_MIN];
}

inline static void VP8YuvToRgba(int y, int u, int v, uint8_t* const rgba) {
  VP8YuvToRgb(y, u, v, rgba);
  rgba[3] = 0xff;
}

inline static void VP8YuvToBgr(uint8_t y, uint8_t u, uint8_t v,
                               uint8_t* const bgr) {
  const int r_off = VP8kVToR[v];
  const int g_off = (VP8kVToG[v] + VP8kUToG[u]) >> YUV_FIX;
  const int b_off = VP8kUToB[u];
  bgr[0] = VP8kClip[y + b_off - YUV_RANGE_MIN];
  bgr[1] = VP8kClip[y + g_off - YUV_RANGE_MIN];
  bgr[2] = VP8kClip[y + r_off - YUV_RANGE_MIN];
}

inline static void VP8YuvToBgra(int y, int u, int v, uint8_t* const bgra) {
  VP8YuvToBgr(y, u, v, bgra);
  bgra[3] = 0xff;
}

// Must be called before everything, to initialize the tables.
void VP8YUVInit(void);

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_DEC_YUV_H_
