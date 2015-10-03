// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Author: Mislav Bradac (mislavm@google.com)
//

#ifndef WEBP_ENC_DELTA_PALETTIZATION_H_
#define WEBP_ENC_DELTA_PALETTIZATION_H_

#ifdef WEBP_EXPERIMENTAL_FEATURES

#include "../webp/types.h"

// Format allows palette up to 256 entries, but more palette entries produce
// bigger entropy. In the future it will probably be useful to add more entries
// that are far from the origin of the palette or choose remaining entries
// dynamically.
#define DELTA_PALETTE_SIZE 226

extern const uint32_t kDeltaPalette[DELTA_PALETTE_SIZE];

// Find palette entry with minimum error from difference of actual pixel value
// and predicted pixel value. Propagate error of pixel to its top and left pixel
// in src array. Write predicted_value + palette_entry to new_image. Return
// index of best palette entry.
int FindBestPaletteEntry(int x, int y, const uint32_t* palette,
                         int palette_size, uint32_t* src,
                         int src_stride, uint32_t* new_image);

#endif  // WEBP_EXPERIMENTAL_FEATURES

#endif  // WEBP_ENC_DELTA_PALETTIZATION_H_
