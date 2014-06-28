// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Endian related functions.

#ifndef WEBP_UTILS_ENDIAN_INL_H_
#define WEBP_UTILS_ENDIAN_INL_H_

#ifdef HAVE_CONFIG_H
#include "../webp/config.h"
#endif

#include "../webp/types.h"

// some endian fix (e.g.: mips-gcc doesn't define __BIG_ENDIAN__)
#if !defined(WORDS_BIGENDIAN) && \
    (defined(__BIG_ENDIAN__) || \
     (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)))
#define WORDS_BIGENDIAN
#endif

//  endian-specific htoleXX() definition
// TODO(skal): add a test for htoleXX() in endian.h and others in autoconf or
// remove it and replace with a generic implementation.
#if defined(_WIN32)
#if !defined(_M_PPC)
#define htole32(x) (x)
#define htole16(x) (x)
#else     // PPC is BIG_ENDIAN
#include <stdlib.h>
#define htole32(x) (_byteswap_ulong((unsigned long)(x)))
#define htole16(x) (_byteswap_ushort((unsigned short)(x)))
#endif    // _M_PPC
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || \
      defined(__DragonFly__)
#include <sys/endian.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htole32 OSSwapHostToLittleInt32
#define htole16 OSSwapHostToLittleInt16
#elif defined(__native_client__) && !defined(__GLIBC__)
// NaCl without glibc is assumed to be little-endian
#define htole32(x) (x)
#define htole16(x) (x)
#elif defined(__QNX__)
#include <net/netbyte.h>
#else     // pretty much all linux and/or glibc
#include <endian.h>
#endif

// gcc 4.3 has builtin functions for swap32/swap64
// TODO(jzern): this should have a corresponding autoconf check.
#if defined(__GNUC__) && \
           (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define HAVE_BUILTIN_BSWAP
#endif

static WEBP_INLINE uint32_t BSwap32(uint32_t x) {
#if defined(HAVE_BUILTIN_BSWAP)
  return __builtin_bswap32(x);
#elif defined(__i386__) || defined(__x86_64__)
  uint32_t swapped_bytes;
  __asm__ volatile("bswap %0" : "=r"(swapped_bytes) : "0"(x));
  return swapped_bytes;
#elif defined(_MSC_VER)
  return (uint32_t)_byteswap_ulong(x);
#else
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
#endif  // HAVE_BUILTIN_BSWAP
}

static WEBP_INLINE uint64_t BSwap64(uint64_t x) {
#if defined(HAVE_BUILTIN_BSWAP)
  return __builtin_bswap64(x);
#elif defined(__x86_64__)
  uint64_t swapped_bytes;
  __asm__ volatile("bswapq %0" : "=r"(swapped_bytes) : "0"(x));
  return swapped_bytes;
#elif defined(_MSC_VER)
  return (uint64_t)_byteswap_uint64(x);
#else  // generic code for swapping 64-bit values (suggested by bdb@)
  x = ((x & 0xffffffff00000000ull) >> 32) | ((x & 0x00000000ffffffffull) << 32);
  x = ((x & 0xffff0000ffff0000ull) >> 16) | ((x & 0x0000ffff0000ffffull) << 16);
  x = ((x & 0xff00ff00ff00ff00ull) >>  8) | ((x & 0x00ff00ff00ff00ffull) <<  8);
  return x;
#endif  // HAVE_BUILTIN_BSWAP
}

#endif  // WEBP_UTILS_ENDIAN_INL_H_
