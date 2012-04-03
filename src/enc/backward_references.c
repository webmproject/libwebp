// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: Jyrki Alakuijala (jyrki@google.com)
//

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "./backward_references.h"
#include "./histogram.h"
#include "../utils/color_cache.h"

#define VALUES_IN_BYTE 256

static const uint8_t plane_to_code_lut[128] = {
 96,   73,  55,  39,  23,  13,   5,  1,  255, 255, 255, 255, 255, 255, 255, 255,
 101,  78,  58,  42,  26,  16,   8,  2,    0,   3,  9,   17,  27,  43,  59,  79,
 102,  86,  62,  46,  32,  20,  10,  6,    4,   7,  11,  21,  33,  47,  63,  87,
 105,  90,  70,  52,  37,  28,  18,  14,  12,  15,  19,  29,  38,  53,  71,  91,
 110,  99,  82,  66,  48,  35,  30,  24,  22,  25,  31,  36,  49,  67,  83, 100,
 115, 108,  94,  76,  64,  50,  44,  40,  34,  41,  45,  51,  65,  77,  95, 109,
 118, 113, 103,  92,  80,  68,  60,  56,  54,  57,  61,  69,  81,  93, 104, 114,
 119, 116, 111, 106,  97,  88,  84,  74,  72,  75,  85,  89,  98, 107, 112, 117,
};

static const int kMinLength = 2;

int DistanceToPlaneCode(int xsize, int dist) {
  int yoffset = dist / xsize;
  int xoffset = dist - yoffset * xsize;
  if (xoffset <= 8 && yoffset < 8) {
    return plane_to_code_lut[yoffset * 16 + 8 - xoffset] + 1;
  } else if (xoffset > xsize - 8 && yoffset < 7) {
    return plane_to_code_lut[(yoffset + 1) * 16 + 8 + (xsize - xoffset)] + 1;
  }
  return dist + 120;
}

static WEBP_INLINE int FindMatchLength(const uint32_t* array1,
                                       const uint32_t* array2,
                                       const int max_limit) {
  int matched = 0;
  while (matched < max_limit && array1[matched] == array2[matched]) {
    ++matched;
  }
  return matched;
}

#define HASH_BITS 18
#define HASH_SIZE (1 << HASH_BITS)
static const uint64_t kHashMultiplier = 0xc6a4a7935bd1e995ULL;
static const int kWindowSize = (1 << 20) - 120;  // A window with 1M pixels
                                                 // (4 megabytes) - 120
                                                 // special codes for short
                                                 // distances.

static WEBP_INLINE uint64_t GetHash64(uint64_t num) {
  num *= kHashMultiplier;
  num >>= 64 - HASH_BITS;
  return num;
}

static WEBP_INLINE uint64_t GetPixPair(const uint32_t* argb) {
  return ((uint64_t)(argb[1]) << 32) | argb[0];
}

typedef struct {
  // Stores the most recently added position with the given hash value.
  int32_t hash_to_first_index_[HASH_SIZE];
  // chain_[pos] stores the previous position with the same hash value
  // for every pixel in the image.
  int32_t* chain_;
} VP8LHashChain;

static int VP8LHashChain_Init(VP8LHashChain* p, int size) {
  int i;
  p->chain_ = (int*)malloc(size * sizeof(*p->chain_));
  if (!p->chain_) {
    return 0;
  }
  for (i = 0; i < size; ++i) {
    p->chain_[i] = -1;
  }
  for (i = 0; i < HASH_SIZE; ++i) {
    p->hash_to_first_index_[i] = -1;
  }
  return 1;
}

static void VP8LHashChain_Delete(VP8LHashChain* p) {
  if (p != NULL) {
    free(p->chain_);
  }
}

static void VP8LHashChain_Insert(VP8LHashChain* p,
                                 const uint32_t* argb, int32_t ix) {
  // Insertion of two pixels at a time.
  const uint64_t key = GetPixPair(argb);
  const uint64_t hash_code = GetHash64(key);
  p->chain_[ix] = p->hash_to_first_index_[hash_code];
  p->hash_to_first_index_[hash_code] = ix;
}

