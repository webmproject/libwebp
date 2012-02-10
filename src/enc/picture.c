// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// WebPPicture utils: colorspace conversion, crop, ...
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "./vp8enci.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define HALVE(x) (((x) + 1) >> 1)

//------------------------------------------------------------------------------
// WebPPicture
//------------------------------------------------------------------------------

int WebPPictureAlloc(WebPPicture* const picture) {
  if (picture != NULL) {
    const WebPEncCSP uv_csp = picture->colorspace & WEBP_CSP_UV_MASK;
    const int has_alpha = picture->colorspace & WEBP_CSP_ALPHA_BIT;
    const int width = picture->width;
    const int height = picture->height;
    const int y_stride = width;
    const int uv_width = HALVE(width);
    const int uv_height = HALVE(height);
    const int uv_stride = uv_width;
    int uv0_stride = 0;
    int a_width, a_stride;
    uint64_t y_size, uv_size, uv0_size, a_size, total_size;
    uint8_t* mem;

    // U/V
    switch (uv_csp) {
      case WEBP_YUV420:
        break;
#ifdef WEBP_EXPERIMENTAL_FEATURES
      case WEBP_YUV400:    // for now, we'll just reset the U/V samples
        break;
      case WEBP_YUV422:
        uv0_stride = uv_width;
        break;
      case WEBP_YUV444:
        uv0_stride = width;
        break;
#endif
      default:
        return 0;
    }
    uv0_size = height * uv0_stride;

    // alpha
    a_width = has_alpha ? width : 0;
    a_stride = a_width;
    y_size = (uint64_t)y_stride * height;
    uv_size = (uint64_t)uv_stride * uv_height;
    a_size =  (uint64_t)a_stride * height;

    total_size = y_size + a_size + 2 * uv_size + 2 * uv0_size;

    // Security and validation checks
    if (width <= 0 || height <= 0 ||       // check for luma/alpha param error
        uv_width < 0 || uv_height < 0 ||   // check for u/v param error
        y_size >= (1ULL << 40) ||            // check for reasonable global size
        (size_t)total_size != total_size) {  // check for overflow on 32bit
      return 0;
    }
    picture->y_stride  = y_stride;
    picture->uv_stride = uv_stride;
    picture->a_stride  = a_stride;
    picture->uv0_stride  = uv0_stride;
    WebPPictureFree(picture);   // erase previous buffer
    mem = (uint8_t*)malloc((size_t)total_size);
    if (mem == NULL) return 0;

    picture->y = mem;
    mem += y_size;

    picture->u = mem;
    mem += uv_size;
    picture->v = mem;
    mem += uv_size;

    if (a_size) {
      picture->a = mem;
      mem += a_size;
    }
    if (uv0_size) {
      picture->u0 = mem;
      mem += uv0_size;
      picture->v0 = mem;
      mem += uv0_size;
    }
  }
  return 1;
}

// Grab the 'specs' (writer, *opaque, width, height...) from 'src' and copy them
// into 'dst'. Mark 'dst' as not owning any memory. 'src' can be NULL.
static void WebPPictureGrabSpecs(const WebPPicture* const src,
                                 WebPPicture* const dst) {
  if (src != NULL) *dst = *src;
  dst->y = dst->u = dst->v = NULL;
  dst->u0 = dst->v0 = NULL;
  dst->a = NULL;
}

// Release memory owned by 'picture'.
void WebPPictureFree(WebPPicture* const picture) {
  if (picture != NULL) {
    free(picture->y);
    WebPPictureGrabSpecs(NULL, picture);
  }
}

//------------------------------------------------------------------------------
// Picture copying

// Not worth moving to dsp/enc.c (only used here).
static void CopyPlane(const uint8_t* src, int src_stride,
                      uint8_t* dst, int dst_stride, int width, int height) {
  while (height-- > 0) {
    memcpy(dst, src, width);
    src += src_stride;
    dst += dst_stride;
  }
}

