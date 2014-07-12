// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebPPicture utils: colorspace conversion, crop, ...
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "./vp8enci.h"
#include "../utils/random.h"
#include "../utils/utils.h"
#include "../dsp/yuv.h"

// Uncomment to disable gamma-compression during RGB->U/V averaging
#define USE_GAMMA_COMPRESSION

#define HALVE(x) (((x) + 1) >> 1)

static const union {
  uint32_t argb;
  uint8_t  bytes[4];
} test_endian = { 0xff000000u };
#define ALPHA_IS_LAST (test_endian.bytes[3] == 0xff)

static WEBP_INLINE uint32_t MakeARGB32(int r, int g, int b) {
  return (0xff000000u | (r << 16) | (g << 8) | b);
}

//------------------------------------------------------------------------------
// WebPPicture
//------------------------------------------------------------------------------

int WebPPictureAlloc(WebPPicture* picture) {
  if (picture != NULL) {
    const WebPEncCSP uv_csp = picture->colorspace & WEBP_CSP_UV_MASK;
    const int has_alpha = picture->colorspace & WEBP_CSP_ALPHA_BIT;
    const int width = picture->width;
    const int height = picture->height;

    if (!picture->use_argb) {
      const int y_stride = width;
      const int uv_width = HALVE(width);
      const int uv_height = HALVE(height);
      const int uv_stride = uv_width;
      int a_width, a_stride;
      uint64_t y_size, uv_size, a_size, total_size;
      uint8_t* mem;

      // U/V
      switch (uv_csp) {
        case WEBP_YUV420:
          break;
        default:
          return 0;
      }

      // alpha
      a_width = has_alpha ? width : 0;
      a_stride = a_width;
      y_size = (uint64_t)y_stride * height;
      uv_size = (uint64_t)uv_stride * uv_height;
      a_size =  (uint64_t)a_stride * height;

      total_size = y_size + a_size + 2 * uv_size;

      // Security and validation checks
      if (width <= 0 || height <= 0 ||         // luma/alpha param error
          uv_width < 0 || uv_height < 0) {     // u/v param error
        return 0;
      }
      // Clear previous buffer and allocate a new one.
      WebPPictureFree(picture);   // erase previous buffer
      mem = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*mem));
      if (mem == NULL) return 0;

      // From now on, we're in the clear, we can no longer fail...
      picture->memory_ = (void*)mem;
      picture->y_stride  = y_stride;
      picture->uv_stride = uv_stride;
      picture->a_stride  = a_stride;

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
    } else {
      void* memory;
      const uint64_t argb_size = (uint64_t)width * height;
      if (width <= 0 || height <= 0) {
        return 0;
      }
      // Clear previous buffer and allocate a new one.
      WebPPictureFree(picture);   // erase previous buffer
      memory = WebPSafeMalloc(argb_size, sizeof(*picture->argb));
      if (memory == NULL) return 0;

      // TODO(skal): align plane to cache line?
      picture->memory_argb_ = memory;
      picture->argb = (uint32_t*)memory;
      picture->argb_stride = width;
    }
  }
  return 1;
}

// Remove reference to the ARGB buffer (doesn't free anything).
static void PictureResetARGB(WebPPicture* const picture) {
  picture->memory_argb_ = NULL;
  picture->argb = NULL;
  picture->argb_stride = 0;
}

// Remove reference to the YUVA buffer (doesn't free anything).
static void PictureResetYUVA(WebPPicture* const picture) {
  picture->memory_ = NULL;
  picture->y = picture->u = picture->v = picture->a = NULL;
  picture->y_stride = picture->uv_stride = 0;
  picture->a_stride = 0;
}

// Grab the 'specs' (writer, *opaque, width, height...) from 'src' and copy them
// into 'dst'. Mark 'dst' as not owning any memory.
void WebPPictureGrabSpecs(const WebPPicture* const src,
                          WebPPicture* const dst) {
  assert(src != NULL && dst != NULL);
  *dst = *src;
  PictureResetYUVA(dst);
  PictureResetARGB(dst);
}

