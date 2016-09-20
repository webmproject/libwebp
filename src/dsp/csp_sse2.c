// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Colorspace-related functions, SSE2 variant.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_SSE2)

#include <emmintrin.h>

#define kFIX2 16  // final descaling
#define FILTER(A, B, C, K0, K1)   ((K0) * (B) - (K1) * ((A) + (C)))

// 8b -> 16b conversion and horizontal filtering
static void SharpenImportRow_SSE2(const uint8_t* src, int16_t* dst, int s,
                                  int c0, int c1) {
  int a = src[0];
  int b = src[1];
  int i = 1;
  const __m128i mC0 = _mm_set1_epi16(c0);
  const __m128i mC1 = _mm_set1_epi16(c1);
  const __m128i zero = _mm_setzero_si128();

  dst[0] = FILTER(a, a, b, c0, c1);
  for (; i + 8 < s - 1; i += 8) {
    const __m128i A0 = _mm_loadl_epi64((const __m128i*)&src[i - 1]);
    const __m128i B0 = _mm_loadl_epi64((const __m128i*)&src[i + 0]);
    const __m128i C0 = _mm_loadl_epi64((const __m128i*)&src[i + 1]);
    const __m128i A1 = _mm_unpacklo_epi8(A0, zero);
    const __m128i B1 = _mm_unpacklo_epi8(B0, zero);
    const __m128i C1 = _mm_unpacklo_epi8(C0, zero);
    const __m128i A2 = _mm_add_epi16(A1, C1);
    const __m128i B2 = _mm_mullo_epi16(B1, mC0);
    const __m128i A3 = _mm_mullo_epi16(A2, mC1);
    const __m128i A4 = _mm_sub_epi16(B2, A3);
    _mm_storeu_si128((__m128i*)&dst[i], A4);
  }
  for (; i < s - 1; ++i) {
    dst[i] = FILTER(src[i - 1], src[i], src[i + 1], c0, c1);
  }
  dst[s - 1] = FILTER(src[s - 2], src[s - 1], src[s - 1], c0, c1);
}

// vertical filtering plus 16b->8b conversion
static void SharpenExportRow_SSE2(const int16_t* a, const int16_t* b,
                                  const int16_t* c, uint8_t* dst,
                                  int width, int c0, int c1) {
  int i = 0;
  const __m128i mC0 = _mm_set1_epi16(c0);
  const __m128i mC1 = _mm_set1_epi16(c1);
  for (; i + 8 < width; i += 8) {
    const __m128i A0 = _mm_loadu_si128((const __m128i*)&a[i]);
    const __m128i B0 = _mm_loadu_si128((const __m128i*)&b[i]);
    const __m128i C0 = _mm_loadu_si128((const __m128i*)&c[i]);
    const __m128i A1 = _mm_add_epi16(A0, C0);
    const __m128i B1 = _mm_mulhi_epi16(B0, mC0);
    const __m128i A2 = _mm_mulhi_epi16(A1, mC1);
    const __m128i A3 = _mm_sub_epi16(B1, A2);
    const __m128i A4 = _mm_packus_epi16(A3, A3);
    _mm_storel_epi64((__m128i*)&dst[i], A4);
  }
  for (; i < width; ++i) {
    const int A = a[i], B = b[i], C = c[i];
    const int16_t V = ((B * c0) >> kFIX2) - (((A + C) * c1) >> kFIX2);
    dst[i] = (V < 0) ? 0 : (V > 255) ? 255 : (uint8_t)V;
  }
}

//-----------------------------------------------------------------------------

extern void WebPInitCSPSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitCSPSSE2(void) {
  WebPSharpenImportRow = SharpenImportRow_SSE2;
  WebPSharpenExportRow = SharpenExportRow_SSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(WebPInitCSPSSE2)

#endif  // WEBP_USE_SSE2