int WebPPictureCopy(const WebPPicture* const src, WebPPicture* const dst) {
  if (src == NULL || dst == NULL) return 0;
  if (src == dst) return 1;

  WebPPictureGrabSpecs(src, dst);
  if (!WebPPictureAlloc(dst)) return 0;

  CopyPlane(src->y, src->y_stride,
            dst->y, dst->y_stride, dst->width, dst->height);
  CopyPlane(src->u, src->uv_stride,
            dst->u, dst->uv_stride, HALVE(dst->width), HALVE(dst->height));
  CopyPlane(src->v, src->uv_stride,
            dst->v, dst->uv_stride, HALVE(dst->width), HALVE(dst->height));
  if (dst->a != NULL)  {
    CopyPlane(src->a, src->a_stride,
              dst->a, dst->a_stride, dst->width, dst->height);
  }
#ifdef WEBP_EXPERIMENTAL_FEATURES
  if (dst->u0 != NULL)  {
    int uv0_width = src->width;
    if ((dst->colorspace & WEBP_CSP_UV_MASK) == WEBP_YUV422) {
      uv0_width = HALVE(uv0_width);
    }
    CopyPlane(src->u0, src->uv0_stride,
              dst->u0, dst->uv0_stride, uv0_width, dst->height);
    CopyPlane(src->v0, src->uv0_stride,
              dst->v0, dst->uv0_stride, uv0_width, dst->height);
  }
#endif
  return 1;
}

//------------------------------------------------------------------------------
// Picture cropping

int WebPPictureCrop(WebPPicture* const pic,
                    int left, int top, int width, int height) {
  WebPPicture tmp;

  if (pic == NULL) return 0;
  if (width <= 0 || height <= 0) return 0;
  if (left < 0 || ((left + width + 1) & ~1) > pic->width) return 0;
  if (top < 0 || ((top + height + 1) & ~1) > pic->height) return 0;

  WebPPictureGrabSpecs(pic, &tmp);
  tmp.width = width;
  tmp.height = height;
  if (!WebPPictureAlloc(&tmp)) return 0;

  {
    const int y_offset = top * pic->y_stride + left;
    const int uv_offset = (top / 2) * pic->uv_stride + left / 2;
    CopyPlane(pic->y + y_offset, pic->y_stride,
              tmp.y, tmp.y_stride, width, height);
    CopyPlane(pic->u + uv_offset, pic->uv_stride,
              tmp.u, tmp.uv_stride, HALVE(width), HALVE(height));
    CopyPlane(pic->v + uv_offset, pic->uv_stride,
              tmp.v, tmp.uv_stride, HALVE(width), HALVE(height));
  }

  if (tmp.a != NULL) {
    const int a_offset = top * pic->a_stride + left;
    CopyPlane(pic->a + a_offset, pic->a_stride,
              tmp.a, tmp.a_stride, width, height);
  }
#ifdef WEBP_EXPERIMENTAL_FEATURES
  if (tmp.u0 != NULL) {
    int w = width;
    int l = left;
    if (tmp.colorspace == WEBP_YUV422) {
      w = HALVE(w);
      l = HALVE(l);
    }
    CopyPlane(pic->u0 + top * pic->uv0_stride + l, pic->uv0_stride,
              tmp.u0, tmp.uv0_stride, w, l);
    CopyPlane(pic->v0 + top * pic->uv0_stride + l, pic->uv0_stride,
              tmp.v0, tmp.uv0_stride, w, l);
  }
#endif

  WebPPictureFree(pic);
  *pic = tmp;
  return 1;
}

//------------------------------------------------------------------------------
// Simple picture rescaler

