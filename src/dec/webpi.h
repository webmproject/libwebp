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

#include "../webp/decode_vp8.h"

//------------------------------------------------------------------------------
// WebPDecParams: Decoding output parameters. Transcient internal object.

typedef struct WebPDecParams WebPDecParams;
typedef int (*OutputFunc)(const VP8Io* const io, WebPDecParams* const p);

// Structure use for on-the-fly rescaling
typedef struct {
  int x_expand;               // true if we're expanding in the x direction
  int fy_scale, fx_scale;     // fixed-point scaling factor
  int64_t fxy_scale;          // ''
  // we need hpel-precise add/sub increments, for the downsampled U/V planes.
  int y_accum;                // vertical accumulator
  int y_add, y_sub;           // vertical increments (add ~= src, sub ~= dst)
  int x_add, x_sub;           // horizontal increments (add ~= src, sub ~= dst)
  int src_width, src_height;  // source dimensions
  int dst_width, dst_height;  // destination dimensions
  uint8_t* dst;
  int dst_stride;
  int32_t* irow, *frow;       // work buffer
} WebPRescaler;

struct WebPDecParams {
  WebPDecBuffer* output;             // output buffer.
  uint8_t* tmp_y, *tmp_u, *tmp_v;    // cache for the fancy upsampler
                                     // or used for tmp rescaling

  int last_y;                 // coordinate of the line that was last output
  const WebPDecoderOptions* options;  // if not NULL, use alt decoding features
  // rescalers
  WebPRescaler scaler_y, scaler_u, scaler_v, scaler_a;
  void* memory;               // overall scratch memory for the output work.
  OutputFunc emit;            // output RGB or YUV samples
  OutputFunc emit_alpha;      // output alpha channel
};

// Should be called first, before any use of the WebPDecParams object.
void WebPResetDecParams(WebPDecParams* const params);

//------------------------------------------------------------------------------
// Upsampler function to overwrite fancy upsampler.

typedef void (*WebPUpsampleLinePairFunc)(
  const uint8_t* top_y, const uint8_t* bottom_y,
  const uint8_t* top_u, const uint8_t* top_v,
  const uint8_t* cur_u, const uint8_t* cur_v,
  uint8_t* top_dst, uint8_t* bottom_dst, int len);

// Upsampler functions to be used to convert YUV to RGB(A) modes
extern WebPUpsampleLinePairFunc WebPUpsamplers[MODE_LAST];
extern WebPUpsampleLinePairFunc WebPUpsamplersKeepAlpha[MODE_LAST];

// Initializes SSE2 version of the fancy upsamplers.
void WebPInitUpsamplersSSE2(void);

//------------------------------------------------------------------------------
// Misc utils

// Validates the RIFF container (if detected) and skip over it.
// If a RIFF container is detected, returns 0 for invalid header. Else return 1.
// In case there are not enough bytes (partial RIFF container), return 0 for
// riff_size. Else return the riff_size extracted from the header.
uint32_t WebPCheckAndSkipRIFFHeader(const uint8_t** data_ptr,
                                    uint32_t* data_size_ptr,
                                    uint32_t* riff_size_ptr);

// Initializes VP8Io with custom setup, io and teardown functions. The default
// hooks will use the supplied 'params' as io->opaque handle.
void WebPInitCustomIo(WebPDecParams* const params, VP8Io* const io);

//------------------------------------------------------------------------------
// Internal functions regarding WebPDecBuffer memory (in buffer.c).
// Don't really need to be externally visible for now.

// Prepare 'buffer' with the requested initial dimensions width/height.
// If no external storage is supplied, initializes buffer by allocating output
// memory and setting up the stride information. Validate the parameters. Return
// an error code in case of problem (no memory, or invalid stride / size /
// dimension / etc.). If *options is not NULL, also verify that the options'
// parameters are valid and apply them to the width/height dimensions of the
// output buffer. This takes cropping / scaling / rotation into account.
VP8StatusCode WebPAllocateDecBuffer(int width, int height,
                                    const WebPDecoderOptions* const options,
                                    WebPDecBuffer* const buffer);

// Copy 'src' into 'dst' buffer, making sure 'dst' is not marked as owner of the
// memory (still held by 'src').
void WebPCopyDecBuffer(const WebPDecBuffer* const src,
                       WebPDecBuffer* const dst);

// Copy and transfer ownership from src to dst (beware of parameter order!)
void WebPGrabDecBuffer(WebPDecBuffer* const src, WebPDecBuffer* const dst);

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_DEC_WEBPI_H
