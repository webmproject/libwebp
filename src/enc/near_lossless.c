// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Near-lossless image preprocessing adjusts pixel values to help
// compressibility with a guarantee of maximum deviation between original and
// resulting pixel values.
//
// Author: Jyrki Alakuijala (jyrki@google.com)
// Converted to C by Aleksander Kramarz (akramarz@google.com)

#include <stdlib.h>

#include "../dsp/lossless.h"
#include "../utils/utils.h"
#include "./vp8enci.h"

#ifdef WEBP_EXPERIMENTAL_FEATURES
// Computes quantized pixel value and distance from original value.
static void GetValAndDistance(int a, int initial, int bits,
                              int* const val, int* const distance) {
  const int mask = ~((1 << bits) - 1);
  *val = (initial & mask) | (initial >> (8 - bits));
  *distance = 2 * abs(a - *val);
}

// Quantizes values {a, a+(1<<bits), a-(1<<bits)}, checks if in [min, max] range
// and returns the nearest one.
static int FindClosestDiscretized(int a, int bits, int min, int max) {
  int best_val = a, i;
  int min_distance = 256;

  for (i = -1; i <= 1; ++i) {
    int val = a + i * (1 << bits);
    int candidate, distance;
    if (val < 0) {
      val = 0;
    } else if (val > 255) {
      val = 255;
    }
    GetValAndDistance(a, val, bits, &candidate, &distance);
    if (i != 0) {
      ++distance;
    }
    // Smallest distance but favor i == 0 over i == -1 and i == 1
    // since that keeps the overall intensity more constant in the
    // images.
    if (distance < min_distance && candidate >= min && candidate <= max) {
      min_distance = distance;
      best_val = candidate;
    }
  }
  return best_val;
}

// Discretizes value (actual - predicted) in the way that actual pixel value
// stays within error bounds.
static WEBP_INLINE uint32_t DiscretizedResidual(uint32_t actual,
                                                uint32_t predicted,
                                                int limit_bits) {
  const uint32_t res = (actual - predicted) & 0xff;
  uint32_t min, max;
  if (actual < predicted) {
    min = 256 - predicted;
    max = 255;
  } else {
    min = 0;
    max = 255 - predicted;
  }
  return FindClosestDiscretized(res, limit_bits, min, max);
}

// Applies FindClosestDiscretized to all channels of pixel.
static uint32_t ClosestDiscretizedArgb(uint32_t a, int bits,
                                       uint32_t min, uint32_t max) {
  return (FindClosestDiscretized(a >> 24, bits, min >> 24, max >> 24) << 24) |
         (FindClosestDiscretized((a >> 16) & 0xff, bits,
                                 (min >> 16) & 0xff,
                                 (max >> 16) & 0xff) << 16) |
         (FindClosestDiscretized((a >> 8) & 0xff, bits,
                                 (min >> 8) & 0xff,
                                 (max >> 8) & 0xff) << 8) |
         (FindClosestDiscretized(a & 0xff, bits, min & 0xff, max & 0xff));
}

// Checks if distance between corresponding channel values of pixels a and b
// exceeds given limit.
static int IsFar(uint32_t a, uint32_t b, int limit) {
  int k;
  for (k = 0; k < 4; ++k) {
    const int delta = (int)((a >> (k * 8)) & 0xff) -
                      (int)((b >> (k * 8)) & 0xff);
    if (delta >= limit || delta <= -limit) {
      return 1;
    }
  }
  return 0;
}

// Adjusts pixel values of image with given maximum error.
static void NearLossless(int xsize, int ysize, uint32_t* argb,
                        int limit_bits, uint32_t* copy_buffer) {
  int x, y;
  const int limit = 1 << limit_bits;
  memcpy(copy_buffer, argb, xsize * ysize * sizeof(argb[0]));

  for (y = 0; y < ysize; ++y) {
    const int offset = y * xsize;
    for (x = 0; x < xsize; ++x) {
      const int ix = offset + x;
      // Check that all pixels in 4-connected neighborhood are smooth.
      int smooth_area = 1;
      if (x != 0 && IsFar(copy_buffer[ix], copy_buffer[ix - 1], limit)) {
        smooth_area = 0;
      } else if (y != 0 &&
                 IsFar(copy_buffer[ix], copy_buffer[ix - xsize], limit)) {
        smooth_area = 0;
      } else if (x != xsize - 1 &&
                 IsFar(copy_buffer[ix], copy_buffer[ix + 1], limit)) {
        smooth_area = 0;
      } else if (y != ysize - 1 &&
                 IsFar(copy_buffer[ix], copy_buffer[ix + xsize], limit)) {
        smooth_area = 0;
      }
      if (!smooth_area) {
        argb[ix] = ClosestDiscretizedArgb(argb[ix], limit_bits, 0, 0xffffffff);
      }
    }
  }
}

static int QualityToLimitBits(int quality) {
  return 5 - (quality + 12) / 25;
}
#endif  // WEBP_EXPERIMENTAL_FEATURES

