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
#include "./vp8enci.h"
#include "./vp8li.h"
#include "../utils/bit_writer.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static const uint32_t kImageSizeBits = 14;

static int VP8LEncAnalyze(VP8LEncoder* const enc) {
  (void)enc;
  return 1;
}

static int EncodeImageInternal(VP8LEncoder* const enc) {
  (void)enc;
  return 1;
}

static int CreatePalette(VP8LEncoder* const enc) {
  (void)enc;
  return 1;
}

static void EvalSubtractGreen(VP8LEncoder* const enc) {
  (void)enc;
}

static int ApplyPredictFilter(VP8LEncoder* const enc) {
  (void)enc;
  return 1;
}

static int ApplyCrossColorFilter(VP8LEncoder* const enc) {
  (void)enc;
  return 1;
}

static void EvalColorCache(VP8LEncoder* const enc) {
  (void)enc;
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
  enc->pic_ = picture;
  enc->use_lz77_ = 1;
  enc->palette_bits_ = 7;

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

int VP8LEncodeImage(const WebPConfig* const config,
                    WebPPicture* const picture) {
  int ok = 0;
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

  VP8LBitWriterInit(&bw, (picture->width * picture->height) >> 1);

  // ---------------------------------------------------------------------------
  // Analyze image (entropy, num_palettes etc)

  if (!VP8LEncAnalyze(enc)) goto Error;

  if (enc->use_palette_) {
    CreatePalette(enc);
  }

  // Write image size.
  WriteImageSize(enc, &bw);

  // ---------------------------------------------------------------------------
  // Apply transforms and write transform data.

  EvalSubtractGreen(enc);

  if (enc->use_predict_) {
    if (!ApplyPredictFilter(enc)) goto Error;
  }

  if (enc->use_cross_color_) {
    if (!ApplyCrossColorFilter(enc)) goto Error;
  }

  if (enc->use_color_cache) {
    EvalColorCache(enc);
  }

  // ---------------------------------------------------------------------------
  // Encode and write the transformed image.

  ok = EncodeImageInternal(enc);
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
