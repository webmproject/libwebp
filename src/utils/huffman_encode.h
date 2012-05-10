// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: jyrki@google.com (Jyrki Alakuijala)
//
// Flate-like entropy encoding (Huffman) for webp lossless

#ifndef WEBP_UTILS_HUFFMAN_ENCODE_H_
#define WEBP_UTILS_HUFFMAN_ENCODE_H_

#ifdef USE_LOSSLESS_ENCODER

#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// Create a Huffman tree.
//
// (data,length): population counts.
// tree_limit: maximum bit depth (inclusive) of the codes.
// bit_depths[]: how many bits are used for the symbol.
//
// Returns 0 when an error has occured.
int VP8LCreateHuffmanTree(const int* data, const int length,
                          const int tree_limit, uint8_t* bit_depths);

// Turn the Huffman tree into a token sequence.
// Returns the number of tokens used.
typedef struct {
  uint8_t code;         // value (0..15) or escape code (16,17,18)
  uint8_t extra_bits;   // extra bits for escape codes
} HuffmanTreeToken;

int VP8LCreateCompressedHuffmanTree(const uint8_t* const depth, int len,
                                    HuffmanTreeToken* tokens, int max_tokens);

// Get the actual bit values for a tree of bit depths.
void VP8LConvertBitDepthsToSymbols(const uint8_t* const depth, int len,
                                   uint16_t* const bits);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#endif  // WEBP_UTILS_HUFFMAN_ENCODE_H_
