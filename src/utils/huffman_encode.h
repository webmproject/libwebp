// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: jyrki@google.com (Jyrki Alakuijala)
//
// Flate like entropy encoding (Huffman) for webp lossless

#ifndef WEBP_UTILS_HUFFMAN_ENCODE_H_
#define WEBP_UTILS_HUFFMAN_ENCODE_H_

#ifdef USE_LOSSLESS_ENCODER

#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// This function will create a Huffman tree.
//
// 'histogram' contains the population counts.
// 'tree_depth_limit' is the maximum bit depth of the Huffman codes.
// The created tree is returned as 'bit_depths', which stores how many bits are
// used for each symbol.
// See http://en.wikipedia.org/wiki/Huffman_coding
// Returns 0 on memory error.
int VP8LCreateHuffmanTree(const int* const histogram, int histogram_size,
                          int tree_depth_limit, uint8_t* const bit_depths);

// Write a huffman tree from bit depths. The generated Huffman tree is
// compressed once more using a Huffman tree.
void VP8LCreateCompressedHuffmanTree(const uint8_t* const depth, int len,
                                     int* num_symbols,
                                     uint8_t* tree,
                                     uint8_t* extra_bits_data);

// Get the actual bit values for a tree of bit depths.
void VP8LConvertBitDepthsToSymbols(const uint8_t* depth, int len,
                                   uint16_t* bits);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#endif  // WEBP_UTILS_HUFFMAN_ENCODE_H_
