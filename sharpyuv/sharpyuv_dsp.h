// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Speed-critical functions for Sharp YUV.

#ifndef WEBP_SHARPYUV_SHARPYUV_DSP_H_
#define WEBP_SHARPYUV_SHARPYUV_DSP_H_

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_cpu.h"
#include "src/webp/types.h"

extern uint64_t (*SharpYuvUpdateY)(const uint16_t* src, const uint16_t* ref,
                                   uint16_t* dst, int len, int bit_depth);
extern void (*SharpYuvUpdateRGB)(const int16_t* src, const int16_t* ref,
                                 int16_t* dst, int len);
extern void (*SharpYuvFilterRow)(const int16_t* A, const int16_t* B, int len,
                                 const uint16_t* best_y, uint16_t* out,
                                 int bit_depth);

// For each pixel, computes the index to look up that color in a precomputed
// risk score table where the YUV space is subsampled to a size of
// precomputed_scores_table_sampling^3 (see sharpyuv_risk_table.h)
extern void (*SharpYuvRowToYuvSharpnessIndex)(
    const uint8_t* r_ptr, const uint8_t* g_ptr, const uint8_t* b_ptr,
    int rgb_step, int rgb_bit_depth, int width, uint16_t* dst,
    const SharpYuvConversionMatrix* matrix,
    int precomputed_scores_table_sampling);

void SharpYuvInitDsp(void);

#endif  // WEBP_SHARPYUV_SHARPYUV_DSP_H_
