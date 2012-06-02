// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  WebP container demux.
//

#include "../webp/mux.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

WebPDemuxer* WebPDemuxInternal(
    const WebPData* const data, int allow_partial,
    WebPDemuxState* const state, int version) {
  (void)data;
  (void)allow_partial;
  (void)state;
  (void)version;
  return NULL;
}

void WebPDemuxDelete(WebPDemuxer* const dmux) {
  (void)dmux;
}

#if defined(__cplusplus) || defined(c_plusplus)
}  // extern "C"
#endif
