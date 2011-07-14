// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Main decoding functions for WEBP images.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include "vp8i.h"
#include "webpi.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// RIFF layout is:
//   0ffset  tag
//   0...3   "RIFF" 4-byte tag
//   4...7   size of image data (including metadata) starting at offset 8
//   8...11  "WEBP"   our form-type signature
//   12..15  "VP8 ": 4-bytes tags, describing the raw video format used
//   16..19  size of the raw VP8 image data, starting at offset 20
//   20....  the VP8 bytes
// There can be extra chunks after the "VP8 " chunk (ICMT, ICOP, ...)
// All 32-bits sizes are in little-endian order.
// Note: chunk data must be padded to multiple of 2 in size

static inline uint32_t get_le32(const uint8_t* const data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// If a RIFF container is detected, validate it and skip over it.
uint32_t WebPCheckRIFFHeader(const uint8_t** data_ptr,
                             uint32_t* data_size_ptr) {
  uint32_t chunk_size = 0xffffffffu;
  if (*data_size_ptr >= 10 + 20 && !memcmp(*data_ptr, "RIFF", 4)) {
    if (memcmp(*data_ptr + 8, "WEBP", 4)) {
      return 0;  // wrong image file signature
    } else {
      const uint32_t riff_size = get_le32(*data_ptr + 4);
      if (riff_size < 12) {
        return 0;   // we should have at least one chunk
      }
      if (memcmp(*data_ptr + 12, "VP8 ", 4)) {
        return 0;   // invalid compression format
      }
      chunk_size = get_le32(*data_ptr + 16);
      if (chunk_size > riff_size - 12) {
        return 0;  // inconsistent size information.
      }
      // We have a RIFF container. Skip it.
      *data_ptr += 20;
      *data_size_ptr -= 20;
      // Note: we don't report error for odd-sized chunks.
    }
    return chunk_size;
  }
  return *data_size_ptr;
}

//-----------------------------------------------------------------------------
// WebPDecParams

void WebPResetDecParams(WebPDecParams* const params) {
  if (params) {
    memset(params, 0, sizeof(*params));
  }
}

//-----------------------------------------------------------------------------
// "Into" decoding variants

// Main flow
static VP8StatusCode DecodeInto(const uint8_t* data, uint32_t data_size,
                                WebPDecParams* const params) {
  VP8Decoder* dec = VP8New();
  VP8StatusCode status = VP8_STATUS_OK;
  VP8Io io;

  assert(params);
  if (dec == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }

  VP8InitIo(&io);
  io.data = data;
  io.data_size = data_size;
  WebPInitCustomIo(params, &io);  // Plug the I/O functions.

  // Decode bitstream header, update io->width/io->height.
  if (!VP8GetHeaders(dec, &io)) {
    status = VP8_STATUS_BITSTREAM_ERROR;
  } else {
    // Allocate/check output buffers.
    status = WebPAllocateDecBuffer(io.width, io.height, params->options,
                                   params->output);
    if (status == VP8_STATUS_OK) {
      // Decode
      if (!VP8Decode(dec, &io)) {
        status = dec->status_;
      }
    }
  }
  VP8Delete(dec);
  if (status != VP8_STATUS_OK) {
    WebPFreeDecBuffer(params->output);
  }
  return status;
}

// Helpers
static uint8_t* DecodeIntoRGBABuffer(WEBP_CSP_MODE colorspace,
                                     const uint8_t* data, uint32_t data_size,
                                     uint8_t* rgba, int stride, int size) {
  WebPDecParams params;
  WebPDecBuffer buf;
  if (rgba == NULL) {
    return NULL;
  }
  WebPInitDecBuffer(&buf);
  WebPResetDecParams(&params);
  params.output = &buf;
  buf.colorspace    = colorspace;
  buf.u.RGBA.rgba   = rgba;
  buf.u.RGBA.stride = stride;
  buf.u.RGBA.size   = size;
  buf.is_external_memory = 1;
  if (DecodeInto(data, data_size, &params) != VP8_STATUS_OK) {
    return NULL;
  }
  return rgba;
}

uint8_t* WebPDecodeRGBInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* output, int size, int stride) {
  return DecodeIntoRGBABuffer(MODE_RGB, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeRGBAInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output, int size, int stride) {
  return DecodeIntoRGBABuffer(MODE_RGBA, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeARGBInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output, int size, int stride) {
  return DecodeIntoRGBABuffer(MODE_ARGB, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeBGRInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* output, int size, int stride) {
  return DecodeIntoRGBABuffer(MODE_BGR, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeBGRAInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output, int size, int stride) {
  return DecodeIntoRGBABuffer(MODE_BGRA, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeYUVInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* luma, int luma_size, int luma_stride,
                           uint8_t* u, int u_size, int u_stride,
                           uint8_t* v, int v_size, int v_stride) {
  WebPDecParams params;
  WebPDecBuffer output;
  if (luma == NULL) return NULL;
  WebPInitDecBuffer(&output);
  WebPResetDecParams(&params);
  params.output = &output;
  output.colorspace      = MODE_YUV;
  output.u.YUVA.y        = luma;
  output.u.YUVA.y_stride = luma_stride;
  output.u.YUVA.y_size   = luma_size;
  output.u.YUVA.u        = u;
  output.u.YUVA.u_stride = u_stride;
  output.u.YUVA.u_size   = u_size;
  output.u.YUVA.v        = v;
  output.u.YUVA.v_stride = v_stride;
  output.u.YUVA.v_size   = v_size;
  output.is_external_memory = 1;
  if (DecodeInto(data, data_size, &params) != VP8_STATUS_OK) {
    return NULL;
  }
  return luma;
}

//-----------------------------------------------------------------------------

static uint8_t* Decode(WEBP_CSP_MODE mode, const uint8_t* data,
                       uint32_t data_size, int* width, int* height,
                       WebPDecBuffer* keep_info) {
  WebPDecParams params;
  WebPDecBuffer output;

  WebPInitDecBuffer(&output);
  WebPResetDecParams(&params);
  params.output = &output;
  output.colorspace = mode;

  // Retrieve (and report back) the required dimensions from bitstream.
  if (!WebPGetInfo(data, data_size, &output.width, &output.height)) {
    return NULL;
  }
  if (width) *width = output.width;
  if (height) *height = output.height;

  // Decode
  if (DecodeInto(data, data_size, &params) != VP8_STATUS_OK) {
    return NULL;
  }
  if (keep_info) {    // keep track of the side-info
    WebPCopyDecBuffer(&output, keep_info);
  }
  // return decoded samples (don't clear 'output'!)
  return (mode >= MODE_YUV) ? output.u.YUVA.y : output.u.RGBA.rgba;
}

uint8_t* WebPDecodeRGB(const uint8_t* data, uint32_t data_size,
                       int* width, int* height) {
  return Decode(MODE_RGB, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeRGBA(const uint8_t* data, uint32_t data_size,
                        int* width, int* height) {
  return Decode(MODE_RGBA, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeARGB(const uint8_t* data, uint32_t data_size,
                        int* width, int* height) {
  return Decode(MODE_ARGB, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeBGR(const uint8_t* data, uint32_t data_size,
                       int* width, int* height) {
  return Decode(MODE_BGR, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeBGRA(const uint8_t* data, uint32_t data_size,
                        int* width, int* height) {
  return Decode(MODE_BGRA, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeYUV(const uint8_t* data, uint32_t data_size,
                       int* width, int* height, uint8_t** u, uint8_t** v,
                       int* stride, int* uv_stride) {
  WebPDecBuffer output;   // only to preserve the side-infos
  uint8_t* const out = Decode(MODE_YUV, data, data_size,
                              width, height, &output);

  if (out) {
    const WebPYUVABuffer* const buf = &output.u.YUVA;
    *u = buf->u;
    *v = buf->v;
    *stride = buf->y_stride;
    *uv_stride = buf->u_stride;
    assert(buf->u_stride == buf->v_stride);
  }
  return out;
}

//-----------------------------------------------------------------------------
// WebPGetInfo()

int WebPGetInfo(const uint8_t* data, uint32_t data_size,
                int* width, int* height) {
  const uint32_t chunk_size = WebPCheckRIFFHeader(&data, &data_size);
  if (!chunk_size) {
    return 0;         // unsupported RIFF header
  }
  // Validate raw video data
  return VP8GetInfo(data, data_size, chunk_size, width, height, NULL);
}

static void DefaultFeatures(WebPBitstreamFeatures* const features) {
  assert(features);
  memset(features, 0, sizeof(*features));
  features->bitstream_version = 0;
}

static VP8StatusCode GetFeatures(const uint8_t** data, uint32_t* data_size,
                                 WebPBitstreamFeatures* const features) {
  uint32_t chunk_size;
  if (features == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  DefaultFeatures(features);
  if (data == NULL || *data == NULL || data_size == 0) {
    return VP8_STATUS_INVALID_PARAM;
  }
  chunk_size = WebPCheckRIFFHeader(data, data_size);
  if (chunk_size == 0) {
    return VP8_STATUS_BITSTREAM_ERROR;   // unsupported RIFF header
  }
  if (!VP8GetInfo(*data, *data_size, chunk_size,
                  &features->width, &features->height, &features->has_alpha)) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }
  return VP8_STATUS_OK;
}

//-----------------------------------------------------------------------------
// Advance decoding API

int WebPInitDecoderConfigInternal(WebPDecoderConfig* const config,
                                  int version) {
  if (version != WEBP_DECODER_ABI_VERSION) {
    return 0;   // version mismatch
  }
  if (config == NULL) {
    return 0;
  }
  memset(config, 0, sizeof(*config));
  DefaultFeatures(&config->input);
  WebPInitDecBuffer(&config->output);
  return 1;
}

VP8StatusCode WebPGetFeaturesInternal(const uint8_t* data, uint32_t data_size,
                                      WebPBitstreamFeatures* const features,
                            int version) {
  if (version != WEBP_DECODER_ABI_VERSION) {
    return VP8_STATUS_INVALID_PARAM;   // version mismatch
  }
  if (features == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  return GetFeatures(&data, &data_size, features);
}

VP8StatusCode WebPDecode(const uint8_t* data, uint32_t data_size,
                         WebPDecoderConfig* const config) {
  WebPDecParams params;
  VP8StatusCode status;

  if (!config) {
    return VP8_STATUS_INVALID_PARAM;
  }

  status = GetFeatures(&data, &data_size, &config->input);
  if (status != VP8_STATUS_OK) {
    return status;
  }

  WebPResetDecParams(&params);
  params.output = &config->output;
  params.options = &config->options;
  status = DecodeInto(data, data_size, &params);

  return status;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
