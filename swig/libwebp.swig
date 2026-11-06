// Copyright 2011 Google Inc.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// libwebp swig interface definition
//
// Author: James Zern (jzern@google.com)

/*
  Go bindings:
  $ swig -go \
         -outdir . \
         -o libwebp_go_wrap.c libwebp.swig

  Java bindings:
  $ mkdir -p java/com/google/webp
  $ swig -java \
         -package com.google.webp \
         -outdir java/com/google/webp \
         -o libwebp_java_wrap.c libwebp.swig

  Python bindings:
  $ swig -python \
         -outdir . \
         -o libwebp_python_wrap.c libwebp.swig
*/

#ifdef SWIGPYTHON
%module(package="com.google.webp") libwebp
%begin %{
#define SWIG_PYTHON_STRICT_BYTE_CHAR
%}
#else
%module libwebp
#endif  /* SWIGPYTHON */

%include "constraints.i"
%include "typemaps.i"

#ifdef SWIGGO
%apply (char* STRING, size_t LENGTH) { (const uint8_t* data, size_t data_size) }

%rename(wrapped_WebPGetInfo) WebPGetInfo(const uint8_t* data, size_t data_size,
                                         int* width, int* height);
#endif  /* SWIGGO */

#ifdef SWIGJAVA
%include "arrays_java.i";
%include "enums.swg" /*NB: requires JDK-1.5+
                       See: http://www.swig.org/Doc1.3/Java.html#enumerations */

// map uint8_t* such that a byte[] is used
%{
#include "webp/types.h"
%}
// from arrays_java.i (signed char)
JAVA_ARRAYS_DECL(uint8_t, jbyte, Byte, Uint8)
JAVA_ARRAYS_IMPL(uint8_t, jbyte, Byte, Uint8)
JAVA_ARRAYS_TYPEMAPS(uint8_t, byte, jbyte, Uint8, "[B")
%apply uint8_t[] { uint8_t* }
%typemap(in) size_t data_size {
  if (!jarg1 || $input < 0 ||
      $input > (jlong)JCALL1(GetArrayLength, jenv, jarg1)) {
    $1 = 0;
  } else {
    $1 = ($1_ltype)$input;
  }
}
%typemap(jni) size_t unused1 "jint"
%typemap(jtype) size_t unused1 "int"
%typemap(jstype) size_t unused1 "int"
%typemap(javain) size_t unused1 "$javainput"
%typemap(in) size_t unused1 {
  (void)$input;
  $1 = jarg1 ? (size_t)JCALL1(GetArrayLength, jenv, jarg1) : 0;
}
#endif  /* SWIGJAVA */

#ifdef SWIGPYTHON
%apply (char* STRING, size_t LENGTH) { (const uint8_t* data, size_t data_size) }
%typemap(out) uint8_t* {
  $result = PyString_FromStringAndSize(
      (const char*)$1,
      ($1 == NULL) ? 0 : ReturnedBufferSize("$symname", arg3, arg4));
}

%typemap (in) const uint8_t* rgb (Py_buffer rgb_buffer) {
  // NB: with Python < 2.6 the old style buffer protocol may be used:
  // Py_ssize_t unused;
  // PyObject_AsReadBuffer($input, (const void**)(&$1), &unused);
  if (!PyObject_CheckBuffer($input)) {
    SWIG_exception_fail(SWIG_TypeError,
                        "in method '$symname', argument $argnum"
                        " does not support the buffer interface");
  }
  if (PyObject_GetBuffer($input, &rgb_buffer, PyBUF_SIMPLE)) {
    SWIG_exception_fail(SWIG_RuntimeError,
                        "in method '$symname', unable to get buffer view");
  }
  $1 = ($1_ltype)rgb_buffer.buf;
}

%typemap(freearg) const uint8_t* rgb {
  PyBuffer_Release(&rgb_buffer$argnum);
}

%typemap(in) size_t unused1 {
  (void)$input;
  $1 = (rgb_buffer1.len < 0) ? 0 : (size_t)rgb_buffer1.len;
}

