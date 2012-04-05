// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Lossless decoder: internal header.
//
// Author: Skal (pascal.massimino@gmail.com)
//         Vikas Arora(vikaas.arora@gmail.com)

#ifndef WEBP_DEC_VP8LI_H_
#define WEBP_DEC_VP8LI_H_

#include <string.h>     // for memcpy()
#include "./webpi.h"
#include "../utils/bit_reader.h"
#include "../utils/color_cache.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define NUM_TRANSFORMS               8
#define HUFFMAN_CODES_PER_META_CODE  5
#define ARGB_BLACK                   0xff000000
#define NUM_LITERAL_CODES            256
#define NUM_ARGB_CACHE_ROWS          16
#define LOSSLESS_MAGIC_BYTE          0x64
#define LOSSLESS_MAGIC_BYTE_RSVD     0x65

struct HuffmanTree;

typedef enum {
  READ_DATA = 0,
  READ_HDR = 1,
  READ_DIM = 2
} VP8LDecodeState;

typedef enum {
  PREDICTOR_TRANSFORM      = 0,
  CROSS_COLOR_TRANSFORM    = 1,
  SUBTRACT_GREEN           = 2,
  COLOR_INDEXING_TRANSFORM = 3
} VP8LImageTransformType;

typedef struct VP8LTransform VP8LTransform;
struct VP8LTransform {
  VP8LImageTransformType type_;   // transform type.
  int                    bits_;   // subsampling bits defining transform window.
  size_t                 xsize_;  // transform window X index.
  size_t                 ysize_;  // transform window Y index.
  uint32_t              *data_;   // transform data.
};

typedef struct {
  int             color_cache_size_;
  VP8LColorCache *color_cache_;

  int             num_huffman_trees_;
  int             huffman_mask_;
  int             huffman_subsample_bits_;
  int             huffman_xsize_;
  uint32_t       *meta_codes_;
  uint32_t       *huffman_image_;
  struct HuffmanTree *htrees_;
  struct HuffmanTree *meta_htrees_[HUFFMAN_CODES_PER_META_CODE];
} VP8LMetadata;

typedef struct {
  VP8StatusCode    status_;
  VP8LDecodeState  action_;
  VP8LDecodeState  state_;
  VP8Io           *io_;

  uint32_t        *argb_;          // Internal data: always in BGRA color mode.
  uint32_t        *argb_cache_;    // Scratch buffer for temporary BGRA storage.

  BitReader        br_;

  int              width_;
  int              height_;
  int              last_row_;      // last input row decoded so far.
  int              last_out_row_;  // last row output so far.

  VP8LMetadata     hdr_;

  int              next_transform_;
  VP8LTransform    transforms_[NUM_TRANSFORMS];

  uint8_t         *rescaler_memory;  // Working memory for rescaling work.
  WebPRescaler    *rescaler;         // Common rescaler for all channels.
} VP8LDecoder;

//------------------------------------------------------------------------------
// internal functions. Not public.

// in vp8l.c

// Validates the VP8L data-header and retrieves basic header information viz
// width and height. Returns 0 in case of formatting error. width/height
// can be passed NULL.
int VP8LGetInfo(const uint8_t* data,
                int data_size,    // data available so far
                int *width, int *height);

// Allocates and initialize a new lossless decoder instance.
VP8LDecoder* VP8LNew(void);

// Decodes the image header. Returns false in case of error.
int VP8LDecodeHeader(VP8LDecoder* const dec, VP8Io* const io);

// Decodes an image. It's required to decode the lossless header before calling
// this function. Returns false in case of error, with updated dec->status_.
int VP8LDecodeImage(VP8LDecoder* const dec);

// Resets the decoder in its initial state, reclaiming memory.
// Preserves the dec->status_ value.
void VP8LClear(VP8LDecoder* const dec);

// Clears and deallocate a lossless decoder instance.
void VP8LDelete(VP8LDecoder* const dec);

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  /* WEBP_DEC_VP8LI_H_ */