static int VP8LHashChain_FindCopy(VP8LHashChain* p,
                                  int quality,
                                  int index, int xsize,
                                  const uint32_t* argb,
                                  int maxlen, int* offset_out,
                                  int* len_out) {
  const uint64_t next_two_pixels = GetPixPair(&argb[index]);
  const uint64_t hash_code = GetHash64(next_two_pixels);
  int prev_length = 0;
  int64_t best_val = 0;
  int give_up = quality * 3 / 4 + 25;
  const int min_pos = (index > kWindowSize) ? index - kWindowSize : 0;
  int32_t pos;
  int64_t length;
  int64_t val;
  int x;
  int y;
  int len = 0;
  int offset = 0;
  for (pos = p->hash_to_first_index_[hash_code];
       pos >= min_pos;
       pos = p->chain_[pos]) {
    if (give_up < 0) {
      if (give_up < -quality * 8 ||
          best_val >= 0xff0000) {
        break;
      }
    }
    --give_up;
    if (len != 0 && argb[pos + len - 1] != argb[index + len - 1]) {
      continue;
    }
    length = FindMatchLength(argb + pos, argb + index, maxlen);
    if (length < prev_length) {
      continue;
    }
    val = 65536 * length;
    // Favoring 2d locality here gives savings for certain images.
    if (index - pos < 9 * xsize) {
      y = (index - pos) / xsize;
      x = (index - pos) % xsize;
      if (x > xsize / 2) {
        x = xsize - x;
      }
      if (x <= 7 && x >= -8) {
        val -= y * y + x * x;
      } else {
        val -= 9 * 9 + 9 * 9;
      }
    } else {
      val -= 9 * 9 + 9 * 9;
    }
    if (best_val < val) {
      prev_length = length;
      best_val = val;
      len = length;
      offset = index - pos;
      if (length >= kMaxLength) {
        break;
      }
      if ((offset == 1 || offset == xsize) && len >= 128) {
        break;
      }
    }
  }
  *offset_out = offset;
  *len_out = len;
  return len >= kMinLength;
}

static WEBP_INLINE void PushBackCopy(int length,
                                     PixOrCopy* stream,
                                     int* stream_size) {
  while (length >= kMaxLength) {
    stream[*stream_size] = PixOrCopyCreateCopy(1, kMaxLength);
    ++(*stream_size);
    length -= kMaxLength;
  }
  if (length > 0) {
    stream[*stream_size] = PixOrCopyCreateCopy(1, length);
    ++(*stream_size);
  }
}

void BackwardReferencesRle(int xsize, int ysize, const uint32_t* argb,
                           PixOrCopy* stream, int* stream_size) {
  const int pix_count = xsize * ysize;
  int streak = 0;
  int i;
  *stream_size = 0;
  for (i = 0; i < pix_count; ++i) {
    if (i >= 1 && argb[i] == argb[i - 1]) {
      ++streak;
    } else {
      PushBackCopy(streak, stream, stream_size);
      streak = 0;
      stream[*stream_size] = PixOrCopyCreateLiteral(argb[i]);
      ++(*stream_size);
    }
  }
  PushBackCopy(streak, stream, stream_size);
}

