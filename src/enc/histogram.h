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
} VP8LHistogram;

static WEBP_INLINE void VP8LHistogramClear(VP8LHistogram* const p) {
  memset(&p->literal_[0], 0, sizeof(p->literal_));
  memset(&p->red_[0], 0, sizeof(p->red_));
  memset(&p->blue_[0], 0, sizeof(p->blue_));
  memset(&p->alpha_[0], 0, sizeof(p->alpha_));
  memset(&p->distance_[0], 0, sizeof(p->distance_));
}

static WEBP_INLINE void VP8LHistogramInit(VP8LHistogram* const p,
                                      int palette_code_bits) {
  p->palette_code_bits_ = palette_code_bits;
  VP8LHistogramClear(p);
}

// Create the histogram.
//
// The input data is the PixOrCopy data, which models the
// literals, stop codes and backward references (both distances and lengths)
void VP8LHistogramCreate(VP8LHistogram* const p,
                         const PixOrCopy* const literal_and_length,
                         int n_literal_and_length);

void VP8LHistogramAddSinglePixOrCopy(VP8LHistogram* const p, const PixOrCopy v);

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

double VP8LShannonEntropy(const int* const array, int n);

// Build a 2d image of histograms, subresolutioned by (1 << histobits) to
// the original image.
int VP8LHistogramBuildImage(int xsize, int ysize,
                            int histobits, int palette_bits,
                            const PixOrCopy* backward_refs,
                            int backward_refs_size,
                            VP8LHistogram*** image,
                            int* histogram_size);

// Combines several histograms into fewer histograms.
int VP8LHistogramCombine(VP8LHistogram** in,
                         int in_size,
                         int quality,
                         VP8LHistogram*** out,
                         int* out_size);

// Moves histograms from one cluster to another if smaller entropy can
// be achieved by doing that.
void VP8LHistogramRefine(VP8LHistogram** raw,
                         int raw_size,
                         uint32_t* symbols,
                         int out_size,
                         VP8LHistogram** out);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif  // WEBP_ENC_HISTOGRAM_H_