#define RFIX 30
#define MULT(x,y) (((int64_t)(x) * (y) + (1 << (RFIX - 1))) >> RFIX)
static WEBP_INLINE void ImportRow(const uint8_t* src, int src_width,
                                  int32_t* frow, int32_t* irow, int dst_width) {
  const int x_expand = (src_width < dst_width);
  const int fx_scale = (1 << RFIX) / dst_width;
  int x_in = 0;
  int x_out;
  int x_accum = 0;
  if (!x_expand) {
    int sum = 0;
    for (x_out = 0; x_out < dst_width; ++x_out) {
      x_accum += src_width - dst_width;
      for (; x_accum > 0; x_accum -= dst_width) {
        sum += src[x_in++];
      }
      {        // Emit next horizontal pixel.
        const int32_t base = src[x_in++];
        const int32_t frac = base * (-x_accum);
        frow[x_out] = (sum + base) * dst_width - frac;
        sum = MULT(frac, fx_scale);    // fresh fractional start for next pixel
      }
    }
  } else {        // simple bilinear interpolation
    int left = src[0], right = src[0];
    for (x_out = 0; x_out < dst_width; ++x_out) {
      if (x_accum < 0) {
        left = right;
        right = src[++x_in];
        x_accum += dst_width - 1;
      }
      frow[x_out] = right * (dst_width - 1) + (left - right) * x_accum;
      x_accum -= src_width - 1;
    }
  }
  // Accumulate the new row's contribution
  for (x_out = 0; x_out < dst_width; ++x_out) {
    irow[x_out] += frow[x_out];
  }
}

