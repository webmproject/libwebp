// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 Rescaling functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_SSE2)
#include <emmintrin.h>

#include <assert.h>
#include "../utils/rescaler.h"

//------------------------------------------------------------------------------
// Implementations of critical functions ImportRow / ExportRow

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)

//------------------------------------------------------------------------------
// Row export

// load *src as epi64, multiply by mult and store result in [out0 ... out3]
static WEBP_INLINE void LoadDispatchAndMult(const rescaler_t* const src,
                                            const __m128i* const mult,
                                            __m128i* const out0,
                                            __m128i* const out1,
                                            __m128i* const out2,
                                            __m128i* const out3) {
  const __m128i A0 = _mm_loadu_si128((const __m128i*)(src + 0));
  const __m128i A1 = _mm_loadu_si128((const __m128i*)(src + 4));
  const __m128i A2 = _mm_srli_epi64(A0, 32);
  const __m128i A3 = _mm_srli_epi64(A1, 32);
  if (mult != NULL) {
    *out0 = _mm_mul_epu32(A0, *mult);
    *out1 = _mm_mul_epu32(A1, *mult);
    *out2 = _mm_mul_epu32(A2, *mult);
    *out3 = _mm_mul_epu32(A3, *mult);
  } else {
    *out0 = A0;
    *out1 = A1;
    *out2 = A2;
    *out3 = A3;
  }
}

static WEBP_INLINE void ProcessRow(const __m128i* const A0,
                                   const __m128i* const A1,
                                   const __m128i* const A2,
                                   const __m128i* const A3,
                                   const __m128i* const mult,
                                   uint8_t* const dst) {
  const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
  const __m128i mask = _mm_set_epi32(0xffffffffu, 0, 0xffffffffu, 0);
  const __m128i B0 = _mm_mul_epu32(*A0, *mult);
  const __m128i B1 = _mm_mul_epu32(*A1, *mult);
  const __m128i B2 = _mm_mul_epu32(*A2, *mult);
  const __m128i B3 = _mm_mul_epu32(*A3, *mult);
  const __m128i C0 = _mm_add_epi64(B0, rounder);
  const __m128i C1 = _mm_add_epi64(B1, rounder);
  const __m128i C2 = _mm_add_epi64(B2, rounder);
  const __m128i C3 = _mm_add_epi64(B3, rounder);
  const __m128i D0 = _mm_srli_epi64(C0, WEBP_RESCALER_RFIX);
  const __m128i D1 = _mm_srli_epi64(C1, WEBP_RESCALER_RFIX);
#if (WEBP_RESCALER_FIX < 32)
  const __m128i D2 =
      _mm_and_si128(_mm_slli_epi64(C2, 32 - WEBP_RESCALER_RFIX), mask);
  const __m128i D3 =
      _mm_and_si128(_mm_slli_epi64(C3, 32 - WEBP_RESCALER_RFIX), mask);
#else
  const __m128i D2 = _mm_and_si128(C2, mask);
  const __m128i D3 = _mm_and_si128(C3, mask);
#endif
  const __m128i E0 = _mm_or_si128(D0, D2);
  const __m128i E1 = _mm_or_si128(D1, D3);
  const __m128i F = _mm_packs_epi32(E0, E1);
  const __m128i G = _mm_packus_epi16(F, F);
  _mm_storel_epi64((__m128i*)dst, G);
}

static void RescalerExportRowExpandSSE2(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const __m128i mult = _mm_set_epi32(0, wrk->fy_scale, 0, wrk->fy_scale);

  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0 && wrk->y_sub + wrk->y_accum >= 0);
  assert(wrk->y_expand);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3;
      LoadDispatchAndMult(frow + x_out, NULL, &A0, &A1, &A2, &A3);
      ProcessRow(&A0, &A1, &A2, &A3, &mult, dst + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      assert(v >= 0 && v <= 255);
      dst[x_out] = v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    const __m128i mA = _mm_set_epi32(0, A, 0, A);
    const __m128i mB = _mm_set_epi32(0, B, 0, B);
    const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3, B0, B1, B2, B3;
      LoadDispatchAndMult(frow + x_out, &mA, &A0, &A1, &A2, &A3);
      LoadDispatchAndMult(irow + x_out, &mB, &B0, &B1, &B2, &B3);
      {
        const __m128i C0 = _mm_add_epi64(A0, B0);
        const __m128i C1 = _mm_add_epi64(A1, B1);
        const __m128i C2 = _mm_add_epi64(A2, B2);
        const __m128i C3 = _mm_add_epi64(A3, B3);
        const __m128i D0 = _mm_add_epi64(C0, rounder);
        const __m128i D1 = _mm_add_epi64(C1, rounder);
        const __m128i D2 = _mm_add_epi64(C2, rounder);
        const __m128i D3 = _mm_add_epi64(C3, rounder);
        const __m128i E0 = _mm_srli_epi64(D0, WEBP_RESCALER_RFIX);
        const __m128i E1 = _mm_srli_epi64(D1, WEBP_RESCALER_RFIX);
        const __m128i E2 = _mm_srli_epi64(D2, WEBP_RESCALER_RFIX);
        const __m128i E3 = _mm_srli_epi64(D3, WEBP_RESCALER_RFIX);
        ProcessRow(&E0, &E1, &E2, &E3, &mult, dst + x_out);
      }
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      assert(v >= 0 && v <= 255);
      dst[x_out] = v;
    }
  }
}

