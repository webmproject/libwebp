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

// Return the decoder's version number, packed in hexadecimal using 8bits for
// each of major/minor/revision. E.g: v2.5.7 is 0x020507.
int WebPGetDecoderVersion(void);

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

// Output colorspaces
typedef enum { MODE_RGB = 0, MODE_RGBA = 1,
               MODE_BGR = 2, MODE_BGRA = 3,
               MODE_YUV = 4 } WEBP_CSP_MODE;

// Enumeration of the status codes
typedef enum {
  VP8_STATUS_OK = 0,
  VP8_STATUS_OUT_OF_MEMORY,
  VP8_STATUS_INVALID_PARAM,
  VP8_STATUS_BITSTREAM_ERROR,
  VP8_STATUS_UNSUPPORTED_FEATURE,
  VP8_STATUS_SUSPENDED,
  VP8_STATUS_USER_ABORT,
  VP8_STATUS_NOT_ENOUGH_DATA
} VP8StatusCode;

//-----------------------------------------------------------------------------
// Incremental decoding
//
//  This API allows streamlined decoding of partial data.
//  Picture can be incrementally decoded as data become available thanks to the
// WebPIDecoder object. This object can be left in a SUSPENDED state if the
// picture is only partially decoded, pending additional input.
// Code example:
//
//   WebPIDecoder* const idec = WebPINew(mode);
//   while (has_more_data) {
//     // ... (get additional data)
//     status = WebPIAppend(idec, new_data, new_data_size);
//     if (status != VP8_STATUS_SUSPENDED ||
//       break;
//     }
//
//     // The above call decodes the current available buffer.
//     // Part of the image can now be refreshed by calling to
//     // WebPIDecGetRGB()/WebPIDecGetYUV() etc.
//   }
//   WebPIDelete(idec);

typedef struct WebPIDecoder WebPIDecoder;

// Creates a WebPIDecoder object. Returns NULL in case of failure.
WebPIDecoder* WebPINew(WEBP_CSP_MODE mode);

// This function allocates and initializes an incremental-decoder object, which
// will output the r/g/b(/a) samples specified by 'mode' into a preallocated
// buffer 'output_buffer'. The size of this buffer is at least
// 'output_buffer_size' and the stride (distance in bytes between two scanlines)
// is specified by 'output_stride'. Returns NULL if the allocation failed.
WebPIDecoder* WebPINewRGB(WEBP_CSP_MODE mode, uint8_t* output_buffer,
                          int output_buffer_size, int output_stride);

// This function allocates and initializes an incremental-decoder object, which
// will output the raw luma/chroma samples into a preallocated planes. The luma
// plane is specified by its pointer 'luma', its size 'luma_size' and its stride
// 'luma_stride'. Similarly, the chroma-u plane is specified by the 'u',
// 'u_size' and 'u_stride' parameters, and the chroma-v plane by 'v', 'v_size'
// and 'v_size'.
// Returns NULL if the allocation failed.
WebPIDecoder* WebPINewYUV(uint8_t* luma, int luma_size, int luma_stride,
                          uint8_t* u, int u_size, int u_stride,
                          uint8_t* v, int v_size, int v_stride);

// Deletes the WebpBuffer object and associated memory. Must always be called
// if WebPINew, WebPINewRGB or WebPINewYUV succeeded.
void WebPIDelete(WebPIDecoder* const idec);

// Copies and decodes the next available data. Returns VP8_STATUS_OK when
// the image is successfully decoded. Returns VP8_STATUS_SUSPENDED when more
// data is expected. Returns error in other cases.
VP8StatusCode WebPIAppend(WebPIDecoder* const idec, const uint8_t* data,
                          uint32_t data_size);

// A variant of the above function to be used when data buffer contains
// partial data from the beginning. In this case data buffer is not copied
// to the internal memory.
// Note that the value of the 'data' pointer can change between calls to
// WebPIUpdate, for instance when the data buffer is resized to fit larger data.
VP8StatusCode WebPIUpdate(WebPIDecoder* const idec, const uint8_t* data,
                          uint32_t data_size);

// Returns the RGB image decoded so far. Returns NULL if output params are not
// initialized yet. *last_y is the index of last decoded row in raster scan
// order. Some pointers (*last_y, *width etc.) can be NULL if corresponding
// information is not needed.
uint8_t* WebPIDecGetRGB(const WebPIDecoder* const idec, int *last_y,
                        int* width, int* height, int* stride);

// Same as above function to get YUV image. Returns pointer to the luma plane
// or NULL in case of error.
uint8_t* WebPIDecGetYUV(const WebPIDecoder* const idec, int* last_y,
                        uint8_t** u, uint8_t** v,
                        int* width, int* height, int* stride, int* uv_stride);


#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  /* WEBP_WEBP_DECODE_H_ */
