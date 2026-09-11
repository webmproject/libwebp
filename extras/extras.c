// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Additional WebP utilities.
//

#include "./extras.h"

#include <assert.h>
#include <limits.h>
#include <math.h>  // for fabs, ceil
#include <string.h>

#include "./sharpyuv_risk_table.h"
#include "sharpyuv/sharpyuv.h"
#include "src/dsp/dsp.h"
#include "src/utils/utils.h"
#include "webp/encode.h"
#include "webp/format_constants.h"
#include "webp/types.h"

#define XTRA_MAJ_VERSION 1
#define XTRA_MIN_VERSION 7
#define XTRA_REV_VERSION 0

//------------------------------------------------------------------------------

int WebPGetExtrasVersion(void) {
  return (XTRA_MAJ_VERSION << 16) | (XTRA_MIN_VERSION << 8) | XTRA_REV_VERSION;
}

//------------------------------------------------------------------------------

int WebPImportGray(const uint8_t* gray_data, WebPPicture* pic) {
  int y, width, uv_width;
  if (pic == NULL || gray_data == NULL) return 0;
  pic->colorspace = WEBP_YUV420;
  if (!WebPPictureAlloc(pic)) return 0;
  width = pic->width;
  uv_width = (width + 1) >> 1;
  for (y = 0; y < pic->height; ++y) {
    memcpy(pic->y + y * pic->y_stride, gray_data, width);
    gray_data += width;  // <- we could use some 'data_stride' here if needed
    if ((y & 1) == 0) {
      memset(pic->u + (y >> 1) * pic->uv_stride, 128, uv_width);
      memset(pic->v + (y >> 1) * pic->uv_stride, 128, uv_width);
    }
  }
  return 1;
}

int WebPImportRGB565(const uint8_t* rgb565, WebPPicture* pic) {
  int x, y;
  uint32_t* dst;
  if (pic == NULL || rgb565 == NULL) return 0;
  pic->colorspace = WEBP_YUV420;
  pic->use_argb = 1;
  if (!WebPPictureAlloc(pic)) return 0;
  dst = pic->argb;
  for (y = 0; y < pic->height; ++y) {
    const int width = pic->width;
    for (x = 0; x < width; ++x) {
#if defined(WEBP_SWAP_16BIT_CSP) && (WEBP_SWAP_16BIT_CSP == 1)
      const uint32_t rg = rgb565[2 * x + 1];
      const uint32_t gb = rgb565[2 * x + 0];
#else
      const uint32_t rg = rgb565[2 * x + 0];
      const uint32_t gb = rgb565[2 * x + 1];
#endif
      uint32_t r = rg & 0xf8;
      uint32_t g = ((rg << 5) | (gb >> 3)) & 0xfc;
      uint32_t b = (gb << 5);
      // dithering
      r = r | (r >> 5);
      g = g | (g >> 6);
      b = b | (b >> 5);
      dst[x] = (0xffu << 24) | (r << 16) | (g << 8) | b;
    }
    rgb565 += 2 * width;
    dst += pic->argb_stride;
  }
  return 1;
}