static void ExportRow(int32_t* frow, int32_t* irow, uint8_t* dst, int dst_width,
                      const int yscale, const int64_t fxy_scale) {
  int x_out;
  for (x_out = 0; x_out < dst_width; ++x_out) {
    const int frac = MULT(frow[x_out], yscale);
    const int v = (int)(MULT(irow[x_out] - frac, fxy_scale));
    dst[x_out] = (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
    irow[x_out] = frac;   // new fractional start
  }
}

static void RescalePlane(const uint8_t* src,
                         int src_width, int src_height, int src_stride,
                         uint8_t* dst,
                         int dst_width, int dst_height, int dst_stride,
                         int32_t* const work) {
  const int x_expand = (src_width < dst_width);
  const int fy_scale = (1 << RFIX) / dst_height;
  const int64_t fxy_scale = x_expand ?
      ((int64_t)dst_height << RFIX) / (dst_width * src_height) :
      ((int64_t)dst_height << RFIX) / (src_width * src_height);
  int y_accum = src_height;
  int y;
  int32_t* irow = work;              // integral contribution
  int32_t* frow = work + dst_width;  // fractional contribution

  memset(work, 0, 2 * dst_width * sizeof(*work));
  for (y = 0; y < src_height; ++y) {
    // import new contribution of one source row.
    ImportRow(src, src_width, frow, irow, dst_width);
    src += src_stride;
    // emit output row(s)
    y_accum -= dst_height;
    for (; y_accum <= 0; y_accum += src_height) {
      const int yscale = fy_scale * (-y_accum);
      ExportRow(frow, irow, dst, dst_width, yscale, fxy_scale);
      dst += dst_stride;
    }
  }
}
#undef MULT
#undef RFIX

int WebPPictureRescale(WebPPicture* const pic, int width, int height) {
  WebPPicture tmp;
  int prev_width, prev_height;
  int32_t* work;

  if (pic == NULL) return 0;
  prev_width = pic->width;
  prev_height = pic->height;
  // if width is unspecified, scale original proportionally to height ratio.
  if (width == 0) {
    width = (prev_width * height + prev_height / 2) / prev_height;
  }
  // if height is unspecified, scale original proportionally to width ratio.
  if (height == 0) {
    height = (prev_height * width + prev_width / 2) / prev_width;
  }
  // Check if the overall dimensions still make sense.
  if (width <= 0 || height <= 0) return 0;

  WebPPictureGrabSpecs(pic, &tmp);
  tmp.width = width;
  tmp.height = height;
  if (!WebPPictureAlloc(&tmp)) return 0;

  work = (int32_t*)malloc(2 * width * sizeof(int32_t));
  if (work == NULL) {
    WebPPictureFree(&tmp);
    return 0;
  }

  RescalePlane(pic->y, prev_width, prev_height, pic->y_stride,
               tmp.y, width, height, tmp.y_stride, work);
  RescalePlane(pic->u,
               HALVE(prev_width), HALVE(prev_height), pic->uv_stride,
               tmp.u,
               HALVE(width), HALVE(height), tmp.uv_stride, work);
  RescalePlane(pic->v,
               HALVE(prev_width), HALVE(prev_height), pic->uv_stride,
               tmp.v,
               HALVE(width), HALVE(height), tmp.uv_stride, work);

  if (tmp.a != NULL) {
    RescalePlane(pic->a, prev_width, prev_height, pic->a_stride,
                 tmp.a, width, height, tmp.a_stride, work);
  }
#ifdef WEBP_EXPERIMENTAL_FEATURES
  if (tmp.u0 != NULL) {
    int s = 1;
    if ((tmp.colorspace & WEBP_CSP_UV_MASK) == WEBP_YUV422) {
      s = 2;
    }
    RescalePlane(
        pic->u0, (prev_width + s / 2) / s, prev_height, pic->uv0_stride,
        tmp.u0, (width + s / 2) / s, height, tmp.uv0_stride, work);
    RescalePlane(
        pic->v0, (prev_width + s / 2) / s, prev_height, pic->uv0_stride,
        tmp.v0, (width + s / 2) / s, height, tmp.uv0_stride, work);
  }
#endif

  WebPPictureFree(pic);
  free(work);
  *pic = tmp;
  return 1;
}

//------------------------------------------------------------------------------
// Write-to-memory

typedef struct {
  uint8_t** mem;
  size_t    max_size;
  size_t*   size;
} WebPMemoryWriter;

static void WebPMemoryWriterInit(WebPMemoryWriter* const writer) {
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
  if (data_size > 0) {
    memcpy((*w->mem) + (*w->size), data, data_size);
    *w->size += data_size;
  }
  return 1;
}

//------------------------------------------------------------------------------
// RGB -> YUV conversion
// The exact naming is Y'CbCr, following the ITU-R BT.601 standard.
// More information at: http://en.wikipedia.org/wiki/YCbCr
// Y = 0.2569 * R + 0.5044 * G + 0.0979 * B + 16
// U = -0.1483 * R - 0.2911 * G + 0.4394 * B + 128
// V = 0.4394 * R - 0.3679 * G - 0.0715 * B + 128
// We use 16bit fixed point operations.

enum { YUV_FRAC = 16 };

static WEBP_INLINE int clip_uv(int v) {
   v = (v + (257 << (YUV_FRAC + 2 - 1))) >> (YUV_FRAC + 2);
   return ((v & ~0xff) == 0) ? v : (v < 0) ? 0 : 255;
}

static WEBP_INLINE int rgb_to_y(int r, int g, int b) {
  const int kRound = (1 << (YUV_FRAC - 1)) + (16 << YUV_FRAC);
  const int luma = 16839 * r + 33059 * g + 6420 * b;
  return (luma + kRound) >> YUV_FRAC;  // no need to clip
}

static WEBP_INLINE int rgb_to_u(int r, int g, int b) {
  return clip_uv(-9719 * r - 19081 * g + 28800 * b);
}

static WEBP_INLINE int rgb_to_v(int r, int g, int b) {
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

#define RGB_TO_UV0(x_in, x_out, y, SUM) {                \
  const int src = (step * (x_in) + (y) * rgb_stride);    \
  const int dst = (x_out) + (y) * picture->uv0_stride;   \
  const int r = SUM(r_ptr + src);                        \
  const int g = SUM(g_ptr + src);                        \
  const int b = SUM(b_ptr + src);                        \
  picture->u0[dst] = rgb_to_u(r, g, b);                  \
  picture->v0[dst] = rgb_to_v(r, g, b);                  \
}

static void MakeGray(WebPPicture* const picture) {
  int y;
  const int uv_width = HALVE(picture->width);
  const int uv_height = HALVE(picture->height);
  for (y = 0; y < uv_height; ++y) {
    memset(picture->u + y * picture->uv_stride, 128, uv_width);
    memset(picture->v + y * picture->uv_stride, 128, uv_width);
  }
}

static int Import(WebPPicture* const picture,
                  const uint8_t* const rgb, int rgb_stride,
                  int step, int swap_rb, int import_alpha) {
  const WebPEncCSP uv_csp = picture->colorspace & WEBP_CSP_UV_MASK;
  int x, y;
  const uint8_t* const r_ptr = rgb + (swap_rb ? 2 : 0);
  const uint8_t* const g_ptr = rgb + 1;
  const uint8_t* const b_ptr = rgb + (swap_rb ? 0 : 2);
  const int width = picture->width;
  const int height = picture->height;

  // Import luma plane
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      const int offset = step * x + y * rgb_stride;
      picture->y[x + y * picture->y_stride] =
        rgb_to_y(r_ptr[offset], g_ptr[offset], b_ptr[offset]);
    }
  }

  // Downsample U/V plane
  if (uv_csp != WEBP_YUV400) {
    for (y = 0; y < (height >> 1); ++y) {
      for (x = 0; x < (width >> 1); ++x) {
        RGB_TO_UV(x, y, SUM4);
      }
      if (picture->width & 1) {
        RGB_TO_UV(x, y, SUM2V);
      }
    }
    if (height & 1) {
      for (x = 0; x < (width >> 1); ++x) {
        RGB_TO_UV(x, y, SUM2H);
      }
      if (width & 1) {
        RGB_TO_UV(x, y, SUM1);
      }
    }

#ifdef WEBP_EXPERIMENTAL_FEATURES
    // Store original U/V samples too
    if (uv_csp == WEBP_YUV422) {
      for (y = 0; y < height; ++y) {
        for (x = 0; x < (width >> 1); ++x) {
          RGB_TO_UV0(2 * x, x, y, SUM2H);
        }
        if (width & 1) {
          RGB_TO_UV0(2 * x, x, y, SUM1);
        }
      }
    } else if (uv_csp == WEBP_YUV444) {
      for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
          RGB_TO_UV0(x, x, y, SUM1);
        }
      }
    }
