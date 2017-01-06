// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Boolean decoder non-inlined methods
//
// Author: Skal (pascal.massimino@gmail.com)

#ifdef HAVE_CONFIG_H
#include "../webp/config.h"
#endif

#include "./bit_reader_inl.h"
#include "../utils/utils.h"

//------------------------------------------------------------------------------
// VP8BitReader

void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start,
                           size_t size) {
  br->buf_     = start;
  br->buf_end_ = start + size;
  br->buf_max_ =
      (size >= sizeof(lbit_t)) ? start + size - sizeof(lbit_t) + 1
                               : start;
}

void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size) {
  assert(br != NULL);
  assert(start != NULL);
  assert(size < (1u << 31));   // limit ensured by format and upstream checks
  br->range_   = 255 - 1;
  br->value_   = 0;
  br->bits_    = -8;   // to load the very first 8bits
  br->eof_     = 0;
  VP8BitReaderSetBuffer(br, start, size);
  VP8LoadNewBytes(br);
}

void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset) {
  if (br->buf_ != NULL) {
    br->buf_ += offset;
    br->buf_end_ += offset;
    br->buf_max_ += offset;
  }
}

void VP8LoadFinalBytes(VP8BitReader* const br) {
  assert(br != NULL && br->buf_ != NULL);
  // Only read 8bits at a time
  if (br->buf_ < br->buf_end_) {
    br->bits_ += 8;
    br->value_ = (bit_t)(*br->buf_++) | (br->value_ << 8);
  } else if (!br->eof_) {
    br->value_ <<= 8;
    br->bits_ += 8;
    br->eof_ = 1;
  } else {
    br->bits_ = 0;  // This is to avoid undefined behaviour with shifts.
  }
}

//------------------------------------------------------------------------------
// Higher-level calls

uint32_t VP8GetValue(VP8BitReader* const br, int bits) {
  uint32_t v = 0;
  while (bits-- > 0) {
    v |= VP8GetBit(br, 0x80) << bits;
  }
  return v;
}

int32_t VP8GetSignedValue(VP8BitReader* const br, int bits) {
  const int value = VP8GetValue(br, bits);
  return VP8Get(br) ? -value : value;
}

//------------------------------------------------------------------------------
// VP8LBitReader

#define VP8L_LOG8_WBITS 4  // Number of bytes needed to store VP8L_WBITS bits.

#if defined(__arm__) || defined(_M_ARM) || defined(__aarch64__) || \
    defined(__i386__) || defined(_M_IX86) || \
    defined(__x86_64__) || defined(_M_X64)
#define VP8L_USE_FAST_LOAD
#endif

static const uint32_t kBitMask[VP8L_MAX_NUM_BIT_READ + 1] = {
  0,
  0x000001, 0x000003, 0x000007, 0x00000f,
  0x00001f, 0x00003f, 0x00007f, 0x0000ff,
  0x0001ff, 0x0003ff, 0x0007ff, 0x000fff,
  0x001fff, 0x003fff, 0x007fff, 0x00ffff,
  0x01ffff, 0x03ffff, 0x07ffff, 0x0fffff,
  0x1fffff, 0x3fffff, 0x7fffff, 0xffffff
};

void VP8LInitBitReader(VP8LBitReader* const br, const uint8_t* const start,
                       size_t length) {
  size_t i;
  vp8l_val_t value = 0;
  assert(br != NULL);
  assert(start != NULL);
  assert(length < 0xfffffff8u);   // can't happen with a RIFF chunk.

  br->len_ = length;
  br->val_ = 0;
  br->bit_pos_ = 0;
  br->eos_ = 0;

  if (length > sizeof(br->val_)) {
    length = sizeof(br->val_);
  }
  for (i = 0; i < length; ++i) {
    value |= (vp8l_val_t)start[i] << (8 * i);
  }
  br->val_ = value;
  br->pos_ = length;
  br->buf_ = start;
}

void VP8LBitReaderSetBuffer(VP8LBitReader* const br,
                            const uint8_t* const buf, size_t len) {
  assert(br != NULL);
  assert(buf != NULL);
  assert(len < 0xfffffff8u);   // can't happen with a RIFF chunk.
  br->buf_ = buf;
  br->len_ = len;
  // pos_ > len_ should be considered a param error.
  br->eos_ = (br->pos_ > br->len_) || VP8LIsEndOfStream(br);
}

static void VP8LSetEndOfStream(VP8LBitReader* const br) {
  br->eos_ = 1;
  br->bit_pos_ = 0;  // To avoid undefined behaviour with shifts.
}

// If not at EOS, reload up to VP8L_LBITS byte-by-byte
static void ShiftBytes(VP8LBitReader* const br) {
  while (br->bit_pos_ >= 8 && br->pos_ < br->len_) {
    br->val_ >>= 8;
    br->val_ |= ((vp8l_val_t)br->buf_[br->pos_]) << (VP8L_LBITS - 8);
    ++br->pos_;
    br->bit_pos_ -= 8;
  }
  if (VP8LIsEndOfStream(br)) {
    VP8LSetEndOfStream(br);
  }
}

void VP8LDoFillBitWindow(VP8LBitReader* const br) {
  assert(br->bit_pos_ >= VP8L_WBITS);
#if defined(VP8L_USE_FAST_LOAD)
  if (br->pos_ + sizeof(br->val_) < br->len_) {
    br->val_ >>= VP8L_WBITS;
    br->bit_pos_ -= VP8L_WBITS;
    br->val_ |= (vp8l_val_t)HToLE32(WebPMemToUint32(br->buf_ + br->pos_)) <<
                (VP8L_LBITS - VP8L_WBITS);
    br->pos_ += VP8L_LOG8_WBITS;
    return;
  }
#endif
  ShiftBytes(br);       // Slow path.
}

uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits) {
  assert(n_bits >= 0);
  // Flag an error if end_of_stream or n_bits is more than allowed limit.
  if (!br->eos_ && n_bits <= VP8L_MAX_NUM_BIT_READ) {
    const uint32_t val = VP8LPrefetchBits(br) & kBitMask[n_bits];
    const int new_bits = br->bit_pos_ + n_bits;
    br->bit_pos_ = new_bits;
    ShiftBytes(br);
    return val;
  } else {
    VP8LSetEndOfStream(br);
    return 0;
  }
}

//------------------------------------------------------------------------------
