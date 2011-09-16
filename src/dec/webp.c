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

//------------------------------------------------------------------------------
// RIFF layout is:
//   Offset  tag
//   0...3   "RIFF" 4-byte tag
//   4...7   size of image data (including metadata) starting at offset 8
//   8...11  "WEBP"   our form-type signature
// The RIFF container (12 bytes) is followed by appropriate chunks:
//   12..15  "VP8 ": 4-bytes tags, describing the raw video format used
//   16..19  size of the raw VP8 image data, starting at offset 20
//   20....  the VP8 bytes
// Or,
//   12..15  "VP8X": 4-bytes tags, describing the extended-VP8 chunk.
//   16..19  size of the VP8X chunk starting at offset 20.
//   20..23  VP8X flags bit-map corresponding to the chunk-types present.
//   24..27  Width of the Canvas Image.
//   28..31  Height of the Canvas Image.
// There can be extra chunks after the "VP8X" chunk (ICCP, TILE, FRM, VP8,
// META  ...)
// All 32-bits sizes are in little-endian order.
// Note: chunk data must be padded to multiple of 2 in size

static inline uint32_t get_le32(const uint8_t* const data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

VP8StatusCode WebPParseRIFF(const uint8_t** data, uint32_t* data_size,
                            uint32_t* riff_size) {
  assert(data);
  assert(data_size);
  assert(riff_size);

  if (*data_size >= RIFF_HEADER_SIZE &&
      !memcmp(*data, "RIFF", TAG_SIZE)) {
    if (memcmp(*data + 8, "WEBP", TAG_SIZE)) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Wrong image file signature.
    } else {
      *riff_size = get_le32(*data + TAG_SIZE);
      // Check that we have at least one chunk (i.e "WEBP" + "VP8?nnnn").
      if (*riff_size < TAG_SIZE + CHUNK_HEADER_SIZE) {
        return VP8_STATUS_BITSTREAM_ERROR;
      }
      // We have a RIFF container. Skip it.
      *data += RIFF_HEADER_SIZE;
      *data_size -= RIFF_HEADER_SIZE;
    }
  } else {
    *riff_size = 0;  // Did not get full RIFF Header.
  }
  return VP8_STATUS_OK;
}

VP8StatusCode WebPParseVP8X(const uint8_t** data, uint32_t* data_size,
                            uint32_t* bytes_skipped,
                            int* width, int* height, uint32_t* flags) {
  assert(data);
  assert(data_size);
  assert(bytes_skipped);

  *bytes_skipped = 0;

  if (*data_size < CHUNK_HEADER_SIZE + VP8X_CHUNK_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;  // Insufficient data.
  }

  if (!memcmp(*data, "VP8X", TAG_SIZE)) {
    const uint32_t chunk_size = get_le32(*data + TAG_SIZE);
    if (chunk_size != VP8X_CHUNK_SIZE) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Wrong chunk size.
    }
    if (flags) {
      *flags = get_le32(*data + 8);
    }
    if (width) {
      *width = get_le32(*data + 12);
    }
    if (height) {
      *height = get_le32(*data + 16);
    }
    // We have consumed 20 bytes from VP8X. Skip them.
    *bytes_skipped = CHUNK_HEADER_SIZE + VP8X_CHUNK_SIZE;
    *data += *bytes_skipped;
    *data_size -= *bytes_skipped;
  }
  return VP8_STATUS_OK;
}

