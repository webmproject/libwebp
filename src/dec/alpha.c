// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Alpha-plane decompression.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include "./vp8i.h"
#include "../utils/alpha.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------

const uint8_t* VP8DecompressAlphaRows(VP8Decoder* const dec,
                                      int row, int num_rows) {
  const int stride = dec->pic_hdr_.width_;

  if (row < 0 || num_rows < 0 || row + num_rows > dec->pic_hdr_.height_) {
    return NULL;    // sanity check.
  }

  if (row == 0) {
    // Decode everything during the first call.
    if (!DecodeAlpha(dec->alpha_data_, (size_t)dec->alpha_data_size_,
                     dec->pic_hdr_.width_, dec->pic_hdr_.height_, stride,
                     dec->alpha_plane_)) {
      return NULL;  // Error.
    }
  }

  // Return a pointer to the current decoded row.
  return dec->alpha_plane_ + row * stride;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
