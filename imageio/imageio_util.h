// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Utility functions used by the image decoders.
//

#ifndef WEBP_IMAGEIO_IMAGEIO_UTIL_H_
#define WEBP_IMAGEIO_IMAGEIO_UTIL_H_

#include <stdio.h>
#include "webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// File I/O

// Reopen file in binary (O_BINARY) mode.
// Returns 'file' on success, NULL otherwise.
FILE* ExUtilSetBinaryMode(FILE* file);

// Allocates storage for entire file 'file_name' and returns contents and size
// in 'data' and 'data_size'. Returns 1 on success, 0 otherwise. '*data' should
// be deleted using free().
// If 'file_name' is NULL or equal to "-", input is read from stdin by calling
// the function ExUtilReadFromStdin().
int ExUtilReadFile(const char* const file_name,
                   const uint8_t** data, size_t* data_size);

// Same as ExUtilReadFile(), but reads until EOF from stdin instead.
int ExUtilReadFromStdin(const uint8_t** data, size_t* data_size);

// Write a data segment into a file named 'file_name'. Returns true if ok.
// If 'file_name' is NULL or equal to "-", output is written to stdout.
int ExUtilWriteFile(const char* const file_name,
                    const uint8_t* data, size_t data_size);

//------------------------------------------------------------------------------

// Copy width x height pixels from 'src' to 'dst' honoring the strides.
void ExUtilCopyPlane(const uint8_t* src, int src_stride,
                     uint8_t* dst, int dst_stride, int width, int height);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_IMAGEIO_IMAGEIO_UTIL_H_
