// Copyright 2023 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Utilities for palette analysis.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WEBP_UTILS_PALETTE_H_
#define WEBP_UTILS_PALETTE_H_

#include "src/webp/types.h"

struct WebPPicture;

// The different ways a palette can be sorted.
typedef enum PaletteSorting {
  kSortedDefault = 0,
  kMinimizeDelta = 1,
  kModifiedZeng = 2,
  kUnusedPalette = 3
} PaletteSorting;

// Returns the index of 'color' in the sorted palette 'sorted' of size
// 'num_colors'.
int SearchColorNoIdx(const uint32_t sorted[], uint32_t color, int num_colors);

// Sort palette in increasing order and prepare an inverse mapping array.
void PrepareMapToPalette(const uint32_t palette[], uint32_t num_colors,
                         uint32_t sorted[], uint32_t idx_map[]);

// Returns count of unique colors in 'pic', assuming pic->use_argb is true.
// If the unique color count is more than MAX_PALETTE_SIZE, returns
// MAX_PALETTE_SIZE+1.
// If 'palette' is not NULL and the number of unique colors is less than or
// equal to MAX_PALETTE_SIZE, also outputs the actual unique colors into
// 'palette' in a sorted order. Note: 'palette' is assumed to be an array
// already allocated with at least MAX_PALETTE_SIZE elements.
int GetColorPalette(const struct WebPPicture* const pic,
                    uint32_t* const palette);

// Sorts the color sorted palette in 'palette_sorted'/'num_colors' by
// minimizing deltas between consecutive colors and stores it in 'palette'.
void PaletteSortMinimizeDeltas(const uint32_t* const palette_sorted,
                               int num_colors, uint32_t* const palette);

// Implements the modified Zeng method from "A Survey on Palette Reordering
// Methods for Improving the Compression of Color-Indexed Images" by Armando J.
// Pinho and Antonio J. R. Neves. The palette defined by 'palette_in' and
// 'num_colors' is sorted using information from 'pic' and output in
// 'palette'.
// Returns 0 on memory allocation error.
int PaletteSortModifiedZeng(const struct WebPPicture* const pic,
                            const uint32_t* const palette_in,
                            uint32_t num_colors, uint32_t* const palette);

#endif  // WEBP_UTILS_PALETTE_H_
