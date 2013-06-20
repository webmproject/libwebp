// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Alpha-plane decompression.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include "./alphai.h"
#include "./vp8i.h"
#include "./vp8li.h"
#include "../utils/quant_levels_dec.h"
#include "../webp/format_constants.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
//------------------------------------------------------------------------------
// ALPHDecoder object.

ALPHDecoder* ALPHNew(void) {
  ALPHDecoder* const dec = (ALPHDecoder*)calloc(1, sizeof(*dec));
  return dec;
}

void ALPHDelete(ALPHDecoder* const dec) {
  if (dec != NULL) {
    VP8LDelete(dec->vp8l_dec_);
    dec->vp8l_dec_ = NULL;
    free(dec);
  }
}

=======
>>>>>>> BRANCH (2a04b0 update ChangeLog)
//------------------------------------------------------------------------------
<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
// Decoding.
=======
// Decodes the compressed data 'data' of size 'data_size' into the 'output'.
// The 'output' buffer should be pre-allocated and must be of the same
// dimension 'height'x'width', as that of the image.
//
// Returns 1 on successfully decoding the compressed alpha and
//         0 if either:
//           error in bit-stream header (invalid compression mode or filter), or
//           error returned by appropriate compression method.
>>>>>>> BRANCH (2a04b0 update ChangeLog)

