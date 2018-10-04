// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------

#ifndef WEBP_EXAMPLES_UNICODE_H_
#define WEBP_EXAMPLES_UNICODE_H_

#if defined(_WIN32) && _UNICODE
#include <wchar.h>
#include <windows.h>

// Macros used for Unicode support on Windows. Linux and Mac don't need those.

#define MAIN wmain
#define GCHAR wchar_t
#define WINT wint_t
#define TO_GCHAR(STR) (L##STR)
#define PRINTF(STR, ...) wprintf(L##STR, __VA_ARGS__)
#define FPRINTF(STDERR, STR, ...) fwprintf(STDERR, L##STR, __VA_ARGS__)
#define SNPRINTF(A, B, STR, ...) _snwprintf(A, B, L##STR, __VA_ARGS__)
#define FOPEN(ARG, OPT) _wfopen(ARG, L##OPT)
#define OPEN_RDONLY(ARG) _wopen(ARG, _O_RDONLY | _O_BINARY)
#define FGETC(ARG) fgetwc(ARG)
#define GCHAR_EOF WEOF
#define STRCMP(ARG, STR) wcscmp(ARG, L##STR)
#define STRCMP_NO_LITERAL wcscmp
#define STRNCMP wcsncmp
#define STRCHR wcschr
#define STRRCHR wcsrchr
#define STRLEN wcslen
#define SSCANF(ARG, STR, ...) swscanf(ARG, L##STR, __VA_ARGS__)
#define EXUTILGETINT(A, B, C) ExUtilGetIntW(A, B, C)
#define EXUTILGETINTS(A, B, C, D) ExUtilGetIntsW(A, B, C, D)
#define EXUTILGETFLOAT(A, B) ExUtilGetFloatW(A, B)
#else
#define MAIN main
#define GCHAR char
#define WINT int
#define TO_GCHAR(STR) (STR)
#define PRINTF(STR, ...) printf(STR, __VA_ARGS__)
#define FPRINTF(STDERR, STR, ...) fprintf(STDERR, STR, __VA_ARGS__)
#define SNPRINTF(A, B, STR, ...) snprintf(A, B, STR, __VA_ARGS__)
#define FOPEN(ARG, OPT) fopen(ARG, OPT)
#if defined(_WIN32)
#include <io.h>
#define OPEN_RDONLY(ARG) _open(ARG, _O_RDONLY | _O_BINARY)
#else
#define OPEN_RDONLY(ARG) open(ARG, O_RDONLY)
#endif
#define FGETC(ARG) fgetc(ARG)
#define GCHAR_EOF EOF
#define STRCMP(ARG, STR) strcmp(ARG, STR)
#define STRCMP_NO_LITERAL strcmp
#define STRNCMP strncmp
#define STRCHR strchr
#define STRRCHR strrchr
#define STRLEN strlen
#define SSCANF(ARG, STR, ...) sscanf(ARG, STR, __VA_ARGS__)
#define EXUTILGETINT(A, B, C) ExUtilGetInt(A, B, C)
#define EXUTILGETINTS(A, B, C, D) ExUtilGetInts(A, B, C, D)
#define EXUTILGETFLOAT(A, B) ExUtilGetFloat(A, B)
#endif

// Extract int or float from a string. wchar_t* is converted to char* first.

#if defined(_WIN32) && _UNICODE
#include "./example_util.h"

#define MAX_V_GCHAR (MAX_PATH + 1)  // For terminating null character.
static int ExUtilGetIntW(const wchar_t* const v, int base, int* const error) {
  char v_char[MAX_V_GCHAR];
  WideCharToMultiByte(CP_UTF8, 0, v, -1, v_char, MAX_V_GCHAR, NULL, NULL);
  return ExUtilGetInt(v_char, base, error);
}
static int ExUtilGetIntsW(const wchar_t* v, int base, int max_output,
                          int output[]) {
  char v_char[MAX_V_GCHAR];
  WideCharToMultiByte(CP_UTF8, 0, v, -1, v_char, MAX_V_GCHAR, NULL, NULL);
  return ExUtilGetInts(v_char, base, max_output, output);
}
static float ExUtilGetFloatW(const wchar_t* const v, int* const error) {
  char v_char[MAX_V_GCHAR];
  WideCharToMultiByte(CP_UTF8, 0, v, -1, v_char, MAX_V_GCHAR, NULL, NULL);
  return ExUtilGetFloat(v_char, error);
}
//static int ReplaceMultiByteByWideChar(char* str) {
//  char* to_free = str;
//  wchar_t* wstr;
//  int wstr_len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
//  wstr = (wchar_t*)malloc(wstr_len * sizeof(wchar_t));
//  if (wstr == NULL) return 0;
//  MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wstr_len);
//  free(to_free);
//  return 1;
//}
//#else
//static int ReplaceMultiByteByWideChar(char* str) {
//  (void)str;
//  return 1;
//}
#endif

#endif  // WEBP_EXAMPLES_UNICODE_H_
