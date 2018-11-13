// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------

#include "src/dsp/dsp.h"
#include "src/enc/vp8i_enc.h"

static score_t IsFlat_C(const int16_t* levels, int num_blocks, score_t thresh) {
  score_t score = 0;
  while (num_blocks-- > 0) {      // TODO(skal): refine positional scoring?
    int i;
    for (i = 1; i < 16; ++i) {    // omit DC, we're only interested in AC
      score += (levels[i] != 0);
      if (score > thresh) return 0;
    }
    levels += 16;
  }
  return 1;
}

//------------------------------------------------------------------------------
// init function

VP8IsFlatFunc VP8IsFlat;
//VP8SetResidualCoeffsFunc VP8SetResidualCoeffs;

extern void VP8EncDspQuantInitNEON(void);

WEBP_DSP_INIT_FUNC(VP8EncDspQuantInit) {
  VP8IsFlat = IsFlat_C;
  //VP8SetResidualCoeffs = SetResidualCoeffs_C;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_USE_NEON)
    if (VP8GetCPUInfo(kNEON)) {
      VP8EncDspQuantInitNEON();
    }
#endif
  }
}

//------------------------------------------------------------------------------
