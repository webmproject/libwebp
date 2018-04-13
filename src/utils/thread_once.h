// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Once-only initialization.
//
// Author: James Zern (jzern@google.com)

#ifndef WEBP_UTILS_THREAD_ONCE_H_
#define WEBP_UTILS_THREAD_ONCE_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define WEBP_TSAN_IGNORE_FUNCTION

// Allow WEBP_ONCE to be overridden from the environment.
#if !defined(WEBP_ONCE)

#ifdef WEBP_USE_THREAD

#if defined(_WIN32)

#include <windows.h>

#if _WIN32_WINNT >= 0x0600

#include <assert.h>

static BOOL CALLBACK InitOnceCallback(PINIT_ONCE init_once, PVOID init_func,
                                      PVOID* context) {
  (void)init_once;
  (void)context;
  ((void (*)(void))init_func)();
  return TRUE;
}

#define WEBP_ONCE(func) do {                                                  \
  static INIT_ONCE func ## _init_once = INIT_ONCE_STATIC_INIT;                \
  const BOOL res = InitOnceExecuteOnce(&func ## _init_once, InitOnceCallback, \
                                       func, NULL /*Context*/);               \
  assert(res == TRUE);                                                        \
  (void)res;                                                                  \
} while (0)

#endif  // _WIN32_WINNT >= 0x0600

#else  // !_WIN32

#include <pthread.h>

#define WEBP_ONCE(func) do {                                       \
  static pthread_once_t func ## _once_control = PTHREAD_ONCE_INIT; \
  pthread_once(&func ## _once_control, func);                      \
} while (0)

#endif  // _WIN32

#endif  // WEBP_USE_THREAD

#ifndef WEBP_USE_THREAD
#error WEBP_USE_THREAD is undefined
#endif

#if !defined(WEBP_ONCE)

// This macro prevents thread_sanitizer from reporting known concurrent writes.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#undef WEBP_TSAN_IGNORE_FUNCTION
#define WEBP_TSAN_IGNORE_FUNCTION __attribute__((no_sanitize_thread))
#endif
#endif

#define WEBP_ONCE(func) do {         \
  static volatile int func ## _done; \
  if (func ## _done) break;          \
  func();                            \
  func ## _done = 1;                 \
} while (0)
#endif

#endif  // !defined(WEBP_ONCE)

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  /* WEBP_UTILS_THREAD_ONCE_H_ */
