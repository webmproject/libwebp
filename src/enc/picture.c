// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// WebPPicture utils: colorspace conversion, crop, ...
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include "vp8enci.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// WebPPicture
//-----------------------------------------------------------------------------

int WebPPictureAlloc(WebPPicture* const picture) {
  if (picture) {
    const int width = picture->width;
    const int height = picture->height;
    const int uv_width = (width + 1) / 2;
    const int uv_height = (height + 1) / 2;
    const uint64_t y_size = (uint64_t)width * height;
    const uint64_t uv_size = (uint64_t)uv_width * uv_height;
    const uint64_t total_size = y_size + 2 * uv_size;
    // Security and validation checks
    if (uv_width <= 0 || uv_height <= 0 ||   // check param error
        y_size >= (1ULL << 40) ||            // check for reasonable global size
        (size_t)total_size != total_size) {  // check for overflow on 32bit
      return 0;
    }
    picture->y_stride = width;
    picture->uv_stride = uv_width;
    WebPPictureFree(picture);   // erase previous buffer
    picture->y = (uint8_t*)malloc(total_size);
    if (picture->y == NULL) return 0;
    picture->u = picture->y + y_size;
    picture->v = picture->u + uv_size;
  }
  return 1;
}

void WebPPictureFree(WebPPicture* const picture) {
  if (picture) {
    free(picture->y);
    picture->y = picture->u = picture->v = NULL;
  }
}

//-----------------------------------------------------------------------------

int WebPPictureCopy(const WebPPicture* const src, WebPPicture* const dst) {
  int y;
  if (src == NULL || dst == NULL) return 0;
  if (src == dst) return 1;
  *dst = *src;
  dst->y = NULL;
  if (!WebPPictureAlloc(dst)) return 0;
  for (y = 0; y < dst->height; ++y) {
    memcpy(dst->y + y * dst->y_stride, src->y + y * src->y_stride, src->width);
  }
  for (y = 0; y < (dst->height + 1) / 2; ++y) {
    memcpy(dst->u + y * dst->uv_stride,
           src->u + y * src->uv_stride, (src->width + 1) / 2);
    memcpy(dst->v + y * dst->uv_stride,
           src->v + y * src->uv_stride, (src->width + 1) / 2);
  }
  return 1;
}

int WebPPictureCrop(WebPPicture* const pic,
                    int left, int top, int width, int height) {
  WebPPicture tmp;
  int y;

  if (pic == NULL) return 0;
  if (width <= 0 || height <= 0) return 0;
  if (left < 0 || ((left + width + 1) & ~1) > pic->width) return 0;
  if (top < 0 || ((top + height + 1) & ~1) > pic->height) return 0;

  tmp = *pic;
  tmp.y = NULL;
  tmp.width = width;
  tmp.height = height;
  if (!WebPPictureAlloc(&tmp)) return 0;

  for (y = 0; y < height; ++y) {
    memcpy(tmp.y + y * tmp.y_stride,
           pic->y + (top + y) * pic->y_stride + left, width);
  }
  for (y = 0; y < (height + 1) / 2; ++y) {
    const int offset = (y + top / 2) * pic->uv_stride + left / 2;
    memcpy(tmp.u + y * tmp.uv_stride, pic->u + offset, (width + 1) / 2);
    memcpy(tmp.v + y * tmp.uv_stride, pic->v + offset, (width + 1) / 2);
  }
  WebPPictureFree(pic);
  *pic = tmp;
  return 1;
}

//-----------------------------------------------------------------------------
// Write-to-memory

typedef struct {
  uint8_t** mem;
  size_t    max_size;
  size_t*   size;
} WebPMemoryWriter;

static void InitMemoryWriter(WebPMemoryWriter* const writer) {
  *writer->mem = NULL;
  *writer->size = 0;
  writer->max_size = 0;
}

static int WebPMemoryWrite(const uint8_t* data, size_t data_size,
                           const WebPPicture* const picture) {
  WebPMemoryWriter* const w = (WebPMemoryWriter*)picture->custom_ptr;
  size_t next_size;
  if (w == NULL) {
    return 1;
  }
  next_size = (*w->size) + data_size;
  if (next_size > w->max_size) {
    uint8_t* new_mem;
    size_t next_max_size = w->max_size * 2;
    if (next_max_size < next_size) next_max_size = next_size;
    if (next_max_size < 8192) next_max_size = 8192;
    new_mem = (uint8_t*)malloc(next_max_size);
    if (new_mem == NULL) {
      return 0;
    }
    if ((*w->size) > 0) {
      memcpy(new_mem, *w->mem, *w->size);
    }
    free(*w->mem);
    *w->mem = new_mem;
    w->max_size = next_max_size;
  }
  if (data_size) {
    memcpy((*w->mem) + (*w->size), data, data_size);
    *w->size += data_size;
  }
  return 1;
}

//-----------------------------------------------------------------------------
// RGB -> YUV conversion
// The exact naming is Y'CbCr, following the ITU-R BT.601 standard.
// More information at: http://en.wikipedia.org/wiki/YCbCr
// Y = 0.2569 * R + 0.5044 * G + 0.0979 * B + 16
// U = -0.1483 * R - 0.2911 * G + 0.4394 * B + 128
// V = 0.4394 * R - 0.3679 * G - 0.0715 * B + 128
// We use 16bit fixed point operations.

enum { YUV_FRAC = 16 };

