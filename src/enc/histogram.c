// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: Jyrki Alakuijala (jyrki@google.com)
//

#ifdef USE_LOSSLESS_ENCODER

#include <math.h>
#include <stdio.h>

#include "./backward_references.h"
#include "./histogram.h"
#include "../dsp/lossless.h"

void VP8LConvertPopulationCountTableToBitEstimates(
    int num_symbols,
    const int* const population_counts,
    double* const output) {
  int sum = 0;
  int nonzeros = 0;
  int i;
  for (i = 0; i < num_symbols; ++i) {
    sum += population_counts[i];
    if (population_counts[i] > 0) {
      ++nonzeros;
    }
  }
  if (nonzeros <= 1) {
    memset(output, 0, num_symbols * sizeof(*output));
    return;
  }
  {
    const double log2sum = log2(sum);
    for (i = 0; i < num_symbols; ++i) {
      if (population_counts[i] == 0) {
        output[i] = log2sum;
      } else {
        output[i] = log2sum - log2(population_counts[i]);
      }
    }
  }
}

void VP8LHistogramAddSinglePixOrCopy(VP8LHistogram* const p,
                                     const PixOrCopy v) {
  if (PixOrCopyIsLiteral(&v)) {
    ++p->alpha_[PixOrCopyLiteral(&v, 3)];
    ++p->red_[PixOrCopyLiteral(&v, 2)];
    ++p->literal_[PixOrCopyLiteral(&v, 1)];
    ++p->blue_[PixOrCopyLiteral(&v, 0)];
  } else if (PixOrCopyIsCacheIdx(&v)) {
    int literal_ix = 256 + kLengthCodes + PixOrCopyCacheIdx(&v);
    ++p->literal_[literal_ix];
  } else {
    int code, extra_bits_count, extra_bits_value;
    PrefixEncode(PixOrCopyLength(&v),
                 &code, &extra_bits_count, &extra_bits_value);
    ++p->literal_[256 + code];
    PrefixEncode(PixOrCopyDistance(&v),
                 &code, &extra_bits_count, &extra_bits_value);
    ++p->distance_[code];
  }
}

void VP8LHistogramCreate(VP8LHistogram* const p,
                         const VP8LBackwardRefs* const refs) {
  int i;
  VP8LHistogramClear(p);
  for (i = 0; i < refs->size; ++i) {
    VP8LHistogramAddSinglePixOrCopy(p, refs->refs[i]);
  }
}

static double BitsEntropy(const int* const array, int n) {
  double retval = 0;
  int sum = 0;
  int nonzeros = 0;
  int max_val = 0;
  int i;
  double mix;
  for (i = 0; i < n; ++i) {
    if (array[i] != 0) {
      sum += array[i];
      ++nonzeros;
      retval += array[i] * VP8LFastLog(array[i]);
      if (max_val < array[i]) {
        max_val = array[i];
      }
    }
  }
  retval -= sum * VP8LFastLog(sum);
  retval *= -1.4426950408889634;  // 1.0 / -Log(2);
  mix = 0.627;
  if (nonzeros < 5) {
    if (nonzeros <= 1) {
      return 0;
    }
    // Two symbols, they will be 0 and 1 in a Huffman code.
    // Let's mix in a bit of entropy to favor good clustering when
    // distributions of these are combined.
    if (nonzeros == 2) {
      return 0.99 * sum + 0.01 * retval;
    }
    // No matter what the entropy says, we cannot be better than min_limit
    // with Huffman coding. I am mixing a bit of entropy into the
    // min_limit since it produces much better (~0.5 %) compression results
    // perhaps because of better entropy clustering.
    if (nonzeros == 3) {
      mix = 0.95;
    } else {
      mix = 0.7;  // nonzeros == 4.
    }
  }
  {
    double min_limit = 2 * sum - max_val;
    min_limit = mix * min_limit + (1.0 - mix) * retval;
    if (retval < min_limit) {
      return min_limit;
    }
  }
  return retval;
}