// Returns 1 when successful.
int BackwardReferencesHashChain(int xsize, int ysize, int use_palette,
                                const uint32_t* argb, int palette_bits,
                                int quality,
                                PixOrCopy* stream, int* stream_size) {
  const int pix_count = xsize * ysize;
  int i;
  int ok = 0;
  VP8LHashChain* hash_chain = (VP8LHashChain*)malloc(sizeof(*hash_chain));
  VP8LColorCache hashers;
  if (!hash_chain ||
      !VP8LColorCacheInit(&hashers, palette_bits) ||
      !VP8LHashChain_Init(hash_chain, pix_count)) {
    goto Error;
  }
  *stream_size = 0;
  for (i = 0; i < pix_count; ) {
    // Alternative#1: Code the pixels starting at 'i' using backward reference.
    int offset = 0;
    int len = 0;
    if (i < pix_count - 1) {  // FindCopy(i,..) reads pixels at [i] and [i + 1].
      int maxlen = pix_count - i;
      if (maxlen > kMaxLength) {
        maxlen = kMaxLength;
      }
      VP8LHashChain_FindCopy(hash_chain, quality,
                             i, xsize, argb, maxlen, &offset, &len);
    }
    if (len >= kMinLength) {
      // Alternative#2: Insert the pixel at 'i' as literal, and code the
      // pixels starting at 'i + 1' using backward reference.
      int offset2 = 0;
      int len2 = 0;
      int k;
      VP8LHashChain_Insert(hash_chain, &argb[i], i);
      if (i < pix_count - 2) {  // FindCopy(i+1,..) reads [i + 1] and [i + 2].
        int maxlen = pix_count - (i + 1);
        if (maxlen > kMaxLength) {
          maxlen = kMaxLength;
        }
        VP8LHashChain_FindCopy(hash_chain, quality,
                               i + 1, xsize, argb, maxlen, &offset2, &len2);
        if (len2 > len + 1) {
          // Alternative#2 is a better match. So push pixel at 'i' as literal.
          if (use_palette && VP8LColorCacheContains(&hashers, argb[i])) {
            const int ix = VP8LColorCacheGetIndex(&hashers, argb[i]);
            stream[*stream_size] = PixOrCopyCreatePaletteIx(ix);
          } else {
            stream[*stream_size] = PixOrCopyCreateLiteral(argb[i]);
          }
          ++(*stream_size);
          VP8LColorCacheInsert(&hashers, argb[i]);
          i++;  // Backward reference to be done for next pixel.
          len = len2;
          offset = offset2;
        }
      }
      if (len >= kMaxLength) {
        len = kMaxLength - 1;
      }
      stream[*stream_size] = PixOrCopyCreateCopy(offset, len);
      ++(*stream_size);
      for (k = 0; k < len; ++k) {
        VP8LColorCacheInsert(&hashers, argb[i + k]);
        if (k != 0 && i + k + 1 < pix_count) {
          // Add to the hash_chain (but cannot add the last pixel).
          VP8LHashChain_Insert(hash_chain, &argb[i + k], i + k);
        }
      }
      i += len;
    } else {
      if (use_palette && VP8LColorCacheContains(&hashers, argb[i])) {
        // push pixel as a palette pixel
        int ix = VP8LColorCacheGetIndex(&hashers, argb[i]);
        stream[*stream_size] = PixOrCopyCreatePaletteIx(ix);
      } else {
        stream[*stream_size] = PixOrCopyCreateLiteral(argb[i]);
      }
      ++(*stream_size);
      VP8LColorCacheInsert(&hashers, argb[i]);
      if (i + 1 < pix_count) {
        VP8LHashChain_Insert(hash_chain, &argb[i], i);
      }
      ++i;
    }
  }
  ok = 1;
Error:
  VP8LHashChain_Delete(hash_chain);
  free(hash_chain);
  VP8LColorCacheDelete(&hashers);
  return ok;
}

typedef struct {
  double alpha_[VALUES_IN_BYTE];
  double red_[VALUES_IN_BYTE];
  double literal_[PIX_OR_COPY_CODES_MAX];
  double blue_[VALUES_IN_BYTE];
  double distance_[DISTANCE_CODES_MAX];
  int palette_bits_;
} CostModel;

static int CostModel_Build(CostModel* p, int xsize, int ysize,
                           int recursion_level, int use_palette,
                           const uint32_t* argb, int palette_bits) {
  int ok = 0;
  int stream_size;
  Histogram histo;
  int i;
  PixOrCopy* stream = (PixOrCopy*)malloc(xsize * ysize * sizeof(*stream));
  if (stream == NULL) {
    goto Error;
  }
  p->palette_bits_ = palette_bits;
  if (recursion_level > 0) {
    if (!BackwardReferencesTraceBackwards(xsize, ysize, recursion_level - 1,
                                          use_palette, argb,
                                          palette_bits,
                                          &stream[0], &stream_size)) {
      goto Error;
    }
  } else {
    const int quality = 100;
    if (!BackwardReferencesHashChain(xsize, ysize, use_palette, argb,
                                     palette_bits, quality,
                                     &stream[0], &stream_size)) {
      goto Error;
    }
  }
  HistogramInit(&histo, palette_bits);
  for (i = 0; i < stream_size; ++i) {
    HistogramAddSinglePixOrCopy(&histo, stream[i]);
  }
  ConvertPopulationCountTableToBitEstimates(
      HistogramNumPixOrCopyCodes(&histo),
      &histo.literal_[0], &p->literal_[0]);
  ConvertPopulationCountTableToBitEstimates(
      VALUES_IN_BYTE, &histo.red_[0], &p->red_[0]);
  ConvertPopulationCountTableToBitEstimates(
      VALUES_IN_BYTE, &histo.blue_[0], &p->blue_[0]);
  ConvertPopulationCountTableToBitEstimates(
      VALUES_IN_BYTE, &histo.alpha_[0], &p->alpha_[0]);
  ConvertPopulationCountTableToBitEstimates(
      DISTANCE_CODES_MAX, &histo.distance_[0], &p->distance_[0]);
  ok = 1;
Error:
  free(stream);
  return ok;
}