#endif
  } else {
    MakeGray(picture);
  }

  if (import_alpha) {
    const uint8_t* const a_ptr = rgb + 3;
    assert(step >= 4);
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        picture->a[x + y * picture->a_stride] =
          a_ptr[step * x + y * rgb_stride];
      }
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
  picture->colorspace &= ~WEBP_CSP_ALPHA_BIT;
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgb, rgb_stride, 3, 0, 0);
}

int WebPPictureImportBGR(WebPPicture* const picture,
                         const uint8_t* const rgb, int rgb_stride) {
  picture->colorspace &= ~WEBP_CSP_ALPHA_BIT;
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgb, rgb_stride, 3, 1, 0);
}

int WebPPictureImportRGBA(WebPPicture* const picture,
                          const uint8_t* const rgba, int rgba_stride) {
  picture->colorspace |= WEBP_CSP_ALPHA_BIT;
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgba, rgba_stride, 4, 0, 1);
}

int WebPPictureImportBGRA(WebPPicture* const picture,
                          const uint8_t* const rgba, int rgba_stride) {
  picture->colorspace |= WEBP_CSP_ALPHA_BIT;
  if (!WebPPictureAlloc(picture)) return 0;
  return Import(picture, rgba, rgba_stride, 4, 1, 1);
}

//------------------------------------------------------------------------------
// Helper: clean up fully transparent area to help compressibility.

#define SIZE 8
#define SIZE2 (SIZE / 2)
static int is_transparent_area(const uint8_t* ptr, int stride, int size) {
  int y, x;
  for (y = 0; y < size; ++y) {
    for (x = 0; x < size; ++x) {
      if (ptr[x]) {
        return 0;
      }
    }
    ptr += stride;
  }
  return 1;
}

static WEBP_INLINE void flatten(uint8_t* ptr, int v, int stride, int size) {
  int y;
  for (y = 0; y < size; ++y) {
    memset(ptr, v, size);
    ptr += stride;
  }
}

