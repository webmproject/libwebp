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

typedef struct {
  const WebPConfig* config_;    // user configuration and parameters
  WebPPicture* pic_;            // input / output picture

  // Encoding parameters derived from quality parameter.
  int use_lz77_;
  int palette_bits_;
  int histo_bits_;
  int transform_bits_;

  // Encoding parameters derived from image characteristics.
  int predicted_bits_;
  int non_predicted_bits_;
  int use_palette_;
  int num_palette_colors;
  int use_predict_;
  int use_cross_color_;
  int use_color_cache;
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

#endif  /* WEBP_ENC_VP8LI_H_ */
