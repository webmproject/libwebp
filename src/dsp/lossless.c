// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Image transforms and color space conversion methods for lossless decoder.
//
// Authors: Vikas Arora (vikaas.arora@gmail.com)
//          jyrki@google.com (Jyrki Alakuijala)
//          Urvang Joshi (urvang@google.com)

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <stdlib.h>
#include "./lossless.h"
#include "../dec/vp8li.h"

//------------------------------------------------------------------------------
// Inverse image transforms.

// In-place sum of each component with mod 256.
static WEBP_INLINE void AddPixelsEq(uint32_t* a, uint32_t b) {
  const uint32_t alpha_and_green = (*a & 0xff00ff00u) + (b & 0xff00ff00u);
  const uint32_t red_and_blue = (*a & 0x00ff00ffu) + (b & 0x00ff00ffu);
  *a = (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

static WEBP_INLINE uint32_t Average2(uint32_t a0, uint32_t a1) {
  return (((a0 ^ a1) & 0xfefefefeL) >> 1) + (a0 & a1);
}

static WEBP_INLINE uint32_t Average3(uint32_t a0, uint32_t a1, uint32_t a2) {
  return Average2(Average2(a0, a2), a1);
}

static WEBP_INLINE uint32_t Average4(uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3) {
  return Average2(Average2(a0, a1), Average2(a2, a3));
}

static WEBP_INLINE uint32_t Clip255(uint32_t a) {
  if (a < NUM_LITERAL_CODES) {
    return a;
  }
  // return 0, when a is a negative integer.
  // return 255, when a is positive.
  return ~a >> 24;
}

static WEBP_INLINE int AddSubtractComponentFull(int a, int b, int c) {
  return Clip255(a + b - c);
}

static WEBP_INLINE uint32_t ClampedAddSubtractFull(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const int a = AddSubtractComponentFull(c0 >> 24, c1 >> 24, c2 >> 24);
  const int r = AddSubtractComponentFull((c0 >> 16) & 0xff,
                                         (c1 >> 16) & 0xff,
                                         (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentFull((c0 >> 8) & 0xff,
                                         (c1 >> 8) & 0xff,
                                         (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentFull(c0 & 0xff, c1 & 0xff, c2 & 0xff);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

static WEBP_INLINE int AddSubtractComponentHalf(int a, int b) {
  return Clip255(a + (a - b) / 2);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const uint32_t ave = Average2(c0, c1);
  const int a = AddSubtractComponentHalf(ave >> 24, c2 >> 24);
  const int r = AddSubtractComponentHalf((ave >> 16) & 0xff, (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentHalf((ave >> 8) & 0xff, (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentHalf((ave >> 0) & 0xff, (c2 >> 0) & 0xff);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

static WEBP_INLINE int Sub3(int a, int b, int c) {
  const int pa = b - c;
  const int pb = a - c;
  return abs(pa) - abs(pb);
}

static WEBP_INLINE uint32_t Select(uint32_t a, uint32_t b, uint32_t c) {
  const int pa_minus_pb =
      Sub3((a >> 24)       , (b >> 24)       , (c >> 24)       ) +
      Sub3((a >> 16) & 0xff, (b >> 16) & 0xff, (c >> 16) & 0xff) +
      Sub3((a >>  8) & 0xff, (b >>  8) & 0xff, (c >>  8) & 0xff) +
      Sub3((a      ) & 0xff, (b      ) & 0xff, (c      ) & 0xff);

  return (pa_minus_pb <= 0) ? a : b;
}

//------------------------------------------------------------------------------
// Predictors

static void Predictor0(uint32_t* src, const uint32_t* top) {
  (void)top;
  AddPixelsEq(src, ARGB_BLACK);
}
static void Predictor1(uint32_t* src, const uint32_t* top) {
  (void)top;
  AddPixelsEq(src, src[-1]);  // left
}
static void Predictor2(uint32_t* src, const uint32_t* top) {
  AddPixelsEq(src, top[0]);
}
static void Predictor3(uint32_t* src, const uint32_t* top) {
  AddPixelsEq(src, top[1]);
}
static void Predictor4(uint32_t* src, const uint32_t* top) {
  AddPixelsEq(src, top[-1]);
}
static void Predictor5(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Average3(src[-1], top[0], top[1]);
  AddPixelsEq(src, pred);
}
static void Predictor6(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Average2(src[-1], top[-1]);
  AddPixelsEq(src, pred);
}
static void Predictor7(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Average2(src[-1], top[0]);
  AddPixelsEq(src, pred);
}
static void Predictor8(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Average2(top[-1], top[0]);
  AddPixelsEq(src, pred);
}
static void Predictor9(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Average2(top[0], top[1]);
  AddPixelsEq(src, pred);
}
static void Predictor10(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Average4(src[-1], top[-1], top[0], top[1]);
  AddPixelsEq(src, pred);
}
static void Predictor11(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = Select(top[0], src[-1], top[-1]);
  AddPixelsEq(src, pred);
}
static void Predictor12(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = ClampedAddSubtractFull(src[-1], top[0], top[-1]);
  AddPixelsEq(src, pred);
}
static void Predictor13(uint32_t* src, const uint32_t* top) {
  const uint32_t pred = ClampedAddSubtractHalf(src[-1], top[0], top[-1]);
  AddPixelsEq(src, pred);
}

typedef void (*PredictorFunc)(uint32_t* src, const uint32_t* top);
static const PredictorFunc kPredictors[16] = {
  Predictor0, Predictor1, Predictor2, Predictor3,
  Predictor4, Predictor5, Predictor6, Predictor7,
  Predictor8, Predictor9, Predictor10, Predictor11,
  Predictor12, Predictor13,
  Predictor0, Predictor0    // <- padding security sentinels
};

// Inverse prediction.
static void PredictorInverseTransform(const VP8LTransform* const transform,
                                      int y_start, int y_end, uint32_t* data) {
  const int width = transform->xsize_;
  if (y_start == 0) {  // First Row follows the L (mode=1) mode.
    int x;
    Predictor0(data, NULL);
    for (x = 1; x < width; ++x) {
      Predictor1(data + x, NULL);
    }
    data += width;
    ++y_start;
  }

  {
    int y = y_start;
    const int mask = (1 << transform->bits_) - 1;
    const int tiles_per_row = VP8LSubSampleSize(width, transform->bits_);
    const uint32_t* pred_mode_base =
        transform->data_ + (y >> transform->bits_) * tiles_per_row;

    while (y < y_end) {
      const uint32_t* pred_mode_src = pred_mode_base;
      PredictorFunc pred_func;
      int x;

      // First pixel follows the T (mode=2) mode.
      Predictor2(data, data - width);

      // .. the rest:
      pred_func = kPredictors[((*pred_mode_src++) >> 8) & 0xf];
      for (x = 1; x < width; ++x) {
        if ((x & mask) == 0) {    // start of tile. Read predictor function.
          pred_func = kPredictors[((*pred_mode_src++) >> 8) & 0xf];
        }
        pred_func(data + x, data + x - width);
      }
      data += width;
      ++y;
      if ((y & mask) == 0) {   // Use the same mask, since tiles are squares.
        pred_mode_base += tiles_per_row;
      }
    }
  }
}

// Add Green to Blue and Red channels (i.e. perform the inverse transform of
// 'Subtract Green').
static void AddGreenToBlueAndRed(const VP8LTransform* const transform,
                                 int y_start, int y_end, uint32_t* data) {
  const int width = transform->xsize_;
  const uint32_t* const data_end = data + (y_end - y_start) * width;
  while (data < data_end) {
    const uint32_t argb = *data;
    // "* 0001001u" is equivalent to "(green << 16) + green)"
    const uint32_t green = ((argb >> 8) & 0xff);
    uint32_t red_blue = (argb & 0x00ff00ffu);
    red_blue += (green << 16) | green;
    red_blue &= 0x00ff00ffu;
    *data++ = (argb & 0xff00ff00u) | red_blue;
  }
}

typedef struct {
  int green_to_red_;
  int green_to_blue_;
  int red_to_blue_;
} Multipliers;

static WEBP_INLINE uint32_t ColorTransformDelta(int8_t color_pred,
                                              int8_t color) {
  return (uint32_t)((int)(color_pred) * color) >> 5;
}

static WEBP_INLINE void ColorCodeToMultipliers(uint32_t color_code,
                                               Multipliers* const m) {
  m->green_to_red_  = (color_code >>  0) & 0xff;
  m->green_to_blue_ = (color_code >>  8) & 0xff;
  m->red_to_blue_   = (color_code >> 16) & 0xff;
}

static WEBP_INLINE void TransformColor(const Multipliers* const m,
                                       uint32_t* const argb) {
  const uint32_t green = *argb >> 8;
  const uint32_t red = *argb >> 16;
  uint32_t new_red = red;
  uint32_t new_blue = *argb;

  new_red += ColorTransformDelta(m->green_to_red_, green);
  new_red &= 0xff;
  new_blue += ColorTransformDelta(m->green_to_blue_, green);
  new_blue += ColorTransformDelta(m->red_to_blue_, new_red);
  new_blue &= 0xff;
  *argb = (*argb & 0xff00ff00u) | (new_red << 16) | (new_blue);
}

// Color space inverse transform.
static void ColorSpaceInverseTransform(const VP8LTransform* const transform,
                                       int y_start, int y_end, uint32_t* data) {
  const int width = transform->xsize_;
  const int mask = (1 << transform->bits_) - 1;
  const int tiles_per_row = VP8LSubSampleSize(width, transform->bits_);
  int y = y_start;
  const uint32_t* pred_row =
      transform->data_ + (y >> transform->bits_) * tiles_per_row;

  while (y < y_end) {
    const uint32_t* pred = pred_row;
    Multipliers m;
    int x;

    for (x = 0; x < width; ++x) {
      if ((x & mask) == 0) ColorCodeToMultipliers(*pred++, &m);
      TransformColor(&m, data + x);
    }
    data += width;
    ++y;
    if ((y & mask) == 0) pred_row += tiles_per_row;;
  }
}

// Separate out pixels packed together using pixel-bundling.
static void ColorIndexInverseTransform(
    const VP8LTransform* const transform,
    int y_start, int y_end,
    uint32_t* const data_in, uint32_t* const data_out) {
  int y;
  const int bits_per_pixel = 8 >> transform->bits_;
  const int width = transform->xsize_;
  const uint32_t* const color_map = transform->data_;
  uint32_t* dst = data_out;
  const uint32_t* src = data_in;
  if (bits_per_pixel < 8) {
    const int pixels_per_byte = 1 << transform->bits_;
    const int count_mask = pixels_per_byte - 1;
    const uint32_t bit_mask = (1 << bits_per_pixel) - 1;
    for (y = y_start; y < y_end; ++y) {
      uint32_t packed_pixels;
      int x;
      for (x = 0; x < width; ++x) {
        // We need to load fresh 'packed_pixels' once every 'bytes_per_pixels'
        // increments of x. Fortunately, pixels_per_byte is a power of 2, so
        // can just use a mask for that, instead of decrementing a counter.
        if ((x & count_mask) == 0) packed_pixels = ((*src++) >> 8) & 0xff;
        *dst++ = color_map[packed_pixels & bit_mask];
        packed_pixels >>= bits_per_pixel;
      }
    }
  } else {
    for (y = y_start; y < y_end; ++y) {
      int x;
      for (x = 0; x < width; ++x) {
        *dst++ = color_map[((*src++) >> 8) & 0xff];
      }
    }
  }
}

void VP8LInverseTransform(const VP8LTransform* const transform,
                          size_t row_start, size_t row_end,
                          uint32_t* const data_in, uint32_t* const data_out) {
  assert(row_start < row_end);
  assert(row_end <= transform->ysize_);
  switch (transform->type_) {
    case SUBTRACT_GREEN:
      AddGreenToBlueAndRed(transform, row_start, row_end, data_out);
      break;
    case PREDICTOR_TRANSFORM:
      PredictorInverseTransform(transform, row_start, row_end, data_out);
      if (row_end != transform->ysize_) {
        // The last predicted row in this iteration will be the top-pred row
        // for the first row in next iteration.
        const int width = transform->xsize_;
        memcpy(data_out - width, data_out + (row_end - row_start - 1) * width,
               width * sizeof(*data_out));
      }
      break;
    case CROSS_COLOR_TRANSFORM:
      ColorSpaceInverseTransform(transform, row_start, row_end, data_out);
      break;
    case COLOR_INDEXING_TRANSFORM:
      ColorIndexInverseTransform(transform, row_start, row_end,
                                 data_in, data_out);
      break;
  }
}

//------------------------------------------------------------------------------
// Color space conversion.

static int is_big_endian(void) {
  static const union {
    uint16_t w;
    uint8_t b[2];
  } tmp = { 1 };
  return (tmp.b[0] != 1);
}

static void ConvertBGRAToRGB(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
  }
}

static void ConvertBGRAToRGBA(const uint32_t* src,
                              int num_pixels, uint8_t* dst) {
  const uint32_t* src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >> 24) & 0xff;
  }
}

static void ConvertBGRAToBGR(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >> 16) & 0xff;
  }
}

static void CopyOrSwap(const uint32_t* src, int num_pixels, uint8_t* dst,
                       int swap_on_big_endian) {
  if (is_big_endian() == swap_on_big_endian) {
    const uint32_t* src_end = src + num_pixels;
    while (src < src_end) {
      uint32_t argb = *src++;
#if !defined(__BIG_ENDIAN__) && (defined(__i386__) || defined(__x86_64__))
      __asm__ volatile("bswap %0" : "=r"(argb) : "0"(argb));
      *(uint32_t*)dst = argb;
      dst += sizeof(argb);
#elif !defined(__BIG_ENDIAN__) && defined(_MSC_VER)
      argb = _byteswap_ulong(argb);
      *(uint32_t*)dst = argb;
      dst += sizeof(argb);
#else
      *dst++ = (argb >> 24) & 0xff;
      *dst++ = (argb >> 16) & 0xff;
      *dst++ = (argb >>  8) & 0xff;
      *dst++ = (argb >>  0) & 0xff;
#endif
    }
  } else {
    memcpy(dst, src, num_pixels * sizeof(*src));
  }
}

void VP8LConvertFromBGRA(const uint32_t* const in_data, int num_pixels,
                        WEBP_CSP_MODE out_colorspace,
                        uint8_t* const rgba) {
  switch (out_colorspace) {
    case MODE_RGB:
      ConvertBGRAToRGB(in_data, num_pixels, rgba);
      break;
    case MODE_RGBA:
      ConvertBGRAToRGBA(in_data, num_pixels, rgba);
      break;
    case MODE_BGR:
      ConvertBGRAToBGR(in_data, num_pixels, rgba);
      break;
    case MODE_BGRA:
      CopyOrSwap(in_data, num_pixels, rgba, 1);
      break;
    case MODE_ARGB:
      CopyOrSwap(in_data, num_pixels, rgba, 0);
      break;
    default:
      assert(0);          // Code flow should not reach here.
  }
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
