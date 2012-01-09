// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Spatial prediction using various filters
//
// Author: Urvang (urvang@google.com)

#include "./filters.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Helpful macro.

# define SANITY_CHECK(in, out)                              \
  assert(in != NULL);                                       \
  assert(out != NULL);                                      \
  assert(width > 0);                                        \
  assert(height > 0);                                       \
  assert(bpp > 0);                                          \
  assert(stride >= width * bpp);

//------------------------------------------------------------------------------
// Horizontal filter.

static void HorizontalFilter(const uint8_t* data, int width, int height,
                             int bpp, int stride, uint8_t* filtered_data) {
  int h;
  SANITY_CHECK(data, filtered_data);

  // Filter line-by-line.
  for (h = 0; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = filtered_data + h * stride;

    memcpy((void*)out, (const void*)scan_line, bpp);
    for (w = bpp; w < width * bpp; ++w) {
      out[w] = scan_line[w] - scan_line[w - bpp];
    }
  }
}

static void HorizontalUnfilter(const uint8_t* data, int width, int height,
                               int bpp, int stride, uint8_t* recon_data) {
  int h;
  SANITY_CHECK(data, recon_data);

  // Unfilter line-by-line.
  for (h = 0; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = recon_data + h * stride;

    memcpy((void*)out, (const void*)scan_line, bpp);
    for (w = bpp; w < width * bpp; ++w) {
      out[w] = scan_line[w] + out[w - bpp];
    }
  }
}

//------------------------------------------------------------------------------
// Vertical filter.

static void VerticalFilter(const uint8_t* data, int width, int height,
                           int bpp, int stride, uint8_t* filtered_data) {
  int h;
  SANITY_CHECK(data, filtered_data);

  // Copy top scan-line as it is.
  memcpy((void*)filtered_data, (const void*)data, width * bpp);

  // Filter line-by-line.
  for (h = 1; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = filtered_data + h * stride;
    const uint8_t* const prev_line = scan_line - stride;
    for (w = 0; w < width * bpp; ++w) {
      out[w] = scan_line[w] - prev_line[w];
    }
  }
}

static void VerticalUnfilter(const uint8_t* data, int width, int height,
                             int bpp, int stride, uint8_t* recon_data) {
  int h;
  SANITY_CHECK(data, recon_data);

  // Copy top scan-line as it is.
  memcpy((void*)recon_data, (const void*)data, width * bpp);

  // Unfilter line-by-line.
  for (h = 1; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = recon_data + h * stride;
    const uint8_t* const out_prev_line = out - stride;
    for (w = 0; w < width * bpp; ++w) {
      out[w] = scan_line[w] + out_prev_line[w];
    }
  }
}

//------------------------------------------------------------------------------
// Gradient filter.

static WEBP_INLINE uint8_t GradientPredictor(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return (g < 0) ? 0 : (g > 255) ? 255 : (uint8_t)g;
}

static void GradientFilter(const uint8_t* data, int width, int height,
                           int bpp, int stride, uint8_t* filtered_data) {
  int h;
  SANITY_CHECK(data, filtered_data);

  // Copy top scan-line as it is.
  memcpy((void*)filtered_data, (const void*)data, width * bpp);

  // Filter line-by-line.
  for (h = 1; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = filtered_data + h * stride;
    const uint8_t* const prev_line = scan_line - stride;
    memcpy((void*)out, (const void*)scan_line, bpp);
    for (w = bpp; w < width * bpp; ++w) {
      out[w] = scan_line[w] - GradientPredictor(scan_line[w - bpp],
                                                prev_line[w],
                                                prev_line[w - bpp]);
    }
  }
}

static void GradientUnfilter(const uint8_t* data, int width, int height,
                             int bpp, int stride, uint8_t* recon_data) {
  int h;
  SANITY_CHECK(data, recon_data);

  // Copy top scan-line as it is.
  memcpy((void*)recon_data, (const void*)data, width * bpp);

  // Unfilter line-by-line.
  for (h = 1; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = recon_data + h * stride;
    const uint8_t* const out_prev_line = out - stride;
    memcpy((void*)out, (const void*)scan_line, bpp);
    for (w = bpp; w < width * bpp; ++w) {
      out[w] = scan_line[w] + GradientPredictor(out[w - bpp],
                                                out_prev_line[w],
                                                out_prev_line[w - bpp]);
    }
  }
}

#undef SANITY_CHECK

// -----------------------------------------------------------------------------
// Quick estimate of a potentially interesting filter mode to try, in addition
// to the default NONE.

#define SMAX 16
#define SDIFF(a, b) (abs((a) - (b)) >> 4)   // Scoring diff, in [0..SMAX)

WEBP_FILTER_TYPE EstimateBestFilter(const uint8_t* data,
                                    int width, int height, int stride) {
  int i, j;
  int bins[WEBP_FILTER_LAST][SMAX];
  memset(bins, 0, sizeof(bins));
  // We only sample every other pixels. That's enough.
  for (j = 2; j < height - 1; j += 2) {
    const uint8_t* const p = data + j * stride;
    int mean = p[0];
    for (i = 2; i < width - 1; i += 2) {
      const int diff0 = SDIFF(p[i], mean);
      const int diff1 = SDIFF(p[i], p[i - 1]);
      const int diff2 = SDIFF(p[i], p[i - width]);
      const int grad_pred =
          GradientPredictor(p[i - 1], p[i - width], p[i - width - 1]);
      const int diff3 = SDIFF(p[i], grad_pred);
      bins[WEBP_FILTER_NONE][diff0] = 1;
      bins[WEBP_FILTER_HORIZONTAL][diff1] = 1;
      bins[WEBP_FILTER_VERTICAL][diff2] = 1;
      bins[WEBP_FILTER_GRADIENT][diff3] = 1;
      mean = (3 * mean + p[i] + 2) >> 2;
    }
  }
  {
    WEBP_FILTER_TYPE filter, best_filter = WEBP_FILTER_NONE;
    int best_score = 0x7fffffff;
    for (filter = WEBP_FILTER_NONE; filter < WEBP_FILTER_LAST; ++filter) {
      int score = 0;
      for (i = 0; i < SMAX; ++i) {
        if (bins[filter][i] > 0) {
          score += i;
        }
      }
      if (score < best_score) {
        best_score = score;
        best_filter = filter;
      }
    }
    return best_filter;
  }
}

#undef SMAX
#undef SDIFF

//------------------------------------------------------------------------------

const WebPFilterFunc WebPFilters[WEBP_FILTER_LAST] = {
    NULL,              // WEBP_FILTER_NONE
    HorizontalFilter,  // WEBP_FILTER_HORIZONTAL
    VerticalFilter,    // WEBP_FILTER_VERTICAL
    GradientFilter     // WEBP_FILTER_GRADIENT
};

const WebPFilterFunc WebPUnfilters[WEBP_FILTER_LAST] = {
    NULL,                // WEBP_FILTER_NONE
    HorizontalUnfilter,  // WEBP_FILTER_HORIZONTAL
    VerticalUnfilter,    // WEBP_FILTER_VERTICAL
    GradientUnfilter     // WEBP_FILTER_GRADIENT
};

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
