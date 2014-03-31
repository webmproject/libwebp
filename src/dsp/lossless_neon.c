// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON variant of methods for lossless decoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#include "./lossless.h"

//------------------------------------------------------------------------------
// Colorspace conversion functions

static void ConvertBGRAToRGBA(const uint32_t* src,
                              int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + num_pixels - 16;
  for (; src <= end; src += 16) {
    uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    // swap B and R. (VSWP d0,d2 has no intrinsics equivalent!)
    const uint8x16_t tmp = pixel.val[0];
    pixel.val[0] = pixel.val[2];
    pixel.val[2] = tmp;
    vst4q_u8(dst, pixel);
    dst += 64;
  }
  num_pixels &= 15;
  VP8LConvertBGRAToRGBA_C(src, num_pixels, dst);  // left-overs
}

static void ConvertBGRAToBGR(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + num_pixels - 16;
  for (; src <= end; src += 16) {
    const uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    const uint8x16x3_t tmp = { { pixel.val[0], pixel.val[1], pixel.val[2] } };
    vst3q_u8(dst, tmp);
    dst += 48;
  }
  num_pixels &= 15;
  VP8LConvertBGRAToBGR_C(src, num_pixels, dst);  // left-overs
}

static void ConvertBGRAToRGB(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const end = src + num_pixels - 16;
  for (; src <= end; src += 16) {
    const uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    const uint8x16x3_t tmp = { { pixel.val[2], pixel.val[1], pixel.val[0] } };
    vst3q_u8(dst, tmp);
    dst += 48;
  }
  num_pixels &= 15;
  VP8LConvertBGRAToRGB_C(src, num_pixels, dst);  // left-overs
}

#endif   // WEBP_USE_NEON

//------------------------------------------------------------------------------

extern void VP8LDspInitNEON(void);

void VP8LDspInitNEON(void) {
#if defined(WEBP_USE_NEON)
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB;
#endif   // WEBP_USE_NEON
}

//------------------------------------------------------------------------------
