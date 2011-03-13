// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Internal header: WebP decoding parameters and custom IO on buffer
//
// Author: somnath@google.com (Somnath Banerjee)

#ifndef WEBP_DEC_WEBPI_H
#define WEBP_DEC_WEBPI_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "webp/decode_vp8.h"

typedef enum { MODE_RGB = 0, MODE_RGBA = 1,
               MODE_BGR = 2, MODE_BGRA = 3,
               MODE_YUV = 4 } WEBP_CSP_MODE;

  // Decoding output parameters.
typedef struct {
  uint8_t* output;      // rgb(a) or luma
  uint8_t *u, *v;
  uint8_t *top_y, *top_u, *top_v;
  int stride;           // rgb(a) stride or luma stride
  int u_stride;
  int v_stride;
  WEBP_CSP_MODE mode;
} WebPDecParams;

// If a RIFF container is detected, validate it and skip over it. Returns
// VP8 bit-stream size if RIFF header is valid else returns 0
uint32_t WebPCheckRIFFHeader(const uint8_t** data_ptr,
                             uint32_t *data_size_ptr);

// Initializes VP8Io with custom setup, io and teardown functions
void WebPInitCustomIo(VP8Io* const io);

// Initializes params_out by allocating output buffer and setting the
// stride information. It also outputs width and height information of
// the WebP image. Returns 1 if succeeds.
int WebPInitDecParams(const uint8_t* data, uint32_t data_size, int* width,
                      int* height, WebPDecParams* const params_out);

// Verifies various size configurations (e.g stride >= width, specified
// output size <= stride * height etc.). Returns 0 if checks fail.
int WebPCheckDecParams(const VP8Io* io, const WebPDecParams* params,
                       int output_size, int output_u_size, int output_v_size);

// Deallocate memory allocated by WebPInitDecParams() and reset the
// WebPDecParams object.
void WebPClearDecParams(WebPDecParams* params);

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_DEC_WEBPI_H
