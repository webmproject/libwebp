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

#ifndef WEBP_DEC_WEBPI_H_
#define WEBP_DEC_WEBPI_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "../webp/decode_vp8.h"

//------------------------------------------------------------------------------
// WebPDecParams: Decoding output parameters. Transient internal object.

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
// Header parsing helpers

#define TAG_SIZE 4
#define CHUNK_HEADER_SIZE 8
#define RIFF_HEADER_SIZE 12
#define FRAME_CHUNK_SIZE 20
#define LOOP_CHUNK_SIZE 4
#define TILE_CHUNK_SIZE 8
#define VP8X_CHUNK_SIZE 12
#define VP8_FRAME_HEADER_SIZE 10  // Size of the frame header within VP8 data.

// Validates the RIFF container (if detected) and skips over it.
// If a RIFF container is detected,
// Returns VP8_STATUS_BITSTREAM_ERROR for invalid header, and
//         VP8_STATUS_OK otherwise.
// In case there are not enough bytes (partial RIFF container), return 0 for
// riff_size. Else return the riff_size extracted from the header.
VP8StatusCode WebPParseRIFF(const uint8_t** data, uint32_t* data_size,
                            uint32_t* riff_size);

// Validates the VP8X Header and skips over it.
// Returns VP8_STATUS_BITSTREAM_ERROR for invalid VP8X header,
//         VP8_STATUS_NOT_ENOUGH_DATA in case of insufficient data, and
//         VP8_STATUS_OK otherwise.
// If a VP8 chunk is found, bytes_skipped is set to the total number of bytes
// that are skipped; also Width, Height & Flags are set to the corresponding
// fields extracted from the VP8X chunk.
VP8StatusCode WebPParseVP8X(const uint8_t** data, uint32_t* data_size,
                            uint32_t* bytes_skipped,
                            int* width, int* height, uint32_t* flags);

// Skips to the next VP8 chunk header in the data given the size of the RIFF
// chunk 'riff_size'.
// Returns VP8_STATUS_BITSTREAM_ERROR if any invalid chunk size is encountered,
//         VP8_STATUS_NOT_ENOUGH_DATA in case of insufficient data, and
//         VP8_STATUS_OK otherwise.
// If a VP8 chunk is found, bytes_skipped is set to the total number of bytes
// that are skipped.
VP8StatusCode WebPParseOptionalChunks(const uint8_t** data, uint32_t* data_size,
                                      uint32_t riff_size,
                                      uint32_t* bytes_skipped);

// Validates the VP8 Header ("VP8 nnnn") and skips over it.
// Returns VP8_STATUS_BITSTREAM_ERROR for invalid (vp8_chunk_size greater than
//         riff_size) VP8 header,
//         VP8_STATUS_NOT_ENOUGH_DATA in case of insufficient data, and
//         VP8_STATUS_OK otherwise.
// If a VP8 chunk is found, bytes_skipped is set to the total number of bytes
// that are skipped and vp8_chunk_size is set to the corresponding size
// extracted from the VP8 chunk header.
// For a partial VP8 chunk, vp8_chunk_size is set to 0.
VP8StatusCode WebPParseVP8Header(const uint8_t** data, uint32_t* data_size,
                                 uint32_t riff_size, uint32_t* bytes_skipped,
                                 uint32_t* vp8_chunk_size);

// Skips over all valid chunks prior to the first VP8 frame header.
// Returns VP8_STATUS_OK on success,
//         VP8_STATUS_BITSTREAM_ERROR if an invalid header/chunk is found, and
//         VP8_STATUS_NOT_ENOUGH_DATA if case of insufficient data.
// Also, data, data_size, vp8_size & bytes_skipped are updated appropriately
// on success, where
// vp8_size is the size of VP8 chunk data (extracted from VP8 chunk header) and
// bytes_skipped is set to the total number of bytes that are skipped.
VP8StatusCode WebPParseHeaders(const uint8_t** data, uint32_t* data_size,
                               uint32_t* vp8_size, uint32_t* bytes_skipped);

//------------------------------------------------------------------------------
// Misc utils

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

#endif  /* WEBP_DEC_WEBPI_H_ */