double VP8LHistogramEstimateBitsBulk(const VP8LHistogram* const p) {
  double retval = BitsEntropy(&p->literal_[0], VP8LHistogramNumCodes(p)) +
      BitsEntropy(&p->red_[0], 256) +
      BitsEntropy(&p->blue_[0], 256) +
      BitsEntropy(&p->alpha_[0], 256) +
      BitsEntropy(&p->distance_[0], DISTANCE_CODES_MAX);
  // Compute the extra bits cost.
  size_t i;
  for (i = 2; i < kLengthCodes - 2; ++i) {
    retval +=
        (i >> 1) * p->literal_[256 + i + 2];
  }
  for (i = 2; i < DISTANCE_CODES_MAX - 2; ++i) {
    retval += (i >> 1) * p->distance_[i + 2];
  }
  return retval;
}

double VP8LHistogramEstimateBits(const VP8LHistogram* const p) {
  return VP8LHistogramEstimateBitsHeader(p) + VP8LHistogramEstimateBitsBulk(p);
}

// Returns the cost encode the rle-encoded entropy code.
// The constants in this function are experimental.
static double HuffmanCost(const int* const population, int length) {
  // Small bias because Huffman code length is typically not stored in
  // full length.
  static const int kHuffmanCodeOfHuffmanCodeSize = CODE_LENGTH_CODES * 3;
  static const double kSmallBias = 9.1;
  double retval = kHuffmanCodeOfHuffmanCodeSize - kSmallBias;
  int streak = 0;
  int i = 0;
  for (; i < length - 1; ++i) {
    ++streak;
    if (population[i] == population[i + 1]) {
      continue;
    }
 last_streak_hack:
    // population[i] points now to the symbol in the streak of same values.
    if (streak > 3) {
      if (population[i] == 0) {
        retval += 1.5625 + 0.234375 * streak;
      } else {
        retval += 2.578125 + 0.703125 * streak;
      }
    } else {
      if (population[i] == 0) {
        retval += 1.796875 * streak;
      } else {
        retval += 3.28125 * streak;
      }
    }
    streak = 0;
  }
  if (i == length - 1) {
    ++streak;
    goto last_streak_hack;
  }
  return retval;
}

double VP8LHistogramEstimateBitsHeader(const VP8LHistogram* const p) {
  return HuffmanCost(&p->alpha_[0], 256) +
      HuffmanCost(&p->red_[0], 256) +
      HuffmanCost(&p->literal_[0], VP8LHistogramNumCodes(p)) +
      HuffmanCost(&p->blue_[0], 256) +
      HuffmanCost(&p->distance_[0], DISTANCE_CODES_MAX);
}

static int HistogramBuildImage(int xsize, int ysize,
                               int histobits, int palettebits,
                               const VP8LBackwardRefs* const backward_refs,
                               VP8LHistogram*** const image_arg,
                               int* const image_size) {
  int histo_xsize = histobits ? (xsize + (1 << histobits) - 1) >> histobits : 1;
  int histo_ysize = histobits ? (ysize + (1 << histobits) - 1) >> histobits : 1;
  int i;
  int x = 0;
  int y = 0;
  VP8LHistogram** image;
  *image_arg = NULL;
  *image_size = histo_xsize * histo_ysize;
  image = (VP8LHistogram**)calloc(*image_size, sizeof(*image));
  if (image == NULL) {
    return 0;
  }
  for (i = 0; i < *image_size; ++i) {
    image[i] = (VP8LHistogram*)malloc(sizeof(*image[i]));
    if (!image[i]) {
      int k;
      for (k = 0; k < *image_size; ++k) {
        free(image[k]);
      }
      free(image);
      return 0;
    }
    VP8LHistogramInit(image[i], palettebits);
  }
  // x and y trace the position in the image.
  for (i = 0; i < backward_refs->size; ++i) {
    const PixOrCopy v = backward_refs->refs[i];
    const int ix =
        histobits ? (y >> histobits) * histo_xsize + (x >> histobits) : 0;
    VP8LHistogramAddSinglePixOrCopy(image[ix], v);
    x += PixOrCopyLength(&v);
    while (x >= xsize) {
      x -= xsize;
      ++y;
    }
  }
  *image_arg = image;
  return 1;
}

