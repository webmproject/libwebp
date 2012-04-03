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
} Histogram;

static WEBP_INLINE void HistogramClear(Histogram* const p) {
  memset(&p->literal_[0], 0, sizeof(p->literal_));
  memset(&p->red_[0], 0, sizeof(p->red_));
  memset(&p->blue_[0], 0, sizeof(p->blue_));
  memset(&p->alpha_[0], 0, sizeof(p->alpha_));
  memset(&p->distance_[0], 0, sizeof(p->distance_));
}

static WEBP_INLINE void HistogramInit(Histogram* const p,
                                      int palette_code_bits) {
  p->palette_code_bits_ = palette_code_bits;
  HistogramClear(p);
}

// Create the histogram.
//
// The input data is the PixOrCopy data, which models the
// literals, stop codes and backward references (both distances and lengths)
void HistogramBuild(Histogram* const p,
                    const PixOrCopy* const literal_and_length,
                    int n_literal_and_length);

void HistogramAddSinglePixOrCopy(Histogram* const p, const PixOrCopy v);

// Estimate how many bits the combined entropy of literals and distance
// approximately maps to.
double HistogramEstimateBits(const Histogram* const p);

// This function estimates the Huffman dictionary + other block overhead
// size for creating a new deflate block.
double HistogramEstimateBitsHeader(const Histogram* const p);

// This function estimates the cost in bits excluding the bits needed to
// represent the entropy code itself.
double HistogramEstimateBitsBulk(const Histogram* const p);

static WEBP_INLINE void HistogramAdd(Histogram* const p,
                                     const Histogram* const a) {
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

static WEBP_INLINE void HistogramRemove(Histogram* const p,
                                   const Histogram* const a) {
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

static WEBP_INLINE int HistogramNumPixOrCopyCodes(const Histogram* const p) {
  return 256 + kLengthCodes + (1 << p->palette_code_bits_);
}

void ConvertPopulationCountTableToBitEstimates(
    int n, const int* const population_counts, double* const output);

double ShannonEntropy(const int* const array, int n);

// Build a 2d image of histograms, subresolutioned by (1 << histobits) to
// the original image.
int BuildHistogramImage(int xsize, int ysize,
                        int histobits,
                        int palette_bits,
                        const PixOrCopy* backward_refs,
                        int backward_refs_size,
                        Histogram*** image,
                        int* histogram_size);

// Combines several histograms into fewer histograms.
int CombineHistogramImage(Histogram** in,
                          int in_size,
                          int quality,
                          Histogram*** out,
                          int* out_size);

// Moves histograms from one cluster to another if smaller entropy can
// be achieved by doing that.
void RefineHistogramImage(Histogram** raw,
                          int raw_size,
                          uint32_t* symbols,
                          int out_size,
                          Histogram** out);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif  // WEBP_ENC_HISTOGRAM_H_
