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
// Unicode ExUtilGet... are defined in example_util.c but still take char*

#define EXUTILGETINT(...) ExUtilGetInt((const char*)__VA_ARGS__)
#define EXUTILGETINTS(...) ExUtilGetInts((const char*)__VA_ARGS__)
#define EXUTILGETFLOAT(...) ExUtilGetFloat((const char*)__VA_ARGS__)

#endif  // WEBP_EXAMPLES_UNICODE_H_