static WEBP_INLINE double CostModel_LiteralCost(const CostModel* p,
                                                uint32_t v) {
  return p->alpha_[v >> 24] +
      p->red_[(v >> 16) & 0xff] +
      p->literal_[(v >> 8) & 0xff] +
      p->blue_[v & 0xff];
}

static WEBP_INLINE double CostModel_PaletteCost(const CostModel* p,
                                                uint32_t ix) {
  int literal_ix = VALUES_IN_BYTE + kLengthCodes + ix;
  return p->literal_[literal_ix];
}

static WEBP_INLINE double CostModel_LengthCost(const CostModel* p,
                                               uint32_t len) {
  int code, extra_bits_count, extra_bits_value;
  PrefixEncode(len, &code, &extra_bits_count, &extra_bits_value);
  return p->literal_[VALUES_IN_BYTE + code] + extra_bits_count;
}

static WEBP_INLINE double CostModel_DistanceCost(const CostModel* p,
                                                 uint32_t distance) {
  int code, extra_bits_count, extra_bits_value;
  PrefixEncode(distance, &code, &extra_bits_count, &extra_bits_value);
  return p->distance_[code] + extra_bits_count;
}

static int BackwardReferencesHashChainDistanceOnly(
    int xsize, int ysize,
    int recursive_cost_model,
    int use_palette,
    const uint32_t* argb,
    int palette_bits,
    uint32_t* dist_array) {
  const int quality = 100;
  const int pix_count = xsize * ysize;
  double* cost = (double*)malloc(pix_count * sizeof(*cost));
  int i;
  CostModel* cost_model = (CostModel*)malloc(sizeof(*cost_model));

  VP8LColorCache hashers;
  VP8LHashChain* hash_chain = (VP8LHashChain*)malloc(sizeof(*hash_chain));
  int ok = 0;
  if (cost == NULL ||
      cost_model == NULL ||
      hash_chain == NULL ||
      !VP8LColorCacheInit(&hashers, palette_bits)) {
    goto Error;
  }
  VP8LHashChain_Init(hash_chain, pix_count);
  CostModel_Build(cost_model, xsize, ysize, recursive_cost_model,
                  use_palette, argb, palette_bits);
  for (i = 0; i < pix_count; ++i) {
    cost[i] = 1e100;
  }
  // We loop one pixel at a time, but store all currently best points to
  // non-processed locations from this point.
  dist_array[0] = 0;
  for (i = 0; i < pix_count; ++i) {
    double prev_cost = 0.0;
    int shortmax;
    if (i > 0) {
      prev_cost = cost[i - 1];
    }
    for (shortmax = 0; shortmax < 2; ++shortmax) {
      int offset = 0;
      int len = 0;
      if (i < pix_count - 1) {  // FindCopy reads pixels at [i] and [i + 1].
        int maxlen = shortmax ? 2 : kMaxLength;
        if (maxlen > pix_count - i) {
          maxlen = pix_count - i;
        }
        VP8LHashChain_FindCopy(hash_chain, quality, i, xsize, argb, maxlen,
                               &offset, &len);
      }
      if (len >= kMinLength) {
        const int code = DistanceToPlaneCode(xsize, offset);
        const double distance_cost =
            prev_cost + CostModel_DistanceCost(cost_model, code);
        int k;
        for (k = 1; k < len; ++k) {
          const double cost_val =
              distance_cost + CostModel_LengthCost(cost_model, k);
          if (cost[i + k] > cost_val) {
            cost[i + k] = cost_val;
            dist_array[i + k] = k + 1;
          }
        }
        // This if is for speedup only. It roughly doubles the speed, and
        // makes compression worse by .1 %.
        if (len >= 128 && code < 2) {
          // Long copy for short distances, let's skip the middle
          // lookups for better copies.
          // 1) insert the hashes.
          for (k = 0; k < len; ++k) {
            VP8LColorCacheInsert(&hashers, argb[i + k]);
            if (i + k + 1 < pix_count) {
              // Add to the hash_chain (but cannot add the last pixel).
              VP8LHashChain_Insert(hash_chain, &argb[i + k], i + k);
            }
          }
          // 2) jump.
          i += len - 1;  // for loop does ++i, thus -1 here.
          goto next_symbol;
        }
      }
    }
    if (i < pix_count - 1) {
      VP8LHashChain_Insert(hash_chain, &argb[i], i);
    }
    {
      // inserting a literal pixel
      double cost_val = prev_cost;
      double mul0 = 1.0;
      double mul1 = 1.0;
      if (recursive_cost_model == 0) {
        mul0 = 0.68;
        mul1 = 0.82;
      }
      if (use_palette && VP8LColorCacheContains(&hashers, argb[i])) {
        int ix = VP8LColorCacheGetIndex(&hashers, argb[i]);
        cost_val += CostModel_PaletteCost(cost_model, ix) * mul0;
      } else {
        cost_val += CostModel_LiteralCost(cost_model, argb[i]) * mul1;
      }
      if (cost[i] > cost_val) {
        cost[i] = cost_val;
        dist_array[i] = 1;  // only one is inserted.
      }
      VP8LColorCacheInsert(&hashers, argb[i]);
    }
 next_symbol: ;
  }
  // Last pixel still to do, it can only be a single step if not reached
  // through cheaper means already.
  ok = 1;
Error:
  if (hash_chain) VP8LHashChain_Delete(hash_chain);
  free(hash_chain);
  free(cost_model);
  free(cost);
  VP8LColorCacheDelete(&hashers);
  return ok;
}

