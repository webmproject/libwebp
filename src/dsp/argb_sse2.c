// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//   ARGB making functions (SSE2 version).
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"
#include "./lossless.h"

#if defined(WEBP_USE_SSE2)

#include <assert.h>
#include <emmintrin.h>
#include <string.h>

static void PackARGB(const uint8_t* a, const uint8_t* r, const uint8_t* g,
                     const uint8_t* b, int len, uint32_t* out) {
  (void)a;
  if (g == r + 1) {  // RGBA input order. Need to swap R and B.
    assert(b == r + 2);
    assert(a == r + 3);
    VP8LConvertBGRAToRGBA((const uint32_t*)r, len, (uint8_t*)out);
  } else {
    assert(g == b + 1);
    assert(r == b + 2);
    assert(a == b + 3);
    memcpy(out, b, len * 4);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspARGBInitSSE2(void);
extern void VP8LDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspARGBInitSSE2(void) {
  VP8LDspInitSSE2();
  VP8PackARGB = PackARGB;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(VP8EncDspARGBInitSSE2)

#endif  // WEBP_USE_SSE2