void WebPCleanupTransparentArea(WebPPicture* const pic) {
  int x, y, w, h;
  const uint8_t* a_ptr;
  int values[3] = { 0 };

  if (pic == NULL) return;

  a_ptr = pic->a;
  if (a_ptr == NULL) return;    // nothing to do

  w = pic->width / SIZE;
  h = pic->height / SIZE;
  for (y = 0; y < h; ++y) {
    int need_reset = 1;
    for (x = 0; x < w; ++x) {
      const int off_a = (y * pic->a_stride + x) * SIZE;
      const int off_y = (y * pic->y_stride + x) * SIZE;
      const int off_uv = (y * pic->uv_stride + x) * SIZE2;
      if (is_transparent_area(a_ptr + off_a, pic->a_stride, SIZE)) {
        if (need_reset) {
          values[0] = pic->y[off_y];
          values[1] = pic->u[off_uv];
          values[2] = pic->v[off_uv];
          need_reset = 0;
        }
        flatten(pic->y + off_y, values[0], pic->y_stride, SIZE);
        flatten(pic->u + off_uv, values[1], pic->uv_stride, SIZE2);
        flatten(pic->v + off_uv, values[2], pic->uv_stride, SIZE2);
      } else {
        need_reset = 1;
      }
    }
    // ignore the left-overs on right/bottom
  }
}

#undef SIZE
#undef SIZE2

//------------------------------------------------------------------------------
// Distortion

// Max value returned in case of exact similarity.
static const double kMinDistortion_dB = 99.;

int WebPPictureDistortion(const WebPPicture* const pic1,
                          const WebPPicture* const pic2,
                          int type, float result[5]) {
  int c;
  DistoStats stats[5];
  int has_alpha;

  if (pic1 == NULL || pic2 == NULL ||
      pic1->width != pic2->width || pic1->height != pic2->height ||
      pic1->y == NULL || pic2->y == NULL ||
      pic1->u == NULL || pic2->u == NULL ||
      pic1->v == NULL || pic2->v == NULL ||
      result == NULL) {
    return 0;
  }

  has_alpha = !!(pic1->colorspace & WEBP_CSP_ALPHA_BIT);
  if (has_alpha != !!(pic2->colorspace & WEBP_CSP_ALPHA_BIT) ||
      (has_alpha && (pic1->a == NULL || pic2->a == NULL))) {
    return 0;
  }

  memset(stats, 0, sizeof(stats));
  VP8SSIMAccumulatePlane(pic1->y, pic1->y_stride,
                         pic2->y, pic2->y_stride,
                         pic1->width, pic1->height, &stats[0]);
  VP8SSIMAccumulatePlane(pic1->u, pic1->uv_stride,
                         pic2->u, pic2->uv_stride,
                         (pic1->width + 1) >> 1, (pic1->height + 1) >> 1,
                         &stats[1]);
  VP8SSIMAccumulatePlane(pic1->v, pic1->uv_stride,
                         pic2->v, pic2->uv_stride,
                         (pic1->width + 1) >> 1, (pic1->height + 1) >> 1,
                         &stats[2]);
  if (has_alpha) {
    VP8SSIMAccumulatePlane(pic1->a, pic1->a_stride,
                           pic2->a, pic2->a_stride,
                           pic1->width, pic1->height, &stats[3]);
  }
  for (c = 0; c <= 4; ++c) {
    if (type == 1) {
      const double v = VP8SSIMGet(&stats[c]);
      result[c] = (float)((v < 1.) ? -10.0 * log10(1. - v)
                                   : kMinDistortion_dB);
    } else {
      const double v = VP8SSIMGetSquaredError(&stats[c]);
      result[c] = (float)((v > 0.) ? -4.3429448 * log(v / (255 * 255.))
                                   : kMinDistortion_dB);
    }
    // Accumulate forward
    if (c < 4) VP8SSIMAddStats(&stats[c], &stats[4]);
  }
  return 1;
}

//------------------------------------------------------------------------------
// Simplest high-level calls:

typedef int (*Importer)(WebPPicture* const, const uint8_t* const, int);

static size_t Encode(const uint8_t* rgba, int width, int height, int stride,
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
  WebPMemoryWriterInit(&wrt);

  ok = import(&pic, rgba, stride) && WebPEncode(&config, &pic);
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

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
