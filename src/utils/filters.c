// Copyright 2011 Google Inc.
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
      const uint8_t predictor = scan_line[w - bpp] + prev_line[w] -
                                prev_line[w - bpp];
      out[w] = scan_line[w] - predictor;
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
      const uint8_t predictor = out[w - bpp] + out_prev_line[w] -
                                out_prev_line[w - bpp];
      out[w] = scan_line[w] + predictor;
    }
  }
}

//------------------------------------------------------------------------------
// Paeth filter.

static inline int AbsDiff(int a, int b) {
  return (a > b) ? a - b : b - a;
}

static inline uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c) {
  const int p = a + b - c;  // Base.
  const int pa = AbsDiff(p, a);
  const int pb = AbsDiff(p, b);
  const int pc = AbsDiff(p, c);

  // Return nearest to base of a, b, c.
  return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}

static void PaethFilter(const uint8_t* data, int width, int height,
                        int bpp, int stride, uint8_t* filtered_data) {
  int w;
  int h;
  SANITY_CHECK(data, filtered_data);

  // Top scan line (special case).
  memcpy((void*)filtered_data, (const void*)data, bpp);
  for (w = bpp; w < width * bpp; ++w) {
    // Note: PaethPredictor(scan_line[w - bpp], 0, 0) == scan_line[w - bpp].
    filtered_data[w] = data[w] - data[w - bpp];
  }

  // Filter line-by-line.
  for (h = 1; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = filtered_data + h * stride;
    const uint8_t* const prev_line = scan_line - stride;
    for (w = 0; w < bpp; ++w) {
      // Note: PaethPredictor(0, prev_line[w], 0) == prev_line[w].
      out[w] = scan_line[w] - prev_line[w];
    }
    for (w = bpp; w < width * bpp; ++w) {
      out[w] = scan_line[w] - PaethPredictor(scan_line[w - bpp], prev_line[w],
                                             prev_line[w - bpp]);
    }
  }
}

static void PaethUnfilter(const uint8_t* data, int width, int height,
                          int bpp, int stride, uint8_t* recon_data) {
  int w;
  int h;
  SANITY_CHECK(data, recon_data);

  // Top scan line (special case).
  memcpy((void*)recon_data, (const void*)data, bpp);
  for (w = bpp; w < width * bpp; ++w) {
    // Note: PaethPredictor(out[w - bpp], 0, 0) == out[w - bpp].
    recon_data[w] = data[w] + recon_data[w - bpp];
  }

  // Unfilter line-by-line.
  for (h = 1; h < height; ++h) {
    int w;
    const uint8_t* const scan_line = data + h * stride;
    uint8_t* const out = recon_data + h * stride;
    const uint8_t* const out_prev = out - stride;
    for (w = 0; w < bpp; ++w) {
      // Note: PaethPredictor(0, out_prev[w], 0) == out_prev[w].
      out[w] = scan_line[w] + out_prev[w];
    }
    for (w = bpp; w < width * bpp; ++w) {
      out[w] = scan_line[w] + PaethPredictor(out[w - bpp], out_prev[w],
                                             out_prev[w - bpp]);
    }
  }
}

#undef SANITY_CHECK

//------------------------------------------------------------------------------

const WebPFilterFunc WebPFilters[WEBP_FILTER_LAST] = {
    NULL,              // WEBP_FILTER_NONE
    HorizontalFilter,  // WEBP_FILTER_HORIZONTAL
    VerticalFilter,    // WEBP_FILTER_VERTICAL
    GradientFilter,    // WEBP_FILTER_GRADIENT
    PaethFilter,       // WEBP_FILTER_PAETH
    NULL               // WEBP_FILTER_BEST
};

const WebPFilterFunc WebPUnfilters[WEBP_FILTER_LAST] = {
    NULL,                // WEBP_FILTER_NONE
    HorizontalUnfilter,  // WEBP_FILTER_HORIZONTAL
    VerticalUnfilter,    // WEBP_FILTER_VERTICAL
    GradientUnfilter,    // WEBP_FILTER_GRADIENT
    PaethUnfilter,       // WEBP_FILTER_PAETH
    NULL                 // WEBP_FILTER_BEST
};

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
