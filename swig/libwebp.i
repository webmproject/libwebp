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
%apply int { uint32_t data_size }
%apply Number NONNEGATIVE { uint32_t data_size }

// free the buffer returned by these functions after copying into
// the native type
%newobject WebPDecodeRGB;
%newobject WebPDecodeRGBA;
%newobject WebPDecodeARGB;
%newobject WebPDecodeBGR;
%newobject WebPDecodeBGRA;
%typemap(newfree) uint8_t* "free($1);"

int WebPGetDecoderVersion(void);
int WebPGetInfo(const uint8_t* data, uint32_t data_size,
                int *width, int *height);

uint8_t* WebPDecodeRGB(const uint8_t* data, uint32_t data_size,
                       int *width, int *height);
uint8_t* WebPDecodeRGBA(const uint8_t* data, uint32_t data_size,
                        int *width, int *height);
uint8_t* WebPDecodeARGB(const uint8_t* data, uint32_t data_size,
                        int* width, int* height);
uint8_t* WebPDecodeBGR(const uint8_t* data, uint32_t data_size,
                       int *width, int *height);
uint8_t* WebPDecodeBGRA(const uint8_t* data, uint32_t data_size,
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

#ifdef SWIGJAVA
// There's no reason to call these directly
%javamethodmodifiers wrap_WebPEncodeRGB "private";
%javamethodmodifiers wrap_WebPEncodeBGR "private";
%javamethodmodifiers wrap_WebPEncodeRGBA "private";
%javamethodmodifiers wrap_WebPEncodeBGRA "private";
#endif  /* SWIGJAVA */

%inline %{
// Changes the return type of WebPEncode* to more closely match Decode*.
// This also makes it easier to wrap the output buffer in a native type rather
// than dealing with the return pointer.
// The additional parameters are to allow reuse of returned_buffer_size(),
// unused2 and output_size will be used in this case.
static uint8_t* wrap_WebPEncodeRGB(
    const uint8_t* rgb, int* unused1, int* unused2, int* output_size,
    int width, int height, int stride, float quality_factor) {
  return encode(rgb, width, height, stride, quality_factor,
                WebPEncodeRGB, output_size, unused2);
}

static uint8_t* wrap_WebPEncodeBGR(
    const uint8_t* bgr, int* unused1, int* unused2, int* output_size,
    int width, int height, int stride, float quality_factor) {
  return encode(bgr, width, height, stride, quality_factor,
                WebPEncodeBGR, output_size, unused2);
}

static uint8_t* wrap_WebPEncodeRGBA(
    const uint8_t* rgba, int* unused1, int* unused2, int* output_size,
    int width, int height, int stride, float quality_factor) {
  return encode(rgba, width, height, stride, quality_factor,
                WebPEncodeRGBA, output_size, unused2);
}

static uint8_t* wrap_WebPEncodeBGRA(
    const uint8_t* bgra, int* unused1, int* unused2, int* output_size,
    int width, int height, int stride, float quality_factor) {
  return encode(bgra, width, height, stride, quality_factor,
                WebPEncodeBGRA, output_size, unused2);
}
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
%}
#endif  /* SWIGJAVA */
