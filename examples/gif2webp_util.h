// Copyright 2013 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Helper structs and methods for gif2webp tool.
//
// Author: Urvang (urvang@google.com)

#ifndef WEBP_EXAMPLES_GIF2WEBP_UTIL_H_
#define WEBP_EXAMPLES_GIF2WEBP_UTIL_H_

#include <stdlib.h>

#include "webp/mux.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Frame cache.

typedef struct WebPFrameCache WebPFrameCache;

// Given the minimum distance between key frames 'kmin' and maximum distance
// between key frames 'kmax', returns an appropriately allocated cache object.
// Use WebPFrameCacheDelete() to deallocate the 'cache'.
WebPFrameCache* WebPFrameCacheNew(size_t kmin, size_t kmax);

// Release all the frame data from 'cache' and free 'cache'.
void WebPFrameCacheDelete(WebPFrameCache* const cache);

// Add encoded frame in the cache. 'sub_frame_info' and 'sub_frame_pic' are used
// to encode the frame rectangle, while 'key_frame_info' and 'key_frame_pic' are
// used to encode the key frame. Either 'sub_frame_pic' (and 'sub_frame_info')
// or 'key_frame_pic' (and 'key_frame_info') can be NULL; in which case the
// corresponding variant will be omitted.
// Returns true on success.
int WebPFrameCacheAddFrame(WebPFrameCache* const cache,
                           const WebPConfig* const config,
                           const WebPMuxFrameInfo* const sub_frame_info,
                           WebPPicture* const sub_frame_pic,
                           const WebPMuxFrameInfo* const key_frame_info,
                           WebPPicture* const key_frame_pic);

// Flush the *ready* frames from cache and add them to 'mux'. If 'verbose' is
// true, prints the information about these frames.
WebPMuxError WebPFrameCacheFlush(WebPFrameCache* const cache, int verbose,
                                 WebPMux* const mux);

// Similar to 'WebPFrameCacheFlushFrames()', but flushes *all* the frames.
WebPMuxError WebPFrameCacheFlushAll(WebPFrameCache* const cache, int verbose,
                                    WebPMux* const mux);

// Returns true if subsequent call to WebPFrameCacheAddFrame() should
// incorporate a potential keyframe.
int WebPFrameCacheShouldTryKeyFrame(const WebPFrameCache* const cache);

//------------------------------------------------------------------------------
// Frame rectangle and related utilities.

#define TRANSPARENT_COLOR    0x00ffffff

typedef struct {
  int x_offset, y_offset, width, height;
} WebPFrameRect;

struct WebPPicture;

// Clear pixels in 'picture' within given 'rect' to transparent color.
void WebPUtilClearPic(struct WebPPicture* const picture,
                      const WebPFrameRect* const rect);

// Copy pixels from 'src' to 'dst' honoring strides. 'src' and 'dst' are assumed
// to be already allocated.
void WebPUtilCopyPixels(const struct WebPPicture* const src,
                        WebPPicture* const dst);

// Given 'src' picture and its frame rectangle 'rect', blend it into 'dst'.
void WebPUtilBlendPixels(const struct WebPPicture* const src,
                         const WebPFrameRect* const src_rect,
                         struct WebPPicture* const dst);

// Replace transparent pixels within 'dst_rect' of 'dst' by those in the 'src'.
void WebPUtilReduceTransparency(const struct WebPPicture* const src,
                                const WebPFrameRect* const dst_rect,
                                struct WebPPicture* const dst);

// Replace similar blocks of pixels by a 'see-through' transparent block
// with uniform average color.
void WebPUtilFlattenSimilarBlocks(const WebPPicture* const src,
                                  const WebPFrameRect* const rect,
                                  WebPPicture* const dst);

//------------------------------------------------------------------------------
// Key frame related.

// Returns true if 'curr' frame with frame rectangle 'curr_rect' is a key frame,
// that is, it can be decoded independently of 'prev' canvas.
int WebPUtilIsKeyFrame(const WebPPicture* const curr,
                       const WebPFrameRect* const curr_rect,
                       const WebPPicture* const prev);

// Given 'prev' frame and current frame rectangle 'rect', convert 'curr' frame
// to a key frame.
void WebPUtilConvertToKeyFrame(const WebPPicture* const prev,
                               WebPFrameRect* const rect,
                               WebPPicture* const curr);

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_EXAMPLES_GIF2WEBP_UTIL_H_