<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
// Initialize alpha decoding by parsing the alpha header and decoding the image
// header for alpha data stored using lossless compression.
// Returns false in case of error in alpha header (data too short, invalid
// compression method or filter, error in lossless header data etc).
static int ALPHInit(ALPHDecoder* const dec, const uint8_t* data,
                    size_t data_size, int width, int height, uint8_t* output) {
=======
static int DecodeAlpha(const uint8_t* data, size_t data_size,
                       int width, int height, uint8_t* output) {
  WEBP_FILTER_TYPE filter;
  int pre_processing;
  int rsrv;
>>>>>>> BRANCH (2a04b0 update ChangeLog)
  int ok = 0;
<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
  const uint8_t* const alpha_data = data + ALPHA_HEADER_LEN;
  const size_t alpha_data_size = data_size - ALPHA_HEADER_LEN;
  int rsrv;
=======
  int method;
  const uint8_t* const alpha_data = data + ALPHA_HEADER_LEN;
  const size_t alpha_data_size = data_size - ALPHA_HEADER_LEN;
>>>>>>> BRANCH (2a04b0 update ChangeLog)

  assert(width > 0 && height > 0);
  assert(data != NULL && output != NULL);

  dec->width_ = width;
  dec->height_ = height;

  if (data_size <= ALPHA_HEADER_LEN) {
    return 0;
  }

  dec->method_ = (data[0] >> 0) & 0x03;
  dec->filter_ = (data[0] >> 2) & 0x03;
  dec->pre_processing_ = (data[0] >> 4) & 0x03;
  rsrv = (data[0] >> 6) & 0x03;
  if (dec->method_ < ALPHA_NO_COMPRESSION ||
      dec->method_ > ALPHA_LOSSLESS_COMPRESSION ||
      dec->filter_ >= WEBP_FILTER_LAST ||
      dec->pre_processing_ > ALPHA_PREPROCESSED_LEVELS ||
      rsrv != 0) {
    return 0;
  }

<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
  if (dec->method_ == ALPHA_NO_COMPRESSION) {
    const size_t alpha_decoded_size = dec->width_ * dec->height_;
    ok = (alpha_data_size >= alpha_decoded_size);
=======
  if (method == ALPHA_NO_COMPRESSION) {
    const size_t alpha_decoded_size = height * width;
    ok = (alpha_data_size >= alpha_decoded_size);
    if (ok) memcpy(output, alpha_data, alpha_decoded_size);
>>>>>>> BRANCH (2a04b0 update ChangeLog)
  } else {
<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
    assert(dec->method_ == ALPHA_LOSSLESS_COMPRESSION);
    ok = VP8LDecodeAlphaHeader(dec, alpha_data, alpha_data_size, output);
  }
=======
    ok = VP8LDecodeAlphaImageStream(width, height, alpha_data, alpha_data_size,
                                    output);
  }

  if (ok) {
    WebPUnfilterFunc unfilter_func = WebPUnfilters[filter];
    if (unfilter_func != NULL) {
      // TODO(vikas): Implement on-the-fly decoding & filter mechanism to decode
      // and apply filter per image-row.
      unfilter_func(width, height, width, output);
    }
    if (pre_processing == ALPHA_PREPROCESSED_LEVELS) {
      ok = DequantizeLevels(output, width, height);
    }
  }

>>>>>>> BRANCH (2a04b0 update ChangeLog)
  return ok;
}

// Decodes, unfilters and dequantizes *at least* 'num_rows' rows of alpha
// starting from row number 'row'. It assumes that rows upto (row - 1) have
// already been decoded.
// Returns false in case of bitstream error.
static int ALPHDecode(VP8Decoder* const dec, int row, int num_rows) {
  ALPHDecoder* const alph_dec = dec->alph_dec_;
  const int width = alph_dec->width_;
  const int height = alph_dec->height_;
  WebPUnfilterFunc unfilter_func = WebPUnfilters[alph_dec->filter_];
  uint8_t* const output = dec->alpha_plane_;
  if (alph_dec->method_ == ALPHA_NO_COMPRESSION) {
    const size_t offset = row * width;
    const size_t num_pixels = num_rows * width;
    assert(dec->alpha_data_size_ >= ALPHA_HEADER_LEN + offset + num_pixels);
    memcpy(dec->alpha_plane_ + offset,
           dec->alpha_data_ + ALPHA_HEADER_LEN + offset, num_pixels);
  } else {  // alph_dec->method_ == ALPHA_LOSSLESS_COMPRESSION
    assert(alph_dec->vp8l_dec_ != NULL);
    if (!VP8LDecodeAlphaImageStream(alph_dec, row + num_rows)) {
      return 0;
    }
  }

  if (unfilter_func != NULL) {
    unfilter_func(width, height, width, row, num_rows, output);
  }

  if (alph_dec->pre_processing_ == ALPHA_PREPROCESSED_LEVELS) {
    if (!DequantizeLevels(output, width, height, row, num_rows)) {
      return 0;
    }
  }

  if (row + num_rows == dec->pic_hdr_.height_) {
    dec->is_alpha_decoded_ = 1;
  }
  return 1;
}

//------------------------------------------------------------------------------
// Main entry point.

const uint8_t* VP8DecompressAlphaRows(VP8Decoder* const dec,
                                      int row, int num_rows) {
  const int width = dec->pic_hdr_.width_;
  const int height = dec->pic_hdr_.height_;

<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
  if (row < 0 || num_rows <= 0 || row + num_rows > height) {
=======
  if (row < 0 || num_rows < 0 || row + num_rows > height) {
>>>>>>> BRANCH (2a04b0 update ChangeLog)
    return NULL;    // sanity check.
  }

  if (row == 0) {
<<<<<<< HEAD   (24cc30 ~20% faster lossless decoding)
    // Initialize decoding.
    assert(dec->alpha_plane_ != NULL);
    dec->alph_dec_ = ALPHNew();
    if (dec->alph_dec_ == NULL) return NULL;
    if (!ALPHInit(dec->alph_dec_, dec->alpha_data_, dec->alpha_data_size_,
                  width, height, dec->alpha_plane_)) {
      ALPHDelete(dec->alph_dec_);
      dec->alph_dec_ = NULL;
      return NULL;
=======
    // Decode everything during the first call.
    assert(!dec->is_alpha_decoded_);
    if (!DecodeAlpha(dec->alpha_data_, (size_t)dec->alpha_data_size_,
                     width, height, dec->alpha_plane_)) {
      return NULL;  // Error.
>>>>>>> BRANCH (2a04b0 update ChangeLog)
    }
    dec->is_alpha_decoded_ = 1;
  }

  if (!dec->is_alpha_decoded_) {
    int ok = 0;
    assert(dec->alph_dec_ != NULL);
    ok = ALPHDecode(dec, row, num_rows);
    if (!ok || dec->is_alpha_decoded_) {
      ALPHDelete(dec->alph_dec_);
      dec->alph_dec_ = NULL;
    }
    if (!ok) return NULL;  // Error.
  }

  // Return a pointer to the current decoded row.
  return dec->alpha_plane_ + row * width;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
