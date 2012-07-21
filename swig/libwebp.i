// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// libwebp swig interface definition
//
// Author: James Zern (jzern@google.com)
//
// For java bindings compile with:
//  $ mkdir -p java/com/google/webp
//  $ swig -ignoremissing -I../src \
//         -java \
//         -package com.google.webp \
//         -outdir java/com/google/webp \
//         -o libwebp_java_wrap.c libwebp.i
%module libwebp

%include "constraints.i"
%include "typemaps.i"

#ifdef SWIGJAVA
%include "arrays_java.i";
%include "enums.swg" /*NB: requires JDK-1.5+
                       See: http://www.swig.org/Doc1.3/Java.html#enumerations */

// map uint8_t* such that a byte[] is used
// this will generate a few spurious warnings in the wrapper code
%apply signed char[] { uint8_t * }
#endif  /* SWIGJAVA */

//------------------------------------------------------------------------------
// Decoder specific

%apply int *OUTPUT { int *width, int *height }

// free the buffer returned by these functions after copying into
// the native type
%newobject WebPDecodeRGB;
%newobject WebPDecodeRGBA;
%newobject WebPDecodeARGB;
%newobject WebPDecodeBGR;
%newobject WebPDecodeBGRA;
%typemap(newfree) uint8_t* "free($1);"

int WebPGetDecoderVersion(void);
int WebPGetInfo(const uint8_t* data, size_t data_size,
                int *width, int *height);

uint8_t* WebPDecodeRGB(const uint8_t* data, size_t data_size,
                       int *width, int *height);
uint8_t* WebPDecodeRGBA(const uint8_t* data, size_t data_size,
                        int *width, int *height);
uint8_t* WebPDecodeARGB(const uint8_t* data, size_t data_size,
                        int* width, int* height);
uint8_t* WebPDecodeBGR(const uint8_t* data, size_t data_size,
                       int *width, int *height);
uint8_t* WebPDecodeBGRA(const uint8_t* data, size_t data_size,
                        int *width, int *height);

//------------------------------------------------------------------------------
// Encoder specific

int WebPGetEncoderVersion(void);

//------------------------------------------------------------------------------
// Wrapper code additions

%{
#include "webp/decode.h"
#include "webp/encode.h"

#define FillMeInAsSizeCannotBeDeterminedAutomatically \
    (result ? returned_buffer_size(__FUNCTION__, arg3, arg4) : 0)

static jint returned_buffer_size(
    const char *function, int *width, int *height) {
  static const struct sizemap {
    const char *function;
    int size_multiplier;
  } size_map[] = {
    { "Java_com_google_webp_libwebpJNI_WebPDecodeRGB",  3 },
    { "Java_com_google_webp_libwebpJNI_WebPDecodeRGBA", 4 },
    { "Java_com_google_webp_libwebpJNI_WebPDecodeARGB", 4 },
    { "Java_com_google_webp_libwebpJNI_WebPDecodeBGR",  3 },
    { "Java_com_google_webp_libwebpJNI_WebPDecodeBGRA", 4 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeRGB",  1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeBGR",  1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeRGBA", 1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeBGRA", 1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeLosslessRGB",  1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeLosslessBGR",  1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeLosslessRGBA", 1 },
    { "Java_com_google_webp_libwebpJNI_wrap_1WebPEncodeLosslessBGRA", 1 },
    { NULL, 0 }
  };
  const struct sizemap *p;
  jint size = -1;

  for (p = size_map; p->function; p++) {
    if (!strcmp(function, p->function)) {
      size = *width * *height * p->size_multiplier;
      break;
    }
  }

  return size;
}

typedef size_t (*WebPEncodeFunction)(const uint8_t* rgb,
                                     int width, int height, int stride,
                                     float quality_factor, uint8_t** output);
typedef size_t (*WebPEncodeLosslessFunction)(const uint8_t* rgb,
                                             int width, int height, int stride,
                                             uint8_t** output);

static uint8_t* encode(const uint8_t* rgb,
                       int width, int height, int stride,
                       float quality_factor,
                       WebPEncodeFunction encfn,
                       int* output_size, int* unused) {
  uint8_t *output = NULL;
  const size_t image_size =
      encfn(rgb, width, height, stride, quality_factor, &output);
  // the values of following two will be interpreted by returned_buffer_size()
  // as 'width' and 'height' in the size calculation.
  *output_size = image_size;
  *unused = 1;
  return image_size ? output : NULL;
}

static uint8_t* encode_lossless(const uint8_t* rgb,
                                int width, int height, int stride,
                                WebPEncodeLosslessFunction encfn,
                                int* output_size, int* unused) {
  uint8_t *output = NULL;
  const size_t image_size = encfn(rgb, width, height, stride, &output);
  // the values of following two will be interpreted by returned_buffer_size()
  // as 'width' and 'height' in the size calculation.
  *output_size = image_size;
  *unused = 1;
  return image_size ? output : NULL;
}
%}

//------------------------------------------------------------------------------
// libwebp/encode wrapper functions

%apply int *INPUT { int *unused1, int *unused2 }
%apply int *OUTPUT { int *output_size }

// free the buffer returned by these functions after copying into
// the native type
%newobject wrap_WebPEncodeRGB;
%newobject wrap_WebPEncodeBGR;
%newobject wrap_WebPEncodeRGBA;
%newobject wrap_WebPEncodeBGRA;
%newobject wrap_WebPEncodeLosslessRGB;
%newobject wrap_WebPEncodeLosslessBGR;
%newobject wrap_WebPEncodeLosslessRGBA;
%newobject wrap_WebPEncodeLosslessBGRA;

#ifdef SWIGJAVA
// There's no reason to call these directly
%javamethodmodifiers wrap_WebPEncodeRGB "private";
%javamethodmodifiers wrap_WebPEncodeBGR "private";
%javamethodmodifiers wrap_WebPEncodeRGBA "private";
%javamethodmodifiers wrap_WebPEncodeBGRA "private";
%javamethodmodifiers wrap_WebPEncodeLosslessRGB "private";
%javamethodmodifiers wrap_WebPEncodeLosslessBGR "private";
%javamethodmodifiers wrap_WebPEncodeLosslessRGBA "private";
%javamethodmodifiers wrap_WebPEncodeLosslessBGRA "private";
#endif  /* SWIGJAVA */

%inline %{
// Changes the return type of WebPEncode* to more closely match Decode*.
// This also makes it easier to wrap the output buffer in a native type rather
// than dealing with the return pointer.
// The additional parameters are to allow reuse of returned_buffer_size(),
// unused2 and output_size will be used in this case.
#define LOSSY_WRAPPER(FUNC)                                             \
  static uint8_t* wrap_##FUNC(                                          \
      const uint8_t* rgb, int* unused1, int* unused2, int* output_size, \
      int width, int height, int stride, float quality_factor) {        \
    return encode(rgb, width, height, stride, quality_factor,           \
                  FUNC, output_size, unused2);                          \
  }                                                                     \

LOSSY_WRAPPER(WebPEncodeRGB)
LOSSY_WRAPPER(WebPEncodeBGR)
LOSSY_WRAPPER(WebPEncodeRGBA)
LOSSY_WRAPPER(WebPEncodeBGRA)

#undef LOSSY_WRAPPER

#define LOSSLESS_WRAPPER(FUNC)                                          \
  static uint8_t* wrap_##FUNC(                                          \
      const uint8_t* rgb, int* unused1, int* unused2, int* output_size, \
      int width, int height, int stride) {                              \
    return encode_lossless(rgb, width, height, stride,                  \
                           FUNC, output_size, unused2);                 \
  }                                                                     \

