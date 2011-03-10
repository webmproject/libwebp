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

#include <assert.h>
#include <stdlib.h>
#include "vp8enci.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// VP8BitWriter

static int BitWriterResize(VP8BitWriter* const bw, size_t extra_size) {
  uint8_t* new_buf;
  size_t new_size;
  const size_t needed_size = bw->pos_ + extra_size;
  if (needed_size <= bw->max_pos_) return 1;
  new_size = 2 * bw->max_pos_;
  if (new_size < needed_size)
    new_size = needed_size;
  if (new_size < 1024) new_size = 1024;
  new_buf = (uint8_t*)malloc(new_size);
  if (new_buf == NULL) {
    bw->error_ = 1;
    return 0;
  }
  if (bw->pos_ > 0) memcpy(new_buf, bw->buf_, bw->pos_);
  free(bw->buf_);
  bw->buf_ = new_buf;
  bw->max_pos_ = new_size;
  return 1;
}

static void kFlush(VP8BitWriter* const bw) {
  const int s = 8 + bw->nb_bits_;
  const int32_t bits = bw->value_ >> s;
  assert(bw->nb_bits_ >= 0);
  bw->value_ -= bits << s;
  bw->nb_bits_ -= 8;
  if ((bits & 0xff) != 0xff) {
    size_t pos = bw->pos_;
    if (pos + bw->run_ >= bw->max_pos_) {  // reallocate
      if (!BitWriterResize(bw,  bw->run_ + 1)) {
        return;
      }
    }
    if (bits & 0x100) {  // overflow -> propagate carry over pending 0xff's
      if (pos > 0) bw->buf_[pos - 1]++;
    }
    if (bw->run_ > 0) {
      const int value = (bits & 0x100) ? 0x00 : 0xff;
      for (; bw->run_ > 0; --bw->run_) bw->buf_[pos++] = value;
    }
    bw->buf_[pos++] = bits;
    bw->pos_ = pos;
  } else {
    bw->run_++;   // delay writing of bytes 0xff, pending eventual carry.
  }
}

//-----------------------------------------------------------------------------
// renormalization

static const uint8_t kNorm[128] = {  // renorm_sizes[i] = 8 - log2(i)
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

// range = ((range + 1) << kVP8Log2Range[range]) - 1
const uint8_t kNewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127, 143, 159, 175, 191, 207, 223, 239,
  127, 135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239,
  247, 127, 131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179,
  183, 187, 191, 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239,
  243, 247, 251, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149,
  151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179,
  181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209,
  211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

int VP8PutBit(VP8BitWriter* const bw, int bit, int prob) {
  const int split = (bw->range_ * prob) >> 8;
  if (bit) {
    bw->value_ += split + 1;
    bw->range_ -= split + 1;
  } else {
    bw->range_ = split;
  }
  if (bw->range_ < 127) {   // emit 'shift' bits out and renormalize
    const int shift = kNorm[bw->range_];
    bw->range_ = kNewRange[bw->range_];
    bw->value_ <<= shift;
    bw->nb_bits_ += shift;
    if (bw->nb_bits_ > 0) kFlush(bw);
  }
  return bit;
}

int VP8PutBitUniform(VP8BitWriter* const bw, int bit) {
  const int split = bw->range_ >> 1;
  if (bit) {
    bw->value_ += split + 1;
    bw->range_ -= split + 1;
  } else {
    bw->range_ = split;
  }
  if (bw->range_ < 127) {
    bw->range_ = kNewRange[bw->range_];
    bw->value_ <<= 1;
    bw->nb_bits_ += 1;
    if (bw->nb_bits_ > 0) kFlush(bw);
  }
  return bit;
}

void VP8PutValue(VP8BitWriter* const bw, int value, int nb_bits) {
  int mask;
  for (mask = 1 << (nb_bits - 1); mask; mask >>= 1)
    VP8PutBitUniform(bw, value & mask);
}

void VP8PutSignedValue(VP8BitWriter* const bw, int value, int nb_bits) {
  if (!VP8PutBitUniform(bw, value != 0))
    return;
  if (value < 0) {
    VP8PutValue(bw, ((-value) << 1) | 1, nb_bits + 1);
  } else {
    VP8PutValue(bw, value << 1, nb_bits + 1);
  }
}

//-----------------------------------------------------------------------------

int VP8BitWriterInit(VP8BitWriter* const bw, size_t expected_size) {
  bw->range_   = 255 - 1;
  bw->value_   = 0;
  bw->run_     = 0;
  bw->nb_bits_ = -8;
  bw->pos_     = 0;
  bw->max_pos_ = 0;
  bw->error_   = 0;
  bw->buf_     = NULL;
  return (expected_size > 0) ? BitWriterResize(bw, expected_size) : 1;
}

uint8_t* VP8BitWriterFinish(VP8BitWriter* const bw) {
  VP8PutValue(bw, 0, 9 - bw->nb_bits_);
  bw->nb_bits_ = 0;   // pad with zeroes
  kFlush(bw);
  return bw->buf_;
}

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
