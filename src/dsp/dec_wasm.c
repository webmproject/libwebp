// Copyright 2021 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebAssembly (Wasm) version of some decoding functions.
//
// This will contain Wasm implementation of some decoding functions.

#include "./dsp.h"

#if defined(WEBP_USE_WASM_SIMD)

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitWasmSIMD(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitWasmSIMD(void) {
  // TODO(crbug.com/v8/12371): No special implementation for Wasm yet, will be
  // added later.
}

#else

WEBP_DSP_INIT_STUB(VP8DspInitWasmSIMD)

#endif  // WEBP_USE_WASM_SIMD
