// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Colorspace-related functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#define kFIX 3    // 8bit precision per pass
#define kFIX2 16  // final descaling
#define kC0 (4 << kFIX)
#define FILTER(A, B, C,  K0, K1)   ((K0) * (B) - (K1) * ((A) + (C)))
#define kNORM ((1 << kFIX2) / (kC0 * kC0))

// 8b -> 16b conversion and horizontal filtering
static void SharpenImportRow_C(const uint8_t* src, int16_t* dst, int s,
                               int c0, int c1) {
  int a = src[0];
  int b = src[1];
  int i;
  dst[0] = FILTER(a, a, b, c0, c1);
  for (i = 1; i < s - 1; ++i) {
    const int c = src[i + 1];
    dst[i] = FILTER(a, b, c, c0, c1);
    a = b;
    b = c;
  }
  dst[s - 1] = FILTER(a, b, b, c0, c1);
}

// vertical filtering plus 16b->8b conversion
static void SharpenExportRow_C(const int16_t* a, const int16_t* b,
                               const int16_t* c,
                               uint8_t* dst, int width, int c0, int c1) {
  int i;
  for (i = 0; i < width; ++i) {
    const int A = a[i], B = b[i], C = c[i];
    const int16_t V = ((B * c0) >> kFIX2) - (((A + C) * c1) >> kFIX2);
    dst[i] = (V < 0) ? 0 : (V > 255) ? 255 : (uint8_t)V;
  }
}

//-----------------------------------------------------------------------------

void (*WebPSharpenImportRow)(const uint8_t* src, int16_t* dst, int s,
                             int c0, int c1);
void (*WebPSharpenExportRow)(const int16_t* a, const int16_t* b,
                             const int16_t* c, uint8_t* dst,
                             int width, int c0, int c1);

static volatile VP8CPUInfo csp_last_cpuinfo_used =
    (VP8CPUInfo)&csp_last_cpuinfo_used;

extern void WebPInitCSPSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitCSP(void) {
  if (csp_last_cpuinfo_used == VP8GetCPUInfo) return;

  WebPSharpenExportRow = SharpenExportRow_C;
  WebPSharpenImportRow = SharpenImportRow_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_USE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitCSPSSE2();
    }
#endif  // WEBP_USE_SSE2
  }
  csp_last_cpuinfo_used = VP8GetCPUInfo;
}