int WebPImportRGB4444(const uint8_t* rgb4444, WebPPicture* pic) {
  int x, y;
  uint32_t* dst;
  if (pic == NULL || rgb4444 == NULL) return 0;
  pic->colorspace = WEBP_YUV420;
  pic->use_argb = 1;
  if (!WebPPictureAlloc(pic)) return 0;
  dst = pic->argb;
  for (y = 0; y < pic->height; ++y) {
    const int width = pic->width;
    for (x = 0; x < width; ++x) {
#if defined(WEBP_SWAP_16BIT_CSP) && (WEBP_SWAP_16BIT_CSP == 1)
      const uint32_t rg = rgb4444[2 * x + 1];
      const uint32_t ba = rgb4444[2 * x + 0];
#else
      const uint32_t rg = rgb4444[2 * x + 0];
      const uint32_t ba = rgb4444[2 * x + 1];
#endif
      uint32_t r = rg & 0xf0;
      uint32_t g = (rg << 4);
      uint32_t b = (ba & 0xf0);
      uint32_t a = (ba << 4);
      // dithering
      r = r | (r >> 4);
      g = g | (g >> 4);
      b = b | (b >> 4);
      a = a | (a >> 4);
      dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    rgb4444 += 2 * width;
    dst += pic->argb_stride;
  }
  return 1;
}

int WebPImportColorMappedARGB(const uint8_t* indexed, int indexed_stride,
                              const uint32_t palette[], int palette_size,
                              WebPPicture* pic) {
  int x, y;
  uint32_t* dst;
  // 256 as the input buffer is uint8_t.
  assert(MAX_PALETTE_SIZE <= 256);
  if (pic == NULL || indexed == NULL || indexed_stride < pic->width ||
      palette == NULL || palette_size > MAX_PALETTE_SIZE || palette_size <= 0) {
    return 0;
  }
  pic->use_argb = 1;
  if (!WebPPictureAlloc(pic)) return 0;
  dst = pic->argb;
  for (y = 0; y < pic->height; ++y) {
    for (x = 0; x < pic->width; ++x) {
      // Make sure we are within the palette.
      if (indexed[x] >= palette_size) {
        WebPPictureFree(pic);
        return 0;
      }
      dst[x] = palette[indexed[x]];
    }
    indexed += indexed_stride;
    dst += pic->argb_stride;
  }
  return 1;
}

//------------------------------------------------------------------------------

int WebPUnmultiplyARGB(WebPPicture* pic) {
  int y;
  uint32_t* dst;
  if (pic == NULL || pic->use_argb != 1 || pic->argb == NULL) return 0;
  WebPInitAlphaProcessing();
  dst = pic->argb;
  for (y = 0; y < pic->height; ++y) {
    WebPMultARGBRow(dst, pic->width, /*inverse=*/1);
    dst += pic->argb_stride;
  }
  return 1;
}

//------------------------------------------------------------------------------
// 420 risk metric

#define YUV_FIX 16  // fixed-point precision for RGB->YUV
static const int kYuvHalf = 1 << (YUV_FIX - 1);

// Maps a value in [0, (256 << YUV_FIX) - 1] to [0,
// precomputed_scores_table_sampling - 1]. It is important that the extremal
// values are preserved and 1:1 mapped:
//  ConvertValue(0) = 0
//  ConvertValue((256 << 16) - 1) = rgb_sampling_size - 1
static int SharpYuvConvertValueToSampledIdx(int v, int rgb_sampling_size) {
  v = (v + kYuvHalf) >> YUV_FIX;
  v = (v < 0) ? 0 : (v > 255) ? 255 : v;
  return (v * (rgb_sampling_size - 1)) / 255;
}

#undef YUV_FIX

// For each pixel, computes the index to look up that color in a precomputed
// risk score table where the YUV space is subsampled to a size of
// precomputed_scores_table_sampling^3 (see sharpyuv_risk_table.h)
static int SharpYuvConvertToYuvSharpnessIndex(
    int r, int g, int b, const SharpYuvConversionMatrix* matrix,
    int precomputed_scores_table_sampling) {
  const int y = SharpYuvConvertValueToSampledIdx(
      matrix->rgb_to_y[0] * r + matrix->rgb_to_y[1] * g +
          matrix->rgb_to_y[2] * b + matrix->rgb_to_y[3],
      precomputed_scores_table_sampling);
  const int u = SharpYuvConvertValueToSampledIdx(
      matrix->rgb_to_u[0] * r + matrix->rgb_to_u[1] * g +
          matrix->rgb_to_u[2] * b + matrix->rgb_to_u[3],
      precomputed_scores_table_sampling);
  const int v = SharpYuvConvertValueToSampledIdx(
      matrix->rgb_to_v[0] * r + matrix->rgb_to_v[1] * g +
          matrix->rgb_to_v[2] * b + matrix->rgb_to_v[3],
      precomputed_scores_table_sampling);
  return y + u * precomputed_scores_table_sampling +
         v * precomputed_scores_table_sampling *
             precomputed_scores_table_sampling;
}

// TODO(later): use SIMD variants already used in SharpYuvConvert() if possible
static void SharpYuvRowToYuvSharpnessIndex(
    const uint8_t* r_ptr, const uint8_t* g_ptr, const uint8_t* b_ptr,
    int rgb_step, int rgb_bit_depth, int width, uint16_t* dst,
    const SharpYuvConversionMatrix* matrix,
    int precomputed_scores_table_sampling) {
  int i;
  assert(rgb_bit_depth == 8);
  (void)rgb_bit_depth;  // Unused for now.
  for (i = 0; i < width;
       ++i, r_ptr += rgb_step, g_ptr += rgb_step, b_ptr += rgb_step) {
    dst[i] =
        SharpYuvConvertToYuvSharpnessIndex(r_ptr[0], g_ptr[0], b_ptr[0], matrix,
                                           precomputed_scores_table_sampling);
  }
}

//------------------------------------------------------------------------------
// Progressive scoring: the image is scanned by horizontal bands, each
// kBandHeight rows tall and spanning the full image width. Bands are visited
// in bit-reversal order in order to maximize the spread during partial scan,
// while ensuring that the full image will eventually get covered.
// See https://en.wikipedia.org/wiki/Bit-reversal_permutation.
// The running score is checked after every band, and the scan returns early
// once we're confidently inside a decision band, away from the decision
// values in SharpYuvRiskThresholds[]. Here, "confidently" means the current
// std-dev over the last WINDOW_SIZE bands is small relative to the distance
// to the cutoff (see MovingWindowRecord()).

static const int kBandHeight = 8;
// Mirrors the low/medium/high risk boundaries documented in extras.h; keep
// in sync since callers may rely on these exact values.
const double SharpYuvRiskThresholds[2] = {40.0, 70.0};

#define WINDOW_SIZE 32
static const double kFracZScore = 8.0;
static const double kFracMinMargin = 0.1;  // percentage points
static const double kScoreZScore = 5.0;
static const double kScoreMinMargin = 2.0;  // score points (0-100 scale)
static const double kFracCutoff = 1.0;      // percent
static const double kMaxScore = 25.0;  // pre-rescale score saturation point

// Circular buffer of WINDOW_SIZE values, with running sum/sumsq.
typedef struct {
  double values[WINDOW_SIZE];
  double sum, sumsq;
  int count;  // saturates at WINDOW_SIZE
  int next;   // next slot to overwrite
} MovingWindow;

static void MovingWindowInit(MovingWindow* const w) {
  memset(w, 0, sizeof(*w));
}

// Records 'value' and returns how far (in 'value' units) the trend still
// needs to move before we reach confidence: HUGE_VAL until the window is
// full, else z_score * std-dev (floored at 'min_margin').
// 'min_margin' guards against a near-zero std-dev falsely reading as
// confident when 'value' still sits close to its cutoff.
static double MovingWindowRecord(MovingWindow* const w, double value,
                                 double z_score, double min_margin) {
  double mean, variance, margin;
  if (w->count == WINDOW_SIZE) {
    const double old = w->values[w->next];
    w->sum -= old;
    w->sumsq -= old * old;
  } else {
    ++w->count;
  }
  w->values[w->next] = value;
  w->sum += value;
  w->sumsq += value * value;
  w->next = (w->next + 1) % WINDOW_SIZE;

  if (w->count < WINDOW_SIZE) return HUGE_VAL;
  mean = w->sum / w->count;
  variance = w->sumsq / w->count - mean * mean;
  margin = z_score * sqrt((variance > 0.) ? variance : 0.);
  return (margin > min_margin) ? margin : min_margin;
}

#undef WINDOW_SIZE

// Reverses the low 'bits' bits of v, bits in [0, 16].
static int BitReversal16b(int v, int bits) {
  int r = 0;
  int i;
  for (i = 0; i < bits; ++i) {
    r = (r << 1) | (v & 1);
    v >>= 1;
  }
  return r;
}

static const int kNoiseLevel = 4;

// Scores one adjacent row-pair (row1 = above, row2 = below) and accumulates
// into '*sum' / '*count'.
static void ScoreRowPair(const uint16_t* row1, const uint16_t* row2, int width,
                         const uint8_t precomputed_scores_table[],
                         int sampling3, int64_t* sum, int64_t* count) {
  int i;
  for (i = 0; i < width - 1; ++i) {
    const int idx0 = row1[i + 0];
    const int idx1 = row1[i + 1];
    const int idx2 = row2[i + 0];
    const int score = precomputed_scores_table[idx0 + sampling3 * idx1] +
                      precomputed_scores_table[idx0 + sampling3 * idx2] +
                      precomputed_scores_table[idx1 + sampling3 * idx2];
    if (score > kNoiseLevel) {
      *sum += score;
      ++*count;
    }
  }
}

// Derives the risk score in [0:100] from the accumulators collected so far
// (full or partial coverage). '*frac_out', if not NULL, gets the percentage
// of the image area effectively covered by 'rows_scored' rows so far, to be
// compared against kFracCutoff by the caller's progressive-scan trend check.
static double FinalizeRiskScore(int64_t sum, int64_t count, int width,
                                int height, int64_t rows_scored,
                                double* const frac_out) {
  const double cnt = (double)count;
  double total_score = (cnt > 0) ? (double)sum / cnt : 0.;
  // pixels evaluated so far, scaled by how many rows were actually scored
  const double effective_area =
      (rows_scored > 0)
          ? (double)width * height * (double)rows_scored / (height - 1.)
          : 0.;
  const double frac = (effective_area > 0.) ? 100. * cnt / effective_area : 0.;
  if (frac_out != NULL) *frac_out = frac;
  // if less than kFracCutoff% of pixels were evaluated -> need more rows
  if (frac < kFracCutoff) return 0.;
  // rescale to [0:100]
  return (total_score > kMaxScore) ? 100. : total_score * 100. / kMaxScore;
}

static int DoEstimateRisk(const uint8_t* r_ptr, const uint8_t* g_ptr,
                          const uint8_t* b_ptr, int rgb_step, int rgb_stride,
                          int rgb_bit_depth, int width, int height,
                          const SharpYuvOptions* options,
                          const uint8_t precomputed_scores_table[],
                          int precomputed_scores_table_sampling, int full_scan,
                          float* score_out) {
  const int sampling3 = precomputed_scores_table_sampling *
                        precomputed_scores_table_sampling *
                        precomputed_scores_table_sampling;
  // Use faster int64_t accumulation instead of double. A score is at most
  // 3 * 255, so even a 16k x 16k image stays under 2^38.
  int64_t sum = 0;
  int64_t count = 0;
  int64_t rows_scored = 0;
  MovingWindow frac_trend, score_trend;
  // single allocation for the row1/row2 pair
  uint16_t* const rows =
      (uint16_t*)WebPSafeMalloc((uint64_t)width * 2, sizeof(uint16_t));
  uint16_t* row1 = rows;
  uint16_t* row2 = (rows != NULL) ? rows + width : NULL;
  // height is capped well under 16 bits (WEBP_MAX_DIMENSION), so num_bands
  // stays small and bits stays under 16, as required by BitReversal16b().
  const int num_bands = (height - 1 + kBandHeight - 1) / kBandHeight;
  int bits = 0;
  int cursor = 0;
  int k;

  if (rows == NULL) return 0;

  while ((1 << bits) < num_bands) ++bits;
  assert(bits <= 16);
  MovingWindowInit(&frac_trend);
  MovingWindowInit(&score_trend);

  // Visit bands in bit-reversed order (band offsets: 0, 1/2, 1/4, 3/4, ...)
  // instead of top-to-bottom. Each additional band evenly improves coverage
  // and updates the trends until we get an early meaningful estimate.
  for (k = 0; k < num_bands; ++k) {
    int band, j, last_row;
    uint16_t* tmp;
    do {
      band = BitReversal16b(cursor, bits);
      ++cursor;
    } while (band >= num_bands);
    j = band * kBandHeight;
    last_row = (j + 1 + kBandHeight < height) ? j + 1 + kBandHeight : height;
    SharpYuvRowToYuvSharpnessIndex(
        r_ptr + (size_t)j * rgb_stride, g_ptr + (size_t)j * rgb_stride,
        b_ptr + (size_t)j * rgb_stride, rgb_step, rgb_bit_depth, width, row1,
        options->yuv_matrix, precomputed_scores_table_sampling);
    for (++j; j < last_row; ++j) {
      SharpYuvRowToYuvSharpnessIndex(
          r_ptr + (size_t)j * rgb_stride, g_ptr + (size_t)j * rgb_stride,
          b_ptr + (size_t)j * rgb_stride, rgb_step, rgb_bit_depth, width, row2,
          options->yuv_matrix, precomputed_scores_table_sampling);
      ScoreRowPair(row1, row2, width, precomputed_scores_table, sampling3, &sum,
                   &count);
      tmp = row1;
      row1 = row2;
      row2 = tmp;
      ++rows_scored;
    }

    // trend check after every band (negligible cost compared to row scoring)
    if (!full_scan) {
      double frac;
      const double candidate =
          FinalizeRiskScore(sum, count, width, height, rows_scored, &frac);
      const double frac_margin =
          MovingWindowRecord(&frac_trend, frac, kFracZScore, kFracMinMargin);
      const double score_margin = MovingWindowRecord(
          &score_trend, candidate, kScoreZScore, kScoreMinMargin);
      if (fabs(frac - kFracCutoff) >= frac_margin &&
          fabs(candidate - SharpYuvRiskThresholds[0]) >= score_margin &&
          fabs(candidate - SharpYuvRiskThresholds[1]) >= score_margin) {
        WebPFree(rows);
        *score_out = (float)candidate;
        return 1;
      }
    }
  }

  WebPFree(rows);
  *score_out =
      (float)FinalizeRiskScore(sum, count, width, height, rows_scored, NULL);
  return 1;
}

static int SharpYuvEstimate420RiskImpl(const void* r_ptr, const void* g_ptr,
                                       const void* b_ptr, int rgb_step,
                                       int rgb_stride, int rgb_bit_depth,
                                       int width, int height,
                                       const SharpYuvOptions* options,
                                       int full_scan, float* score) {
  if (width < 1 || height < 1 || width == INT_MAX || height == INT_MAX ||
      r_ptr == NULL || g_ptr == NULL || b_ptr == NULL || options == NULL ||
      score == NULL) {
    return 0;
  }
  if (rgb_bit_depth != 8) {
    return 0;
  }

  if (width <= 4 || height <= 4) {
    *score = 0.0f;  // too small, no real risk.
    return 1;
  }

  return DoEstimateRisk((const uint8_t*)r_ptr, (const uint8_t*)g_ptr,
                        (const uint8_t*)b_ptr, rgb_step, rgb_stride,
                        rgb_bit_depth, width, height, options,
                        kSharpYuvPrecomputedRisk,
                        kSharpYuvPrecomputedRiskYuvSampling, full_scan, score);
}

int SharpYuvEstimate420Risk(const void* r_ptr, const void* g_ptr,
                            const void* b_ptr, int rgb_step, int rgb_stride,
                            int rgb_bit_depth, int width, int height,
                            const SharpYuvOptions* options, float* score) {
  return SharpYuvEstimate420RiskImpl(r_ptr, g_ptr, b_ptr, rgb_step, rgb_stride,
                                     rgb_bit_depth, width, height, options,
                                     /*full_scan=*/0, score);
}

int SharpYuvEstimate420RiskFull(const void* r_ptr, const void* g_ptr,
                                const void* b_ptr, int rgb_step, int rgb_stride,
                                int rgb_bit_depth, int width, int height,
                                const SharpYuvOptions* options, float* score) {
  return SharpYuvEstimate420RiskImpl(r_ptr, g_ptr, b_ptr, rgb_step, rgb_stride,
                                     rgb_bit_depth, width, height, options,
                                     /*full_scan=*/1, score);
}

//------------------------------------------------------------------------------
