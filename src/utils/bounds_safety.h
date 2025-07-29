// Copyright 2025 Google LLC
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Adds compatibility / portability macros to support usage of -fbounds-safety

#ifndef WEBP_UTILS_BOUNDS_SAFETY_H_
#define WEBP_UTILS_BOUNDS_SAFETY_H_

#include <string.h>  // For memcpy and friends

// There's some inherent complexity here due to the way -fbounds-safety works.
// Some annotations (notably __indexable and __bidi_indexable) change the ABI
// of the function or struct, so we don't want those annotations to silently
// disappear if they're expected.
//
// In ptrcheck.h provided by the compiler, ABI changing annotations do not
// "vanish" under any build configuration. This is intentional. Consider the
// following example:
//
// == Safe.h, where Safe.c is always compiled with -fbounds-safety ==
// Forward declare my_function, implemented in Safe.c
// void my_function(char *__bidi_indexable ptr);
//
// If we have a project that does not use -fbounds-safety, and we want to call
// my_function that was pre-built with -fbounds-safety, this annotation cannot
// vanish or there'll be an ABI mismatch, which may fail to compile or have
// worse behaviors at runtime.
//
// TODO: https://issues.webmproject.org/432511225 - In the future, we should
// have CMake append to a header file (like this one) that libwebp was built
// with -fbounds-safety, so that we know to never make annotations vanish.

#ifdef WEBP_SUPPORT_FBOUNDS_SAFETY

#include <ptrcheck.h>

#define WEBP_ASSUME_UNSAFE_INDEXABLE_ABI \
  __ptrcheck_abi_assume_unsafe_indexable()

#define WEBP_COUNTED_BY(x) __counted_by(x)
#define WEBP_COUNTED_BY_OR_NULL(x) __counted_by_or_null(x)
#define WEBP_SIZED_BY(x) __sized_by(x)
#define WEBP_SIZED_BY_OR_NULL(x) __sized_by_or_null(x)
#define WEBP_ENDED_BY(x) __ended_by(x)

#define WEBP_UNSAFE_INDEXABLE __unsafe_indexable
#define WEBP_SINGLE __single

// The annotations below are ABI breaking as they turn normal pointers into
// "wide" pointers. Breaking them down:
// * __indexable is akin to { ptr_curr, ptr_end }, and can only be
//   forward-indexed.
// * __bidi_indexable (bidirectional indexable) is
//   { ptr_begin, ptr_curr, ptr_end }
//   and can be both forward and backward indexed.
// See https://clang.llvm.org/docs/BoundsSafety.html for more comprehensive
// documentation
#define WEBP_INDEXABLE __indexable
#define WEBP_BIDI_INDEXABLE __bidi_indexable

#define WEBP_UNSAFE_FORGE_SINGLE(typ, ptr) __unsafe_forge_single(typ, ptr)

#define WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(typ, ptr, size) \
  __unsafe_forge_bidi_indexable(typ, ptr, size)

// Provide memcpy/memset/memmove wrappers to make migration easier.
#define WEBP_UNSAFE_MEMCPY(dst, src, size)                               \
  do {                                                                   \
    memcpy(WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, dst, size),        \
           WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, src, size), size); \
  } while (0)

#define WEBP_UNSAFE_MEMSET(dst, c, size)                                    \
  do {                                                                      \
    memset(WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, dst, size), c, size); \
  } while (0)

#define WEBP_UNSAFE_MEMMOVE(dst, src, size)                               \
  do {                                                                    \
    memmove(WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, dst, size),        \
            WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, src, size), size); \
  } while (0)

#else  // WEBP_SUPPORT_FBOUNDS_SAFETY

#define WEBP_ASSUME_UNSAFE_INDEXABLE_ABI

#define WEBP_COUNTED_BY(x)
#define WEBP_COUNTED_BY_OR_NULL(x)
#define WEBP_SIZED_BY(x)
#define WEBP_SIZED_BY_OR_NULL(x)
#define WEBP_ENDED_BY(x)

#define WEBP_UNSAFE_INDEXABLE
#define WEBP_SINGLE
#define WEBP_INDEXABLE
#define WEBP_BIDI_INDEXABLE

#define WEBP_UNSAFE_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define WEBP_UNSAFE_MEMSET(dst, c, size) memset(dst, c, size)
#define WEBP_UNSAFE_MEMMOVE(dst, src, size) memmove(dst, src, size)

#define WEBP_UNSAFE_FORGE_SINGLE(typ, ptr) ((typ)(ptr))
#define WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(typ, ptr, size) ((typ)(ptr))

#endif  // WEBP_SUPPORT_FBOUNDS_SAFETY
#endif  // WEBP_UTILS_BOUNDS_SAFETY_H_