VP8StatusCode WebPParseOptionalChunks(const uint8_t** data, uint32_t* data_size,
                                      uint32_t riff_size,
                                      uint32_t* bytes_skipped) {
  const uint8_t* buf;
  uint32_t buf_size;

  assert(data);
  assert(data_size);
  assert(bytes_skipped);

  buf = *data;
  buf_size = *data_size;
  *bytes_skipped = 0;

  while (1) {
    uint32_t chunk_size;
    uint32_t cur_skip_size;
    const uint32_t bytes_skipped_header = TAG_SIZE +           // "WEBP".
                                          CHUNK_HEADER_SIZE +  // "VP8Xnnnn".
                                          VP8X_CHUNK_SIZE;     // Data.
    *data = buf;
    *data_size = buf_size;

    if (buf_size < CHUNK_HEADER_SIZE) {  // Insufficient data.
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    chunk_size = get_le32(buf + TAG_SIZE);
    cur_skip_size = CHUNK_HEADER_SIZE + chunk_size;

    // Check that total bytes skipped along with current chunk size
    // does not exceed riff_size.
    if (riff_size > 0 &&
        (bytes_skipped_header + *bytes_skipped + cur_skip_size > riff_size)) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Not a valid chunk size.
    }

    if (buf_size < cur_skip_size) {  // Insufficient data.
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    if (!memcmp(buf, "VP8 ", TAG_SIZE)) {  // A valid VP8 header.
      return VP8_STATUS_OK;  // Found.
    }

    // We have a full & valid chunk; skip it.
    buf += cur_skip_size;
    buf_size -= cur_skip_size;
    *bytes_skipped += cur_skip_size;
  }
}

VP8StatusCode WebPParseVP8Header(const uint8_t** data, uint32_t* data_size,
                                 uint32_t riff_size, uint32_t* bytes_skipped,
                                 uint32_t* vp8_chunk_size) {
  assert(data);
  assert(data_size);
  assert(bytes_skipped);
  assert(vp8_chunk_size);

  *bytes_skipped = 0;
  *vp8_chunk_size = 0;

  if (*data_size < CHUNK_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;  // Insufficient data.
  }

  if (!memcmp(*data, "VP8 ", TAG_SIZE)) {
    *vp8_chunk_size = get_le32(*data + TAG_SIZE);
    if (riff_size >= TAG_SIZE + CHUNK_HEADER_SIZE &&  // "WEBP" + "VP8 nnnn".
        (*vp8_chunk_size > riff_size - (TAG_SIZE + CHUNK_HEADER_SIZE))) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Inconsistent size information.
    }
    // We have consumed CHUNK_HEADER_SIZE bytes from VP8 Header. Skip them.
    *bytes_skipped = CHUNK_HEADER_SIZE;
    *data += *bytes_skipped;
    *data_size -= *bytes_skipped;
  }
  return VP8_STATUS_OK;
}

VP8StatusCode WebPParseHeaders(const uint8_t** data, uint32_t* data_size,
                               uint32_t* vp8_size, uint32_t* bytes_skipped) {
  const uint8_t* buf;
  uint32_t buf_size;
  uint32_t riff_size;
  uint32_t vp8_size_tmp;
  uint32_t optional_data_size;
  uint32_t vp8x_skip_size;
  uint32_t vp8_skip_size;
  VP8StatusCode status;

  assert(data);
  assert(data_size);
  assert(vp8_size);
  assert(bytes_skipped);

  buf = *data;
  buf_size = *data_size;

  *vp8_size = 0;
  *bytes_skipped = 0;

  if (buf == NULL || buf_size < RIFF_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;
  }

  // Skip over RIFF header.
  if (WebPParseRIFF(&buf, &buf_size, &riff_size) != VP8_STATUS_OK) {
    return VP8_STATUS_BITSTREAM_ERROR;  // Wrong RIFF Header.
  }

  // Skip over VP8X header.
  status = WebPParseVP8X(&buf, &buf_size, &vp8x_skip_size, NULL, NULL, NULL);
  if (status != VP8_STATUS_OK) {
    return status;  // Wrong VP8X Chunk / Insufficient data.
  }
  if (vp8x_skip_size > 0) {
    // Skip over optional chunks.
    status = WebPParseOptionalChunks(&buf, &buf_size, riff_size,
                                     &optional_data_size);
    if (status != VP8_STATUS_OK) {
      return status;  // Found an invalid chunk size / Insufficient data.
    }
  }

  // Skip over VP8 chunk header.
  status = WebPParseVP8Header(&buf, &buf_size, riff_size, &vp8_skip_size,
                              &vp8_size_tmp);
  if (status != VP8_STATUS_OK) {
    return status;  // Invalid VP8 header / Insufficient data.
  }
  if (vp8_skip_size > 0) {
    *vp8_size = vp8_size_tmp;
  }

  *bytes_skipped = buf - *data;
  assert(*bytes_skipped == *data_size - buf_size);
  *data = buf;
  *data_size = buf_size;
  return VP8_STATUS_OK;
}

