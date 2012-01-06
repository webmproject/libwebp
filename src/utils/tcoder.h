// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Tree-coder using VP8's boolean coder
//
// Symbols are stored as nodes of a tree that records their frequencies and
// is dynamically updated.
//
// Author: Skal (pascal.massimino@gmail.com)
//
// Encoding example:
/*
static int Compress(const uint8_t* src, int src_length,
                    uint8_t** output, size_t* output_size) {
  int i;
  TCoder* coder = TCoderNew(255);
  VP8BitWriter bw;

  VP8BitWriterInit(&bw, 0);
  for (i = 0; i < src_length; ++i)
    TCoderEncode(coder, src[i], &bw);
  TCoderDelete(coder);
  VP8BitWriterFinish(&bw);

  *output = VP8BitWriterBuf(&bw);
  *output_size = VP8BitWriterSize(&bw);
  return !bw.error_;
}
*/
//
// Decoding example:
/*
static int Decompress(const uint8_t* src, size_t src_size,
                      uint8_t* dst, int dst_length) {
  int i;
  TCoder* coder = TCoderNew(255);
  VP8BitReader br;

  VP8InitBitReader(&br, src, src + src_size);
  for (i = 0; i < dst_length; ++i)
    dst[i] = TCoderDecode(coder, &br);
  TCoderDelete(coder);
  return !br.eof_;
}
*/

#ifndef WEBP_UTILS_TCODER_H_
#define WEBP_UTILS_TCODER_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

struct VP8BitReader;
struct VP8BitWriter;
typedef struct TCoder TCoder;

// Creates a tree-coder capable of coding symbols in
// the [0, max_symbol] range. Returns NULL in case of memory error.
// 'max_symbol' must be in the range [0, TCODER_MAX_SYMBOL)
#define TCODER_MAX_SYMBOL (1 << 24)
TCoder* TCoderNew(int max_symbol);
// Re-initialize an existing object, make it ready for a new encoding or
// decoding cycle.
void TCoderInit(TCoder* const c);
// destroys the tree-coder object and frees memory.
void TCoderDelete(TCoder* const c);

// Code next symbol 's'. If the bit-writer 'bw' is NULL, the function will
// just record the symbol, and update the internal frequency counters.
void TCoderEncode(TCoder* const c, int s, struct VP8BitWriter* const bw);
// Decode and return next symbol.
int TCoderDecode(TCoder* const c, struct VP8BitReader* const br);

// Theoretical number of bits needed to code 'symbol' in the current state.
double TCoderSymbolCost(const TCoder* const c, int symbol);

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_UTILS_TCODER_H_
