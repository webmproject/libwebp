// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON variant of methods for lossless encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#include "./lossless.h"
#include "./neon.h"

#ifdef WEBP_USE_INTRINSICS

//------------------------------------------------------------------------------
// Subtract-Green Transform

// vtbl?_u8 are marked unavailable for iOS arm64 with Xcode < 6.3, use
// non-standard versions there.
#if defined(__APPLE__) && defined(__aarch64__) && \
    defined(__apple_build_version__) && (__apple_build_version__< 6020037)
#define USE_VTBLQ
#endif

#ifdef USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[16] = {
  1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13, 255
};

static WEBP_INLINE uint8x16_t DoGreenShuffle(const uint8x16_t argb,
                                             const uint8x16_t shuffle) {
  return vcombine_u8(vtbl1q_u8(argb, vget_low_u8(shuffle)),
                     vtbl1q_u8(argb, vget_high_u8(shuffle)));
}
#else  // !USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[8] = { 1, 255, 1, 255, 5, 255, 5, 255  };

static WEBP_INLINE uint8x16_t DoGreenShuffle(const uint8x16_t argb,
                                             const uint8x8_t shuffle) {
  return vcombine_u8(vtbl1_u8(vget_low_u8(argb), shuffle),
                     vtbl1_u8(vget_high_u8(argb), shuffle));
}
#endif  // USE_VTBLQ

static void SubtractGreenFromBlueAndRed(uint32_t* argb_data, int num_pixels) {
  const uint32_t* const end = argb_data + (num_pixels & ~3);
#ifdef USE_VTBLQ
  const uint8x16_t shuffle = vld1q_u8(kGreenShuffle);
#else
  const uint8x8_t shuffle = vld1_u8(kGreenShuffle);
#endif
  for (; argb_data < end; argb_data += 4) {
    const uint8x16_t argb = vld1q_u8((uint8_t*)argb_data);
    const uint8x16_t greens = DoGreenShuffle(argb, shuffle);
    vst1q_u8((uint8_t*)argb_data, vsubq_u8(argb, greens));
  }
  // fallthrough and finish off with plain-C
  VP8LSubtractGreenFromBlueAndRed_C(argb_data, num_pixels & 3);
}

#undef USE_VTBLQ

#endif   // WEBP_USE_INTRINSICS

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitNEON(void) {
#ifdef WEBP_USE_INTRINSICS
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed;
#endif
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8LEncDspInitNEON)

#endif  // WEBP_USE_NEON
