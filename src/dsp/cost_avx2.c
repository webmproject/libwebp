// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// AVX2 version of cost functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./dsp.h"

#if defined(WEBP_USE_AVX2)

#include <immintrin.h>

#include "../enc/cost.h"
#include "../enc/vp8enci.h"
#include "../utils/utils.h"

#define LOAD256U(p) _mm256_loadu_si256((const __m256i*)(p))
#define STORE256U(v, dst) _mm256_storeu_si256((__m256i*)(dst), (v))

//------------------------------------------------------------------------------

static void SetResidualCoeffs_AVX2(const int16_t* const coeffs,
                                   VP8Residual* const res) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i C = LOAD256U(coeffs);
  const __m256i m_mask = _mm256_cmpeq_epi16(C, zero);
  // Get the comparison results as a bitmask into 32bits. Negate the mask to get
  // the position of entries that are not equal to zero. We don't need to mask
  // out least significant bits according to res->first, since coeffs[0] is 0
  // if res->first > 0.
  const uint32_t mask = 0xffffffffu ^ (uint32_t)_mm256_movemask_epi8(m_mask);
  // The position of the most significant non-zero bit indicates the position of
  // the last non-zero value.
  assert(res->first == 0 || coeffs[0] == 0);
  // we use >>1 because the movemask above extracted bit-mask for 16b values.
  res->last = mask ? BitsLog2Floor(mask) >> 1 : -1;
  res->coeffs = coeffs;
}

static int GetResidualCost_AVX2(int ctx0, const VP8Residual* const res) {
  uint16_t levels[16], ctxs[16];
  uint16_t abs_levels[16];
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  const int p0 = res->prob[n][ctx0][0];
  CostArrayPtr const costs = res->costs;
  const uint16_t* t = costs[n][ctx0];
  // bit_cost(1, p0) is already incorporated in t[] tables, but only if ctx != 0
  // (as required by the syntax). For ctx0 == 0, we need to add it here or it'll
  // be missing during the loop.
  int cost = (ctx0 == 0) ? VP8BitCost(1, p0) : 0;

  if (res->last < 0) {
    return VP8BitCost(0, p0);
  }

  {   // precompute clamped levels and contexts, packed to 8b.
    const __m256i kCst2 = _mm256_set1_epi16(2);
    const __m256i kCst67 = _mm256_set1_epi16(MAX_VARIABLE_LEVEL);
    const __m256i C = LOAD256U(res->coeffs);
    const __m256i D = _mm256_abs_epi16(C);   // abs(v), 16b
    const __m256i E = _mm256_min_epu16(D, kCst2);    // context = 0,1,2
    const __m256i F = _mm256_min_epu16(D, kCst67);   // clamp_level in [0..67]

    STORE256U(E, ctxs);
    STORE256U(F, levels);
    STORE256U(D, abs_levels);
  }
  for (; n < res->last; ++n) {
    const int ctx = ctxs[n];
    const int level = levels[n];
    const int flevel = abs_levels[n];   // full level
    cost += VP8LevelFixedCosts[flevel] + t[level];  // simplified VP8LevelCost()
    t = costs[n + 1][ctx];
  }
  // Last coefficient is always non-zero
  {
    const int level = levels[n];
    const int flevel = abs_levels[n];
    assert(flevel != 0);
    cost += VP8LevelFixedCosts[flevel] + t[level];
    if (n < 15) {
      const int b = VP8EncBands[n + 1];
      const int ctx = ctxs[n];
      const int last_p0 = res->prob[b][ctx][0];
      cost += VP8BitCost(0, last_p0);
    }
  }
  return cost;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspCostInitAVX2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspCostInitAVX2(void) {
  VP8SetResidualCoeffs = SetResidualCoeffs_AVX2;
  VP8GetResidualCost = GetResidualCost_AVX2;
}

#else  // !WEBP_USE_AVX2

WEBP_DSP_INIT_STUB(VP8EncDspCostInitAVX2)

#endif  // WEBP_USE_AVX2
