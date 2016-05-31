// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//

#include "./image_dec.h"

static WEBP_INLINE uint32_t GetBE32(const uint8_t buf[]) {
  return ((uint32_t)buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

WebPInputFileFormat WebPGuessImageType(const uint8_t buf[12]) {
  WebPInputFileFormat format = WEBP_UNSUPPORTED;
  const uint32_t magic1 = GetBE32(buf + 0);
  const uint32_t magic2 = GetBE32(buf + 8);
  if (magic1 == 0x89504E47U) {
    format = WEBP_PNG_;
  } else if (magic1 >= 0xFFD8FF00U && magic1 <= 0xFFD8FFFFU) {
    format = WEBP_JPEG_;
  } else if (magic1 == 0x49492A00 || magic1 == 0x4D4D002A) {
    format = WEBP_TIFF_;
  } else if (magic1 == 0x52494646 && magic2 == 0x57454250) {
    format = WEBP_WEBP_;
  }
  return format;
}
