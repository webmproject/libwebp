// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// functions for sample output.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include "../dec/vp8i.h"
#include "./webpi.h"
#include "../dsp/dsp.h"
#include "../dsp/yuv.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Main YUV<->RGB conversion functions

static int EmitYUV(const VP8Io* const io, WebPDecParams* const p) {
  WebPDecBuffer* output = p->output;
  const WebPYUVABuffer* const buf = &output->u.YUVA;
  uint8_t* const y_dst = buf->y + io->mb_y * buf->y_stride;
  uint8_t* const u_dst = buf->u + (io->mb_y >> 1) * buf->u_stride;
  uint8_t* const v_dst = buf->v + (io->mb_y >> 1) * buf->v_stride;
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  const int uv_w = (mb_w + 1) / 2;
  int j;
  for (j = 0; j < mb_h; ++j) {
    memcpy(y_dst + j * buf->y_stride, io->y + j * io->y_stride, mb_w);
  }
  for (j = 0; j < (mb_h + 1) / 2; ++j) {
    memcpy(u_dst + j * buf->u_stride, io->u + j * io->uv_stride, uv_w);
    memcpy(v_dst + j * buf->v_stride, io->v + j * io->uv_stride, uv_w);
  }
  return io->mb_h;
}

// Point-sampling U/V sampler.
static int EmitSampledRGB(const VP8Io* const io, WebPDecParams* const p) {
  WebPDecBuffer* output = p->output;
  const WebPRGBABuffer* const buf = &output->u.RGBA;
  uint8_t* dst = buf->rgba + io->mb_y * buf->stride;
  const uint8_t* y_src = io->y;
  const uint8_t* u_src = io->u;
  const uint8_t* v_src = io->v;
  const WebPSampleLinePairFunc sample = WebPSamplers[output->colorspace];
  const int mb_w = io->mb_w;
  const int last = io->mb_h - 1;
  int j;
  for (j = 0; j < last; j += 2) {
    sample(y_src, y_src + io->y_stride, u_src, v_src,
           dst, dst + buf->stride, mb_w);
    y_src += 2 * io->y_stride;
    u_src += io->uv_stride;
    v_src += io->uv_stride;
    dst += 2 * buf->stride;
  }
  if (j == last) {  // Just do the last line twice
    sample(y_src, y_src, u_src, v_src, dst, dst, mb_w);
  }
  return io->mb_h;
}

//------------------------------------------------------------------------------
// YUV444 -> RGB conversion

#if 0   // TODO(skal): this is for future rescaling.
static int EmitRGB(const VP8Io* const io, WebPDecParams* const p) {
  WebPDecBuffer* output = p->output;
  const WebPRGBABuffer* const buf = &output->u.RGBA;
  uint8_t* dst = buf->rgba + io->mb_y * buf->stride;
  const uint8_t* y_src = io->y;
  const uint8_t* u_src = io->u;
  const uint8_t* v_src = io->v;
  const WebPYUV444Converter convert = WebPYUV444Converters[output->colorspace];
  const int mb_w = io->mb_w;
  const int last = io->mb_h;
  int j;
  for (j = 0; j < last; ++j) {
    convert(y_src, u_src, v_src, dst, mb_w);
    y_src += io->y_stride;
    u_src += io->uv_stride;
    v_src += io->uv_stride;
    dst += buf->stride;
  }
  return io->mb_h;
}
#endif

//------------------------------------------------------------------------------
// Fancy upsampling