static void TraceBackwards(const uint32_t* dist_array, int dist_array_size,
                           uint32_t** chosen_path, int* chosen_path_size) {
  int i;
  // Count how many.
  int count = 0;
  for (i = dist_array_size - 1; i >= 0; ) {
    int k = dist_array[i];
    assert(k >= 1);
    ++count;
    i -= k;
  }
  // Allocate.
  *chosen_path_size = count;
  *chosen_path = (uint32_t*)malloc(count * sizeof(*chosen_path));
  // Write in reverse order.
  for (i = dist_array_size - 1; i >= 0; ) {
    int k = dist_array[i];
    assert(k >= 1);
    (*chosen_path)[--count] = k;
    i -= k;
  }
}

static int BackwardReferencesHashChainFollowChosenPath(
    int xsize,
    int ysize,
    int use_palette,
    const uint32_t* argb,
    int palette_bits,
    uint32_t* chosen_path,
    int chosen_path_size,
    PixOrCopy* stream,
    int* stream_size) {
  const int quality = 100;
  const int pix_count = xsize * ysize;
  int i = 0;
  int k;
  int ix;
  int ok = 0;
  VP8LColorCache hashers;
  VP8LHashChain* hash_chain = (VP8LHashChain*)malloc(sizeof(*hash_chain));
  VP8LHashChain_Init(hash_chain, pix_count);
  if (hash_chain == NULL ||
      !VP8LColorCacheInit(&hashers, palette_bits)) {
    goto Error;
  }
  *stream_size = 0;
  for (ix = 0; ix < chosen_path_size; ++ix) {
    int offset = 0;
    int len = 0;
    int maxlen = chosen_path[ix];
    if (maxlen != 1) {
      VP8LHashChain_FindCopy(hash_chain, quality,
                             i, xsize, argb, maxlen, &offset, &len);
      assert(len == maxlen);
      stream[*stream_size] = PixOrCopyCreateCopy(offset, len);
      ++(*stream_size);
      for (k = 0; k < len; ++k) {
        VP8LColorCacheInsert(&hashers, argb[i + k]);
        if (i + k + 1 < pix_count) {
          // Add to the hash_chain (but cannot add the last pixel).
          VP8LHashChain_Insert(hash_chain, &argb[i + k], i + k);
        }
      }
      i += len;
    } else {
      if (use_palette && VP8LColorCacheContains(&hashers, argb[i])) {
        // push pixel as a palette pixel
        int ix = VP8LColorCacheGetIndex(&hashers, argb[i]);
        stream[*stream_size] = PixOrCopyCreatePaletteIx(ix);
      } else {
        stream[*stream_size] = PixOrCopyCreateLiteral(argb[i]);
      }
      ++(*stream_size);
      VP8LColorCacheInsert(&hashers, argb[i]);
      if (i + 1 < pix_count) {
        VP8LHashChain_Insert(hash_chain, &argb[i], i);
      }
      ++i;
    }
  }
  ok = 1;
Error:
  VP8LHashChain_Delete(hash_chain);
  if (hash_chain) {
    free(hash_chain);
  }
  VP8LColorCacheDelete(&hashers);
  return ok;
}

