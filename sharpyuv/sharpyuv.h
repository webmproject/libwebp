// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Sharp RGB to YUV conversion.

#ifndef WEBP_SHARPYUV_SHARPYUV_H_
#define WEBP_SHARPYUV_SHARPYUV_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Converts RGB to YUV420 using a downsampling algorithm that minimizes
// artefacts caused by chroma subsampling.
// This is slower than standard downsampling (averaging of 4 UV values).
// Assumes that the image will be upsampled using a bilinear filter. If nearest
// neighbor is used instead, the upsampled image might look worse than with
// standard downsampling.
// TODO(maryla): add 10 bits and handling of various colorspaces. Add YUV444 to
// YUV420 conversion. Maybe also add 422 support (it's rarely used in practice,
// especially for images).
int SharpArgbToYuv(const uint8_t* r_ptr, const uint8_t* g_ptr,
                   const uint8_t* b_ptr, int step, int rgb_stride,
                   uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
                   int dst_stride_u, uint8_t* dst_v, int dst_stride_v,
                   int width, int height);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBP_SHARPYUV_SHARPYUV_H_