#ifdef FANCY_UPSAMPLING
static int EmitFancyRGB(const VP8Io* const io, WebPDecParams* const p) {
  int num_lines_out = io->mb_h;   // a priori guess
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + io->mb_y * buf->stride;
  const WebPUpsampleLinePairFunc upsample =
      io->a ? WebPUpsamplersKeepAlpha[p->output->colorspace]
            : WebPUpsamplers[p->output->colorspace];
  const uint8_t* cur_y = io->y;
  const uint8_t* cur_u = io->u;
  const uint8_t* cur_v = io->v;
  const uint8_t* top_u = p->tmp_u;
  const uint8_t* top_v = p->tmp_v;
  int y = io->mb_y;
  int y_end = io->mb_y + io->mb_h;
  const int mb_w = io->mb_w;
  const int uv_w = (mb_w + 1) / 2;

  if (y == 0) {
    // First line is special cased. We mirror the u/v samples at boundary.
    upsample(NULL, cur_y, cur_u, cur_v, cur_u, cur_v, NULL, dst, mb_w);
  } else {
    // We can finish the left-over line from previous call.
    // Warning! Don't overwrite the alpha values (if any), as they
    // are not lagging one line behind but are already written.
    upsample(p->tmp_y, cur_y, top_u, top_v, cur_u, cur_v,
             dst - buf->stride, dst, mb_w);
    num_lines_out++;
  }
  // Loop over each output pairs of row.
  for (; y + 2 < y_end; y += 2) {
    top_u = cur_u;
    top_v = cur_v;
    cur_u += io->uv_stride;
    cur_v += io->uv_stride;
    dst += 2 * buf->stride;
    cur_y += 2 * io->y_stride;
    upsample(cur_y - io->y_stride, cur_y,
             top_u, top_v, cur_u, cur_v,
             dst - buf->stride, dst, mb_w);
  }
  // move to last row
  cur_y += io->y_stride;
  if (io->crop_top + y_end < io->crop_bottom) {
    // Save the unfinished samples for next call (as we're not done yet).
    memcpy(p->tmp_y, cur_y, mb_w * sizeof(*p->tmp_y));
    memcpy(p->tmp_u, cur_u, uv_w * sizeof(*p->tmp_u));
    memcpy(p->tmp_v, cur_v, uv_w * sizeof(*p->tmp_v));
    // The fancy upsampler leaves a row unfinished behind
    // (except for the very last row)
    num_lines_out--;
  } else {
    // Process the very last row of even-sized picture
    if (!(y_end & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v,
              dst + buf->stride, NULL, mb_w);
    }
  }
  return num_lines_out;
}

#endif    /* FANCY_UPSAMPLING */

//------------------------------------------------------------------------------

#ifdef WEBP_EXPERIMENTAL_FEATURES
static int EmitAlphaYUV(const VP8Io* const io, WebPDecParams* const p) {
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  int j;
  const WebPYUVABuffer* const buf = &p->output->u.YUVA;
  uint8_t* dst = buf->a + io->mb_y * buf->a_stride;
  const uint8_t* alpha = io->a;
  if (alpha) {
    for (j = 0; j < mb_h; ++j) {
      memcpy(dst, alpha, mb_w * sizeof(*dst));
      alpha += io->width;
      dst += buf->a_stride;
    }
  }
  return 0;
}

static int EmitAlphaRGB(const VP8Io* const io, WebPDecParams* const p) {
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  int i, j;
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + io->mb_y * buf->stride;
  const uint8_t* alpha = io->a;
  if (alpha) {
    for (j = 0; j < mb_h; ++j) {
      for (i = 0; i < mb_w; ++i) {
        dst[4 * i + 3] = alpha[i];
      }
      alpha += io->width;
      dst += buf->stride;
    }
  }
  return 0;
}

#endif    /* WEBP_EXPERIMENTAL_FEATURES */

//------------------------------------------------------------------------------
// Simple picture rescaler

// TODO(skal): start a common library for encoder and decoder, and factorize
// this code in.

#define RFIX 30
#define MULT(x,y) (((int64_t)(x) * (y) + (1 << (RFIX - 1))) >> RFIX)

static void InitRescaler(WebPRescaler* const wrk,
                         int src_width, int src_height,
                         uint8_t* dst,
                         int dst_width, int dst_height, int dst_stride,
                         int x_add, int x_sub, int y_add, int y_sub,
                         int32_t* work) {
  wrk->x_expand = (src_width < dst_width);
  wrk->src_width = src_width;
  wrk->src_height = src_height;
  wrk->dst_width = dst_width;
  wrk->dst_height = dst_height;
  wrk->dst = dst;
  wrk->dst_stride = dst_stride;
  // for 'x_expand', we use bilinear interpolation
  wrk->x_add = wrk->x_expand ? (x_sub - 1) : x_add - x_sub;
  wrk->x_sub = wrk->x_expand ? (x_add - 1) : x_sub;
  wrk->y_accum = y_add;
  wrk->y_add = y_add;
  wrk->y_sub = y_sub;
  wrk->fx_scale = (1 << RFIX) / x_sub;
  wrk->fy_scale = (1 << RFIX) / y_sub;
  wrk->fxy_scale = wrk->x_expand ?
      ((int64_t)dst_height << RFIX) / (x_sub * src_height) :
      ((int64_t)dst_height << RFIX) / (x_add * src_height);
  wrk->irow = work;
  wrk->frow = work + dst_width;
}