// Allocate a new argb buffer, discarding any existing one and preserving
// the other YUV(A) buffer.
static int PictureAllocARGB(WebPPicture* const picture) {
  WebPPicture tmp;
  WebPSafeFree(picture->memory_argb_);
  PictureResetARGB(picture);
  picture->use_argb = 1;
  WebPPictureGrabSpecs(picture, &tmp);
  if (!WebPPictureAlloc(&tmp)) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  picture->memory_argb_ = tmp.memory_argb_;
  picture->argb = tmp.argb;
  picture->argb_stride = tmp.argb_stride;
  return 1;
}

// Release memory owned by 'picture' (both YUV and ARGB buffers).
void WebPPictureFree(WebPPicture* picture) {
  if (picture != NULL) {
    WebPSafeFree(picture->memory_);
    WebPSafeFree(picture->memory_argb_);
    PictureResetYUVA(picture);
    PictureResetARGB(picture);
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
    writer->mem = NULL;
    writer->size = 0;
    writer->max_size = 0;
  }
}

//------------------------------------------------------------------------------
// Detection of non-trivial transparency

// Returns true if alpha[] has non-0xff values.
static int CheckNonOpaque(const uint8_t* alpha, int width, int height,
                          int x_step, int y_step) {
  if (alpha == NULL) return 0;
  while (height-- > 0) {
    int x;
    for (x = 0; x < width * x_step; x += x_step) {
      if (alpha[x] != 0xff) return 1;  // TODO(skal): check 4/8 bytes at a time.
    }
    alpha += y_step;
  }
  return 0;
}

// Checking for the presence of non-opaque alpha.
int WebPPictureHasTransparency(const WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (!picture->use_argb) {
    return CheckNonOpaque(picture->a, picture->width, picture->height,
                          1, picture->a_stride);
  } else {
    int x, y;
    const uint32_t* argb = picture->argb;
    if (argb == NULL) return 0;
    for (y = 0; y < picture->height; ++y) {
      for (x = 0; x < picture->width; ++x) {
        if (argb[x] < 0xff000000u) return 1;   // test any alpha values != 0xff
      }
      argb += picture->argb_stride;
    }
  }
  return 0;
}

//------------------------------------------------------------------------------
// RGB -> YUV conversion

static int RGBToY(int r, int g, int b, VP8Random* const rg) {
  return VP8RGBToY(r, g, b, VP8RandomBits(rg, YUV_FIX));
}

static int RGBToU(int r, int g, int b, VP8Random* const rg) {
  return VP8RGBToU(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
}

static int RGBToV(int r, int g, int b, VP8Random* const rg) {
  return VP8RGBToV(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
}

//------------------------------------------------------------------------------

#if defined(USE_GAMMA_COMPRESSION)

// gamma-compensates loss of resolution during chroma subsampling
#define kGamma 0.80
#define kGammaFix 12     // fixed-point precision for linear values
#define kGammaScale ((1 << kGammaFix) - 1)
#define kGammaTabFix 7   // fixed-point fractional bits precision
#define kGammaTabScale (1 << kGammaTabFix)
#define kGammaTabRounder (kGammaTabScale >> 1)
#define kGammaTabSize (1 << (kGammaFix - kGammaTabFix))

static int kLinearToGammaTab[kGammaTabSize + 1];
static uint16_t kGammaToLinearTab[256];
static int kGammaTablesOk = 0;

static void InitGammaTables(void) {
  if (!kGammaTablesOk) {
    int v;
    const double scale = 1. / kGammaScale;
    for (v = 0; v <= 255; ++v) {
      kGammaToLinearTab[v] =
          (uint16_t)(pow(v / 255., kGamma) * kGammaScale + .5);
    }
    for (v = 0; v <= kGammaTabSize; ++v) {
      const double x = scale * (v << kGammaTabFix);
      kLinearToGammaTab[v] = (int)(pow(x, 1. / kGamma) * 255. + .5);
    }
    kGammaTablesOk = 1;
  }
}

static WEBP_INLINE uint32_t GammaToLinear(uint8_t v) {
  return kGammaToLinearTab[v];
}

// Convert a linear value 'v' to YUV_FIX+2 fixed-point precision
// U/V value, suitable for RGBToU/V calls.
static WEBP_INLINE int LinearToGamma(uint32_t base_value, int shift) {
  const int v = base_value << shift;              // final uplifted value
  const int tab_pos = v >> (kGammaTabFix + 2);    // integer part
  const int x = v & ((kGammaTabScale << 2) - 1);  // fractional part
  const int v0 = kLinearToGammaTab[tab_pos];
  const int v1 = kLinearToGammaTab[tab_pos + 1];
  const int y = v1 * x + v0 * ((kGammaTabScale << 2) - x);   // interpolate
  return (y + kGammaTabRounder) >> kGammaTabFix;             // descale
}

#else

static void InitGammaTables(void) {}
static WEBP_INLINE uint32_t GammaToLinear(uint8_t v) { return v; }
static WEBP_INLINE int LinearToGamma(uint32_t base_value, int shift) {
  return (int)(base_value << shift);
}

#endif    // USE_GAMMA_COMPRESSION

//------------------------------------------------------------------------------

#define SUM4(ptr) LinearToGamma(                         \
    GammaToLinear((ptr)[0]) +                            \
    GammaToLinear((ptr)[step]) +                         \
    GammaToLinear((ptr)[rgb_stride]) +                   \
    GammaToLinear((ptr)[rgb_stride + step]), 0)          \

#define SUM2H(ptr) \
    LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[step]), 1)