// Returns 1 on success.
int BackwardReferencesTraceBackwards(int xsize, int ysize,
                                     int recursive_cost_model,
                                     int use_palette,
                                     const uint32_t* argb,
                                     int palette_bits,
                                     PixOrCopy* stream,
                                     int* stream_size) {
  int ok = 0;
  const int dist_array_size = xsize * ysize;
  uint32_t* chosen_path = NULL;
  int chosen_path_size = 0;
  uint32_t* const dist_array = (uint32_t*)
      malloc(dist_array_size * sizeof(*dist_array));
  if (dist_array == NULL) {
    goto Error;
  }
  *stream_size = 0;
  if (!BackwardReferencesHashChainDistanceOnly(
          xsize, ysize, recursive_cost_model, use_palette, argb, palette_bits,
          dist_array)) {
    free(dist_array);
    goto Error;
  }
  TraceBackwards(dist_array, dist_array_size, &chosen_path, &chosen_path_size);
  free(dist_array);
  if (!BackwardReferencesHashChainFollowChosenPath(
          xsize, ysize, use_palette, argb, palette_bits,
          chosen_path, chosen_path_size,
          stream, stream_size)) {
    goto Error;
  }
  ok = 1;
Error:
  free(chosen_path);
  return ok;
}

void BackwardReferences2DLocality(int xsize, int data_size, PixOrCopy* data) {
  int i;
  for (i = 0; i < data_size; ++i) {
    if (PixOrCopyIsCopy(&data[i])) {
      int dist = data[i].argb_or_offset;
      int transformed_dist = DistanceToPlaneCode(xsize, dist);
      data[i].argb_or_offset = transformed_dist;
    }
  }
}

int VerifyBackwardReferences(const uint32_t* argb, int xsize, int ysize,
                             int palette_bits,
                             const PixOrCopy* lit,
                             int lit_size) {
  int num_pixels = 0;
  int i;
  VP8LColorCache hashers;
  VP8LColorCacheInit(&hashers, palette_bits);
  for (i = 0; i < lit_size; ++i) {
    if (PixOrCopyIsLiteral(&lit[i])) {
      if (argb[num_pixels] != PixOrCopyArgb(&lit[i])) {
        printf("i %d, pixel %d, original: 0x%08x, literal: 0x%08x\n",
               i, num_pixels, argb[num_pixels], PixOrCopyArgb(&lit[i]));
        VP8LColorCacheDelete(&hashers);
        return 0;
      }
      VP8LColorCacheInsert(&hashers, argb[num_pixels]);
      ++num_pixels;
    } else if (PixOrCopyIsPaletteIx(&lit[i])) {
      uint32_t palette_entry =
          VP8LColorCacheLookup(&hashers, PixOrCopyPaletteIx(&lit[i]));
      if (argb[num_pixels] != palette_entry) {
        printf("i %d, pixel %d, original: 0x%08x, palette_ix: %d, "
               "palette_entry: 0x%08x\n",
               i, num_pixels, argb[num_pixels], PixOrCopyPaletteIx(&lit[i]),
               palette_entry);
        VP8LColorCacheDelete(&hashers);
        return 0;
      }
      VP8LColorCacheInsert(&hashers, argb[num_pixels]);
      ++num_pixels;
    } else if (PixOrCopyIsCopy(&lit[i])) {
      int k;
      if (PixOrCopyDistance(&lit[i]) == 0) {
        printf("Bw reference with zero distance.\n");
        VP8LColorCacheDelete(&hashers);
        return 0;
      }
      for (k = 0; k < lit[i].len; ++k) {
        if (argb[num_pixels] !=
            argb[num_pixels - PixOrCopyDistance(&lit[i])]) {
          printf("i %d, pixel %d, original: 0x%08x, copied: 0x%08x, dist: %d\n",
                 i, num_pixels, argb[num_pixels],
                 argb[num_pixels - PixOrCopyDistance(&lit[i])],
                 PixOrCopyDistance(&lit[i]));
          VP8LColorCacheDelete(&hashers);
          return 0;
        }
        VP8LColorCacheInsert(&hashers, argb[num_pixels]);
        ++num_pixels;
      }
    }
  }
  {
    const int pix_count = xsize * ysize;
    if (num_pixels != pix_count) {
      printf("verify failure: %d != %d\n", num_pixels, pix_count);
      VP8LColorCacheDelete(&hashers);
      return 0;
    }
  }
  VP8LColorCacheDelete(&hashers);
  return 1;
}