// TODO(akramarz): optimize memory to O(xsize)
int VP8ApplyNearLossless(int xsize, int ysize, uint32_t* argb, int quality) {
#ifndef WEBP_EXPERIMENTAL_FEATURES
  (void)xsize;
  (void)ysize;
  (void)argb;
  (void)quality;
#else
  int i;
  uint32_t* const copy_buffer =
      (uint32_t *)WebPSafeMalloc(xsize * ysize, sizeof(*copy_buffer));
  // quality mapping 0..12 -> 5
  //                 13..100 -> 4..1
  const int limit_bits = QualityToLimitBits(quality);
  assert(argb != NULL);
  assert(limit_bits >= 0);
  assert(limit_bits < 31);
  if (copy_buffer == NULL) {
    return 0;
  }
  for (i = limit_bits; i != 0; --i) {
    NearLossless(xsize, ysize, argb, i, copy_buffer);
  }
  WebPSafeFree(copy_buffer);
#endif  // WEBP_EXPERIMENTAL_FEATURES
  return 1;
}

#ifdef WEBP_EXPERIMENTAL_FEATURES

// In-place sum of each component with mod 256.
// This probably should go somewhere else (lossless.h?). This is just copy-paste
// from lossless.c.
static WEBP_INLINE void AddPixelsEq(uint32_t* a, uint32_t b) {
  const uint32_t alpha_and_green = (*a & 0xff00ff00u) + (b & 0xff00ff00u);
  const uint32_t red_and_blue = (*a & 0x00ff00ffu) + (b & 0x00ff00ffu);
  *a = (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

void VP8ApplyNearLosslessPredict(int xsize, int ysize, int pred_bits,
                                 const uint32_t* argb_orig,
                                 uint32_t* argb, uint32_t* argb_scratch,
                                 const uint32_t* const transform_data,
                                 int quality, int subtract_green) {
  const int tiles_per_row = VP8LSubSampleSize(xsize, pred_bits);
  uint32_t* const upper_row = argb_scratch;
  const int limit_bits = QualityToLimitBits(quality);

  int y;
  for (y = 0; y < ysize; ++y) {
    int x;
    uint32_t curr_pix = 0, prev_pix = 0;
    for (x = 0; x < xsize; ++x) {
      const int tile_idx = (y >> pred_bits) * tiles_per_row + (x >> pred_bits);
      const int pred = (transform_data[tile_idx] >> 8) & 0xf;
      const VP8LPredictorFunc pred_func = VP8LPredictors[pred];
      uint32_t predict, rb_shift = 0, delta_g = 0;
      if (y == 0) {
        predict = (x == 0) ? ARGB_BLACK : prev_pix;  // Left.
      } else if (x == 0) {
        predict = upper_row[x];  // Top.
      } else {
        predict = pred_func(prev_pix, upper_row + x);
      }

      // Discretize all residuals keeping the original pixel values in error
      // bounds.
      curr_pix = argb_orig[x];
      {
        const uint32_t a = curr_pix >> 24;
        const uint32_t a_pred = predict >> 24;
        const uint32_t a_res = DiscretizedResidual(a, a_pred, limit_bits);
        curr_pix = (curr_pix & 0x00ffffff) | a_res << 24;
      }

      {
        const uint32_t g = (curr_pix >> 8) & 0xff;
        const uint32_t g_pred = (predict >> 8) & 0xff;
        const uint32_t g_res = DiscretizedResidual(g, g_pred, limit_bits);
        // In case subtract-green transform is used, we need to shift
        // red and blue later.
        if (subtract_green) {
          delta_g = (g_pred + g_res - g) & 0xff;
          rb_shift = g;
        }
        curr_pix = (curr_pix & 0xffff00ff) | (g_res << 8);
      }

      {
        const uint32_t r = ((curr_pix >> 16) + rb_shift) & 0xff;
        const uint32_t r_pred = ((predict >> 16) + rb_shift + delta_g) & 0xff;
        const uint32_t r_res = DiscretizedResidual(r, r_pred, limit_bits);
        curr_pix = (curr_pix & 0xff00ffff) | (r_res << 16);
      }

      {
        const uint32_t b = (curr_pix + rb_shift) & 0xff;
        const uint32_t b_pred = (predict + rb_shift + delta_g) & 0xff;
        const uint32_t b_res = DiscretizedResidual(b, b_pred, limit_bits);
        curr_pix = (curr_pix & 0xffffff00) | b_res;
      }

      // Change pixel value.
      argb[x] = curr_pix;
      curr_pix = predict;
      AddPixelsEq(&curr_pix, argb[x]);
      // Copy previous pixel to upper row.
      if(x > 0) {
        upper_row[x - 1] = prev_pix;
      }
      prev_pix = curr_pix;
    }
    argb += xsize;
    argb_orig += xsize;
    upper_row[xsize - 1] = curr_pix;
  }
}
#endif  // WEBP_EXPERIMENTAL_FEATURES
