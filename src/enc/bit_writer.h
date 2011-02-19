// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Bit writing and boolean coder
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_ENC_BIT_WRITER_H_
#define WEBP_ENC_BIT_WRITER_H_

#include "vp8enci.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Bit-writing

typedef struct VP8BitWriter VP8BitWriter;
struct VP8BitWriter {
  int32_t  range_;      // range-1
  int32_t  value_;
  int      run_;        // number of outstanding bits
  int      nb_bits_;    // number of pending bits
  uint8_t* buf_;
  size_t   pos_;
  size_t   max_pos_;
  int      error_;      // true in case of error
};

int VP8BitWriterInit(VP8BitWriter* const bw, size_t expected_size);
uint8_t* VP8BitWriterFinish(VP8BitWriter* const bw);
int VP8PutBit(VP8BitWriter* const bw, int bit, int prob);
int VP8PutBitUniform(VP8BitWriter* const bw, int bit);
void VP8PutValue(VP8BitWriter* const bw, int value, int nb_bits);
void VP8PutSignedValue(VP8BitWriter* const bw, int value, int nb_bits);

// return approximate write position (in bits)
static inline uint64_t VP8BitWriterPos(const VP8BitWriter* const bw) {
  return (uint64_t)(bw->pos_ + bw->run_) * 8 + 8 + bw->nb_bits_;
}

static inline uint8_t* VP8BitWriterBuf(const VP8BitWriter* const bw) {
  return bw->buf_;
}
static inline size_t VP8BitWriterSize(const VP8BitWriter* const bw) {
  return bw->pos_;
}

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_ENC_BIT_WRITER_H_