%define DECODE_AUTODOC(func)
%feature("autodoc", #func "(uint8_t data) -> (rgb, width, height)") func;
%enddef

%feature("autodoc", "1");
DECODE_AUTODOC(WebPDecodeRGB);
DECODE_AUTODOC(WebPDecodeRGBA);
DECODE_AUTODOC(WebPDecodeARGB);
DECODE_AUTODOC(WebPDecodeBGR);
DECODE_AUTODOC(WebPDecodeBGRA);
%feature("autodoc", "WebPGetInfo(uint8_t data) -> (width, height)") WebPGetInfo;
#endif  /* SWIGPYTHON */

//------------------------------------------------------------------------------
// Decoder specific

%apply int* OUTPUT { int* width, int* height }

int WebPGetDecoderVersion(void);
int WebPGetInfo(const uint8_t* data, size_t data_size,
                int* width, int* height);

#if defined(SWIGJAVA) || defined(SWIGPYTHON)

// free the buffer returned by these functions after copying into
// the native type
%newobject WebPDecodeRGB;
%newobject WebPDecodeRGBA;
%newobject WebPDecodeARGB;
%newobject WebPDecodeBGR;
%newobject WebPDecodeBGRA;
%typemap(newfree) uint8_t* "free($1);"

uint8_t* WebPDecodeRGB(const uint8_t* data, size_t data_size,
                       int* width, int* height);
uint8_t* WebPDecodeRGBA(const uint8_t* data, size_t data_size,
                        int* width, int* height);
uint8_t* WebPDecodeARGB(const uint8_t* data, size_t data_size,
                        int* width, int* height);
uint8_t* WebPDecodeBGR(const uint8_t* data, size_t data_size,
                       int* width, int* height);
uint8_t* WebPDecodeBGRA(const uint8_t* data, size_t data_size,
                        int* width, int* height);

#endif  /* SWIGJAVA || SWIGPYTHON */

//------------------------------------------------------------------------------
// Encoder specific

#if defined(SWIGJAVA) || defined(SWIGPYTHON)

int WebPGetEncoderVersion(void);

#endif  /* SWIGJAVA || SWIGPYTHON */

//------------------------------------------------------------------------------
// Wrapper code additions

%{
#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"
%}

#ifdef SWIGJAVA
%{
#define FillMeInAsSizeCannotBeDeterminedAutomatically \
    (result ? (jint)ReturnedBufferSize(__FUNCTION__, arg3, arg4) : 0)
%}
#endif  /* SWIGJAVA */

#if defined(SWIGJAVA) || defined(SWIGPYTHON)
%{
static size_t ReturnedBufferSize(
    const char* function, int* width, int* height) {
  static const struct sizemap {
    const char* function;
    int size_multiplier;
  } size_map[] = {
#ifdef SWIGJAVA
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
#endif
#ifdef SWIGPYTHON
    { "WebPDecodeRGB",  3 },
    { "WebPDecodeRGBA", 4 },
    { "WebPDecodeARGB", 4 },
    { "WebPDecodeBGR",  3 },
    { "WebPDecodeBGRA", 4 },
    { "wrap_WebPEncodeRGB",  1 },
    { "wrap_WebPEncodeBGR",  1 },
    { "wrap_WebPEncodeRGBA", 1 },
    { "wrap_WebPEncodeBGRA", 1 },
    { "wrap_WebPEncodeLosslessRGB",  1 },
    { "wrap_WebPEncodeLosslessBGR",  1 },
    { "wrap_WebPEncodeLosslessRGBA", 1 },
    { "wrap_WebPEncodeLosslessBGRA", 1 },
#endif
    { NULL, 0 }
  };
  const struct sizemap* p;
  size_t size = 0;

  for (p = size_map; p->function; ++p) {
    if (!strcmp(function, p->function)) {
      size = *width * *height * p->size_multiplier;
      break;
    }
  }

  return size;
}
%}

%{
typedef size_t (*WebPEncodeFunction)(const uint8_t* rgb,
                                     int width, int height, int stride,
                                     float quality_factor, uint8_t** output);
typedef size_t (*WebPEncodeLosslessFunction)(const uint8_t* rgb,
                                             int width, int height, int stride,
                                             uint8_t** output);

static int ValidateEncodeParams(size_t rgb_size, int width, int height,
                                int stride, int bytes_per_pixel) {
  if (width <= 0 || height <= 0 || stride <= 0 || bytes_per_pixel <= 0) {
    return 0;
  }
  if ((uint64_t)stride < (uint64_t)width * bytes_per_pixel) {
    return 0;
  }
  return (uint64_t)rgb_size >=
         (uint64_t)(height - 1) * stride + (uint64_t)width * bytes_per_pixel;
}

static uint8_t* EncodeLossy(const uint8_t* rgb, size_t rgb_size,
                            int width, int height, int stride,
                            int bytes_per_pixel, float quality_factor,
                            WebPEncodeFunction encfn,
                            int* output_size, int* unused) {
  uint8_t* output = NULL;
  const size_t image_size =
      ValidateEncodeParams(rgb_size, width, height, stride, bytes_per_pixel)
          ? encfn(rgb, width, height, stride, quality_factor, &output)
          : 0;
  // the values of following two will be interpreted by ReturnedBufferSize()
  // as 'width' and 'height' in the size calculation.
  *output_size = image_size;
  *unused = 1;
  return image_size ? output : NULL;
}

static uint8_t* EncodeLossless(const uint8_t* rgb, size_t rgb_size,
                               int width, int height, int stride,
                               int bytes_per_pixel,
                               WebPEncodeLosslessFunction encfn,
                               int* output_size, int* unused) {
  uint8_t* output = NULL;
  const size_t image_size =
      ValidateEncodeParams(rgb_size, width, height, stride, bytes_per_pixel)
          ? encfn(rgb, width, height, stride, &output)
          : 0;
  // the values of the following two will be interpreted by
  // ReturnedBufferSize() as 'width' and 'height' in the size calculation.
  *output_size = image_size;
  *unused = 1;
  return image_size ? output : NULL;
}
%}

#endif  /* SWIGJAVA || SWIGPYTHON */

//------------------------------------------------------------------------------
// libwebp/encode wrapper functions

#if defined(SWIGJAVA) || defined(SWIGPYTHON)

%apply int* INPUT { int* unused2 }
%apply int* OUTPUT { int* output_size }

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

#ifdef SWIGPYTHON
// This autodoc will serve as a catch-all for wrap_*.
%feature("autodoc", "private, do not call directly.");
#endif

%inline %{
// Changes the return type of WebPEncode* to more closely match Decode*.
// This also makes it easier to wrap the output buffer in a native type rather
// than dealing with the return pointer.
// The additional parameters are to allow reuse of ReturnedBufferSize(),
// unused2 and output_size will be used in this case.
#define LOSSY_WRAPPER(FUNC, BPS)                                          \
  static uint8_t* wrap_##FUNC(                                            \
      const uint8_t* rgb, size_t unused1, int* unused2, int* output_size, \
      int width, int height, int stride, float quality_factor) {          \
    return EncodeLossy(rgb, unused1, width, height, stride, BPS,          \
                       quality_factor, FUNC, output_size, unused2);       \
  }                                                                       \

LOSSY_WRAPPER(WebPEncodeRGB, 3)
LOSSY_WRAPPER(WebPEncodeBGR, 3)
LOSSY_WRAPPER(WebPEncodeRGBA, 4)
LOSSY_WRAPPER(WebPEncodeBGRA, 4)

#undef LOSSY_WRAPPER

#define LOSSLESS_WRAPPER(FUNC, BPS)                                       \
  static uint8_t* wrap_##FUNC(                                            \
      const uint8_t* rgb, size_t unused1, int* unused2, int* output_size, \
      int width, int height, int stride) {                                \
    return EncodeLossless(rgb, unused1, width, height, stride, BPS,       \
                          FUNC, output_size, unused2);                    \
  }                                                                       \

