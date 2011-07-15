// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Everything about WebPDecBuffer
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include "vp8i.h"
#include "webpi.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// WebPDecBuffer

// Number of bytes per pixel for the different color-spaces.
static const int kModeBpp[MODE_LAST] = { 3, 4, 3, 4, 4, 2, 2, 1, 1 };

static VP8StatusCode CheckDecBuffer(const WebPDecBuffer* const buffer) {
  int ok = 1;
  WEBP_CSP_MODE mode = buffer->colorspace;
  const int width = buffer->width;
  const int height = buffer->height;
  if (mode >= MODE_YUV) {   // YUV checks
    const WebPYUVABuffer* const buf = &buffer->u.YUVA;
    const int size = buf->y_stride * height;
    const int u_size = buf->u_stride * ((height + 1) / 2);
    const int v_size = buf->v_stride * ((height + 1) / 2);
    const int a_size = buf->a_stride * height;
    ok &= (size <= buf->y_size);
    ok &= (u_size <= buf->u_size);
    ok &= (v_size <= buf->v_size);
    ok &= (a_size <= buf->a_size);
    ok &= (buf->y_stride >= width);
    ok &= (buf->u_stride >= (width + 1) / 2);
    ok &= (buf->v_stride >= (width + 1) / 2);
    if (buf->a) {
      ok &= (buf->a_stride >= width);
    }
  } else {    // RGB checks
    const WebPRGBABuffer* const buf = &buffer->u.RGBA;
    ok &= (buf->stride * height <= buf->size);
    ok &= (buf->stride >= width * kModeBpp[mode]);
  }
  return ok ? VP8_STATUS_OK : VP8_STATUS_INVALID_PARAM;
}

static VP8StatusCode AllocateBuffer(WebPDecBuffer* const buffer) {
  const int w = buffer->width;
  const int h = buffer->height;

  if (w <= 0 || h <= 0) {
    return VP8_STATUS_INVALID_PARAM;
  }

  if (!buffer->is_external_memory && buffer->private_memory == NULL) {
    uint8_t* output;
    WEBP_CSP_MODE mode = buffer->colorspace;
    int stride;
    int uv_stride = 0, a_stride = 0;
    int uv_size = 0;
    uint64_t size, a_size = 0, total_size;
    // We need memory and it hasn't been allocated yet.
    // => initialize output buffer, now that dimensions are known.
    stride = w * kModeBpp[mode];
    size = (uint64_t)stride * h;

    if (mode >= MODE_YUV) {
      uv_stride = (w + 1) / 2;
      uv_size = (uint64_t)uv_stride * ((h + 1) / 2);
      if (mode == MODE_YUVA) {
        a_stride = w;
        a_size = (uint64_t)a_stride * h;
      }
    }
    total_size = size + 2 * uv_size + a_size;

    // Security/sanity checks
    if (((size_t)total_size != total_size) || (total_size >= (1ULL << 40))) {
      return VP8_STATUS_INVALID_PARAM;
    }

    buffer->private_memory = output = (uint8_t*)malloc((size_t)total_size);
    if (output == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }

    if (mode >= MODE_YUV) {   // YUVA initialization
      WebPYUVABuffer* const buf = &buffer->u.YUVA;
      buf->y = output;
      buf->y_stride = stride;
      buf->y_size = size;
      buf->u = output + size;
      buf->u_stride = uv_stride;
      buf->u_size = uv_size;
      buf->v = output + size + uv_size;
      buf->v_stride = uv_stride;
      buf->v_size = uv_size;
      if (mode == MODE_YUVA) {
        buf->a = output + size + 2 * uv_size;
      }
      buf->a_size = a_size;
      buf->a_stride = a_stride;
    } else {  // RGBA initialization
      WebPRGBABuffer* const buf = &buffer->u.RGBA;
      buf->rgba = output;
      buf->stride = stride;
      buf->size = size;
    }
  }
  return CheckDecBuffer(buffer);
}

VP8StatusCode WebPAllocateDecBuffer(int w, int h,
                                    const WebPDecoderOptions* const options,
                                    WebPDecBuffer* const out) {
  if (out == NULL || w <= 0 || h <= 0) {
    return VP8_STATUS_INVALID_PARAM;
  }
  if (options != NULL) {    // First, apply options if there is any.
    if (options->use_cropping) {
      const int cw = options->crop_width;
      const int ch = options->crop_height;
      const int x = options->crop_left & ~1;
      const int y = options->crop_top & ~1;
      if (x < 0 || y < 0 || cw <= 0 || ch <= 0 || x + cw > w || y + ch > h) {
        return VP8_STATUS_INVALID_PARAM;   // out of frame boundary.
      }
      w = cw;
      h = ch;
    }
    if (options->use_scaling) {
      if (options->scaled_width <= 0 || options->scaled_height <= 0) {
        return VP8_STATUS_INVALID_PARAM;
      }
      w  = options->scaled_width;
      h = options->scaled_height;
    }
  }
  out->width = w;
  out->height = h;

  // Then, allocate buffer for real
  return AllocateBuffer(out);
}

//-----------------------------------------------------------------------------
// constructors / destructors

int WebPInitDecBufferInternal(WebPDecBuffer* const buffer, int version) {
  if (version != WEBP_DECODER_ABI_VERSION) return 0;  // version mismatch
  if (!buffer) return 0;
  memset(buffer, 0, sizeof(*buffer));
  return 1;
}

void WebPFreeDecBuffer(WebPDecBuffer* const buffer) {
  if (buffer) {
    if (!buffer->is_external_memory)
      free(buffer->private_memory);
    buffer->private_memory = NULL;
  }
}

void WebPCopyDecBuffer(const WebPDecBuffer* const src,
                       WebPDecBuffer* const dst) {
  if (src && dst) {
    *dst = *src;
    if (src->private_memory) {
      dst->is_external_memory = 1;   // dst buffer doesn't own the memory.
      dst->private_memory = NULL;
    }
  }
}

// Copy and transfer ownership from src to dst (beware of parameter order!)
void WebPGrabDecBuffer(WebPDecBuffer* const src, WebPDecBuffer* const dst) {
  if (src && dst) {
    *dst = *src;
    if (src->private_memory) {
      src->is_external_memory = 1;   // src relinquishes ownership
      src->private_memory = NULL;
    }
  }
}

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
