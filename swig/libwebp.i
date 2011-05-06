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

%ignore WEBP_WEBP_DECODE_H_;
// FIXME for these to be available returned_buffer_size() would need to be
// made more intelligent.
%ignore WebPDecodeRGBInto;
%ignore WebPDecodeYUV;
%ignore WebPDecodeRGBAInto;
%ignore WebPDecodeBGRInto;
%ignore WebPDecodeBGRAInto;
%ignore WebPDecodeYUVInto;

// incremental decoding
%ignore WebPIDecGetYUV;
%ignore WebPINew;
%ignore WebPIDecoder;
%ignore WebPIAppend;
%ignore WebPIDecGetRGB;
%ignore WebPIDelete;
%ignore WebPINewRGB;
%ignore WebPINewYUV;
%ignore WebPIUpdate;
%ignore WebPInitCustomIo;
%ignore WebPInitDecParams;

%apply int *OUTPUT { int *width, int *height }
%apply int { uint32_t data_size }
%apply Number NONNEGATIVE { uint32_t data_size }

// free the buffer returned by these functions after copying into
// the native type
%newobject WebPDecodeRGB;
%newobject WebPDecodeRGBA;
%newobject WebPDecodeBGR;
%newobject WebPDecodeBGRA;
%typemap(newfree) uint8_t* "free($1);"

#ifdef SWIGJAVA
%include "arrays_java.i";
%include "enums.swg" /*NB: requires JDK-1.5+
                       See: http://www.swig.org/Doc1.3/Java.html#enumerations */

// map uint8_t* such that a byte[] is used
// this will generate a few spurious warnings in the wrapper code
%apply signed char[] { uint8_t * }

%{
/* Work around broken gcj jni.h */
#ifdef __GCJ_JNI_H__
# undef JNIEXPORT
# define JNIEXPORT
# undef JNICALL
# define JNICALL
#endif
%}
#endif

/*
 * Wrapper code additions
 */
%{
#include "webp/decode.h"

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
    { "Java_com_google_webp_libwebpJNI_WebPDecodeBGR",  3 },
    { "Java_com_google_webp_libwebpJNI_WebPDecodeBGRA", 4 },
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
%}

// All functions, constants, etc. not named above in %ignore will be wrapped
%include "webp/decode.h"
