// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebPPicture class basis
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "src/enc/vp8i_enc.h"
#include "src/utils/utils.h"
#include "src/webp/encode.h"
#include "src/webp/types.h"

//------------------------------------------------------------------------------
// WebPPicture
//------------------------------------------------------------------------------

static int DummyWriter(const uint8_t* data, size_t data_size,
                       const WebPPicture* const picture) {
  // The following are to prevent 'unused variable' error message.
  (void)data;
  (void)data_size;
  (void)picture;
  return 1;
}

int WebPPictureInitInternal(WebPPicture* picture, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_ENCODER_ABI_VERSION)) {
    return 0;  // caller/system version mismatch!
  }
  if (picture != NULL) {
    memset(picture, 0, sizeof(*picture));
    picture->writer = DummyWriter;
    WebPEncodingSetError(picture, VP8_ENC_OK);
  }
  return 1;
}

//------------------------------------------------------------------------------

#ifndef PTRDIFF_MAX
#define PTRDIFF_MAX ((ptrdiff_t)(~(size_t)0 >> 1))
#endif
#ifndef UINTPTR_MAX
#define UINTPTR_MAX ((uintptr_t)-1)
#endif

// Returns 1 if [ptr, ptr + (height - 1) * stride + width*step] (or for negative
// stride, [ptr - (height - 1) * (-stride), ptr + width*step]) fits in
// addressable memory without wrapping around the address space, 0 otherwise.
static int CheckMemoryBounds(const void* ptr, int stride, int step, int width,
                             int height) {
  const uint64_t abs_stride =
      (stride < 0) ? (uint64_t)-(int64_t)stride : (uint64_t)stride;
  const uint64_t row_span = (uint64_t)(height - 1) * abs_stride;
  const uint64_t width_bytes = (uint64_t)width * (uint64_t)step;
  uint64_t total_bytes;
  uintptr_t addr;

  if (ptr == NULL || width <= 0 || height <= 0 || step <= 0) return 0;

  // Check that the total plane size fits in addressable limits (PTRDIFF_MAX).
  if (row_span > (uint64_t)PTRDIFF_MAX - width_bytes) return 0;

  total_bytes = row_span + width_bytes;
  addr = (uintptr_t)ptr;

  if (stride >= 0) {
    if (addr > (uintptr_t)UINTPTR_MAX - (uintptr_t)total_bytes) return 0;
  } else {
    if (addr < (uintptr_t)row_span) return 0;
    if (addr > (uintptr_t)UINTPTR_MAX - (uintptr_t)width_bytes) return 0;
  }
  return 1;
}

int WebPValidatePicture(const WebPPicture* const picture) {
  if (picture == NULL) return 0;
  if (picture->width <= 0 || picture->width > INT_MAX / 4 ||
      picture->height <= 0 || picture->height > INT_MAX / 4) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_BAD_DIMENSION);
  }
  if (picture->colorspace != WEBP_YUV420 &&
      picture->colorspace != WEBP_YUV420A) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }
  // If buffers are provided, validate their strides and memory bounds.
  if (picture->use_argb) {
    if (picture->argb != NULL) {
      // Validate that argb_stride scaled to bytes does not overflow 'int',
      // satisfies the buffer dimension constraints, and fits in memory.
      const int argb_bpp = (int)sizeof(*picture->argb);
      if (picture->argb_stride > INT_MAX / argb_bpp ||
          picture->argb_stride < -INT_MAX / argb_bpp ||
          !CheckStride(argb_bpp * picture->argb_stride, argb_bpp,
                       picture->width) ||
          !CheckMemoryBounds(picture->argb, argb_bpp * picture->argb_stride,
                             argb_bpp, picture->width, picture->height)) {
        return WebPEncodingSetError(picture, VP8_ENC_ERROR_BAD_DIMENSION);
      }
    }
  } else {
    // If YUV planes are provided, validate Y and U/V strides (U/V planes are
    // subsampled by 2 in each dimension for YUV420), as well as alpha stride
    // if an alpha plane is present, and check that each plane fits in memory.
    if (picture->y != NULL || picture->u != NULL || picture->v != NULL) {
      const int uv_width = (int)(((int64_t)picture->width + 1) >> 1);
      const int uv_height = (int)(((int64_t)picture->height + 1) >> 1);
      if (!CheckStride(picture->y_stride, 1, picture->width) ||
          !CheckStride(picture->uv_stride, 1, uv_width) ||
          (picture->y != NULL &&
           !CheckMemoryBounds(picture->y, picture->y_stride, /*step=*/1,
                              picture->width, picture->height)) ||
          (picture->u != NULL &&
           !CheckMemoryBounds(picture->u, picture->uv_stride, /*step=*/1,
                              uv_width, uv_height)) ||
          (picture->v != NULL &&
           !CheckMemoryBounds(picture->v, picture->uv_stride, /*step=*/1,
                              uv_width, uv_height))) {
        return WebPEncodingSetError(picture, VP8_ENC_ERROR_BAD_DIMENSION);
      }
      if (picture->a != NULL &&
          (!CheckStride(picture->a_stride, 1, picture->width) ||
           !CheckMemoryBounds(picture->a, picture->a_stride, /*step=*/1,
                              picture->width, picture->height))) {
        return WebPEncodingSetError(picture, VP8_ENC_ERROR_BAD_DIMENSION);
      }
    }
  }
  return 1;
}

