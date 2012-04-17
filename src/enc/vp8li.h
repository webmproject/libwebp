// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Lossless encoder: internal header.
//
// Author: Vikas Arora(vikaas.arora@gmail.com)

#ifndef WEBP_ENC_VP8LI_H_
#define WEBP_ENC_VP8LI_H_

#ifdef USE_LOSSLESS_ENCODER

#include "./histogram.h"
#include "../webp/encode.h"
#include "../utils/bit_writer.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// TODO(vikasa): factorize these with ones used in lossless decoder.
#define TAG_SIZE             4
#define CHUNK_HEADER_SIZE    8
#define RIFF_HEADER_SIZE     12
#define HEADER_SIZE          (RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE)
#define SIGNATURE_SIZE       1
#define LOSSLESS_MAGIC_BYTE  0x64

#define MAX_PALETTE_SIZE         256
#define PALETTE_KEY_RIGHT_SHIFT   22  // Key for 1K buffer.

typedef struct {
  const WebPConfig* config_;    // user configuration and parameters
  WebPPicture* pic_;            // input picture.

  uint32_t* argb_;              // Transformed argb image data.
  uint32_t* argb_scratch_;      // Scratch memory for one argb tile
                                // (used for prediction).
  uint32_t* transform_data_;    // Scratch memory for transform data.
  int       current_width_;     // Corresponds to packed image width.

  // Encoding parameters derived from quality parameter.
  int use_lz77_;
  int palette_bits_;
  int histo_bits_;
  int transform_bits_;

  // Encoding parameters derived from image characteristics.
  int use_cross_color_;
  int use_predict_;
  int use_palette_;
  int palette_size_;
  uint32_t palette_[MAX_PALETTE_SIZE];
} VP8LEncoder;

//------------------------------------------------------------------------------
// internal functions. Not public.

// in vp8l.c

// Encodes the picture.
// Returns 0 if config or picture is NULL or picture doesn't have valid argb
// input.
int VP8LEncodeImage(const WebPConfig* const config,
                    WebPPicture* const picture);

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif

#endif  /* WEBP_ENC_VP8LI_H_ */
