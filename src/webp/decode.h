// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Main decoding functions for WEBP images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_WEBP_DECODE_H_
#define WEBP_WEBP_DECODE_H_

#include "webp/types.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// Retrieve basic header information: width, height.
// This function will also validate the header and return 0 in
// case of formatting error.
// Pointers *width/*height can be passed NULL if deemed irrelevant.
int WebPGetInfo(const uint8_t* data, uint32_t data_size,
                int *width, int *height);

// Decodes WEBP images pointed to by *data and returns RGB samples, along
// with the dimensions in *width and *height.
// The returned pointer should be deleted calling free().
// Returns NULL in case of error.
uint8_t* WebPDecodeRGB(const uint8_t* data, uint32_t data_size,
                       int *width, int *height);

// Same as WebPDecodeRGB, but returning RGBA data.
uint8_t* WebPDecodeRGBA(const uint8_t* data, uint32_t data_size,
                        int *width, int *height);

// This variant decode to BGR instead of RGB.
uint8_t* WebPDecodeBGR(const uint8_t* data, uint32_t data_size,
                       int *width, int *height);
// This variant decodes to BGRA instead of RGBA.
uint8_t* WebPDecodeBGRA(const uint8_t* data, uint32_t data_size,
                        int *width, int *height);

// Decode WEBP images stored in *data in Y'UV format(*). The pointer returned is
// the Y samples buffer. Upon return, *u and *v will point to the U and V
// chroma data. These U and V buffers need NOT be free()'d, unlike the returned
// Y luma one. The dimension of the U and V planes are both (*width + 1) / 2
// and (*height + 1)/ 2.
// Upon return, the Y buffer has a stride returned as '*stride', while U and V
// have a common stride returned as '*uv_stride'.
// Return NULL in case of error.
// (*) Also named Y'CbCr. See: http://en.wikipedia.org/wiki/YCbCr
uint8_t* WebPDecodeYUV(const uint8_t* data, uint32_t data_size,
                       int *width, int *height, uint8_t** u, uint8_t** v,
                       int *stride, int* uv_stride);

// These three functions are variants of the above ones, that decode the image
// directly into a pre-allocated buffer 'output_buffer'. The maximum storage
// available in this buffer is indicated by 'output_buffer_size'. If this
// storage is not sufficient (or an error occurred), NULL is returned.
// Otherwise, output_buffer is returned, for convenience.
// The parameter 'output_stride' specifies the distance (in bytes)
// between scanlines. Hence, output_buffer_size is expected to be at least
// output_stride x picture-height.
uint8_t* WebPDecodeRGBInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* output_buffer, int output_buffer_size,
                           int output_stride);
uint8_t* WebPDecodeRGBAInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output_buffer, int output_buffer_size,
                            int output_stride);
// BGR variants
uint8_t* WebPDecodeBGRInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* output_buffer, int output_buffer_size,
                           int output_stride);
uint8_t* WebPDecodeBGRAInto(const uint8_t* data, uint32_t data_size,
                            uint8_t* output_buffer, int output_buffer_size,
                            int output_stride);

// WebPDecodeYUVInto() is a variant of WebPDecodeYUV() that operates directly
// into pre-allocated luma/chroma plane buffers. This function requires the
// strides to be passed: one for the luma plane and one for each of the
// chroma ones. The size of each plane buffer is passed as 'luma_size',
// 'u_size' and 'v_size' respectively.
// Pointer to the luma plane ('*luma') is returned or NULL if an error occurred
// during decoding (or because some buffers were found to be too small).
uint8_t* WebPDecodeYUVInto(const uint8_t* data, uint32_t data_size,
                           uint8_t* luma, int luma_size, int luma_stride,
                           uint8_t* u, int u_size, int u_stride,
                           uint8_t* v, int v_size, int v_stride);

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  /* WEBP_WEBP_DECODE_H_ */