#define SUM2V(ptr) \
    LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[rgb_stride]), 1)
#define SUM1(ptr)  \
    LinearToGamma(GammaToLinear((ptr)[0]), 2)

#define RGB_TO_UV(x, y, SUM) {                           \
  const int src = (2 * (step * (x) + (y) * rgb_stride)); \
  const int dst = (x) + (y) * picture->uv_stride;        \
  const int r = SUM(r_ptr + src);                        \
  const int g = SUM(g_ptr + src);                        \
  const int b = SUM(b_ptr + src);                        \
  picture->u[dst] = RGBToU(r, g, b, &rg);                \
  picture->v[dst] = RGBToV(r, g, b, &rg);                \
}

static int ImportYUVAFromRGBA(const uint8_t* const r_ptr,
                              const uint8_t* const g_ptr,
                              const uint8_t* const b_ptr,
                              const uint8_t* const a_ptr,
                              int step,         // bytes per pixel
                              int rgb_stride,   // bytes per scanline
                              float dithering,
                              WebPPicture* const picture) {
  const WebPEncCSP uv_csp = picture->colorspace & WEBP_CSP_UV_MASK;
  int x, y;
  const int width = picture->width;
  const int height = picture->height;
  const int has_alpha = CheckNonOpaque(a_ptr, width, height, step, rgb_stride);
  VP8Random rg;

  picture->colorspace = uv_csp;
  picture->use_argb = 0;
  if (has_alpha) {
    picture->colorspace |= WEBP_CSP_ALPHA_BIT;
  }
  if (!WebPPictureAlloc(picture)) return 0;

  VP8InitRandom(&rg, dithering);
  InitGammaTables();

  // Import luma plane
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      const int offset = step * x + y * rgb_stride;
      picture->y[x + y * picture->y_stride] =
          RGBToY(r_ptr[offset], g_ptr[offset], b_ptr[offset], &rg);
    }
  }

  // Downsample U/V plane
  for (y = 0; y < (height >> 1); ++y) {
    for (x = 0; x < (width >> 1); ++x) {
      RGB_TO_UV(x, y, SUM4);
    }
    if (width & 1) {
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

  if (has_alpha) {
    assert(step >= 4);
    assert(picture->a != NULL);
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        picture->a[x + y * picture->a_stride] =
            a_ptr[step * x + y * rgb_stride];
      }
    }
  }
  return 1;
}

