// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// main entry for the lossless encoder.
//
// Author: Vikas Arora (vikaas.arora@gmail.com)
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "./backward_references.h"
#include "./vp8enci.h"
#include "./vp8li.h"
#include "../dsp/lossless.h"
#include "../utils/bit_writer.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static const uint32_t kImageSizeBits = 14;

static int Uint32Order(const void* p1, const void* p2) {
  const uint32_t a = *(const uint32_t*)p1;
  const uint32_t b = *(const uint32_t*)p2;
  if (a < b) {
    return -1;
  }
  if (a == b) {
    return 0;
  }
  return 1;
}

static int CreatePalette256(const uint32_t* const argb, int num_pix,
                            uint32_t* const palette, int* const palette_size) {
  int i, key;
  int current_size = 0;
  uint8_t in_use[MAX_PALETTE_SIZE * 4];
  uint32_t colors[MAX_PALETTE_SIZE * 4];
  static const uint32_t kHashMul = 0x1e35a7bd;

  memset(in_use, 0, sizeof(in_use));
  key = (kHashMul * argb[0]) >> PALETTE_KEY_RIGHT_SHIFT;
  colors[key] = argb[0];
  in_use[key] = 1;
  ++current_size;

  for (i = 1; i < num_pix; ++i) {
    if (argb[i] == argb[i - 1]) {
      continue;
    }
    key = (kHashMul * argb[i]) >> PALETTE_KEY_RIGHT_SHIFT;
    while (1) {
      if (!in_use[key]) {
        colors[key] = argb[i];
        in_use[key] = 1;
        ++current_size;
        if (current_size > MAX_PALETTE_SIZE) {
          return 0;
        }
        break;
      } else if (colors[key] == argb[i]) {
        // The color is already there.
        break;
      } else {
        // Some other color sits there.
        // Do linear conflict resolution.
        ++key;
        key &= 0x3ff;  // key for 1K buffer.
      }
    }
  }

  *palette_size = 0;
  for (i = 0; i < (int)sizeof(in_use); ++i) {
    if (in_use[i]) {
      palette[*palette_size] = colors[i];
      ++(*palette_size);
    }
  }

  qsort(palette, *palette_size, sizeof(*palette), Uint32Order);
  return 1;
}

static int AnalyzeEntropy(const uint32_t const *argb, int xsize, int ysize,
                          int* nonpredicted_bits, int* predicted_bits) {
  int i;
  uint32_t pix_diff;
  VP8LHistogram* nonpredicted = NULL;
  VP8LHistogram* predicted = (VP8LHistogram*)malloc(2 * sizeof(*predicted));
  if (predicted == NULL) return 0;
  nonpredicted = predicted + sizeof(*predicted);

  VP8LHistogramInit(predicted, 0);
  VP8LHistogramInit(nonpredicted, 0);
  for (i = 1; i < xsize * ysize; ++i) {
    if ((argb[i] == argb[i - 1]) ||
        (i >= xsize && argb[i] == argb[i - xsize])) {
      continue;
    }
    VP8LHistogramAddSinglePixOrCopy(nonpredicted,
                                    PixOrCopyCreateLiteral(argb[i]));
    pix_diff = VP8LSubPixels(argb[i], argb[i - 1]);
    VP8LHistogramAddSinglePixOrCopy(predicted,
                                    PixOrCopyCreateLiteral(pix_diff));
  }
  *nonpredicted_bits = (int)VP8LHistogramEstimateBitsBulk(nonpredicted);
  *predicted_bits = (int)VP8LHistogramEstimateBitsBulk(predicted);
  free(predicted);
  return 1;
}

static int VP8LEncAnalyze(VP8LEncoder* const enc) {
  const WebPPicture* const pic = enc->pic_;
  int non_pred_entropy, pred_entropy;
  int is_photograph = 0;
  assert(pic && pic->argb);

  if (!AnalyzeEntropy(pic->argb, pic->width, pic->height,
                      &non_pred_entropy, &pred_entropy)) {
    return 0;
  }
  is_photograph =
      pred_entropy < (non_pred_entropy - (non_pred_entropy >> 3));

  if (is_photograph) {
    enc->use_predict_ = 1;
    enc->use_cross_color_ = 1;
  }

  enc->use_palette_ = CreatePalette256(pic->argb, pic->width * pic->height,
                                       enc->palette_, &enc->palette_size_);
  return 1;
}