static int HistogramCombine(VP8LHistogram** const in, int in_size, int quality,
                            VP8LHistogram*** const out_arg,
                            int* const out_size) {
  int ok = 0;
  int i;
  unsigned int seed = 0;
  int tries_with_no_success = 0;
  int inner_iters = 10 + quality / 2;
  int iter;
  double* bit_costs = (double*)malloc(in_size * sizeof(*bit_costs));
  VP8LHistogram** out = (VP8LHistogram**)calloc(in_size, sizeof(*out));
  *out_arg = out;
  *out_size = in_size;
  if (bit_costs == NULL || out == NULL) {
    goto Error;
  }
  // Copy
  for (i = 0; i < in_size; ++i) {
    VP8LHistogram* new_histo = (VP8LHistogram*)malloc(sizeof(*new_histo));
    if (new_histo == NULL) {
      goto Error;
    }
    *new_histo = *(in[i]);
    out[i] = new_histo;
    bit_costs[i] = VP8LHistogramEstimateBits(out[i]);
  }
  // Collapse similar histograms.
  for (iter = 0; iter < in_size * 3 && *out_size >= 2; ++iter) {
    double best_val = 0;
    int best_ix0 = 0;
    int best_ix1 = 0;
    // Try a few times.
    int k;
    for (k = 0; k < inner_iters; ++k) {
      // Choose two, build a combo out of them.
      double cost_val;
      VP8LHistogram* combo;
      int ix0 = rand_r(&seed) % *out_size;
      int ix1;
      int diff = ((k & 7) + 1) % (*out_size - 1);
      if (diff >= 3) {
        diff = rand_r(&seed) % (*out_size - 1);
      }
      ix1 = (ix0 + diff + 1) % *out_size;
      if (ix0 == ix1) {
        continue;
      }
      combo = (VP8LHistogram*)malloc(sizeof(*combo));
      if (combo == NULL) {
        goto Error;
      }
      *combo = *out[ix0];
      VP8LHistogramAdd(combo, out[ix1]);
      cost_val =
          VP8LHistogramEstimateBits(combo) - bit_costs[ix0] - bit_costs[ix1];
      if (best_val > cost_val) {
        best_val = cost_val;
        best_ix0 = ix0;
        best_ix1 = ix1;
      }
      free(combo);
    }
    if (best_val < 0.0) {
      VP8LHistogramAdd(out[best_ix0], out[best_ix1]);
      bit_costs[best_ix0] =
          best_val + bit_costs[best_ix0] + bit_costs[best_ix1];
      // Erase (*out)[best_ix1]
      free(out[best_ix1]);
      memmove(&out[best_ix1], &out[best_ix1 + 1],
              (*out_size - best_ix1 - 1) * sizeof(out[0]));
      memmove(&bit_costs[best_ix1], &bit_costs[best_ix1 + 1],
              (*out_size - best_ix1 - 1) * sizeof(bit_costs[0]));
      --(*out_size);
      tries_with_no_success = 0;
    }
    if (++tries_with_no_success >= 50) {
      break;
    }
  }
  ok = 1;
Error:
  free(bit_costs);
  if (!ok) {
    if (out) {
      int i;
      for (i = 0; i < *out_size; ++i) {
        free(out[i]);
      }
      free(out);
    }
  }
  return ok;
}

