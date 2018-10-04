// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------

#ifndef WEBP_EXAMPLES_UNICODE_GIF_H_
#define WEBP_EXAMPLES_UNICODE_GIF_H_

#include "./unicode.h"

// -----------------------------------------------------------------------------
// giflib doesn't have a Unicode DGifOpenFileName(). Let's make one.

#if defined(WEBP_HAVE_GIF)

#ifdef _WIN32
#include <fcntl.h>  // Not standard, needed for _topen and flags.
#include <io.h>
#endif

#include <gif_lib.h>
#include "./gifdec.h"

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

static GifFileType *DGifOpenFileUnicode(const GCHAR *file_name, int *error) {
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

#endif  // WEBP_EXAMPLES_UNICODE_GIF_H_
