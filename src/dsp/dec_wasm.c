// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebAssembly (WASM) version of some decoding functions.
//

#include "./dsp.h"

#if defined(WEBP_USE_WASM)

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitWASM(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitWASM(void) {
}

#else  // !WEBP_USE_WASM

WEBP_DSP_INIT_STUB(VP8DspInitWASM)

#endif  // WEBP_USE_WASM
