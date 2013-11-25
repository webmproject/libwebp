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

#ifndef WEBP_EXAMPLES_EXAMPLE_UTIL_H_
#define WEBP_EXAMPLES_EXAMPLE_UTIL_H_

#include "webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Allocates storage for entire file 'file_name' and returns contents and size
// in 'data' and 'data_size'. Returns 1 on success, 0 otherwise. '*data' should
// be deleted using free().
int ExUtilReadFile(const char* const file_name,
                   const uint8_t** data, size_t* data_size);

// Write a data segment into a file named 'file_name'. Returns true if ok.
int ExUtilWriteFile(const char* const file_name,
                    const uint8_t* data, size_t data_size);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_EXAMPLES_EXAMPLE_UTIL_H_
