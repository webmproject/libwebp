// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Utility functions used by the example programs.
//

#include "./example_util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/mux_types.h"
#include "../imageio/imageio_util.h"

#include "./unicode.h"

//------------------------------------------------------------------------------
// String parsing

uint32_t ExUtilGetUInt(const char* const str, int base, int* const error) {
  const GCHAR* const v = (const GCHAR*)str;
  GCHAR* end = NULL;
  const uint32_t n = (v != NULL) ? (uint32_t)STRTOUL(v, &end, base) : 0u;
  if (end == v && error != NULL && !*error) {
    *error = 1;
    FPRINTF(stderr, "Error! '%s' is not an integer.\n",
            (v != NULL) ? v : TO_GCHAR("(null)"));
  }
  return n;
}

int ExUtilGetInt(const char* const str, int base, int* const error) {
  return (int)ExUtilGetUInt(str, base, error);
}

int ExUtilGetInts(const char* str, int base, int max_output, int output[]) {
  const GCHAR* v = (const GCHAR*)str;
  int n, error = 0;
  for (n = 0; v != NULL && n < max_output; ++n) {
    const int value = ExUtilGetInt((const char*)v, base, &error);
    if (error) return -1;
    output[n] = value;
    v = STRCHR(v, TO_GCHAR(','));
    if (v != NULL) ++v;   // skip over the trailing ','
  }
  return n;
}

float ExUtilGetFloat(const char* const str, int* const error) {
  const GCHAR* const v = (const GCHAR*)str;
  GCHAR* end = NULL;
  const float f = (v != NULL) ? (float)STRTOD(v, &end) : 0.f;
  if (end == v && error != NULL && !*error) {
    *error = 1;
    FPRINTF(stderr, "Error! '%s' is not a floating point number.\n",
            (v != NULL) ? v : TO_GCHAR("(null)"));
  }
  return f;
}

//------------------------------------------------------------------------------

static void ResetCommandLineArguments(int argc, const char* argv[],
                                      CommandLineArguments* const args) {
  assert(args != NULL);
  args->argc_ = argc;
  args->argv_ = argv;
  args->own_argv_ = 0;
  WebPDataInit(&args->argv_data_);
}

void ExUtilDeleteCommandLineArguments(CommandLineArguments* const args) {
  if (args != NULL) {
    if (args->own_argv_) {
      free((void*)args->argv_);
      WebPDataClear(&args->argv_data_);
    }
    ResetCommandLineArguments(0, NULL, args);
  }
}

// Function similar to ExUtilReadFileToWebPData() but handles files containing
// Unicode characters on Windows thanks to _wfopen(path, "rt, ccs=UTF-8")
static int ReadTextFileToWebPData(const GCHAR* const filename,
                                  WebPData* const webp_data) {
  GCHAR* text;
  int64_t text_length;
  const int64_t max_text_length = (1ll << 31) / (int64_t)sizeof(text[0]);
  GCHAR_INT character;
  int64_t character_pos;
  FILE* file;
  if (webp_data == NULL) return 0;
  if ((filename == NULL) || !STRCMP(filename, "-")) {  // From stdin
    return ImgIoUtilReadFromStdin(&webp_data->bytes, &webp_data->size);
  }

  file = FOPEN_RT(filename);  // Read Text, use ccs=UTF-8 if Unicode
  if (file == NULL) {
    FPRINTF(stderr, "Cannot open input file %s\n", filename);
    return 0;
  }
  fseek(file, 0, SEEK_END);
  text_length = ftell(file);
  if (text_length <= 0 || text_length + 1 > max_text_length) {
    FPRINTF(stderr, "Cannot open input file or it is too big: %s\n", filename);
    return 0;
  }

  // Allocate one extra byte for the \0 terminator.
  text = (GCHAR*)malloc((size_t)(text_length + 1) * sizeof(text[0]));
  if (text == NULL) {
    fclose(file);
    FPRINTF(stderr, "Memory allocation failure when reading file %s\n",
            filename);
    return 0;
  }

  fseek(file, 0, SEEK_SET);
  for (character_pos = 0;
       (character = FGETC(file)) != GCHAR_EOF && character_pos < text_length;
       ++character_pos) {
    text[character_pos] = (GCHAR)character;
  }
  fclose(file);

  text[character_pos] = TO_GCHAR('\0');  // character_pos is at most text_length
  webp_data->bytes = (uint8_t*)text;
  webp_data->size = (size_t)text_length;
  return 1;
}

// Advance *from till it points to a character not in sep. Advance *end to *from
// and then till it points to a separator character. Return 1 if token is valid.
static int DelimitToken(const GCHAR** from, const GCHAR** end,
                        const GCHAR* const sep) {
  int token_length = 0;
  if (from == NULL || *from == NULL || end == NULL) return 0;
  *end = *from;
  while (**end != TO_GCHAR('\0')) {    // For each character from *from,
    if (STRCHR(sep, **end) != NULL) {  // If it's a separator:
      if (token_length > 0) {          // If we have a token, we return it;
        return 1;
      }
      ++*from;                         // Otherwise the token didn't start yet.
    } else {
      ++token_length;                  // Remember if we saw a token.
    }
    ++*end;
  }
  return (token_length > 0);           // We might have a token at the end.
}

#define MAX_ARGC 16384
int ExUtilInitCommandLineArguments(int argc, const char* argv[],
                                   CommandLineArguments* const args) {
  if (args == NULL || argv == NULL) return 0;
  ResetCommandLineArguments(argc, argv, args);
  if (argc == 1 && ((const GCHAR**)argv)[0][0] != TO_GCHAR('-')) {
    const GCHAR* token_start;
    GCHAR* token_end = NULL;
    const GCHAR sep[] = TO_GCHAR(" \t\r\n\f\v");
    if (!ReadTextFileToWebPData((const GCHAR*)argv[0], &args->argv_data_)) {
      return 0;
    }
    args->own_argv_ = 1;
    args->argv_ = (const char**)malloc(MAX_ARGC * sizeof(GCHAR*));
    if (args->argv_ == NULL) return 0;

    argc = 0;
    token_start = (GCHAR*)args->argv_data_.bytes;
    while (DelimitToken(&token_start, (const GCHAR**)&token_end, sep)) {
      if (argc == MAX_ARGC) {
        fprintf(stderr, "ERROR: Arguments limit %d reached\n", MAX_ARGC);
        return 0;
      }
      assert(token_start != token_end);
      args->argv_[argc++] = (char*)token_start;
      if (*token_end == TO_GCHAR('\0')) break;  // Hit the end of argv_data.
      *token_end = TO_GCHAR('\0');  // End current token by null character.
      token_start = token_end + 1;
    }
    args->argc_ = argc;
  }
  return 1;
}

//------------------------------------------------------------------------------

int ExUtilReadFileToWebPData(const char* const filename,
                             WebPData* const webp_data) {
  const uint8_t* data;
  size_t size;
  if (webp_data == NULL) return 0;
  if (!ImgIoUtilReadFile(filename, &data, &size)) return 0;
  webp_data->bytes = data;
  webp_data->size = size;
  return 1;
}