//------------------------------------------------------------------------------
// WebPDecParams

void WebPResetDecParams(WebPDecParams* const params) {
  if (params) {
    memset(params, 0, sizeof(*params));
  }
}

//------------------------------------------------------------------------------
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

#ifdef WEBP_USE_THREAD
  dec->use_threads_ = params->options && (params->options->use_threads > 0);
#else
  dec->use_threads_ = 0;
#endif

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

//------------------------------------------------------------------------------

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

static void DefaultFeatures(WebPBitstreamFeatures* const features) {
  assert(features);
  memset(features, 0, sizeof(*features));
  features->bitstream_version = 0;
}

static VP8StatusCode GetFeatures(const uint8_t* data, uint32_t data_size,
                                 WebPBitstreamFeatures* const features) {
  uint32_t vp8_chunk_size = 0;
  uint32_t riff_size = 0;
  uint32_t flags = 0;
  uint32_t vp8x_skip_size = 0;
  uint32_t vp8_skip_size = 0;
  VP8StatusCode status;

  if (features == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  DefaultFeatures(features);

  if (data == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }

  // Skip over RIFF header.
  status = WebPParseRIFF(&data, &data_size, &riff_size);
  if (status != VP8_STATUS_OK) {
    return status;   // Wrong RIFF Header / Insufficient data.
  }

  // Skip over VP8X.
  status = WebPParseVP8X(&data, &data_size, &vp8x_skip_size, &features->width,
                         &features->height, &flags);
  if (status != VP8_STATUS_OK) {
    return status;  // Wrong VP8X / insufficient data.

  }
  if (vp8x_skip_size > 0) {
    return VP8_STATUS_OK;  // Return features from VP8X header.
  }

  // Skip over VP8 header.
  status = WebPParseVP8Header(&data, &data_size, riff_size, &vp8_skip_size,
                              &vp8_chunk_size);
  if (status != VP8_STATUS_OK) {
    return status;  // Wrong VP8 Chunk-header / insufficient data.
  }
  if (vp8_skip_size == 0) {
    vp8_chunk_size = data_size;  // No VP8 chunk wrapper over raw VP8 data.
  }

  // Validates raw VP8 data.
  if (!VP8GetInfo(data, data_size, vp8_chunk_size,
                  &features->width, &features->height, &features->has_alpha)) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }

  return VP8_STATUS_OK;  // Return features from VP8 header.
}

//------------------------------------------------------------------------------
// WebPGetInfo()

int WebPGetInfo(const uint8_t* data, uint32_t data_size,
                int* width, int* height) {
  WebPBitstreamFeatures features;

  if (GetFeatures(data, data_size, &features) != VP8_STATUS_OK) {
    return 0;
  }

  if (width) {
    *width  = features.width;
  }
  if (height) {
    *height = features.height;
  }

  return 1;
}

//------------------------------------------------------------------------------
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
  VP8StatusCode status;
  if (version != WEBP_DECODER_ABI_VERSION) {
    return VP8_STATUS_INVALID_PARAM;   // version mismatch
  }
  if (features == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }

  status = GetFeatures(data, data_size, features);
  if (status == VP8_STATUS_NOT_ENOUGH_DATA) {
    return VP8_STATUS_BITSTREAM_ERROR;  // Not enough data treated as error.
  }
  return status;
}

VP8StatusCode WebPDecode(const uint8_t* data, uint32_t data_size,
                         WebPDecoderConfig* const config) {
  WebPDecParams params;
  VP8StatusCode status;

  if (!config) {
    return VP8_STATUS_INVALID_PARAM;
  }

  status = GetFeatures(data, data_size, &config->input);
  if (status != VP8_STATUS_OK) {
    if (status == VP8_STATUS_NOT_ENOUGH_DATA) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Not enough data treated as error.
    }
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