LOSSLESS_WRAPPER(WebPEncodeLosslessRGB, 3)
LOSSLESS_WRAPPER(WebPEncodeLosslessBGR, 3)
LOSSLESS_WRAPPER(WebPEncodeLosslessRGBA, 4)
LOSSLESS_WRAPPER(WebPEncodeLosslessBGRA, 4)

#undef LOSSLESS_WRAPPER

%}

#endif  /* SWIGJAVA || SWIGPYTHON */

//------------------------------------------------------------------------------
// Language specific

#ifdef SWIGGO
%insert(go_wrapper) %{

// WebPGetInfo has 2 output parameters, provide a version in the more natural
// go idiom:
func WebPGetInfo(webp []byte) (ok bool, width int, height int) {
    w := []int{0}
    h := []int{0}
    ok = Wrapped_WebPGetInfo(string(webp), w, h) != 0
    width = w[0]
    height = h[0]
    return
}

%}
#endif  /* SWIGGO */

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
%}


%define CALL_ENCODE_LOSSY_WRAPPER(func)
%pragma(java) modulecode=%{
  public static byte[] func(
      byte[] rgb, int width, int height, int stride, float quality_factor) {
    return wrap_##func(
        rgb, UNUSED, UNUSED, outputSize, width, height, stride, quality_factor);
  }
%}
%enddef

%define CALL_ENCODE_LOSSLESS_WRAPPER(func)
%pragma(java) modulecode=%{
  public static byte[] func(
      byte[] rgb, int width, int height, int stride) {
    return wrap_##func(
        rgb, UNUSED, UNUSED, outputSize, width, height, stride);
  }
%}
%enddef

CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeRGB)
CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeRGBA)
CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeBGR)
CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeBGRA)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessRGB)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessRGBA)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessBGR)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessBGRA)
#endif  /* SWIGJAVA */

#ifdef SWIGPYTHON
%pythoncode %{
_UNUSED = 1
%}

%define CALL_ENCODE_LOSSY_WRAPPER(func)
%pythoncode %{
def func(rgb, width, height, stride, quality_factor):
  """func(uint8_t rgb, int width, int height, int stride, float quality_factor) -> lossy_webp"""
  webp = wrap_##func(
      rgb, _UNUSED, _UNUSED, width, height, stride, quality_factor)
  if len(webp[0]) == 0:
    return None
  return webp[0]
%}
%enddef

%define CALL_ENCODE_LOSSLESS_WRAPPER(func)
%pythoncode %{
def func(rgb, width, height, stride):
  """func(uint8_t rgb, int width, int height, int stride) -> lossless_webp"""
  webp = wrap_##func(rgb, _UNUSED, _UNUSED, width, height, stride)
  if len(webp[0]) == 0:
    return None
  return webp[0]
%}
%enddef

CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeRGB)
CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeRGBA)
CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeBGR)
CALL_ENCODE_LOSSY_WRAPPER(WebPEncodeBGRA)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessRGB)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessRGBA)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessBGR)
CALL_ENCODE_LOSSLESS_WRAPPER(WebPEncodeLosslessBGRA)
#endif  /* SWIGPYTHON */