static void RescalerExportRowShrinkSSE2(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    const int scale_xy = wrk->fxy_scale;
    const __m128i mult_xy = _mm_set_epi32(0, scale_xy, 0, scale_xy);
    const __m128i mult_y = _mm_set_epi32(0, yscale, 0, yscale);
    const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3, B0, B1, B2, B3;
      LoadDispatchAndMult(irow + x_out, NULL, &A0, &A1, &A2, &A3);
      LoadDispatchAndMult(frow + x_out, &mult_y, &B0, &B1, &B2, &B3);
      {
        const __m128i C0 = _mm_add_epi64(B0, rounder);
        const __m128i C1 = _mm_add_epi64(B1, rounder);
        const __m128i C2 = _mm_add_epi64(B2, rounder);
        const __m128i C3 = _mm_add_epi64(B3, rounder);
        const __m128i D0 = _mm_srli_epi64(C0, WEBP_RESCALER_RFIX);   // = frac
        const __m128i D1 = _mm_srli_epi64(C1, WEBP_RESCALER_RFIX);
        const __m128i D2 = _mm_srli_epi64(C2, WEBP_RESCALER_RFIX);
        const __m128i D3 = _mm_srli_epi64(C3, WEBP_RESCALER_RFIX);
        const __m128i E0 = _mm_sub_epi64(A0, D0);   // irow[x] - frac
        const __m128i E1 = _mm_sub_epi64(A1, D1);
        const __m128i E2 = _mm_sub_epi64(A2, D2);
        const __m128i E3 = _mm_sub_epi64(A3, D3);
        const __m128i F2 = _mm_slli_epi64(D2, 32);
        const __m128i F3 = _mm_slli_epi64(D3, 32);
        const __m128i G0 = _mm_or_si128(D0, F2);
        const __m128i G1 = _mm_or_si128(D1, F3);
        _mm_storeu_si128((__m128i*)(irow + x_out + 0), G0);
        _mm_storeu_si128((__m128i*)(irow + x_out + 4), G1);
        ProcessRow(&E0, &E1, &E2, &E3, &mult_xy, dst + x_out);
      }
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (int)MULT_FIX(frow[x_out], yscale);
      const int v = (int)MULT_FIX(irow[x_out] - frac, wrk->fxy_scale);
      assert(v >= 0 && v <= 255);
      dst[x_out] = v;
      irow[x_out] = frac;   // new fractional start
    }
  } else {
    const uint32_t scale = wrk->fxy_scale;
    const __m128i mult = _mm_set_epi32(0, scale, 0, scale);
    const __m128i zero = _mm_setzero_si128();
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3;
      LoadDispatchAndMult(irow + x_out, NULL, &A0, &A1, &A2, &A3);
      _mm_storeu_si128((__m128i*)(irow + x_out + 0), zero);
      _mm_storeu_si128((__m128i*)(irow + x_out + 4), zero);
      ProcessRow(&A0, &A1, &A2, &A3, &mult, dst + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX(irow[x_out], scale);
      assert(v >= 0 && v <= 255);
      dst[x_out] = v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX
#undef ROUNDER

//------------------------------------------------------------------------------

extern void WebPRescalerDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitSSE2(void) {
  WebPRescalerExportRowExpand = RescalerExportRowExpandSSE2;
  WebPRescalerExportRowShrink = RescalerExportRowShrinkSSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(WebPRescalerDspInitSSE2)

#endif  // WEBP_USE_SSE2
