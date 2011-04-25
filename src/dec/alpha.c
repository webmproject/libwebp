// Copyright 2011 Google Inc.
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
#include "vp8i.h"

#ifdef WEBP_EXPERIMENTAL_FEATURES

#include "zlib.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------

const uint8_t* VP8DecompressAlphaRows(VP8Decoder* const dec,
                                      int row, int num_rows) {
  uint8_t* output = dec->alpha_plane_;
  const int stride = dec->pic_hdr_.width_;
  if (row < 0 || row + num_rows > dec->pic_hdr_.height_) {
    return NULL;    // sanity check
  }
  if (row == 0) {
    // TODO(skal): for now, we just decompress everything during the first call.
    // Later, we'll decode progressively, but we need to store the
    // z_stream state.
    const uint8_t* data = dec->alpha_data_;
    size_t data_size = dec->alpha_data_size_;
    const size_t output_size = stride * dec->pic_hdr_.height_;
    int ret = Z_OK;
    z_stream strm;

    memset(&strm, 0, sizeof(strm));
    if (inflateInit(&strm) != Z_OK) {
      return 0;
    }
    strm.avail_in = data_size;
    strm.next_in = (unsigned char*)data;
    do {
      strm.avail_out = output_size;
      strm.next_out = output;
      ret = inflate(&strm, Z_NO_FLUSH);
      if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
        break;
      }
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    if (ret != Z_STREAM_END) {
      return NULL;    // error
    }
  }
  return output + row * stride;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif    // WEBP_EXPERIMENTAL_FEATURES