// Returns 1 on success.
static int ComputePaletteHistogram(const uint32_t* argb, int xsize, int ysize,
                                   PixOrCopy* stream, int stream_size,
                                   int palette_bits, Histogram* histo) {
  int pixel_index = 0;
  int i;
  uint32_t k;
  VP8LColorCache hashers;
  if (!VP8LColorCacheInit(&hashers, palette_bits)) {
    return 0;
  }
  for (i = 0; i < stream_size; ++i) {
    const PixOrCopy v = stream[i];
    if (PixOrCopyIsLiteral(&v)) {
      if (palette_bits != 0 &&
          VP8LColorCacheContains(&hashers, argb[pixel_index])) {
        // push pixel as a palette pixel
        const int ix = VP8LColorCacheGetIndex(&hashers, argb[pixel_index]);
        HistogramAddSinglePixOrCopy(histo, PixOrCopyCreatePaletteIx(ix));
      } else {
        HistogramAddSinglePixOrCopy(histo, v);
      }
    } else {
      HistogramAddSinglePixOrCopy(histo, v);
    }
    for (k = 0; k < PixOrCopyLength(&v); ++k) {
      VP8LColorCacheInsert(&hashers, argb[pixel_index]);
      ++pixel_index;
    }
  }
  assert(pixel_index == xsize * ysize);
  (void)xsize;  // xsize is not used in non-debug compilations otherwise.
  (void)ysize;  // ysize is not used in non-debug compilations otherwise.
  VP8LColorCacheDelete(&hashers);
  return 1;
}

// Returns how many bits are to be used for a palette.
int CalculateEstimateForPaletteSize(const uint32_t* argb,
                                    int xsize, int ysize,
                                    int* best_palette_bits) {
  int ok = 0;
  int palette_bits;
  double lowest_entropy = 1e99;
  PixOrCopy* stream = (PixOrCopy*)malloc(xsize * ysize * sizeof(*stream));
  int stream_size;
  static const double kSmallPenaltyForLargePalette = 4.0;
  static const int quality = 30;
  if (stream == NULL ||
      !BackwardReferencesHashChain(xsize, ysize,
                                   0, argb, 0, quality, stream, &stream_size)) {
    goto Error;
  }
  for (palette_bits = 0; palette_bits < 12; ++palette_bits) {
    double cur_entropy;
    Histogram histo;
    HistogramInit(&histo, palette_bits);
    ComputePaletteHistogram(argb, xsize, ysize, &stream[0], stream_size,
                            palette_bits, &histo);
    cur_entropy = HistogramEstimateBits(&histo) +
        kSmallPenaltyForLargePalette * palette_bits;
    if (palette_bits == 0 || cur_entropy < lowest_entropy) {
      *best_palette_bits = palette_bits;
      lowest_entropy = cur_entropy;
    }
  }
  ok = 1;
Error:
  free(stream);
  return ok;
}
