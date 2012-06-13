// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Internal header for constants related to WebP file format.
//
// Author: Urvang (urvang@google.com)

#ifndef WEBP_WEBP_FORMAT_CONSTANTS_H_
#define WEBP_WEBP_FORMAT_CONSTANTS_H_

// VP8 related constants.
#define VP8_SIGNATURE 0x9d012a              // Signature in VP8 data.
#define VP8_MAX_PARTITION0_SIZE (1 << 19)   // max size of mode partition
#define VP8_MAX_PARTITION_SIZE  (1 << 24)   // max size for token partition
#define VP8_FRAME_HEADER_SIZE 10  // Size of the frame header within VP8 data.

// VP8L related constants.
#define VP8L_SIGNATURE_SIZE          1      // VP8L signature size.
#define VP8L_MAGIC_BYTE              0x2f   // VP8L signature byte.
#define VP8L_IMAGE_SIZE_BITS         14     // Number of bits used to store
                                            // width and height.
#define VP8L_VERSION_BITS            3      // 3 bits reserved for version.
#define VP8L_VERSION                 0      // version 0
#define VP8L_FRAME_HEADER_SIZE       5      // Size of the VP8L frame header.

#define MAX_PALETTE_SIZE             256
#define MAX_CACHE_BITS               11
#define HUFFMAN_CODES_PER_META_CODE  5
#define ARGB_BLACK                   0xff000000

#define DEFAULT_CODE_LENGTH          8
#define MAX_ALLOWED_CODE_LENGTH      15

#define NUM_LITERAL_CODES            256
#define NUM_LENGTH_CODES             24
#define NUM_DISTANCE_CODES           40
#define CODE_LENGTH_CODES            19

#define TRANSFORM_PRESENT            1  // The bit to be written when next data
                                        // to be read is a transform.
#define NUM_TRANSFORMS               4  // Maximum number of allowed transform
                                        // in a bitstream.
typedef enum {
  PREDICTOR_TRANSFORM      = 0,
  CROSS_COLOR_TRANSFORM    = 1,
  SUBTRACT_GREEN           = 2,
  COLOR_INDEXING_TRANSFORM = 3
} VP8LImageTransformType;

// Alpha related constants.
#define ALPHA_HEADER_LEN            1
#define ALPHA_NO_COMPRESSION        0
#define ALPHA_LOSSLESS_COMPRESSION  1
#define ALPHA_PREPROCESSED_LEVELS   1

// Mux related constants.
#define TAG_SIZE           4     // Size of a chunk tag (e.g. "VP8L").
#define CHUNK_SIZE_BYTES   4     // Size needed to store chunk's size.
#define CHUNK_HEADER_SIZE  8     // Size of a chunk header.
#define RIFF_HEADER_SIZE   12    // Size of the RIFF header ("RIFFnnnnWEBP").
#define FRAME_CHUNK_SIZE   20    // Size of a FRM chunk.
#define LOOP_CHUNK_SIZE    4     // Size of a LOOP chunk.
#define TILE_CHUNK_SIZE    8     // Size of a LOOP chunk.
#define VP8X_CHUNK_SIZE    12    // Size of a VP8X chunk.
#define ALPHA_FLAG_BIT     0x10  // Should be same as the ALPHA_FLAG in mux.h

// Maximum chunk payload is such that adding the header and padding won't
// overflow a uint32_t.
#define MAX_CHUNK_PAYLOAD (~0U - CHUNK_HEADER_SIZE - 1)

#endif  /* WEBP_WEBP_FORMAT_CONSTANTS_H_ */
