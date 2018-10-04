// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------

#ifndef WEBP_EXAMPLES_UNICODE_H_
#define WEBP_EXAMPLES_UNICODE_H_

#ifdef _WIN32

#include <tchar.h>
#include <windows.h>

// Macros used for Unicode support on Windows. Linux and Mac don't need those.

#define MAIN _tmain
#define GCHAR TCHAR
#define GCHAR_INT _TINT
#define GCHAR_EOF _TEOF
#define TO_GCHAR(STR) _T(STR)
#define PRINTF(STR, ...) _tprintf(TO_GCHAR(STR), __VA_ARGS__)
#define FPRINTF(STDERR, STR, ...) _ftprintf(STDERR, TO_GCHAR(STR), __VA_ARGS__)
#define SNPRINTF(A, B, STR, ...) _sntprintf(A, B, TO_GCHAR(STR), __VA_ARGS__)
#define STRCMP(ARG, STR) _tcscmp(ARG, TO_GCHAR(STR))
#define STRCMP_NO_LITERAL _tcscmp
#define STRNCMP _tcsncmp
#define STRCHR _tcschr
#define STRRCHR _tcsrchr
#define STRLEN _tcslen
#define SSCANF(ARG, STR, ...) _stscanf(ARG, TO_GCHAR(STR), __VA_ARGS__)
#define STRTOUL _tcstoul
#define STRTOD _tcstod
#define FOPEN(ARG, OPT) _tfopen(ARG, TO_GCHAR(OPT))
#ifdef _UNICODE
#define FOPEN_RT(ARG) _tfopen(ARG, TO_GCHAR("rt, ccs=UTF-8"))
#else
#define FOPEN_RT(ARG) _tfopen(ARG, TO_GCHAR("rt"))
#endif
#define FGETC(ARG) _fgettc(ARG)

#else

#define MAIN main
#define GCHAR char
#define GCHAR_INT int
#define GCHAR_EOF EOF
#define TO_GCHAR(STR) (STR)
#define PRINTF(STR, ...) printf(STR, __VA_ARGS__)
#define FPRINTF(STDERR, STR, ...) fprintf(STDERR, STR, __VA_ARGS__)
#define SNPRINTF(A, B, STR, ...) snprintf(A, B, STR, __VA_ARGS__)
#define STRCMP(ARG, STR) strcmp(ARG, STR)
#define STRCMP_NO_LITERAL strcmp
#define STRNCMP strncmp
#define STRCHR strchr
#define STRRCHR strrchr
#define STRLEN strlen
#define SSCANF(ARG, STR, ...) sscanf(ARG, STR, __VA_ARGS__)
#define STRTOUL strtoul
#define STRTOD strtod
#define FOPEN(ARG, OPT) fopen(ARG, OPT)
#define FOPEN_RT(ARG) fopen(ARG, "rt")
#define FGETC(ARG) fgetc(ARG)

#endif

// -----------------------------------------------------------------------------
// giflib doesn't have a Unicode DGifOpenFileName(). Let's make one.

#if defined(WEBP_HAVE_GIF)

#ifdef _WIN32
#include <fcntl.h>          // Not standard, needed for _topen and flags.
#include <io.h>
#endif

#include <gif_lib.h>
#include "./gifdec.h"

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

static inline GifFileType *DGifOpenFileUnicode(const GCHAR *file_name,
                                               int *error) {
  if (!STRCMP(file_name, "-")) {
#if LOCAL_GIF_PREREQ(5, 0)
    return DGifOpenFileHandle(STDIN_FILENO, error);
#else
    return DGifOpenFileHandle(STDIN_FILENO);
#endif
  }

#ifdef _WIN32  // Open the file in a Unicode way if it's Windows.

  int file_handle = _topen(file_name, _O_RDONLY | _O_BINARY);
  if (file_handle == -1) {
    if (error != NULL) *error = D_GIF_ERR_OPEN_FAILED;
    return NULL;
  }

#if LOCAL_GIF_PREREQ(5, 0)
  return DGifOpenFileHandle(file_handle, error);
#else
  return DGifOpenFileHandle(file_handle);
#endif

#else  // Use regular DGifOpenFileName() if it's not Windows.

#if LOCAL_GIF_PREREQ(5, 0)
  return DGifOpenFileName(file_name, error);
#else
  return DGifOpenFileName(file_name);
#endif

#endif  // _WIN32
}

#endif  // defined(WEBP_HAVE_GIF)

// -----------------------------------------------------------------------------
// Unicode ExUtilGet... are defined in example_util.c but still take char*

#define EXUTILGETINT(...) ExUtilGetInt((const char*)__VA_ARGS__)
#define EXUTILGETINTS(...) ExUtilGetInts((const char*)__VA_ARGS__)
#define EXUTILGETFLOAT(...) ExUtilGetFloat((const char*)__VA_ARGS__)

#endif  // WEBP_EXAMPLES_UNICODE_H_
