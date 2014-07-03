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

// some endian fix (e.g.: mips-gcc doesn't define __BIG_ENDIAN__)
#if !defined(__BIG_ENDIAN__) && defined(__BYTE_ORDER__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define __BIG_ENDIAN__
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

#endif  // WEBP_UTILS_ENDIAN_INL_H_