static int Import(WebPPicture* const picture,
                  const uint8_t* const rgb, int rgb_stride,
                  int step, int swap_rb, int import_alpha) {
  const uint8_t* const r_ptr = rgb + (swap_rb ? 2 : 0);
  const uint8_t* const g_ptr = rgb + 1;
  const uint8_t* const b_ptr = rgb + (swap_rb ? 0 : 2);
  const uint8_t* const a_ptr = import_alpha ? rgb + 3 : NULL;
  const int width = picture->width;
  const int height = picture->height;

  if (!picture->use_argb) {
    return ImportYUVAFromRGBA(r_ptr, g_ptr, b_ptr, a_ptr, step, rgb_stride,
                              0.f /* no dithering */, picture);
  }
  if (import_alpha) {
    picture->colorspace |= WEBP_CSP_ALPHA_BIT;
  } else {
    picture->colorspace &= ~WEBP_CSP_ALPHA_BIT;
  }
  if (!WebPPictureAlloc(picture)) return 0;

  if (!import_alpha) {
    int x, y;
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        const int offset = step * x + y * rgb_stride;
        const uint32_t argb =
            MakeARGB32(r_ptr[offset], g_ptr[offset], b_ptr[offset]);
        picture->argb[x + y * picture->argb_stride] = argb;
      }
    }
  } else {
    int x, y;
    assert(step >= 4);
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        const int offset = step * x + y * rgb_stride;
        const uint32_t argb = ((uint32_t)a_ptr[offset] << 24) |
                              (r_ptr[offset] << 16) |
                              (g_ptr[offset] <<  8) |
                              (b_ptr[offset]);
        picture->argb[x + y * picture->argb_stride] = argb;
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

//------------------------------------------------------------------------------

int WebPPictureImportRGB(WebPPicture* picture,
                         const uint8_t* rgb, int rgb_stride) {
  return Import(picture, rgb, rgb_stride, 3, 0, 0);
}

int WebPPictureImportBGR(WebPPicture* picture,
                         const uint8_t* rgb, int rgb_stride) {
  return Import(picture, rgb, rgb_stride, 3, 1, 0);
}

int WebPPictureImportRGBA(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return Import(picture, rgba, rgba_stride, 4, 0, 1);
}

int WebPPictureImportBGRA(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return Import(picture, rgba, rgba_stride, 4, 1, 1);
}

int WebPPictureImportRGBX(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return Import(picture, rgba, rgba_stride, 4, 0, 0);
}

int WebPPictureImportBGRX(WebPPicture* picture,
                          const uint8_t* rgba, int rgba_stride) {
  return Import(picture, rgba, rgba_stride, 4, 1, 0);
}

//------------------------------------------------------------------------------
// Automatic YUV <-> ARGB conversions.

int WebPPictureYUVAToARGB(WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (picture->y == NULL || picture->u == NULL || picture->v == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_ALPHA_BIT) && picture->a == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_UV_MASK) != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }
  // Allocate a new argb buffer (discarding the previous one).
  if (!PictureAllocARGB(picture)) return 0;

  // Convert
  {
    int y;
    const int width = picture->width;
    const int height = picture->height;
    const int argb_stride = 4 * picture->argb_stride;
    uint8_t* dst = (uint8_t*)picture->argb;
    const uint8_t *cur_u = picture->u, *cur_v = picture->v, *cur_y = picture->y;
    WebPUpsampleLinePairFunc upsample = WebPGetLinePairConverter(ALPHA_IS_LAST);

    // First row, with replicated top samples.
    upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    cur_y += picture->y_stride;
    dst += argb_stride;
    // Center rows.
    for (y = 1; y + 1 < height; y += 2) {
      const uint8_t* const top_u = cur_u;
      const uint8_t* const top_v = cur_v;
      cur_u += picture->uv_stride;
      cur_v += picture->uv_stride;
      upsample(cur_y, cur_y + picture->y_stride, top_u, top_v, cur_u, cur_v,
               dst, dst + argb_stride, width);
      cur_y += 2 * picture->y_stride;
      dst += 2 * argb_stride;
    }
    // Last row (if needed), with replicated bottom samples.
    if (height > 1 && !(height & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    }
    // Insert alpha values if needed, in replacement for the default 0xff ones.
    if (picture->colorspace & WEBP_CSP_ALPHA_BIT) {
      for (y = 0; y < height; ++y) {
        uint32_t* const argb_dst = picture->argb + y * picture->argb_stride;
        const uint8_t* const src = picture->a + y * picture->a_stride;
        int x;
        for (x = 0; x < width; ++x) {
          argb_dst[x] = (argb_dst[x] & 0x00ffffffu) | ((uint32_t)src[x] << 24);
        }
      }
    }
  }
  return 1;
}