LOSSLESS_WRAPPER(WebPEncodeLosslessRGB)
LOSSLESS_WRAPPER(WebPEncodeLosslessBGR)
LOSSLESS_WRAPPER(WebPEncodeLosslessRGBA)
LOSSLESS_WRAPPER(WebPEncodeLosslessBGRA)

#undef LOSSLESS_WRAPPER

%}

//------------------------------------------------------------------------------
// Language specific

#ifdef SWIGJAVA
%{
/* Work around broken gcj jni.h */
#ifdef __GCJ_JNI_H__
# undef JNIEXPORT
# define JNIEXPORT
# undef JNICALL
# define JNICALL
#endif
%}

%pragma(java) modulecode=%{
  private static final int UNUSED = 1;
  private static int outputSize[] = { 0 };

  public static byte[] WebPEncodeRGB(byte[] rgb,
                                     int width, int height, int stride,
                                     float quality_factor) {
    return wrap_WebPEncodeRGB(
        rgb, UNUSED, UNUSED, outputSize, width, height, stride, quality_factor);
  }

  public static byte[] WebPEncodeBGR(byte[] bgr,
                                     int width, int height, int stride,
                                     float quality_factor) {
    return wrap_WebPEncodeBGR(
        bgr, UNUSED, UNUSED, outputSize, width, height, stride, quality_factor);
  }

  public static byte[] WebPEncodeRGBA(byte[] rgba,
                                      int width, int height, int stride,
                                      float quality_factor) {
    return wrap_WebPEncodeRGBA(
        rgba, UNUSED, UNUSED, outputSize, width, height, stride, quality_factor);
  }

  public static byte[] WebPEncodeBGRA(byte[] bgra,
                                      int width, int height, int stride,
                                      float quality_factor) {
    return wrap_WebPEncodeBGRA(
        bgra, UNUSED, UNUSED, outputSize, width, height, stride, quality_factor);
  }

  public static byte[] WebPEncodeLosslessRGB(
      byte[] rgb, int width, int height, int stride) {
    return wrap_WebPEncodeLosslessRGB(
        rgb, UNUSED, UNUSED, outputSize, width, height, stride);
  }

  public static byte[] WebPEncodeLosslessBGR(
      byte[] bgr, int width, int height, int stride) {
    return wrap_WebPEncodeLosslessBGR(
        bgr, UNUSED, UNUSED, outputSize, width, height, stride);
  }

  public static byte[] WebPEncodeLosslessRGBA(
      byte[] rgba, int width, int height, int stride) {
    return wrap_WebPEncodeLosslessRGBA(
        rgba, UNUSED, UNUSED, outputSize, width, height, stride);
  }

  public static byte[] WebPEncodeLosslessBGRA(
      byte[] bgra, int width, int height, int stride) {
    return wrap_WebPEncodeLosslessBGRA(
        bgra, UNUSED, UNUSED, outputSize, width, height, stride);
  }
%}
#endif  /* SWIGJAVA */