static inline void ImportRow(const uint8_t* const src,
                             WebPRescaler* const wrk) {
  int x_in = 0;
  int x_out;
  int accum = 0;
  if (!wrk->x_expand) {
    int sum = 0;
    for (x_out = 0; x_out < wrk->dst_width; ++x_out) {
      accum += wrk->x_add;
      for (; accum > 0; accum -= wrk->x_sub) {
        sum += src[x_in++];
      }
      {        // Emit next horizontal pixel.
        const int32_t base = src[x_in++];
        const int32_t frac = base * (-accum);
        wrk->frow[x_out] = (sum + base) * wrk->x_sub - frac;
        // fresh fractional start for next pixel
        sum = MULT(frac, wrk->fx_scale);
      }
    }
  } else {        // simple bilinear interpolation
    int left = src[0], right = src[0];
    for (x_out = 0; x_out < wrk->dst_width; ++x_out) {
      if (accum < 0) {
        left = right;
        right = src[++x_in];
        accum += wrk->x_add;
      }
      wrk->frow[x_out] = right * wrk->x_add + (left - right) * accum;
      accum -= wrk->x_sub;
    }
  }
  // Accumulate the new row's contribution
  for (x_out = 0; x_out < wrk->dst_width; ++x_out) {
    wrk->irow[x_out] += wrk->frow[x_out];
  }
}