// Bundles multiple (2, 4 or 8) pixels into a single pixel.
// Returns the new xsize.
static void BundleColorMap(const uint32_t* const argb,
                           int width, int height, int xbits,
                           uint32_t* bundled_argb, int xs) {
  int x, y;
  const int bit_depth = 1 << (3 - xbits);
  uint32_t code = 0;

  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      const int xsub = x & ((1 << xbits) - 1);
      if (xsub == 0) {
        code = 0;
      }
      code |= (argb[y * width + x] & 0xff00) << (bit_depth * xsub);
      bundled_argb[y * xs + (x >> xbits)] = 0xff000000 | code;
    }
  }
}

static int EncodeImageInternal(VP8LBitWriter* const bw,
                               const uint32_t* const argb,
                               int width, int height, int quality,
                               int cache_bits, int histogram_bits) {
  (void)bw;
  (void)argb;
  (void)width;
  (void)height;
  (void)quality;
  (void)cache_bits;
  (void)histogram_bits;
  return 1;
}

static int EvalAndApplySubtractGreen(VP8LBitWriter* const bw,
                                     VP8LEncoder* const enc,
                                     int width, int height) {
  int i;
  VP8LHistogram* before = NULL;
  // Check if it would be a good idea to subtract green from red and blue.
  VP8LHistogram* after = (VP8LHistogram*)malloc(2 * sizeof(*after));
  if (after == NULL) return 0;
  before = after + sizeof(*after);

  VP8LHistogramInit(before, 1);
  VP8LHistogramInit(after, 1);
  for (i = 0; i < width * height; ++i) {
    // We only impact entropy in red and blue components, don't bother
    // to look at others.
    const uint32_t c = enc->argb_[i];
    const int green = (c >> 8) & 0xff;
    ++(before->red_[(c >> 16) & 0xff]);
    ++(before->blue_[c & 0xff]);
    ++(after->red_[((c >> 16) - green) & 0xff]);
    ++(after->blue_[(c - green) & 0xff]);
  }
  // Check if subtracting green yields low entropy.
  if (VP8LHistogramEstimateBits(after) < VP8LHistogramEstimateBits(before)) {
    VP8LWriteBits(bw, 1, 1);
    VP8LWriteBits(bw, 2, 2);
    VP8LSubtractGreenFromBlueAndRed(enc->argb_, width * height);
  }
  free(after);
  return 1;
}

static int ApplyPredictFilter(VP8LBitWriter* const bw,
                              VP8LEncoder* const enc,
                              int width, int height, int quality) {
  const int pred_bits = enc->transform_bits_;
  const int transform_width = VP8LSubSampleSize(width, pred_bits);
  const int transform_height = VP8LSubSampleSize(height, pred_bits);

  VP8LResidualImage(width, height, pred_bits, enc->argb_, enc->transform_data_);
  VP8LWriteBits(bw, 1, 1);
  VP8LWriteBits(bw, 2, 0);
  VP8LWriteBits(bw, 4, pred_bits);
  if (!EncodeImageInternal(bw, enc->transform_data_,
                           transform_width, transform_height, quality, 0, 0)) {
    return 0;
  }
  return 1;
}

static int ApplyCrossColorFilter(VP8LBitWriter* const bw,
                                 VP8LEncoder* const enc,
                                 int width, int height, int quality) {
  const int ccolor_transform_bits = enc->transform_bits_;
  const int transform_width = VP8LSubSampleSize(width, ccolor_transform_bits);
  const int transform_height = VP8LSubSampleSize(height, ccolor_transform_bits);

  VP8LColorSpaceTransform(width, height, ccolor_transform_bits, quality,
                          enc->argb_, enc->transform_data_);
  VP8LWriteBits(bw, 1, 1);
  VP8LWriteBits(bw, 2, 1);
  VP8LWriteBits(bw, 4, ccolor_transform_bits);
  if (!EncodeImageInternal(bw, enc->transform_data_,
                           transform_width, transform_height, quality, 0, 0)) {
    return 0;
  }
  return 1;
}

static void PutLE32(uint8_t* const data, uint32_t val) {
  data[0] = (val >>  0) & 0xff;
  data[1] = (val >>  8) & 0xff;
  data[2] = (val >> 16) & 0xff;
  data[3] = (val >> 24) & 0xff;
}