static void WebPPictureResetBufferARGB(WebPPicture* const picture) {
  picture->memory_argb_ = NULL;
  picture->argb = NULL;
  picture->argb_stride = 0;
}

static void WebPPictureResetBufferYUVA(WebPPicture* const picture) {
  picture->memory_ = NULL;
  picture->y = picture->u = picture->v = picture->a = NULL;
  picture->y_stride = picture->uv_stride = 0;
  picture->a_stride = 0;
}

void WebPPictureResetBuffers(WebPPicture* const picture) {
  WebPPictureResetBufferARGB(picture);
  WebPPictureResetBufferYUVA(picture);
}

int WebPPictureAllocARGB(WebPPicture* const picture) {
  void* memory;
  int width, height;
  uint64_t argb_size;

  if (!WebPValidatePicture(picture)) return 0;

  WebPSafeFree(picture->memory_argb_);
  WebPPictureResetBufferARGB(picture);

  width = picture->width;
  height = picture->height;
  argb_size = (uint64_t)width * height;

  // allocate a new buffer.
  memory = WebPSafeMalloc(argb_size + WEBP_ALIGN_CST, sizeof(*picture->argb));
  if (memory == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  picture->memory_argb_ = memory;
  picture->argb = (uint32_t*)WEBP_ALIGN(memory);
  picture->argb_stride = width;
  return 1;
}

int WebPPictureAllocYUVA(WebPPicture* const picture) {
  int has_alpha, width, height, y_stride, uv_width, uv_height, uv_stride;
  int a_width, a_stride;
  uint64_t y_size, uv_size, a_size, total_size;
  uint8_t* mem;

  if (!WebPValidatePicture(picture)) return 0;

  WebPSafeFree(picture->memory_);
  WebPPictureResetBufferYUVA(picture);

  has_alpha = (int)picture->colorspace & WEBP_CSP_ALPHA_BIT;
  width = picture->width;
  height = picture->height;
  y_stride = width;
  uv_width = (int)(((int64_t)width + 1) >> 1);
  uv_height = (int)(((int64_t)height + 1) >> 1);
  uv_stride = uv_width;
  // alpha
  a_width = has_alpha ? width : 0;
  a_stride = a_width;
  y_size = (uint64_t)y_stride * height;
  uv_size = (uint64_t)uv_stride * uv_height;
  a_size = (uint64_t)a_stride * height;

  total_size = y_size + a_size + 2 * uv_size;

  // allocate a new buffer.
  mem = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*mem));
  if (mem == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }

  // From now on, we're in the clear, we can no longer fail...
  picture->memory_ = (void*)mem;
  picture->y_stride = y_stride;
  picture->uv_stride = uv_stride;
  picture->a_stride = a_stride;

  // TODO(skal): we could align the y/u/v planes and adjust stride.
  picture->y = mem;
  mem += y_size;

  picture->u = mem;
  mem += uv_size;
  picture->v = mem;
  mem += uv_size;

  if (a_size > 0) {
    picture->a = mem;
    mem += a_size;
  }
  (void)mem;  // makes the static analyzer happy
  return 1;
}

int WebPPictureAlloc(WebPPicture* picture) {
  if (picture != NULL) {
    WebPPictureFree(picture);  // erase previous buffer

    if (!picture->use_argb) {
      return WebPPictureAllocYUVA(picture);
    } else {
      return WebPPictureAllocARGB(picture);
    }
  }
  return 1;
}

