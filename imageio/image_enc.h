// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  All-in-one library to save PNG/JPEG/WebP/TIFF/WIC images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_IMAGEIO_IMAGE_ENC_H_
#define WEBP_IMAGEIO_IMAGE_ENC_H_

#include <stdio.h>
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

// Output types
typedef enum {
  PNG = 0,
  PAM,
  PPM,
  PGM,
  BMP,
  TIFF,
  RAW_YUV,
  ALPHA_PLANE_ONLY,  // this is for experimenting only
  // forced colorspace output (for testing, mostly)
  RGB, RGBA, BGR, BGRA, ARGB,
  RGBA_4444, RGB_565,
  rgbA, bgrA, Argb, rgbA_4444,
  YUV, YUVA
} WebPOutputFileFormat;

// Save to PNG
#ifdef HAVE_WINCODEC_H
int WebPWritePNG(const char* out_file_name, int use_stdout,
                 const struct WebPDecBuffer* const buffer);
#else
int WebPWritePNG(FILE* out_file, const WebPDecBuffer* const buffer);
#endif

// Save to PPM
int WebPWritePPM(FILE* fout, const struct WebPDecBuffer* const buffer,
                 int alpha);

// Save 16b mode (RGBA4444, RGB565, ...) for debugging purpose.
int WebPWrite16bAsPGM(FILE* fout, const struct WebPDecBuffer* const buffer);

// Save 16b mode (RGBA4444, RGB565, ...) for debugging purpose.
int WebPWrite16bAsPGM(FILE* fout, const struct WebPDecBuffer* const buffer);

// Save as BMP
int WebPWriteBMP(FILE* fout, const struct WebPDecBuffer* const buffer);

// Save as TIFF
int WebPWriteTIFF(FILE* fout, const struct WebPDecBuffer* const buffer);

// Save the ALPHA plane (only) as a PGM
int WebPWriteAlphaPlane(FILE* fout, const struct WebPDecBuffer* const buffer);

// Save as PGM (TODO(skal): split PGM / YUV)
int WebPWritePGMOrYUV(FILE* fout, const struct WebPDecBuffer* const buffer,
                      WebPOutputFileFormat format);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_IMAGEIO_IMAGE_ENC_H_
