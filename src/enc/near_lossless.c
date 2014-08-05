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

#include "./vp8enci.h"
#include "../utils/utils.h"

#ifdef WEBP_EXPERIMENTAL_FEATURES
// Computes quantized pixel value and distance from original value.
static void GetValAndDistance(int a, int initial, int bits,
                              int* const val, int* const distance) {
  const int mask = ~((1 << bits) - 1);
  *val = (initial & mask) | (initial >> (8 - bits));
  *distance = 2 * abs(a - *val);
}

// Quantizes values {a, a+(1<<bits), a-(1<<bits)} and returns the nearest one.
static int FindClosestDiscretized(int a, int bits) {
  int best_val, min_distance, i;
  GetValAndDistance(a, a, bits, &best_val, &min_distance);

  for (i = -1; i <= 1; i += 2) {
    int val = a + i * (1 << bits);
    int candidate, distance;
    if (val < 0) {
      val = 0;
    } else if (val > 255) {
      val = 255;
    }
    GetValAndDistance(a, val, bits, &candidate, &distance);
    ++distance;
    // Smallest distance but favor i == 0 over i == -1 and i == 1
    // since that keeps the overall intensity more constant in the
    // images.
    if (distance < min_distance) {
      min_distance = distance;
      best_val = candidate;
    }
  }
  return best_val;
}

// Applies FindClosestDiscretized to all channels of pixel.
static uint32_t ClosestDiscretizedArgb(uint32_t a, int bits) {
  return (FindClosestDiscretized(a >> 24, bits) << 24) |
         (FindClosestDiscretized((a >> 16) & 0xff, bits) << 16) |
         (FindClosestDiscretized((a >> 8) & 0xff, bits) << 8) |
         (FindClosestDiscretized(a & 0xff, bits));
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
        argb[ix] = ClosestDiscretizedArgb(argb[ix], limit_bits);
      }
    }
  }
}
#endif

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
  const int limit_bits = 5 - (quality + 12) / 25;
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
#endif
  return 1;
}