static inline int clip_uv(int v) {
   v = (v + (257 << (YUV_FRAC + 2 - 1))) >> (YUV_FRAC + 2);
   return ((v & ~0xff) == 0) ? v : (v < 0) ? 0u : 255u;
}

static inline int rgb_to_y(int r, int g, int b) {
  const int kRound = (1 << (YUV_FRAC - 1)) + (16 << YUV_FRAC);
  const int luma = 16839 * r + 33059 * g + 6420 * b;
  return (luma + kRound) >> YUV_FRAC;  // no need to clip
}

static inline int rgb_to_u(int r, int g, int b) {
  return clip_uv(-9719 * r - 19081 * g + 28800 * b);
}

static inline int rgb_to_v(int r, int g, int b) {
  return clip_uv(+28800 * r - 24116 * g - 4684 * b);
}

// TODO: we can do better than simply 2x2 averaging on U/V samples.
#define SUM4(ptr) ((ptr)[0] + (ptr)[step] + \
                   (ptr)[rgb_stride] + (ptr)[rgb_stride + step])
#define SUM2H(ptr) (2 * (ptr)[0] + 2 * (ptr)[step])
#define SUM2V(ptr) (2 * (ptr)[0] + 2 * (ptr)[rgb_stride])
#define SUM1(ptr)  (4 * (ptr)[0])
#define RGB_TO_UV(x, y, SUM) {                           \
  const int src = (2 * (step * (x) + (y) * rgb_stride)); \
  const int dst = (x) + (y) * picture->uv_stride;        \
  const int r = SUM(r_ptr + src);                        \
  const int g = SUM(g_ptr + src);                        \
  const int b = SUM(b_ptr + src);                        \
  picture->u[dst] = rgb_to_u(r, g, b);                   \
  picture->v[dst] = rgb_to_v(r, g, b);                   \
}

static int Import(WebPPicture* const picture,
                  const uint8_t* const rgb, int rgb_stride,
                  int step, int swap) {
  int x, y;
  const uint8_t* const r_ptr = rgb + (swap ? 2 : 0);
  const uint8_t* const g_ptr = rgb + 1;
  const uint8_t* const b_ptr = rgb + (swap ? 0 : 2);

  for (y = 0; y < picture->height; ++y) {
    for (x = 0; x < picture->width; ++x) {
      const int offset = step * x + y * rgb_stride;
      picture->y[x + y * picture->y_stride] =
        rgb_to_y(r_ptr[offset], g_ptr[offset], b_ptr[offset]);
    }
  }
  for (y = 0; y < (picture->height >> 1); ++y) {
    for (x = 0; x < (picture->width >> 1); ++x) {
      RGB_TO_UV(x, y, SUM4);
    }
    if (picture->width & 1) {
      RGB_TO_UV(x, y, SUM2V);
    }
  }
  if (picture->height & 1) {
    for (x = 0; x < (picture->width >> 1); ++x) {
      RGB_TO_UV(x, y, SUM2H);
    }
    if (picture->width & 1) {
      RGB_TO_UV(x, y, SUM1);
    }
  }
  return 1;
}
#undef SUM4
#undef SUM2V
#undef SUM2H
#undef SUM1
#undef RGB_TO_UV

int WebPPictureImportRGB(WebPPicture* const picture,
                         const uint8_t* const rgb, int rgb_stride) {
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgb, rgb_stride, 3, 0);
}

int WebPPictureImportBGR(WebPPicture* const picture,
                         const uint8_t* const rgb, int rgb_stride) {
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgb, rgb_stride, 3, 1);
}

int WebPPictureImportRGBA(WebPPicture* const picture,
                          const uint8_t* const rgba, int rgba_stride) {
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgba, rgba_stride, 4, 0);
}

int WebPPictureImportBGRA(WebPPicture* const picture,
                          const uint8_t* const rgba, int rgba_stride) {
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgba, rgba_stride, 4, 1);
}

//-----------------------------------------------------------------------------
// Simplest call:

typedef int (*Importer)(WebPPicture* const, const uint8_t* const, int);

static size_t Encode(const uint8_t* rgb, int width, int height, int stride,
                     Importer import, float quality_factor, uint8_t** output) {
  size_t output_size = 0;
  WebPPicture pic;
  WebPConfig config;
  WebPMemoryWriter wrt;
  int ok;

  if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality_factor) ||
      !WebPPictureInit(&pic)) {
    return 0;  // shouldn't happen, except if system installation is broken
  }

  pic.width = width;
  pic.height = height;
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &wrt;

  wrt.mem = output;
  wrt.size = &output_size;
  InitMemoryWriter(&wrt);

  ok = import(&pic, rgb, stride) && WebPEncode(&config, &pic);
  WebPPictureFree(&pic);
  if (!ok) {
    free(*output);
    *output = NULL;
    return 0;
  }
  return output_size;
}

#define ENCODE_FUNC(NAME, IMPORTER) \
size_t NAME(const uint8_t* in, int w, int h, int bps, float q, \
            uint8_t** out) { \
  return Encode(in, w, h, bps, IMPORTER, q, out);  \
}

ENCODE_FUNC(WebPEncodeRGB, WebPPictureImportRGB);
ENCODE_FUNC(WebPEncodeBGR, WebPPictureImportBGR);
ENCODE_FUNC(WebPEncodeRGBA, WebPPictureImportRGBA);
ENCODE_FUNC(WebPEncodeBGRA, WebPPictureImportBGRA);

#undef ENCODE_FUNC

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