static WebPEncodingError WriteRiffHeader(VP8LEncoder* const enc,
                                         size_t riff_size, size_t vp8l_size) {
  const WebPPicture* const pic = enc->pic_;
  uint8_t riff[HEADER_SIZE + SIGNATURE_SIZE] = {
    'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P',
    'V', 'P', '8', 'L', 0, 0, 0, 0, LOSSLESS_MAGIC_BYTE,
  };
  if (riff_size < (vp8l_size + TAG_SIZE + CHUNK_HEADER_SIZE)) {
    return VP8_ENC_ERROR_INVALID_CONFIGURATION;
  }
  PutLE32(riff + TAG_SIZE, (uint32_t)riff_size);
  PutLE32(riff + RIFF_HEADER_SIZE + TAG_SIZE, (uint32_t)vp8l_size);
  if (!pic->writer(riff, sizeof(riff), pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }
  return VP8_ENC_OK;
}

static WebPEncodingError WriteImage(VP8LEncoder* const enc,
                                    VP8LBitWriter* const bw) {
  size_t riff_size, vp8l_size, webpll_size, pad;
  const WebPPicture* const pic = enc->pic_;
  WebPEncodingError err = VP8_ENC_OK;
  const uint8_t* const webpll_data = VP8LBitWriterFinish(bw);

  webpll_size = VP8LBitWriterNumBytes(bw);
  vp8l_size = SIGNATURE_SIZE + webpll_size;
  pad = vp8l_size & 1;
  vp8l_size += pad;

  riff_size = TAG_SIZE + CHUNK_HEADER_SIZE + vp8l_size;
  err = WriteRiffHeader(enc, riff_size, vp8l_size);
  if (err != VP8_ENC_OK) goto Error;

  if (!pic->writer(webpll_data, webpll_size, pic)) {
    err = VP8_ENC_ERROR_BAD_WRITE;
    goto Error;
  }

  if (pad) {
    const uint8_t pad_byte[1] = { 0 };
    if (!pic->writer(pad_byte, 1, pic)) {
      err = VP8_ENC_ERROR_BAD_WRITE;
      goto Error;
    }
  }
  return VP8_ENC_OK;

 Error:
  return err;
}

static VP8LEncoder* InitVP8LEncoder(const WebPConfig* const config,
                                    WebPPicture* const picture) {
  VP8LEncoder* enc;
  (void)config;

  enc = (VP8LEncoder*)malloc(sizeof(*enc));
  if (enc == NULL) {
    WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  memset(enc, 0, sizeof(*enc));

  enc->pic_ = picture;
  enc->use_lz77_ = 1;
  enc->palette_bits_ = 7;

  enc->argb_ = NULL;
  enc->width_ = picture->width;

  // TODO: Use config.quality to initialize histo_bits_ and transform_bits_.
  enc->histo_bits_ = 4;
  enc->transform_bits_ = 4;

  return enc;
}

static void WriteImageSize(VP8LEncoder* const enc, VP8LBitWriter* const bw) {
  WebPPicture* const pic = enc->pic_;
  const int width = pic->width - 1;
  const int height = pic->height -1;
  assert(width < WEBP_MAX_DIMENSION && height < WEBP_MAX_DIMENSION);

  VP8LWriteBits(bw, kImageSizeBits, width);
  VP8LWriteBits(bw, kImageSizeBits, height);
}

static void DeleteVP8LEncoder(VP8LEncoder* enc) {
  free(enc);
}

static WebPEncodingError AllocateEncodeBuffer(VP8LEncoder* const enc,
                                              int height, int width) {
  WebPEncodingError err = VP8_ENC_OK;
  const size_t image_size = height * width;
  const size_t transform_data_size =
      VP8LSubSampleSize(height, enc->transform_bits_) *
      VP8LSubSampleSize(width, enc->transform_bits_);
  const size_t total_size = image_size + transform_data_size;
  enc->argb_ = (uint32_t*)malloc(total_size * sizeof(*enc->argb_));
  if (enc->argb_ == NULL) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }
  enc->transform_data_ = enc->argb_ + image_size;

 Error:
  return err;
}

int VP8LEncodeImage(const WebPConfig* const config,
                    WebPPicture* const picture) {
  int i;
  int ok = 0;
  int use_color_cache = 1;
  int cache_bits = 7;
  int width, height, quality;
  VP8LEncoder* enc = NULL;
  WebPEncodingError err = VP8_ENC_OK;
  VP8LBitWriter bw;

  if (config == NULL || picture == NULL) return 0;

  if (picture->argb == NULL) {
    err = VP8_ENC_ERROR_NULL_PARAMETER;
    goto Error;
  }

  enc = InitVP8LEncoder(config, picture);
  if (enc == NULL) {
    err = VP8_ENC_ERROR_NULL_PARAMETER;
    goto Error;
  }
  width = picture->width;
  height = picture->height;
  quality = config->quality;

  VP8LBitWriterInit(&bw, (width * height) >> 1);

  // ---------------------------------------------------------------------------
  // Analyze image (entropy, num_palettes etc)

  if (!VP8LEncAnalyze(enc)) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  // Write image size.
  WriteImageSize(enc, &bw);

  if (enc->use_palette_) {
    uint32_t* argb = picture->argb;
    const uint32_t* const palette = enc->palette_;
    const int palette_size = enc->palette_size_;
    uint32_t argb_palette[MAX_PALETTE_SIZE];

    for (i = 0; i < width * height; ++i) {
      int k;
      for (k = 0; k < palette_size; ++k) {
        if (argb[i] == palette[k]) {
          argb_palette[i] = 0xff000000 | (k << 8);
          break;
        }
      }
    }
    VP8LWriteBits(&bw, 1, 1);
    VP8LWriteBits(&bw, 2, 3);
    VP8LWriteBits(&bw, 8, palette_size - 1);
    for (i = palette_size - 1; i >= 1; --i) {
      argb_palette[i] = VP8LSubPixels(palette[i], palette[i - 1]);
    }
    if (!EncodeImageInternal(&bw, argb_palette, palette_size, 1, quality,
                             0, 0)) {
      goto Error;
    }
    use_color_cache = 0;
    if (palette_size <= 16) {
      int xbits = 1;
      if (palette_size <= 2) {
        xbits = 3;
      } else if (palette_size <= 4) {
        xbits = 2;
      }

      // Image can be packed (multiple pixels per uint32).
      enc->width_ = VP8LSubSampleSize(width, xbits);
      err = AllocateEncodeBuffer(enc, height, enc->width_);
      if (err != VP8_ENC_OK) goto Error;
      BundleColorMap(argb, width, height, xbits, enc->argb_, enc->width_);
    }
  }

  // In case image is not packed.
  if (enc->argb_ == NULL) {
    const size_t image_size = height * enc->width_;
    err = AllocateEncodeBuffer(enc, height, enc->width_);
    if (err != VP8_ENC_OK) goto Error;
    memcpy(enc->argb_, picture->argb, image_size * sizeof(*enc->argb_));
  }

  // ---------------------------------------------------------------------------
  // Apply transforms and write transform data.

  if (!EvalAndApplySubtractGreen(&bw, enc, enc->width_, height)) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  if (enc->use_predict_) {
    if (!ApplyPredictFilter(&bw, enc, enc->width_, height, quality)) {
      err = VP8_ENC_ERROR_INVALID_CONFIGURATION;
      goto Error;
    }
  }

  if (enc->use_cross_color_) {
    if (!ApplyCrossColorFilter(&bw, enc, enc->width_, height, quality)) {
      err = VP8_ENC_ERROR_INVALID_CONFIGURATION;
      goto Error;
    }
  }

  if (use_color_cache) {
    if (quality > 25) {
      if (!VP8LCalculateEstimateForPaletteSize(enc->argb_, enc->width_, height,
                                               &cache_bits)) {
        err = VP8_ENC_ERROR_INVALID_CONFIGURATION;
        goto Error;
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Encode and write the transformed image.

  ok = EncodeImageInternal(&bw, enc->argb_, enc->width_, height,
                           quality, cache_bits, enc->histo_bits_);
  if (!ok) goto Error;

  err = WriteImage(enc, &bw);
  if (err != VP8_ENC_OK) {
    ok = 0;
    goto Error;
  }

 Error:
  VP8LBitWriterDestroy(&bw);
  DeleteVP8LEncoder(enc);
  if (!ok) {
    // TODO(vikasa): err is not set for all error paths. Set default err.
    if (err == VP8_ENC_OK) err = VP8_ENC_ERROR_BAD_WRITE;
    WebPEncodingSetError(picture, err);
  }
  return ok;
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
