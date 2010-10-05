// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// YUV->RGB conversion function
//
// Author: Skal (pascal.massimino@gmail.com)

#include "yuv.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

enum { YUV_HALF = 1 << (YUV_FIX - 1) };

int16_t VP8kVToR[256], VP8kUToB[256];
int32_t VP8kVToG[256], VP8kUToG[256];
uint8_t VP8kClip[YUV_RANGE_MAX - YUV_RANGE_MIN];

static int done = 0;

void VP8YUVInit() {
  int i;
  if (done) {
    return;
  }
  for (i = 0; i < 256; ++i) {
    VP8kVToR[i] = (89858 * (i - 128) + YUV_HALF) >> YUV_FIX;
    VP8kUToG[i] = -22014 * (i - 128) + YUV_HALF;
    VP8kVToG[i] = -45773 * (i - 128);
    VP8kUToB[i] = (113618 * (i - 128) + YUV_HALF) >> YUV_FIX;
  }
  for (i = YUV_RANGE_MIN; i < YUV_RANGE_MAX; ++i) {
    const int k = ((i - 16) * 76283 + YUV_HALF) >> YUV_FIX;
    VP8kClip[i - YUV_RANGE_MIN] = (k < 0) ? 0 : (k > 255) ? 255 : k;
  }
  done = 1;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
