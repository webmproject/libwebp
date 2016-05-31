// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  All-in-one library to decode PNG/JPEG/WebP/TIFF/WIC input images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_EXAMPLES_IMAGE_DEC_H_
#define WEBP_EXAMPLES_IMAGE_DEC_H_

#include "webp/types.h"

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

#include "./metadata.h"
#include "./jpegdec.h"
#include "./pngdec.h"
#include "./tiffdec.h"
#include "./webpdec.h"
#include "./wicdec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  WEBP_PNG_ = 0,
  WEBP_JPEG_,
  WEBP_TIFF_,
  WEBP_WEBP_,
  WEBP_UNSUPPORTED_
} WebPInputFileFormat;

WebPInputFileFormat WebPGuessImageType(const uint8_t buf[12]);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_EXAMPLES_IMAGE_DEC_H_
