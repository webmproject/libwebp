// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// (limited) NETPBM decoder

#include "./pbmdec.h"

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"
#include "./imageio_util.h"

// -----------------------------------------------------------------------------
// PBM decoding

#define MAX_LINE_SIZE 1024
static const size_t kMinPBMHeaderSize = 3;

static size_t ReadLine(const uint8_t* const data, size_t off, size_t data_size,
                       char out[MAX_LINE_SIZE + 1], size_t* const out_size) {
  size_t i = 0;
  *out_size = 0;
 redo:
  for (i = 0; i < MAX_LINE_SIZE && off < data_size; ++i) {
    out[i] = data[off++];
    if (out[i] == '\n') break;
  }
  if (off < data_size) {
    if (i == 0) goto redo;   // empty line
    if (out[0] == '#') goto redo;  // skip comment
  }
  out[i] = 0; // safety sentinel
  *out_size = i;
  return off;
}

static size_t ReadPGMHeader(const uint8_t* const data, size_t data_size,
                            int* const width, int* const height,
                            int* const type, int* const max_value) {
  size_t off = 0;
  char out[MAX_LINE_SIZE + 1];
  size_t out_size;
  if (width == NULL || height == NULL || type == NULL || max_value == NULL) {
    return 0;
  }
  *width = 0;
  *height = 0;
  *type = -1;
  *max_value = 0;
  if (data == NULL || data_size < kMinPBMHeaderSize) return 0;
  off = ReadLine(data, off, data_size, out, &out_size);
  if (off == 0 || sscanf(out, "P%d", type) != 1) return 0;
  off = ReadLine(data, off, data_size, out, &out_size);
  if (off == 0 || sscanf(out, "%d %d", width, height) != 2) return 0;
  off = ReadLine(data, off, data_size, out, &out_size);
  if (off == 0 || sscanf(out, "%d", max_value) != 1) return 0;
  return off;
}

int ReadPBM(const uint8_t* const data, size_t data_size,
            WebPPicture* const pic, int keep_alpha,
            struct Metadata* const metadata) {
  int ok = 0;
  int width, height, type, max_value;
  int i, j;
  int64_t stride;
  uint8_t* rgb = NULL, * tmp_rgb;
  size_t offset = ReadPGMHeader(data, data_size, &width, &height,
                                &type, &max_value);
  if (offset == 0) goto End;
  if (type != 5 && type != 6) {
    fprintf(stderr, "Unsupported P%d PBM format.\n", type);
    goto End;
  }

  // Some basic validations.
  if (pic == NULL) goto End;

  stride = 3LL * width;
  if (stride != (int)stride ||
      !ImgIoUtilCheckSizeArgumentsOverflow(stride, height)) {
    goto End;
  }
  if ((type == 5 && offset + 1LL * width * height > data_size) ||
      (type == 6 && offset + 3LL * width * height > data_size)) {
    fprintf(stderr, "Truncated PBM file (P%d).\n", type);
    goto End;
  }

  rgb = (uint8_t*)malloc((size_t)stride * height);
  if (rgb == NULL) goto End;

  // Convert input
  tmp_rgb = rgb;
  for (j = 0; j < height; ++j) {
    if (type == 5) {
      assert(offset + width <= data_size);
      for (i = 0; i < width; ++i) {
        const uint8_t v = data[offset + i];
        tmp_rgb[3 * i + 0] = tmp_rgb[3 * i + 1] = tmp_rgb[3 * i + 2] = v;
      }
      offset += width;
    } else if (type == 6) {
      assert(offset + 3LL * width <= data_size);
      memcpy(tmp_rgb, data + offset, 3 * width * sizeof(*data));
      offset += 3 * width;
    }
    tmp_rgb += stride;
  }

  // WebP conversion.
  pic->width = width;
  pic->height = height;
  ok = WebPPictureImportRGB(pic, rgb, (int)stride);
  if (!ok) goto End;

  ok = 1;
 End:
  free((void*)rgb);

  (void)metadata;
  (void)keep_alpha;
  return ok;
}

// -----------------------------------------------------------------------------
