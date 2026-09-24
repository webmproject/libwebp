// Copyright 2013 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Alpha decoder: internal header.
//
// Author: Urvang (urvang@google.com)

#ifndef WEBP_DEC_ALPHAI_DEC_H_
#define WEBP_DEC_ALPHAI_DEC_H_

#include "src/dec/vp8_dec.h"
#include "src/dec/webpi_dec.h"
#include "src/dsp/dsp.h"
#include "src/utils/filters_utils.h"
#include "src/webp/types.h"

WEBP_ASSUME_UNSAFE_INDEXABLE_ABI

#ifdef __cplusplus
extern "C" {
#endif

struct VP8LDecoder;  // Defined in dec/vp8li.h.

typedef struct ALPHDecoder ALPHDecoder;
struct ALPHDecoder {
  int width;
  int height;
  int method;
  WEBP_FILTER_TYPE filter;
  int pre_processing;
  struct VP8LDecoder* vp8l_dec;
  VP8Io io;
  int use_8b_decode;  // Although alpha channel requires only 1 byte per
                      // pixel, sometimes VP8LDecoder may need to allocate
                      // 4 bytes per pixel internally during decode.
  uint8_t* output;
  const uint8_t* prev_line;  // last output row (or NULL)
  int num_output_rows;       // number of rows allocated in output
  int output_start_row;      // image row index corresponding to output[0]
  int min_needed_row;        // earliest row still needed by caller
};

//------------------------------------------------------------------------------
// internal functions. Not public.

// Deallocate memory associated to dec->alpha_plane decoding
void WebPDeallocateAlphaMemory(VP8Decoder* const dec);

// Returns the number of alpha rows to allocate in dec->alpha_plane (either a
// sliding window or io->crop_bottom rows if alpha dithering is active).
int WebPGetAlphaWindowRows(const VP8Decoder* const dec, const VP8Io* const io);

// Shifts the sliding output window forward if needed to fit rows up to
// last_row.
void WebPShiftAlphaWindow(ALPHDecoder* const alph_dec, int current_end_row,
                          int last_row);

// Returns the pixel offset of image row 'row' relative to the start of the
// alpha sliding window buffer ('alph_dec->output').
static WEBP_INLINE ptrdiff_t
GetAlphaWindowRowOffset(const ALPHDecoder* const alph_dec, int row) {
  return (ptrdiff_t)(row - alph_dec->output_start_row) * alph_dec->width;
}

//------------------------------------------------------------------------------

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBP_DEC_ALPHAI_DEC_H_
