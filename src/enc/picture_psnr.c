// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebPPicture tools for measuring distortion
//
// Author: Skal (pascal.massimino@gmail.com)

#include <math.h>
#include <stdlib.h>

#include "./vp8enci.h"
#include "../utils/utils.h"

typedef double (*AccumulateFunc)(const uint8_t* src, int src_stride,
                                 const uint8_t* ref, int ref_stride,
                                 int w, int h);

//------------------------------------------------------------------------------
// local-min distortion
//
// For every pixel in the *reference* picture, we search for the local best
// match in the compressed image. This is not a symmetrical measure.

#define RADIUS 2  // search radius. Shouldn't be too large.

static double AccumulateLSIM(const uint8_t* src, int src_stride,
                             const uint8_t* ref, int ref_stride,
                             int w, int h) {
  int x, y;
  double total_sse = 0.;
  for (y = 0; y < h; ++y) {
    const int y_0 = (y - RADIUS < 0) ? 0 : y - RADIUS;
    const int y_1 = (y + RADIUS + 1 >= h) ? h : y + RADIUS + 1;
    for (x = 0; x < w; ++x) {
      const int x_0 = (x - RADIUS < 0) ? 0 : x - RADIUS;
      const int x_1 = (x + RADIUS + 1 >= w) ? w : x + RADIUS + 1;
      double best_sse = 255. * 255.;
      const double value = (double)ref[y * ref_stride + x];
      int i, j;
      for (j = y_0; j < y_1; ++j) {
        const uint8_t* const s = src + j * src_stride;
        for (i = x_0; i < x_1; ++i) {
          const double diff = s[i] - value;
          const double sse = diff * diff;
          if (sse < best_sse) best_sse = sse;
        }
      }
      total_sse += best_sse;
    }
  }
  return total_sse;
}
#undef RADIUS

static double AccumulateSSE(const uint8_t* src, int src_stride,
                            const uint8_t* ref, int ref_stride,
                            int w, int h) {
  int y;
  double total_sse = 0.;
  for (y = 0; y < h; ++y) {
    total_sse += VP8AccumulateSSE(src, ref, w);
    src += src_stride;
    ref += ref_stride;
  }
  return total_sse;
}

//------------------------------------------------------------------------------

static double AccumulateSSIM(const uint8_t* src, int src_stride,
                             const uint8_t* ref, int ref_stride,
                             int w, int h) {
  const int w0 = (w < VP8_SSIM_KERNEL) ? w : VP8_SSIM_KERNEL;
  const int w1 = w - VP8_SSIM_KERNEL - 1;
  const int h0 = (h < VP8_SSIM_KERNEL) ? h : VP8_SSIM_KERNEL;
  const int h1 = h - VP8_SSIM_KERNEL - 1;
  int x, y;
  double sum = 0.;
  for (y = 0; y < h0; ++y) {
    for (x = 0; x < w; ++x) {
      sum += VP8SSIMGetClipped(src, src_stride, ref, ref_stride, x, y, w, h);
    }
  }
  for (; y < h1; ++y) {
    for (x = 0; x < w0; ++x) {
      sum += VP8SSIMGetClipped(src, src_stride, ref, ref_stride, x, y, w, h);
    }
    for (; x < w1; ++x) {
      const int off1 = x - VP8_SSIM_KERNEL + (y - VP8_SSIM_KERNEL) * src_stride;
      const int off2 = x - VP8_SSIM_KERNEL + (y - VP8_SSIM_KERNEL) * ref_stride;
      sum += VP8SSIMGet(src + off1, src_stride, ref + off2, ref_stride);
    }
    for (; x < w; ++x) {
      sum += VP8SSIMGetClipped(src, src_stride, ref, ref_stride, x, y, w, h);
    }
  }
  for (; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      sum += VP8SSIMGetClipped(src, src_stride, ref, ref_stride, x, y, w, h);
    }
  }
  return sum;
}

//------------------------------------------------------------------------------
// Distortion