void WebPPictureFree(WebPPicture* picture) {
  if (picture != NULL) {
    WebPSafeFree(picture->memory_);
    WebPSafeFree(picture->memory_argb_);
    WebPPictureResetBuffers(picture);
  }
}

//------------------------------------------------------------------------------
// WebPMemoryWriter: Write-to-memory

void WebPMemoryWriterInit(WebPMemoryWriter* writer) {
  writer->mem = NULL;
  writer->size = 0;
  writer->max_size = 0;
}

int WebPMemoryWrite(const uint8_t* data, size_t data_size,
                    const WebPPicture* picture) {
  WebPMemoryWriter* const w = (WebPMemoryWriter*)picture->custom_ptr;
  uint64_t next_size;
  if (w == NULL) {
    return 1;
  }
  next_size = (uint64_t)w->size + data_size;
  if (next_size > w->max_size) {
    uint8_t* new_mem;
    uint64_t next_max_size = 2ULL * w->max_size;
    if (next_max_size < next_size) next_max_size = next_size;
    if (next_max_size < 8192ULL) next_max_size = 8192ULL;
    new_mem = (uint8_t*)WebPSafeMalloc(next_max_size, 1);
    if (new_mem == NULL) {
      return 0;
    }
    if (w->size > 0) {
      memcpy(new_mem, w->mem, w->size);
    }
    WebPSafeFree(w->mem);
    w->mem = new_mem;
    // down-cast is ok, thanks to WebPSafeMalloc
    w->max_size = (size_t)next_max_size;
  }
  if (data_size > 0) {
    memcpy(w->mem + w->size, data, data_size);
    w->size += data_size;
  }
  return 1;
}

void WebPMemoryWriterClear(WebPMemoryWriter* writer) {
  if (writer != NULL) {
    WebPSafeFree(writer->mem);
    WebPMemoryWriterInit(writer);
  }
}

//------------------------------------------------------------------------------
// Simplest high-level calls:

typedef int (*Importer)(WebPPicture* const, const uint8_t* const, int);

WEBP_NODISCARD static size_t Encode(const uint8_t* rgba, int width, int height,
                                    int stride, Importer import,
                                    float quality_factor, int lossless,
                                    uint8_t** output) {
  WebPPicture pic;
  WebPConfig config;
  WebPMemoryWriter wrt;
  int ok;

  if (output == NULL) return 0;

  if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality_factor) ||
      !WebPPictureInit(&pic)) {
    return 0;  // shouldn't happen, except if system installation is broken
  }

  config.lossless = !!lossless;
  pic.use_argb = !!lossless;
  pic.width = width;
  pic.height = height;
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &wrt;
  WebPMemoryWriterInit(&wrt);

  ok = import(&pic, rgba, stride) && WebPEncode(&config, &pic);
  WebPPictureFree(&pic);
  if (!ok) {
    WebPMemoryWriterClear(&wrt);
    *output = NULL;
    return 0;
  }
  *output = wrt.mem;
  return wrt.size;
}

#define ENCODE_FUNC(NAME, IMPORTER)                              \
  size_t NAME(const uint8_t* in, int w, int h, int bps, float q, \
              uint8_t** out) {                                   \
    return Encode(in, w, h, bps, IMPORTER, q, 0, out);           \
  }

ENCODE_FUNC(WebPEncodeRGB, WebPPictureImportRGB)
ENCODE_FUNC(WebPEncodeRGBA, WebPPictureImportRGBA)
#if !defined(WEBP_REDUCE_CSP)
ENCODE_FUNC(WebPEncodeBGR, WebPPictureImportBGR)
ENCODE_FUNC(WebPEncodeBGRA, WebPPictureImportBGRA)
#endif  // WEBP_REDUCE_CSP

#undef ENCODE_FUNC

#define LOSSLESS_DEFAULT_QUALITY 70.
#define LOSSLESS_ENCODE_FUNC(NAME, IMPORTER)                                  \
  size_t NAME(const uint8_t* in, int w, int h, int bps, uint8_t** out) {      \
    return Encode(in, w, h, bps, IMPORTER, LOSSLESS_DEFAULT_QUALITY, 1, out); \
  }

LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessRGB, WebPPictureImportRGB)
LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessRGBA, WebPPictureImportRGBA)
#if !defined(WEBP_REDUCE_CSP)
LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessBGR, WebPPictureImportBGR)
LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessBGRA, WebPPictureImportBGRA)
#endif  // WEBP_REDUCE_CSP

#undef LOSSLESS_ENCODE_FUNC

//------------------------------------------------------------------------------
