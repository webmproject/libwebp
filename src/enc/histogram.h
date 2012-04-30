// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: Jyrki Alakuijala (jyrki@google.com)
//
// Models the histograms of literal and distance codes.

#ifndef WEBP_ENC_HISTOGRAM_H_
#define WEBP_ENC_HISTOGRAM_H_

#ifdef USE_LOSSLESS_ENCODER

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "./backward_references.h"
#include "../webp/types.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// A simple container for histograms of data.
typedef struct {
  // literal_ contains green literal, palette-code and
  // copy-length-prefix histogram
  int literal_[PIX_OR_COPY_CODES_MAX];
  int red_[256];
  int blue_[256];
  int alpha_[256];
  // Backward reference prefix-code histogram.
  int distance_[DISTANCE_CODES_MAX];
  int palette_code_bits_;
  double bit_cost_;   // cached value of VP8LHistogramEstimateBits(this)
} VP8LHistogram;

// Create the histogram.
//
// The input data is the PixOrCopy data, which models the literals, stop
// codes and backward references (both distances and lengths).  Also: if
// palette_code_bits is >= 0, initialize the histogram with this value.
void VP8LHistogramCreate(VP8LHistogram* const p,
                         const VP8LBackwardRefs* const refs,
                         int palette_code_bits);

// Reset the histogram's stats.
void VP8LHistogramClear(VP8LHistogram* const p);

// Set the palette_code_bits and reset the stats.
void VP8LHistogramInit(VP8LHistogram* const p, int palette_code_bits);

// Allocate an array of pointer to histograms, allocated and initialized
// using 'cache_bits'. Return NULL in case of memory error.
VP8LHistogram** VP8LAllocateHistograms(int size, int cache_bits);

// Destroy an array of histograms (and the array itself).
void VP8LDeleteHistograms(VP8LHistogram** const histograms, int size);

void VP8LHistogramAddSinglePixOrCopy(VP8LHistogram* const p,
                                     const PixOrCopy* const v);

// Estimate how many bits the combined entropy of literals and distance
// approximately maps to.
double VP8LHistogramEstimateBits(const VP8LHistogram* const p);

// This function estimates the Huffman dictionary + other block overhead
// size for creating a new deflate block.
double VP8LHistogramEstimateBitsHeader(const VP8LHistogram* const p);

// This function estimates the cost in bits excluding the bits needed to
// represent the entropy code itself.
double VP8LHistogramEstimateBitsBulk(const VP8LHistogram* const p);

static WEBP_INLINE void VP8LHistogramAdd(VP8LHistogram* const p,
                                         const VP8LHistogram* const a) {
  int i;
  for (i = 0; i < PIX_OR_COPY_CODES_MAX; ++i) {
    p->literal_[i] += a->literal_[i];
  }
  for (i = 0; i < DISTANCE_CODES_MAX; ++i) {
    p->distance_[i] += a->distance_[i];
  }
  for (i = 0; i < 256; ++i) {
    p->red_[i] += a->red_[i];
    p->blue_[i] += a->blue_[i];
    p->alpha_[i] += a->alpha_[i];
  }
}

static WEBP_INLINE void VP8LHistogramRemove(VP8LHistogram* const p,
                                            const VP8LHistogram* const a) {
  int i;
  for (i = 0; i < PIX_OR_COPY_CODES_MAX; ++i) {
    p->literal_[i] -= a->literal_[i];
    assert(p->literal_[i] >= 0);
  }
  for (i = 0; i < DISTANCE_CODES_MAX; ++i) {
    p->distance_[i] -= a->distance_[i];
    assert(p->distance_[i] >= 0);
  }
  for (i = 0; i < 256; ++i) {
    p->red_[i] -= a->red_[i];
    p->blue_[i] -= a->blue_[i];
    p->alpha_[i] -= a->alpha_[i];
    assert(p->red_[i] >= 0);
    assert(p->blue_[i] >= 0);
    assert(p->alpha_[i] >= 0);
  }
}

static WEBP_INLINE int VP8LHistogramNumCodes(const VP8LHistogram* const p) {
  return 256 + kLengthCodes + (1 << p->palette_code_bits_);
}

void VP8LConvertPopulationCountTableToBitEstimates(
    int n, const int* const population_counts, double* const output);

// Builds the histogram image.
int VP8LGetHistoImageSymbols(int xsize, int ysize,
                             const VP8LBackwardRefs* const refs,
                             int quality, int histogram_bits,
                             int cache_bits,
                             VP8LHistogram** const histogram_image,
                             int* const histogram_image_size,
                             uint16_t* const histogram_symbols);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#endif  // WEBP_ENC_HISTOGRAM_H_
