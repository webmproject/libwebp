// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Main decoding functions for WEBP images.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include "vp8i.h"
#include "yuv.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define FANCY_UPSCALING   // undefined to remove fancy upscaling support

//-----------------------------------------------------------------------------
// RIFF layout is:
//   0ffset  tag
//   0...3   "RIFF" 4-byte tag
//   4...7   size of image data (including metadata) starting at offset 8
//   8...11  "WEBP"   our form-type signature
//   12..15  "VP8 ": 4-bytes tags, describing the raw video format used
//   16..19  size of the raw VP8 image data, starting at offset 20
//   20....  the VP8 bytes
// There can be extra chunks after the "VP8 " chunk (ICMT, ICOP, ...)
// All 32-bits sizes are in little-endian order.
// Note: chunk data must be padded to multiple of 2 in size

static inline uint32_t get_le32(const uint8_t* const data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// If a RIFF container is detected, validate it and skip over it.
static uint32_t CheckRIFFHeader(const uint8_t** data_ptr,
                                uint32_t *data_size_ptr) {
  uint32_t chunk_size = 0xffffffffu;
  if (*data_size_ptr >= 10 + 20 && !memcmp(*data_ptr, "RIFF", 4)) {
    if (memcmp(*data_ptr + 8, "WEBP", 4)) {
      return 0;  // wrong image file signature
    } else {
      const uint32_t riff_size = get_le32(*data_ptr + 4);
      if (memcmp(*data_ptr + 12, "VP8 ", 4)) {
        return 0;   // invalid compression format
      }
      chunk_size = get_le32(*data_ptr + 16);
      if ((chunk_size > riff_size + 8) || (chunk_size & 1)) {
        return 0;  // inconsistent size information.
      }
      // We have a IFF container. Skip it.
      *data_ptr += 20;
      *data_size_ptr -= 20;
    }
    return chunk_size;
  }
  return *data_size_ptr;
}

//-----------------------------------------------------------------------------
// Fancy upscaling

typedef enum { MODE_RGB = 0, MODE_RGBA = 1,
               MODE_BGR = 2, MODE_BGRA = 3,
               MODE_YUV = 4 } CSP_MODE;

#ifdef FANCY_UPSCALING

// Given samples laid out in a square as:
//  [a b]
//  [c d]
// we interpolate u/v as:
//  ([9*a + 3*b + 3*c +   d    3*a + 9*b + 3*c +   d] + [8 8]) / 16
//  ([3*a +   b + 9*c + 3*d      a + 3*b + 3*c + 9*d]   [8 8]) / 16
#define MIX_ODD(a, b, c, d)        \
  ((9 * (a) + 3 * ((b) + (c)) + (d) + 0x00080008u) >> 4)
#define MIX_EVEN(a, b, c, d)       \
  ((9 * (c) + 3 * ((d) + (a)) + (b) + 0x00080008u) >> 4)

// We process u and v together stashed into 32bit (16bit each).
// Note that we could store the pair (3*t_uv + uv, t_uv + 3*uv)
// instead of (t_uv, uv), into a 64bit variable. Doing so, we could
// simplify the MIXing a bit and save two multiplies. TODO(skal).
#define LOAD_UV(u,v) ((u) | ((v) << 16))

// Macro festival, so we can define all of rgb/bgr/rgba/bgra cases
// for odd and even lines
#define UPSCALE_FUNC(FUNC_NAME, MIX, FUNC, XSTEP)                        \
static void FUNC_NAME(const uint8_t* cur_y,                              \
                      const uint8_t* cur_u, const uint8_t* cur_v,        \
                      const uint8_t* top_u, const uint8_t* top_v,        \
                      int len, uint8_t* dst) {                           \
  int x;                                                                 \
  uint32_t tl_uv = LOAD_UV(top_u[0], top_v[0]);   /* top-left sample */  \
  uint32_t l_uv  = LOAD_UV(cur_u[0], cur_v[0]);   /* left-sample */      \
  uint32_t uv0 = MIX(tl_uv, tl_uv, l_uv, l_uv);                          \
  FUNC(cur_y[0], uv0 & 0xff, (uv0 >> 16), dst);                          \
  len -= 1;   /* first pixel is done. */                                 \
  for (x = 1; x <= (len >> 1); ++x) {                                    \
    const uint32_t t_uv = LOAD_UV(top_u[x], top_v[x]);  /* top sample */ \
    const uint32_t uv   = LOAD_UV(cur_u[x], cur_v[x]);  /* sample */     \
    const uint32_t uv0  = MIX(tl_uv, t_uv, l_uv, uv);                    \
    const uint32_t uv1  = MIX(t_uv, tl_uv, uv, l_uv);                    \
    FUNC(cur_y[2*x-1], uv0 & 0xff, (uv0 >> 16), dst + (2*x-1) * XSTEP);  \
    FUNC(cur_y[2*x  ], uv1 & 0xff, (uv1 >> 16), dst + (2*x  ) * XSTEP);  \
    tl_uv = t_uv;                                                        \
    l_uv = uv;                                                           \
  }                                                                      \
  if (len & 1) {                                                         \
    uv0 = MIX(tl_uv, tl_uv, l_uv, l_uv);                                 \
    FUNC(cur_y[len], uv0 & 0xff, (uv0 >> 16), dst + len * XSTEP);        \
  }                                                                      \
}                                                                        \

// All variants implemented.
UPSCALE_FUNC(UpscaleEvenRgb,  MIX_EVEN, VP8YuvToRgb,  3)
UPSCALE_FUNC(UpscaleOddRgb,   MIX_ODD,  VP8YuvToRgb,  3)
UPSCALE_FUNC(UpscaleEvenBgr,  MIX_EVEN, VP8YuvToBgr,  3)
UPSCALE_FUNC(UpscaleOddBgr,   MIX_ODD,  VP8YuvToBgr,  3)
UPSCALE_FUNC(UpscaleEvenRgba, MIX_EVEN, VP8YuvToRgba, 4)
UPSCALE_FUNC(UpscaleOddRgba,  MIX_ODD,  VP8YuvToRgba, 4)
UPSCALE_FUNC(UpscaleEvenBgra, MIX_EVEN, VP8YuvToBgra, 4)
UPSCALE_FUNC(UpscaleOddBgra,  MIX_ODD,  VP8YuvToBgra, 4)

// Main driver function.
static inline void UpscaleLine(const uint8_t* cur_y,
                               const uint8_t* cur_u, const uint8_t* cur_v,
                               const uint8_t* top_u, const uint8_t* top_v,
                               int len, uint8_t* dst, int odd, CSP_MODE mode) {
  if (odd) {
    if (mode == MODE_RGB) {
      UpscaleOddRgb(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    } else if (mode == MODE_BGR) {
      UpscaleOddBgr(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    } else if (mode == MODE_RGBA) {
      UpscaleOddRgba(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    } else {
      UpscaleOddBgra(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    }
  } else {
    if (mode == MODE_RGB) {
      UpscaleEvenRgb(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    } else if (mode == MODE_BGR) {
      UpscaleEvenBgr(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    } else if (mode == MODE_RGBA) {
      UpscaleEvenRgba(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    } else {
      UpscaleEvenBgra(cur_y, cur_u, cur_v, top_u, top_v, len, dst);
    }
  }
}
#undef LOAD_UV
#undef UPSCALE_FUNC
#undef MIX_ODD
#undef MIX_EVEN

#endif  // FANCY_UPSCALING

//-----------------------------------------------------------------------------
// Main conversion driver.

typedef struct {
  uint8_t* output;      // rgb(a) or luma
  uint8_t *u, *v;
  uint8_t *top_y, *top_u, *top_v;
  int stride;           // rgb(a) stride or luma stride
  int u_stride;
  int v_stride;
  CSP_MODE mode;
} Params;

static void CustomPut(const VP8Io* io) {
  Params *p = (Params*)io->opaque;
  const int w = io->width;
  const int mb_h = io->mb_h;
  const int uv_w = (w + 1) / 2;
  assert(!(io->mb_y & 1));

  if (p->mode == MODE_YUV) {
    uint8_t* const y_dst = p->output + io->mb_y * p->stride;
    uint8_t* const u_dst = p->u + (io->mb_y >> 1) * p->u_stride;
    uint8_t* const v_dst = p->v + (io->mb_y >> 1) * p->v_stride;
    int j;
    for (j = 0; j < mb_h; ++j) {
      memcpy(y_dst + j * p->stride, io->y + j * io->y_stride, w);
    }
    for (j = 0; j < (mb_h + 1) / 2; ++j) {
      memcpy(u_dst + j * p->u_stride, io->u + j * io->uv_stride, uv_w);
      memcpy(v_dst + j * p->v_stride, io->v + j * io->uv_stride, uv_w);
    }
  } else {
    uint8_t* dst = p->output + io->mb_y * p->stride;
    if (io->fancy_upscaling) {
#ifdef FANCY_UPSCALING
      const uint8_t* cur_y;
      const uint8_t* cur_u = io->u;
      const uint8_t* cur_v = io->v;
      const uint8_t* top_u = p->top_u;
      const uint8_t* top_v = p->top_v;
      int y = io->mb_y;
      int y_end = io->mb_y + io->mb_h - 1;
      if (y > 0) {
        // If mid-fly, we need to finish the previous line.
        cur_y = p->top_y;
        dst -= p->stride;
        y -= 1;
      } else {
        // else we "replicate" the u/v sample of the first line
        top_u = cur_u;
        top_v = cur_v;
        // and start with the top line
        cur_y = io->y;
      }
      if (y_end >= io->height - 1) {
        // for the very last rows, we can process them right now
        y_end = io->height;
      } else {
        // we won't process the very last line this time,
        // waiting for the next call instead.
      }

      // Loop over each output row.
      for (; y < y_end; ++y) {
        if (y & 1) {   // odd lines
          UpscaleLine(cur_y, cur_u, cur_v, top_u, top_v, w, dst, 1, p->mode);
        } else {       // even lines
          UpscaleLine(cur_y, cur_u, cur_v, top_u, top_v, w, dst, 0, p->mode);
          top_u = cur_u;
          top_v = cur_v;
          if (y < io->height - 2) {
            cur_u += io->uv_stride;
            cur_v += io->uv_stride;
          }
        }
        dst += p->stride;
        if (cur_y == p->top_y) {
          cur_y = io->y;
        } else {
          cur_y += io->y_stride;
        }
      }
      // Save the unfinished samples for next call (if we're not done yet).
      if (y < io->height - 1) {
        memcpy(p->top_y, cur_y, w * sizeof(*p->top_y));
        memcpy(p->top_u, top_u, uv_w * sizeof(*p->top_u));
        memcpy(p->top_v, top_v, uv_w * sizeof(*p->top_v));
      }
#else
      assert(0);  // shouldn't happen.
#endif
    } else {
      // Point-sampling U/V upscaler.
      // Could be implemented with special MIX functions, too.
      int j;
      for (j = 0; j < mb_h; ++j) {
        const uint8_t* y_src = io->y + j * io->y_stride;
        int i;
        for (i = 0; i < w; ++i) {
          const int y = y_src[i];
          const int u = io->u[(j / 2) * io->uv_stride + (i / 2)];
          const int v = io->v[(j / 2) * io->uv_stride + (i / 2)];
          if (p->mode == MODE_RGB) {
            VP8YuvToRgb(y, u, v, dst + i * 3);
          } else if (p->mode == MODE_BGR) {
            VP8YuvToBgr(y, u, v, dst + i * 3);
          } else if (p->mode == MODE_RGBA) {
            VP8YuvToRgba(y, u, v, dst + i * 4);
          } else {
            VP8YuvToBgra(y, u, v, dst + i * 4);
          }
        }
        dst += p->stride;
      }
    }
  }
}

//-----------------------------------------------------------------------------

static int CustomSetup(VP8Io* io) {
#ifdef FANCY_UPSCALING
  Params *p = (Params*)io->opaque;
  p->top_y = p->top_u = p->top_v = NULL;
  if (p->mode != MODE_YUV) {
    const int uv_width = (io->width + 1) >> 1;
    p->top_y = (uint8_t*)malloc(io->width + 2 * uv_width);
    if (p->top_y == NULL) {
      return 0;   // memory error.
    }
    p->top_u = p->top_y + io->width;
    p->top_v = p->top_u + uv_width;
    io->fancy_upscaling = 1;  // activate fancy upscaling
  }
#endif
  return 1;
}

static void CustomTeardown(const VP8Io* io) {
#ifdef FANCY_UPSCALING
  Params *p = (Params*)io->opaque;
  if (p->top_y) {
    free(p->top_y);
    p->top_y = p->top_u = p->top_v = NULL;
  }
#endif
}

//-----------------------------------------------------------------------------
// "Into" variants

static uint8_t* DecodeInto(CSP_MODE mode,
                           const uint8_t* data, uint32_t data_size,
                           Params* params, int output_size,
                           int output_u_size, int output_v_size) {
  VP8Decoder* dec = VP8New();
  VP8Io io;
  int ok = 1;

  if (dec == NULL) {
    return NULL;
  }

  VP8InitIo(&io);
  io.data = data;
  io.data_size = data_size;

  params->mode = mode;
  io.opaque = params;
  io.put = CustomPut;
  io.setup = CustomSetup;
  io.teardown = CustomTeardown;

  if (!VP8GetHeaders(dec, &io)) {
    VP8Delete(dec);
    return NULL;
  }
  // check output buffers

  ok &= (params->stride * io.height <= output_size);
  if (mode == MODE_RGB || mode == MODE_BGR) {
    ok &= (params->stride >= io.width * 3);
  } else if (mode == MODE_RGBA || mode == MODE_BGRA) {
    ok &= (params->stride >= io.width * 4);
  } else {
    // some extra checks for U/V
    const int u_size = params->u_stride * ((io.height + 1) / 2);
    const int v_size = params->v_stride * ((io.height + 1) / 2);
    ok &= (params->stride >= io.width);
    ok &= (params->u_stride >= (io.width + 1) / 2) &&
          (params->v_stride >= (io.width + 1) / 2);
    ok &= (u_size <= output_u_size && v_size <= output_v_size);
  }
  if (!ok) {
    VP8Delete(dec);
    return NULL;
  }

  if (mode != MODE_YUV) {
    VP8YUVInit();
  }

  ok = VP8Decode(dec, &io);
  VP8Delete(dec);
  return ok ? params->output : NULL;
}

uint8_t* WebPDecodeRGBInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* output, int output_size,
                           int output_stride) {
  Params params;

  if (output == NULL) {
    return NULL;
  }

  params.output = output;
  params.stride = output_stride;
  return DecodeInto(MODE_RGB, data, data_size, &params, output_size, 0, 0);
}

uint8_t* WebPDecodeRGBAInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output, int output_size,
                            int output_stride) {
  Params params;

  if (output == NULL) {
    return NULL;
  }

  params.output = output;
  params.stride = output_stride;
  return DecodeInto(MODE_RGBA, data, data_size, &params, output_size, 0, 0);
}

uint8_t* WebPDecodeBGRInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* output, int output_size,
                           int output_stride) {
  Params params;

  if (output == NULL) {
    return NULL;
  }

  params.output = output;
  params.stride = output_stride;
  return DecodeInto(MODE_BGR, data, data_size, &params, output_size, 0, 0);
}

uint8_t* WebPDecodeBGRAInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output, int output_size,
                            int output_stride) {
  Params params;

  if (output == NULL) {
    return NULL;
  }

  params.output = output;
  params.stride = output_stride;
  return DecodeInto(MODE_BGRA, data, data_size, &params, output_size, 0, 0);
}

uint8_t* WebPDecodeYUVInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* luma, int luma_size, int luma_stride,
                           uint8_t* u, int u_size, int u_stride,
                           uint8_t* v, int v_size, int v_stride) {
  Params params;

  if (luma == NULL) {
    return NULL;
  }

  params.output = luma;
  params.stride = luma_stride;
  params.u = u;
  params.u_stride = u_stride;
  params.v = v;
  params.v_stride = v_stride;
  return DecodeInto(MODE_YUV, data, data_size, &params,
                    luma_size, u_size, v_size);
}

//-----------------------------------------------------------------------------

static uint8_t* Decode(CSP_MODE mode, const uint8_t* data, uint32_t data_size,
                       int* width, int* height, Params* params_out) {
  int w, h, stride;
  int uv_size = 0;
  int uv_stride = 0;
  int size;
  uint8_t* output;
  Params params = { 0 };

  if (!WebPGetInfo(data, data_size, &w, &h)) {
    return NULL;
  }
  if (width) *width = w;
  if (height) *height = h;

  // initialize output buffer, now that dimensions are known.
  stride = (mode == MODE_RGB || mode == MODE_BGR) ? 3 * w
             : (mode == MODE_RGBA || mode == MODE_BGRA) ? 4 * w
             : w;
  size = stride * h;

  if (mode == MODE_YUV) {
    uv_stride = (w + 1) / 2;
    uv_size = uv_stride * ((h + 1) / 2);
  }

  output = (uint8_t*)malloc(size + 2 * uv_size);
  if (!output) {
    return NULL;
  }

  params.output = output;
  params.stride = stride;
  if (mode == MODE_YUV) {
    params.u = output + size;
    params.u_stride = uv_stride;
    params.v = output + size + uv_size;
    params.v_stride = uv_stride;
  }
  if (params_out) *params_out = params;
  return DecodeInto(mode, data, data_size, &params, size, uv_size, uv_size);
}

uint8_t* WebPDecodeRGB(const uint8_t* data, uint32_t data_size,
                       int *width, int *height) {
  return Decode(MODE_RGB, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeRGBA(const uint8_t* data, uint32_t data_size,
                        int *width, int *height) {
  return Decode(MODE_RGBA, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeBGR(const uint8_t* data, uint32_t data_size,
                       int *width, int *height) {
  return Decode(MODE_BGR, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeBGRA(const uint8_t* data, uint32_t data_size,
                        int *width, int *height) {
  return Decode(MODE_BGRA, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeYUV(const uint8_t* data, uint32_t data_size,
                       int *width, int *height, uint8_t** u, uint8_t** v,
                       int *stride, int* uv_stride) {
  Params params;
  uint8_t* const out = Decode(MODE_YUV, data, data_size,
                              width, height, &params);

  if (out) {
    *u = params.u;
    *v = params.v;
    *stride = params.stride;
    *uv_stride = params.u_stride;
    assert(params.u_stride == params.v_stride);
  }
  return out;
}

//-----------------------------------------------------------------------------
// WebPGetInfo()

int WebPGetInfo(const uint8_t* data, uint32_t data_size,
                int *width, int *height) {
  const uint32_t chunk_size = CheckRIFFHeader(&data, &data_size);
  if (!chunk_size) {
    return 0;         // unsupported RIFF header
  }
  // Validate raw video data
  if (data_size < 10) {
    return 0;         // not enough data
  }
  // check signature
  if (data[3] != 0x9d || data[4] != 0x01 || data[5] != 0x2a) {
    return 0;         // Wrong signature.
  } else {
    const uint32_t bits = data[0] | (data[1] << 8) | (data[2] << 16);
    const int key_frame = !(bits & 1);
    const int w = ((data[7] << 8) | data[6]) & 0x3fff;
    const int h = ((data[9] << 8) | data[8]) & 0x3fff;

    if (!key_frame) {   // Not a keyframe.
      return 0;
    }

    if (((bits >> 1) & 7) > 3) {
      return 0;         // unknown profile
    }
    if (!((bits >> 4) & 1)) {
      return 0;         // first frame is invisible!
    }
    if (((bits >> 5)) >= chunk_size) { // partition_length
      return 0;         // inconsistent size information.
    }

    if (width) {
      *width = w;
    }
    if (height) {
      *height = h;
    }

    return 1;
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