static void ExportRow(WebPRescaler* const wrk) {
  int x_out;
  const int yscale = wrk->fy_scale * (-wrk->y_accum);
  assert(wrk->y_accum <= 0);
  for (x_out = 0; x_out < wrk->dst_width; ++x_out) {
    const int frac = MULT(wrk->frow[x_out], yscale);
    const int v = (int)MULT(wrk->irow[x_out] - frac, wrk->fxy_scale);
    wrk->dst[x_out] = (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
    wrk->irow[x_out] = frac;   // new fractional start
  }
  wrk->y_accum += wrk->y_add;
  wrk->dst += wrk->dst_stride;
}

#undef MULT
#undef RFIX

//------------------------------------------------------------------------------
// YUV rescaling (no final RGB conversion needed)

static int Rescale(const uint8_t* src, int src_stride,
                   int new_lines, WebPRescaler* const wrk) {
  int num_lines_out = 0;
  while (new_lines-- > 0) {    // import new contribution of one source row.
    ImportRow(src, wrk);
    src += src_stride;
    wrk->y_accum -= wrk->y_sub;
    while (wrk->y_accum <= 0) {      // emit output row(s)
      ExportRow(wrk);
      num_lines_out++;
    }
  }
  return num_lines_out;
}

static int EmitRescaledYUV(const VP8Io* const io, WebPDecParams* const p) {
  const int mb_h = io->mb_h;
  const int uv_mb_h = (mb_h + 1) >> 1;
  const int num_lines_out = Rescale(io->y, io->y_stride, mb_h, &p->scaler_y);
  Rescale(io->u, io->uv_stride, uv_mb_h, &p->scaler_u);
  Rescale(io->v, io->uv_stride, uv_mb_h, &p->scaler_v);
  return num_lines_out;
}

static int EmitRescaledAlphaYUV(const VP8Io* const io, WebPDecParams* const p) {
  if (io->a) {
    Rescale(io->a, io->width, io->mb_h, &p->scaler_a);
  }
  return 0;
}

static int IsAlphaMode(WEBP_CSP_MODE mode) {
  return (mode == MODE_RGBA || mode == MODE_BGRA || mode == MODE_ARGB ||
          mode == MODE_RGBA_4444 || mode == MODE_YUVA);
}

static int InitYUVRescaler(const VP8Io* const io, WebPDecParams* const p) {
  const int has_alpha = IsAlphaMode(p->output->colorspace);
  const WebPYUVABuffer* const buf = &p->output->u.YUVA;
  const int out_width  = io->scaled_width;
  const int out_height = io->scaled_height;
  const int uv_out_width  = (out_width + 1) >> 1;
  const int uv_out_height = (out_height + 1) >> 1;
  const int uv_in_width  = (io->mb_w + 1) >> 1;
  const int uv_in_height = (io->mb_h + 1) >> 1;
  const size_t work_size = 2 * out_width;   // scratch memory for luma rescaler
  const size_t uv_work_size = 2 * uv_out_width;  // and for each u/v ones
  size_t tmp_size;
  int32_t* work;

  tmp_size = work_size + 2 * uv_work_size;
  if (has_alpha) {
    tmp_size += work_size;
  }
  p->memory = calloc(1, tmp_size * sizeof(*work));
  if (p->memory == NULL) {
    return 0;   // memory error
  }
  work = (int32_t*)p->memory;
  InitRescaler(&p->scaler_y, io->mb_w, io->mb_h,
               buf->y, out_width, out_height, buf->y_stride,
               io->mb_w, out_width, io->mb_h, out_height,
               work);
  InitRescaler(&p->scaler_u, uv_in_width, uv_in_height,
               buf->u, uv_out_width, uv_out_height, buf->u_stride,
               uv_in_width, uv_out_width,
               uv_in_height, uv_out_height,
               work + work_size);
  InitRescaler(&p->scaler_v, uv_in_width, uv_in_height,
               buf->v, uv_out_width, uv_out_height, buf->v_stride,
               uv_in_width, uv_out_width,
               uv_in_height, uv_out_height,
               work + work_size + uv_work_size);
  p->emit = EmitRescaledYUV;
  if (has_alpha) {
    InitRescaler(&p->scaler_a, io->mb_w, io->mb_h,
                 buf->a, out_width, out_height, buf->a_stride,
                 io->mb_w, out_width, io->mb_h, out_height,
                 work + work_size + 2 * uv_work_size);
    p->emit_alpha = EmitRescaledAlphaYUV;
  }
  return 1;
}

//------------------------------------------------------------------------------
// RGBA rescaling

// import new contributions until one row is ready to be output, or all input
// is consumed.
static int Import(const uint8_t* src, int src_stride,
                  int new_lines, WebPRescaler* const wrk) {
  int num_lines_in = 0;
  while (num_lines_in < new_lines && wrk->y_accum > 0) {
    ImportRow(src, wrk);
    src += src_stride;
    ++num_lines_in;
    wrk->y_accum -= wrk->y_sub;
  }
  return num_lines_in;
}

static int ExportRGB(WebPDecParams* const p, int y_pos) {
  const WebPYUV444Converter convert =
      WebPYUV444Converters[p->output->colorspace];
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + (p->last_y + y_pos) * buf->stride;
  int num_lines_out = 0;
  // For RGB rescaling, because of the YUV420, current scan position
  // U/V can be +1/-1 line from the Y one.  Hence the double test.
  while (p->scaler_y.y_accum <= 0 && p->scaler_u.y_accum <= 0) {
    assert(p->last_y + y_pos + num_lines_out < p->output->height);
    assert(p->scaler_u.y_accum == p->scaler_v.y_accum);
    ExportRow(&p->scaler_y);
    ExportRow(&p->scaler_u);
    ExportRow(&p->scaler_v);
    convert(p->scaler_y.dst, p->scaler_u.dst, p->scaler_v.dst,
            dst, p->scaler_y.dst_width);
    dst += buf->stride;
    num_lines_out++;
  }
  return num_lines_out;
}

static int EmitRescaledRGB(const VP8Io* const io, WebPDecParams* const p) {
  const int mb_h = io->mb_h;
  const int uv_mb_h = (mb_h + 1) >> 1;
  int j = 0, uv_j = 0;
  int num_lines_out = 0;
  while (j < mb_h) {
    const int y_lines_in = Import(io->y + j * io->y_stride, io->y_stride,
                                  mb_h - j, &p->scaler_y);
    const int u_lines_in = Import(io->u + uv_j * io->uv_stride, io->uv_stride,
                                  uv_mb_h - uv_j, &p->scaler_u);
    const int v_lines_in = Import(io->v + uv_j * io->uv_stride, io->uv_stride,
                                  uv_mb_h - uv_j, &p->scaler_v);
    (void)v_lines_in;   // remove a gcc warning
    assert(u_lines_in == v_lines_in);
    j += y_lines_in;
    uv_j += u_lines_in;
    num_lines_out += ExportRGB(p, num_lines_out);
  }
  return num_lines_out;
}

static int ExportAlpha(WebPDecParams* const p, int y_pos) {
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + (p->last_y + y_pos) * buf->stride;
  int num_lines_out = 0;
  while (p->scaler_a.y_accum <= 0) {
    int i;
    assert(p->last_y + y_pos + num_lines_out < p->output->height);
    ExportRow(&p->scaler_a);
    for (i = 0; i < p->scaler_a.dst_width; ++i) {
      dst[4 * i + 3] = p->scaler_a.dst[i];
    }
    dst += buf->stride;
    num_lines_out++;
  }
  return num_lines_out;
}

static int EmitRescaledAlphaRGB(const VP8Io* const io, WebPDecParams* const p) {
  if (io->a) {
    int j = 0, pos = 0;
    while (j < io->mb_h) {
      j += Import(io->a + j * io->width, io->width, io->mb_h - j, &p->scaler_a);
      pos += ExportAlpha(p, pos);
    }
  }
  return 0;
}

static int InitRGBRescaler(const VP8Io* const io, WebPDecParams* const p) {
  const int has_alpha = IsAlphaMode(p->output->colorspace);
  const int out_width  = io->scaled_width;
  const int out_height = io->scaled_height;
  const int uv_in_width  = (io->mb_w + 1) >> 1;
  const int uv_in_height = (io->mb_h + 1) >> 1;
  const size_t work_size = 2 * out_width;   // scratch memory for one rescaler
  int32_t* work;  // rescalers work area
  uint8_t* tmp;   // tmp storage for scaled YUV444 samples before RGB conversion
  size_t tmp_size1, tmp_size2;

  tmp_size1 = 3 * work_size;
  tmp_size2 = 3 * out_width;
  if (has_alpha) {
    tmp_size1 += work_size;
    tmp_size2 += out_width;
  }
  p->memory =
      calloc(1, tmp_size1 * sizeof(*work) + tmp_size2 * sizeof(*tmp));
  if (p->memory == NULL) {
    return 0;   // memory error
  }
  work = (int32_t*)p->memory;
  tmp = (uint8_t*)(work + tmp_size1);
  InitRescaler(&p->scaler_y, io->mb_w, io->mb_h,
               tmp + 0 * out_width, out_width, out_height, 0,
               io->mb_w, out_width, io->mb_h, out_height,
               work + 0 * work_size);
  InitRescaler(&p->scaler_u, uv_in_width, uv_in_height,
               tmp + 1 * out_width, out_width, out_height, 0,
               io->mb_w, 2 * out_width, io->mb_h, 2 * out_height,
               work + 1 * work_size);
  InitRescaler(&p->scaler_v, uv_in_width, uv_in_height,
               tmp + 2 * out_width, out_width, out_height, 0,
               io->mb_w, 2 * out_width, io->mb_h, 2 * out_height,
               work + 2 * work_size);
  p->emit = EmitRescaledRGB;

  if (has_alpha) {
    InitRescaler(&p->scaler_a, io->mb_w, io->mb_h,
                 tmp + 3 * out_width, out_width, out_height, 0,
                 io->mb_w, out_width, io->mb_h, out_height,
                 work + 3 * work_size);
    p->emit_alpha = EmitRescaledAlphaRGB;
  }
  return 1;
}

//------------------------------------------------------------------------------
// Default custom functions

// Setup crop_xxx fields, mb_w and mb_h
static int InitFromOptions(const WebPDecoderOptions* const options,
                           VP8Io* const io) {
  const int W = io->width;
  const int H = io->height;
  int x = 0, y = 0, w = W, h = H;

  // Cropping
  io->use_cropping = (options != NULL) && (options->use_cropping > 0);
  if (io->use_cropping) {
    w = options->crop_width;
    h = options->crop_height;
    // TODO(skal): take colorspace into account. Don't assume YUV420.
    x = options->crop_left & ~1;
    y = options->crop_top & ~1;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > W || y + h > H) {
      return 0;  // out of frame boundary error
    }
  }
  io->crop_left   = x;
  io->crop_top    = y;
  io->crop_right  = x + w;
  io->crop_bottom = y + h;
  io->mb_w = w;
  io->mb_h = h;

  // Scaling
  io->use_scaling = (options != NULL) && (options->use_scaling > 0);
  if (io->use_scaling) {
    if (options->scaled_width <= 0 || options->scaled_height <= 0) {
      return 0;
    }
    io->scaled_width = options->scaled_width;
    io->scaled_height = options->scaled_height;
  }

  // Filter
  io->bypass_filtering = options && options->bypass_filtering;

  // Fancy upsampler
#ifdef FANCY_UPSAMPLING
  io->fancy_upsampling = (options == NULL) || (!options->no_fancy_upsampling);
#endif

  if (io->use_scaling) {
    // disable filter (only for large downscaling ratio).
    io->bypass_filtering = (io->scaled_width < W * 3 / 4) &&
                           (io->scaled_height < H * 3 / 4);
    io->fancy_upsampling = 0;
  }
  return 1;
}