// Max value returned in case of exact similarity.
static const double kMinDistortion_dB = 99.;

static double GetPSNR(double v, double size) {
  return (v > 0. && size > 0.) ? -4.3429448 * log(v / (size * 255 * 255.))
                               : kMinDistortion_dB;
}

static double GetLogSSIM(double v, double size) {
  v = (size > 0.) ? v / size : 1.;
  return (v < 1.) ? -10.0 * log10(1. - v) : kMinDistortion_dB;
}

int WebPPictureDistortion(const WebPPicture* src, const WebPPicture* ref,
                          int type, float results[5]) {
  int w, h, c;
  double disto[4] = { 0. };
  double sizes[4] = { 0. };
  double total_size = 0., total_disto = 0.;
  const AccumulateFunc metric = (type == 0) ? AccumulateSSE :
                                (type == 1) ? AccumulateSSIM :
                                              AccumulateLSIM;
  VP8SSIMDspInit();

  if (src == NULL || ref == NULL ||
      src->width != ref->width || src->height != ref->height ||
      src->use_argb != ref->use_argb || results == NULL) {
    return 0;
  }
  w = src->width;
  h = src->height;

  if (src->use_argb == 1) {
    if (src->argb == NULL || ref->argb == NULL) {
      return 0;
    } else {
      int i, j;
      uint8_t* tmp1, *tmp2;
      uint8_t* const tmp_plane =
          (uint8_t*)WebPSafeMalloc(2ULL * w * h, sizeof(*tmp_plane));
      if (tmp_plane == NULL) return 0;
      tmp1 = tmp_plane;
      tmp2 = tmp_plane + w * h;
      for (c = 0; c < 4; ++c) {
        for (j = 0; j < h; ++j) {
          for (i = 0; i < w; ++i) {
            tmp1[j * w + i] = src->argb[i + j * src->argb_stride] >> (c * 8);
            tmp2[j * w + i] = ref->argb[i + j * ref->argb_stride] >> (c * 8);
          }
        }
        sizes[c] = w * h;
        disto[c] = metric(tmp1, w, tmp2, w, w, h);
      }
      WebPSafeFree(tmp_plane);
    }
  } else {
    int has_alpha, uv_w, uv_h;
    if (src->y == NULL || ref->y == NULL ||
        src->u == NULL || ref->u == NULL ||
        src->v == NULL || ref->v == NULL) {
      return 0;
    }
    has_alpha = !!(src->colorspace & WEBP_CSP_ALPHA_BIT);
    if (has_alpha != !!(ref->colorspace & WEBP_CSP_ALPHA_BIT) ||
        (has_alpha && (src->a == NULL || ref->a == NULL))) {
      return 0;
    }

    uv_w = (src->width + 1) >> 1;
    uv_h = (src->height + 1) >> 1;
    sizes[0] = w * h;
    sizes[1] = sizes[2] = uv_w * uv_h;
    sizes[3] = has_alpha ? w * h : 0.;

    disto[0] = metric(src->y, src->y_stride, ref->y, ref->y_stride, w, h);
    disto[1] = metric(src->u, src->uv_stride, ref->u, ref->uv_stride,
                      uv_w, uv_h);
    disto[2] = metric(src->v, src->uv_stride, ref->v, ref->uv_stride,
                      uv_w, uv_h);
    if (has_alpha) {
      disto[3] = metric(src->a, src->a_stride, ref->a, ref->a_stride, w, h);
    }
  }

  for (c = 0; c < 4; ++c) {
    total_disto += disto[c];
    total_size += sizes[c];
    results[c] = (type == 1) ? (float)GetLogSSIM(disto[c], sizes[c])
                             : (float)GetPSNR(disto[c], sizes[c]);
  }
  results[4] = (type == 1) ? (float)GetLogSSIM(total_disto, total_size)
                           : (float)GetPSNR(total_disto, total_size);
  return 1;
}

//------------------------------------------------------------------------------
