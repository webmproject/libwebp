// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Specific inlined methods for boolean decoder [VP8GetBit() ...]
// This file should be included by the .c sources that actually need to call
// these methods.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_UTILS_BIT_READER_INL_H_
#define WEBP_UTILS_BIT_READER_INL_H_

#include "./bit_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Derived type lbit_t = natural type for memory I/O

#if   (BITS > 32)
typedef uint64_t lbit_t;
#elif (BITS > 16)
typedef uint32_t lbit_t;
#elif (BITS >  8)
typedef uint16_t lbit_t;
#else
typedef uint8_t lbit_t;
#endif

// some endian fix (e.g.: mips-gcc doesn't define __BIG_ENDIAN__)
#if !defined(__BIG_ENDIAN__) && defined(__BYTE_ORDER__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define __BIG_ENDIAN__
#endif

extern const uint8_t kVP8Log2Range[128];
extern const range_t kVP8NewRange[128];

// special case for the tail byte-reading
void VP8LoadFinalBytes(VP8BitReader* const br);

//------------------------------------------------------------------------------
// Inlined critical functions

// makes sure br->value_ has at least BITS bits worth of data
static WEBP_INLINE void VP8LoadNewBytes(VP8BitReader* const br) {
  assert(br != NULL && br->buf_ != NULL);
  // Read 'BITS' bits at a time if possible.
  if (br->buf_ + sizeof(lbit_t) <= br->buf_end_) {
    // convert memory type to register type (with some zero'ing!)
    bit_t bits;
#if defined(__mips__)                          // MIPS
    // This is needed because of un-aligned read.
    lbit_t in_bits;
    lbit_t* p_buf_ = (lbit_t*)br->buf_;
    __asm__ volatile(
      ".set   push                             \n\t"
      ".set   at                               \n\t"
      ".set   macro                            \n\t"
      "ulw    %[in_bits], 0(%[p_buf_])         \n\t"
      ".set   pop                              \n\t"
      : [in_bits]"=r"(in_bits)
      : [p_buf_]"r"(p_buf_)
      : "memory", "at"
    );
#else
    const lbit_t in_bits = *(const lbit_t*)br->buf_;
#endif
    br->buf_ += BITS >> 3;
#if !defined(__BIG_ENDIAN__)
#if (BITS > 32)
// gcc 4.3 has builtin functions for swap32/swap64
#if defined(__GNUC__) && \
           (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
    bits = (bit_t)__builtin_bswap64(in_bits);
#elif defined(_MSC_VER)
    bits = (bit_t)_byteswap_uint64(in_bits);
#elif defined(__x86_64__)
    __asm__ volatile("bswapq %0" : "=r"(bits) : "0"(in_bits));
#else  // generic code for swapping 64-bit values (suggested by bdb@)
    bits = (bit_t)in_bits;
    bits = ((bits & 0xffffffff00000000ull) >> 32) |
           ((bits & 0x00000000ffffffffull) << 32);
    bits = ((bits & 0xffff0000ffff0000ull) >> 16) |
           ((bits & 0x0000ffff0000ffffull) << 16);
    bits = ((bits & 0xff00ff00ff00ff00ull) >> 8) |
           ((bits & 0x00ff00ff00ff00ffull) << 8);
#endif
    bits >>= 64 - BITS;
#elif (BITS >= 24)
#if defined(__i386__) || defined(__x86_64__)
    {
      lbit_t swapped_in_bits;
      __asm__ volatile("bswap %k0" : "=r"(swapped_in_bits) : "0"(in_bits));
      bits = (bit_t)swapped_in_bits;   // 24b/32b -> 32b/64b zero-extension
    }
#elif defined(_MSC_VER)
    bits = (bit_t)_byteswap_ulong(in_bits);
#else
    bits = (bit_t)(in_bits >> 24) | ((in_bits >> 8) & 0xff00)
         | ((in_bits << 8) & 0xff0000)  | (in_bits << 24);
#endif  // x86
    bits >>= (32 - BITS);
#elif (BITS == 16)
    // gcc will recognize a 'rorw $8, ...' here:
    bits = (bit_t)(in_bits >> 8) | ((in_bits & 0xff) << 8);
#else   // BITS == 8
    bits = (bit_t)in_bits;
#endif
#else    // BIG_ENDIAN
    bits = (bit_t)in_bits;
    if (BITS != 8 * sizeof(bit_t)) bits >>= (8 * sizeof(bit_t) - BITS);
#endif
    br->value_ = bits | (br->value_ << BITS);
    br->bits_ += BITS;
  } else {
    VP8LoadFinalBytes(br);    // no need to be inlined
  }
}

// Read a bit with proba 'prob'. Speed-critical function!
static WEBP_INLINE int VP8GetBit(VP8BitReader* const br, int prob) {
  // Don't move this declaration! It makes a big speed difference to store
  // 'range' *before* calling VP8LoadNewBytes(), even if this function doesn't
  // alter br->range_ value.
  range_t range = br->range_;
  if (br->bits_ < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits_;
    const range_t split = (range * prob) >> 8;
    const range_t value = (range_t)(br->value_ >> pos);
#if defined(__arm__) || defined(_M_ARM)      // ARM-specific
    const int bit = ((int)(split - value) >> 31) & 1;
    if (value > split) {
      range -= split + 1;
      br->value_ -= (bit_t)(split + 1) << pos;
    } else {
      range = split;
    }
#else  // faster version on x86
    int bit;  // Don't use 'const int bit = (value > split);", it's slower.
    if (value > split) {
      range -= split + 1;
      br->value_ -= (bit_t)(split + 1) << pos;
      bit = 1;
    } else {
      range = split;
      bit = 0;
    }
#endif
    if (range <= (range_t)0x7e) {
      const int shift = kVP8Log2Range[range];
      range = kVP8NewRange[range];
      br->bits_ -= shift;
    }
    br->range_ = range;
    return bit;
  }
}

// simplified version of VP8GetBit() for prob=0x80 (note shift is always 1 here)
static WEBP_INLINE int VP8GetSigned(VP8BitReader* const br, int v) {
  if (br->bits_ < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits_;
    const range_t split = br->range_ >> 1;
    const range_t value = (range_t)(br->value_ >> pos);
    const int32_t mask = (int32_t)(split - value) >> 31;  // -1 or 0
    br->bits_ -= 1;
    br->range_ += mask;
    br->range_ |= 1;
    br->value_ -= (bit_t)((split + 1) & mask) << pos;
    return (v ^ mask) - mask;
  }
}

#ifdef __cplusplus
}    // extern "C"
#endif

#endif   // WEBP_UTILS_BIT_READER_INL_H_