int WebPPictureARGBToYUVADithered(WebPPicture* picture, WebPEncCSP colorspace,
                                  float dithering) {
  if (picture == NULL) return 0;
  if (picture->argb == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  } else {
    const uint8_t* const argb = (const uint8_t*)picture->argb;
    const uint8_t* const r = ALPHA_IS_LAST ? argb + 2 : argb + 1;
    const uint8_t* const g = ALPHA_IS_LAST ? argb + 1 : argb + 2;
    const uint8_t* const b = ALPHA_IS_LAST ? argb + 0 : argb + 3;
    const uint8_t* const a = ALPHA_IS_LAST ? argb + 3 : argb + 0;
    // We work on a tmp copy of 'picture', because ImportYUVAFromRGBA()
    // would be calling WebPPictureFree(picture) otherwise.
    WebPPicture tmp = *picture;
    PictureResetARGB(&tmp);  // reset ARGB buffer so that it's not free()'d.
    tmp.use_argb = 0;
    tmp.colorspace = colorspace & WEBP_CSP_UV_MASK;
    if (!ImportYUVAFromRGBA(r, g, b, a, 4, 4 * picture->argb_stride, dithering,
                            &tmp)) {
      return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    }
    // Copy back the YUV specs into 'picture'.
    tmp.argb = picture->argb;
    tmp.argb_stride = picture->argb_stride;
    tmp.memory_argb_ = picture->memory_argb_;
    *picture = tmp;
  }
  return 1;
}

int WebPPictureARGBToYUVA(WebPPicture* picture, WebPEncCSP colorspace) {
  return WebPPictureARGBToYUVADithered(picture, colorspace, 0.f);
}

//------------------------------------------------------------------------------
// Simplest high-level calls:

typedef int (*Importer)(WebPPicture* const, const uint8_t* const, int);

static size_t Encode(const uint8_t* rgba, int width, int height, int stride,
                     Importer import, float quality_factor, int lossless,
                     uint8_t** output) {
  WebPPicture pic;
  WebPConfig config;
  WebPMemoryWriter wrt;
  int ok;

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

#define ENCODE_FUNC(NAME, IMPORTER)                                     \
size_t NAME(const uint8_t* in, int w, int h, int bps, float q,          \
            uint8_t** out) {                                            \
  return Encode(in, w, h, bps, IMPORTER, q, 0, out);                    \
}

ENCODE_FUNC(WebPEncodeRGB, WebPPictureImportRGB)
ENCODE_FUNC(WebPEncodeBGR, WebPPictureImportBGR)
ENCODE_FUNC(WebPEncodeRGBA, WebPPictureImportRGBA)
ENCODE_FUNC(WebPEncodeBGRA, WebPPictureImportBGRA)

#undef ENCODE_FUNC

#define LOSSLESS_DEFAULT_QUALITY 70.
#define LOSSLESS_ENCODE_FUNC(NAME, IMPORTER)                                 \
size_t NAME(const uint8_t* in, int w, int h, int bps, uint8_t** out) {       \
  return Encode(in, w, h, bps, IMPORTER, LOSSLESS_DEFAULT_QUALITY, 1, out);  \
}

LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessRGB, WebPPictureImportRGB)
LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessBGR, WebPPictureImportBGR)
LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessRGBA, WebPPictureImportRGBA)
LOSSLESS_ENCODE_FUNC(WebPEncodeLosslessBGRA, WebPPictureImportBGRA)

#undef LOSSLESS_ENCODE_FUNC

//------------------------------------------------------------------------------