// What is the bit cost of moving square_histogram from
// cur_symbol to candidate_symbol.
static double HistogramDistance(const VP8LHistogram* const square_histogram,
                                int cur_symbol, int candidate_symbol,
                                const double* const symbol_bit_costs,
                                VP8LHistogram** const candidate_histograms) {
  double new_bit_cost;
  double previous_bit_cost;
  VP8LHistogram modified_histo;
  if (cur_symbol == candidate_symbol) {
    return 0;  // Going nowhere. No savings.
  }
  previous_bit_cost = symbol_bit_costs[candidate_symbol];
  if (cur_symbol != -1) {
    previous_bit_cost += symbol_bit_costs[cur_symbol];
  }

  // Compute the bit cost of the histogram where the data moves to.
  modified_histo = *candidate_histograms[candidate_symbol];
  VP8LHistogramAdd(&modified_histo, square_histogram);
  new_bit_cost = VP8LHistogramEstimateBits(&modified_histo);

  // Compute the bit cost of the histogram where the data moves away.
  if (cur_symbol != -1) {
    modified_histo = *candidate_histograms[cur_symbol];
    VP8LHistogramRemove(&modified_histo, square_histogram);
    new_bit_cost += VP8LHistogramEstimateBits(&modified_histo);
  }
  return new_bit_cost - previous_bit_cost;
}

static int HistogramRefine(VP8LHistogram** const raw, int raw_size,
                           uint32_t* const symbols,
                           VP8LHistogram** const out, int out_size) {
  int i;
  double* const symbol_bit_costs =
      (double*)malloc(out_size * sizeof(*symbol_bit_costs));
  if (symbol_bit_costs == NULL) return 0;
  for (i = 0; i < out_size; ++i) {
    symbol_bit_costs[i] = VP8LHistogramEstimateBits(out[i]);
  }

  // Find the best 'out' histogram for each of the raw histograms
  for (i = 0; i < raw_size; ++i) {
    int best_out = 0;
    double best_bits = HistogramDistance(raw[i], symbols[i], 0,
                                         symbol_bit_costs, out);
    int k;
    for (k = 1; k < out_size; ++k) {
      double cur_bits = HistogramDistance(raw[i], symbols[i], k,
                                          symbol_bit_costs, out);
      if (cur_bits < best_bits) {
        best_bits = cur_bits;
        best_out = k;
      }
    }
    symbols[i] = best_out;
  }
  free(symbol_bit_costs);

  // Recompute each out based on raw and symbols.
  for (i = 0; i < out_size; ++i) {
    VP8LHistogramClear(out[i]);
  }
  for (i = 0; i < raw_size; ++i) {
    VP8LHistogramAdd(out[symbols[i]], raw[i]);
  }

  return 1;
}

int VP8LGetHistImageSymbols(int xsize, int ysize,
                            const VP8LBackwardRefs* const refs,
                            int quality, int histogram_bits,
                            int cache_bits,
                            VP8LHistogram*** const histogram_image,
                            int* const histogram_image_size,
                            uint32_t* const histogram_symbols) {
  // Build histogram image.
  int ok = 0;
  int i;
  int histogram_image_raw_size;
  VP8LHistogram** histogram_image_raw = NULL;

  *histogram_image = NULL;
  if (!HistogramBuildImage(xsize, ysize, histogram_bits, cache_bits, refs,
                           &histogram_image_raw,
                           &histogram_image_raw_size)) {
    goto Error;
  }
  // Collapse similar histograms.
  if (!HistogramCombine(histogram_image_raw, histogram_image_raw_size,
                        quality, histogram_image, histogram_image_size)) {
    goto Error;
  }
  // Refine histogram image.
  for (i = 0; i < histogram_image_raw_size; ++i) {
    histogram_symbols[i] = -1;
  }
  if (!HistogramRefine(histogram_image_raw, histogram_image_raw_size,
                       histogram_symbols,
                       *histogram_image, *histogram_image_size)) {
    goto Error;
  }
  ok = 1;

Error:
  if (!ok) {
    VP8LDeleteHistograms(*histogram_image, *histogram_image_size);
  }
  VP8LDeleteHistograms(histogram_image_raw, histogram_image_raw_size);
  return ok;
}

#endif