static int CustomSetup(VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  const int is_rgb = (p->output->colorspace < MODE_YUV);

  p->memory = NULL;
  p->emit = NULL;
  p->emit_alpha = NULL;
  if (!InitFromOptions(p->options, io)) {
    return 0;
  }

  if (io->use_scaling) {
    const int ok = is_rgb ? InitRGBRescaler(io, p) : InitYUVRescaler(io, p);
    if (!ok) {
      return 0;    // memory error
    }
  } else {
    if (is_rgb) {
      p->emit = EmitSampledRGB;   // default
#ifdef FANCY_UPSAMPLING
      if (io->fancy_upsampling) {
        const int uv_width = (io->mb_w + 1) >> 1;
        p->memory = malloc(io->mb_w + 2 * uv_width);
        if (p->memory == NULL) {
          return 0;   // memory error.
        }
        p->tmp_y = (uint8_t*)p->memory;
        p->tmp_u = p->tmp_y + io->mb_w;
        p->tmp_v = p->tmp_u + uv_width;
        p->emit = EmitFancyRGB;
        WebPInitUpsamplers();
      }
#endif
    } else {
      p->emit = EmitYUV;
    }
#ifdef WEBP_EXPERIMENTAL_FEATURES
    if (IsAlphaMode(p->output->colorspace)) {
      // We need transparency output
      p->emit_alpha = is_rgb ? EmitAlphaRGB : EmitAlphaYUV;
    }
#endif
  }

  if (is_rgb) {
    VP8YUVInit();
  }
  return 1;
}

//------------------------------------------------------------------------------

static int CustomPut(const VP8Io* io) {
  WebPDecParams* p = (WebPDecParams*)io->opaque;
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  int num_lines_out;
  assert(!(io->mb_y & 1));

  if (mb_w <= 0 || mb_h <= 0) {
    return 0;
  }
  num_lines_out = p->emit(io, p);
  if (p->emit_alpha) {
    p->emit_alpha(io, p);
  }
  p->last_y += num_lines_out;
  return 1;
}

//------------------------------------------------------------------------------

static void CustomTeardown(const VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  free(p->memory);
  p->memory = NULL;
}

//------------------------------------------------------------------------------
// Main entry point

void WebPInitCustomIo(WebPDecParams* const params, VP8Io* const io) {
  io->put      = CustomPut;
  io->setup    = CustomSetup;
  io->teardown = CustomTeardown;
  io->opaque   = params;
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
