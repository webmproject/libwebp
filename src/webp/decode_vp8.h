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

#ifndef WEBP_DECODE_WEBP_DECODE_VP8_H_
#define WEBP_DECODE_WEBP_DECODE_VP8_H_

#include "decode.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

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
  int width, height;       // picture dimensions, in pixels

  // set before calling put()
  int mb_x, mb_y;            // position of the current sample (in pixels)
  int mb_w, mb_h;            // size of the current sample (usually 16x16)
  const uint8_t *y, *u, *v;  // samples to copy
  int y_stride;              // stride for luma
  int uv_stride;             // stride for chroma

  void* opaque;              // user data

  // called when fresh samples are available (1 block of 16x16 pixels)
  void (*put)(const VP8Io* io);

  // called just before starting to decode the blocks
  void (*setup)(const VP8Io* io);

  // called just after block decoding is finished
  void (*teardown)(const VP8Io* io);

  // Input buffer.
   uint32_t data_size;
  const uint8_t* data;
};

// Main decoding object. This is an opaque structure.
typedef struct VP8Decoder VP8Decoder;

// Create a new decoder object.
VP8Decoder* VP8New();

// Can be called to make sure 'io' is initialized properly.
void VP8InitIo(VP8Io* const io);

// Start decoding a new picture. Returns true if ok.
int VP8GetHeaders(VP8Decoder* const dec, VP8Io* const io);

// Decode a picture. Will call VP8GetHeaders() if it wasn't done already.
int VP8Decode(VP8Decoder* const dec, VP8Io* const io);

// Return current status of the decoder:
//  0 = OK
//  1 = OUT_OF_MEMORY
//  2 = INVALID_PARAM
//  3 = BITSTREAM_ERROR
//  4 = UNSUPPORTED_FEATURE
int VP8Status(VP8Decoder* const dec);

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

#endif  // WEBP_DECODE_WEBP_DECODE_VP8_H_
