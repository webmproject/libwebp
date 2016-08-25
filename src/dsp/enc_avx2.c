// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// AVX2 version of speed-critical encoding functions.

#include "./dsp.h"

#if defined(WEBP_USE_AVX2)

#include <immintrin.h>
#include "../enc/cost.h"
#include "../enc/vp8enci.h"

#define LOAD256U(p) _mm256_loadu_si256((const __m256i*)(p))

//------------------------------------------------------------------------------
// Quantization

static WEBP_INLINE int DoQuantizeBlock_AVX2(int16_t in[16], int16_t out[16],
                                            const uint16_t* const sharpen,
                                            const VP8Matrix* const mtx) {
  const __m256i max_coeff_2047 = _mm256_set1_epi16(MAX_LEVEL);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i zigzag = _mm256_set_epi8(
#define IDX(n) (2 * (n) + 1) & 0x0f, (2 * (n) + 0) & 0x0f
    IDX(15), IDX(14), IDX(11), IDX(8) /*!*/, IDX(10), IDX(13), IDX(12), IDX(9),
    IDX( 6), IDX( 3), IDX( 2), IDX(5), IDX(7) /*!*/,  IDX( 4), IDX( 1), IDX(0));
#undef IDX
  const __m256i iq = LOAD256U(mtx->iq_);
  const __m256i q = LOAD256U(mtx->q_);
  __m256i in0 = LOAD256U(in);
  __m256i out0;

  // extract sign(in), take abs(in) and add sharpening bias if needed:
  const __m256i sign0 = _mm256_cmpgt_epi16(zero, in0);
  __m256i coeff = _mm256_abs_epi16(in0);
  if (sharpen != NULL) {
    const __m256i sharpen_bias = LOAD256U(sharpen);
    coeff = _mm256_add_epi16(coeff, sharpen_bias);
  }

  // out = (coeff * iQ + B) >> QFIX
  {
    // doing calculations with 32b precision (QFIX=17)
    // out = (coeff * iQ)
    const __m256i coeff_iQH = _mm256_mulhi_epu16(coeff, iq);
    const __m256i coeff_iQL = _mm256_mullo_epi16(coeff, iq);
    __m256i out_0 = _mm256_unpacklo_epi16(coeff_iQL, coeff_iQH);
    __m256i out_8 = _mm256_unpackhi_epi16(coeff_iQL, coeff_iQH);
    // out = (coeff * iQ + B)
    const __m256i bias_0 = LOAD256U(&mtx->bias_[0]);
    const __m256i bias_8 = LOAD256U(&mtx->bias_[8]);
    out_0 = _mm256_add_epi32(out_0, bias_0);
    out_8 = _mm256_add_epi32(out_8, bias_8);
    // out = QUANTDIV(coeff, iQ, B, QFIX)
    out_0 = _mm256_srai_epi32(out_0, QFIX);
    out_8 = _mm256_srai_epi32(out_8, QFIX);

    // pack result as 16b and saturate to 2047
    out0 = _mm256_packs_epi32(out_0, out_8);
    out0 = _mm256_min_epi16(out0, max_coeff_2047);
  }

  // get sign back (if (sign[j]) out_n = -out_n)
  out0 = _mm256_xor_si256(out0, sign0);
  out0 = _mm256_sub_epi16(out0, sign0);

  // in = out * Q
  in0 = _mm256_mullo_epi16(out0, q);
  _mm256_storeu_si256((__m256i*)in, in0);

  // zigzag the output before storing it.
  // The zigzag pattern can almost be reproduced with few pshufb,
  // we only need to swap the 7th and 8th values afterward.
  out0 = _mm256_shuffle_epi8(out0, zigzag);
  _mm256_storeu_si256((__m256i*)out, out0);
  // swap the remaining two misplaced elements
  {
    const int16_t outZ_12 = out[12];
    const int16_t outZ_3 = out[3];
    out[3] = outZ_12;
    out[12] = outZ_3;
  }
  // detect if all 'out' values are zeroes or not
  return (_mm256_movemask_epi8(_mm256_cmpeq_epi16(out0, zero)) != -1);
}

static int QuantizeBlock_AVX2(int16_t in[16], int16_t out[16],
                              const VP8Matrix* const mtx) {
  return DoQuantizeBlock_AVX2(in, out, &mtx->sharpen_[0], mtx);
}

static int QuantizeBlockWHT_AVX2(int16_t in[16], int16_t out[16],
                            const VP8Matrix* const mtx) {
  return DoQuantizeBlock_AVX2(in, out, NULL, mtx);
}

static int Quantize2Blocks_AVX2(int16_t in[32], int16_t out[32],
                                const VP8Matrix* const mtx) {
  int nz;
  const uint16_t* const sharpen = &mtx->sharpen_[0];
  nz  = DoQuantizeBlock_AVX2(in + 0 * 16, out + 0 * 16, sharpen, mtx) << 0;
  nz |= DoQuantizeBlock_AVX2(in + 1 * 16, out + 1 * 16, sharpen, mtx) << 1;
  return nz;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspInitAVX2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitAVX2(void) {
  VP8EncQuantizeBlock = QuantizeBlock_AVX2;
  VP8EncQuantize2Blocks = Quantize2Blocks_AVX2;
  VP8EncQuantizeBlockWHT = QuantizeBlockWHT_AVX2;
}

#else  // !WEBP_USE_AVX2

WEBP_DSP_INIT_STUB(VP8EncDspInitAVX2)

#endif  // WEBP_USE_AVX2
