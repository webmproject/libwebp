// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Gamma correction utilities.

#ifndef WEBP_SHARPYUV_SHARPYUV_GAMMA_H_
#define WEBP_SHARPYUV_SHARPYUV_GAMMA_H_

#include "./sharpyuv.h"
#include "webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes precomputed tables. Must be called once before calling
// SharpYuvGammaToLinear or SharpYuvLinearToGamma.
void SharpYuvInitGammaTables(void);

// Converts a 'bit_depth'-bit gamma color value to a 16-bit linear value.
uint32_t SharpYuvGammaToLinear(uint16_t v, int bit_depth,
                               SharpYuvTransferFunctionType transfer_type);

// Converts a 16-bit linear color value to a 'bit_depth'-bit gamma value.
uint16_t SharpYuvLinearToGamma(uint32_t value, int bit_depth,
                               SharpYuvTransferFunctionType transfer_type);

// Exposed for SIMD fast paths of the sRGB transfer function (most common).
// Must stay in sync with ToLinearSrgb()/FromLinearSrgb(): table entries are
// 16.16 fixed-point values, etc.
#define SHARPYUV_GAMMA_TO_LINEAR_TAB_BITS 10
#define SHARPYUV_GAMMA_TO_LINEAR_BITS 16
extern uint32_t kSharpYuvGammaToLinearTabS[];  // sRGB gamma -> linear
extern uint32_t kSharpYuvLinearToGammaTabS[];  // sRGB linear -> gamma

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBP_SHARPYUV_SHARPYUV_GAMMA_H_
