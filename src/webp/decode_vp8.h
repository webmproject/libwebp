// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Low-level API for VP8 decoder
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_WEBP_DECODE_VP8_H_
#define WEBP_WEBP_DECODE_VP8_H_

#include "decode.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define WEBP_DECODER_ABI_VERSION 0x0001

//-----------------------------------------------------------------------------
// Lower-level API
//
// Thes functions provide fine-grained control of the decoding process.
// The call flow should resemble:
//
//   VP8Io io;
//   VP8InitIo(&io);
//   io.data = data;
//   io.data_size = size;
//   /* customize io's functions (setup()/put()/teardown()) if needed. */
//
//   VP8Decoder* dec = VP8New();
//   bool ok = VP8Decode(dec);
//   if (!ok) printf("Error: %s\n", VP8StatusMessage(dec));
//   VP8Delete(dec);
//   return ok;

// Input / Output
typedef struct VP8Io VP8Io;
struct VP8Io {
  // set by VP8GetHeaders()
  int width, height;         // picture dimensions, in pixels

  // set before calling put()
  int mb_y;                  // position of the current rows (in pixels)
  int mb_h;                  // number of rows in the sample
  const uint8_t *y, *u, *v;  // rows to copy (in yuv420 format)
  int y_stride;              // row stride for luma
  int uv_stride;             // row stride for chroma

  void* opaque;              // user data

  // called when fresh samples are available. Currently, samples are in
  // YUV420 format, and can be up to width x 24 in size (depending on the
  // in-loop filtering level, e.g.). Should return false in case of error
  // or abort request.
  int (*put)(const VP8Io* io);

  // called just before starting to decode the blocks.
  // Should returns 0 in case of error.
  int (*setup)(VP8Io* io);

  // called just after block decoding is finished (or when an error occurred).
  void (*teardown)(const VP8Io* io);

  // this is a recommendation for the user-side yuv->rgb converter. This flag
  // is set when calling setup() hook and can be overwritten by it. It then
  // can be taken into consideration during the put() method.
  int fancy_upscaling;

  // Input buffer.
  uint32_t data_size;
  const uint8_t* data;

  // If true, in-loop filtering will not be performed even if present in the
  // bitstream. Switching off filtering may speed up decoding at the expense
  // of more visible blocking. Note that output will also be non-compliant
  // with the VP8 specifications.
  int bypass_filtering;
};

// Internal, version-checked, entry point
extern int VP8InitIoInternal(VP8Io* const, int);

// Main decoding object. This is an opaque structure.
typedef struct VP8Decoder VP8Decoder;

// Create a new decoder object.
VP8Decoder* VP8New();

// Must be called to make sure 'io' is initialized properly.
// Returns false in case of version mismatch. Upon such failure, no other
// decoding function should be called (VP8Decode, VP8GetHeaders, ...)
static inline int VP8InitIo(VP8Io* const io) {
  return VP8InitIoInternal(io, WEBP_DECODER_ABI_VERSION);
}

// Start decoding a new picture. Returns true if ok.
int VP8GetHeaders(VP8Decoder* const dec, VP8Io* const io);

// Decode a picture. Will call VP8GetHeaders() if it wasn't done already.
// Returns false in case of error.
int VP8Decode(VP8Decoder* const dec, VP8Io* const io);

// Enumeration of the codes returned by VP8Status()
typedef enum {
  VP8_STATUS_OK = 0,
  VP8_STATUS_OUT_OF_MEMORY,
  VP8_STATUS_INVALID_PARAM,
  VP8_STATUS_BITSTREAM_ERROR,
  VP8_STATUS_UNSUPPORTED_FEATURE,
  VP8_STATUS_SUSPENDED,
  VP8_STATUS_USER_ABORT,
  VP8_STATUS_NOT_ENOUGH_DATA,
} VP8StatusCode;

// Return current status of the decoder:
VP8StatusCode VP8Status(VP8Decoder* const dec);

// return readable string corresponding to the last status.
const char* VP8StatusMessage(VP8Decoder* const dec);

// Resets the decoder in its initial state, reclaiming memory.
// Not a mandatory call between calls to VP8Decode().
void VP8Clear(VP8Decoder* const dec);

// Destroy the decoder object.
void VP8Delete(VP8Decoder* const dec);

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  /* WEBP_WEBP_DECODE_VP8_H_ */
