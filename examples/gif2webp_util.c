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

#include <assert.h>
#include <stdio.h>

#include "webp/encode.h"
#include "./gif2webp_util.h"

#define DELTA_INFINITY      1ULL << 32
#define KEYFRAME_NONE       -1

//------------------------------------------------------------------------------
// Encoded frame.

// Used to store two candidates of encoded data for an animation frame. One of
// the two will be chosen later.
typedef struct {
  WebPMuxFrameInfo sub_frame;  // Encoded frame rectangle.
  WebPMuxFrameInfo key_frame;  // Encoded frame if it was converted to keyframe.
} EncodedFrame;

// Release the data contained by 'encoded_frame'.
static void FrameRelease(EncodedFrame* const encoded_frame) {
  WebPDataClear(&encoded_frame->sub_frame.bitstream);
  WebPDataClear(&encoded_frame->key_frame.bitstream);
  memset(encoded_frame, 0, sizeof(*encoded_frame));
}

//------------------------------------------------------------------------------
// Frame cache.

// Used to store encoded frames that haven't been output yet.
struct WebPFrameCache {
  EncodedFrame* encoded_frames;  // Array of encoded frames.
  size_t size;               // Number of allocated data elements.
  size_t start;              // Start index.
  size_t count;              // Number of valid data elements.
  int flush_count;           // If >0, ‘flush_count’ frames starting from
                             // 'start' are ready to be added to mux.
  int64_t best_delta;        // min(canvas size - frame size) over the frames.
                             // Can be negative in certain cases due to
                             // transparent pixels in a frame.
  int keyframe;              // Index of selected keyframe relative to 'start'.

  size_t kmin;                   // Min distance between key frames.
  size_t kmax;                   // Max distance between key frames.
  size_t count_since_key_frame;  // Frames seen since the last key frame.
};

// Reset the counters in the cache struct. Doesn't touch 'cache->encoded_frames'
// and 'cache->size'.
static void CacheReset(WebPFrameCache* const cache) {
  cache->start = 0;
  cache->count = 0;
  cache->flush_count = 0;
  cache->best_delta = DELTA_INFINITY;
  cache->keyframe = KEYFRAME_NONE;
}

WebPFrameCache* WebPFrameCacheNew(size_t kmin, size_t kmax) {
  WebPFrameCache* cache = (WebPFrameCache*)malloc(sizeof(*cache));
  if (cache == NULL) return NULL;
  CacheReset(cache);
  cache->kmin = kmin;
  cache->kmax = kmax;
  cache->count_since_key_frame = 0;
  assert(kmax > kmin);
  cache->size = kmax - kmin;
  cache->encoded_frames =
      (EncodedFrame*)calloc(cache->size, sizeof(*cache->encoded_frames));
  if (cache->encoded_frames == NULL) {
    free(cache);
    return NULL;
  }
  return cache;
}

void WebPFrameCacheDelete(WebPFrameCache* const cache) {
  if (cache != NULL) {
    size_t i;
    for (i = 0; i < cache->size; ++i) {
      FrameRelease(&cache->encoded_frames[i]);
    }
    free(cache->encoded_frames);
    free(cache);
  }
}

static int EncodeFrame(const WebPConfig* const config, WebPPicture* const pic,
                       WebPData* const encoded_data) {
  WebPMemoryWriter memory;
  pic->use_argb = 1;
  pic->writer = WebPMemoryWrite;
  pic->custom_ptr = &memory;
  WebPMemoryWriterInit(&memory);
  if (!WebPEncode(config, pic)) {
    return 0;
  }
  encoded_data->bytes = memory.mem;
  encoded_data->size  = memory.size;
  return 1;
}

// Returns cached frame at given 'position' index.
static EncodedFrame* CacheGetFrame(const WebPFrameCache* const cache,
                                   size_t position) {
  assert(cache->start + position < cache->size);
  return &cache->encoded_frames[cache->start + position];
}

// Calculate the penalty incurred if we encode given frame as a key frame
// instead of a sub-frame.
static int64_t KeyFramePenalty(const EncodedFrame* const encoded_frame) {
  return ((int64_t)encoded_frame->key_frame.bitstream.size -
          encoded_frame->sub_frame.bitstream.size);
}

static int SetFrame(const WebPConfig* const config,
                    const WebPMuxFrameInfo* const info, WebPPicture* const pic,
                    WebPMuxFrameInfo* const dst) {
  *dst = *info;
  if (!EncodeFrame(config, pic, &dst->bitstream)) {
    return 0;
  }
  return 1;
}

int WebPFrameCacheAddFrame(WebPFrameCache* const cache,
                           const WebPConfig* const config,
                           const WebPMuxFrameInfo* const sub_frame_info,
                           WebPPicture* const sub_frame_pic,
                           const WebPMuxFrameInfo* const key_frame_info,
                           WebPPicture* const key_frame_pic) {
  const size_t position = cache->count;
  EncodedFrame* const encoded_frame = CacheGetFrame(cache, position);
  assert(position < cache->size);
  assert(sub_frame_pic != NULL || key_frame_pic != NULL);
  if (sub_frame_pic != NULL && !SetFrame(config, sub_frame_info, sub_frame_pic,
                                         &encoded_frame->sub_frame)) {
    return 0;
  }
  if (key_frame_pic != NULL && !SetFrame(config, key_frame_info, key_frame_pic,
                                         &encoded_frame->key_frame)) {
    return 0;
  }

  ++cache->count;

  if (sub_frame_pic == NULL && key_frame_pic != NULL) {  // Keyframe.
    cache->keyframe = position;
    cache->flush_count = cache->count;
    cache->count_since_key_frame = 0;
  } else {
    ++cache->count_since_key_frame;
    if (sub_frame_pic != NULL && key_frame_pic == NULL) {  // Non-keyframe.
      assert(cache->count_since_key_frame < cache->kmax);
      cache->flush_count = cache->count;
    } else {  // Analyze size difference of the two variants.
      const int64_t curr_delta = KeyFramePenalty(encoded_frame);
      if (curr_delta <= cache->best_delta) {  // Pick this as keyframe.
        cache->keyframe = position;
        cache->best_delta = curr_delta;
        cache->flush_count = cache->count - 1;  // We can flush previous frames.
      }
      if (cache->count_since_key_frame == cache->kmax) {
        cache->flush_count = cache->count;
        cache->count_since_key_frame = 0;
      }
    }
  }

  return 1;
}

WebPMuxError WebPFrameCacheFlush(WebPFrameCache* const cache, int verbose,
                                 WebPMux* const mux) {
  while (cache->flush_count > 0) {
    WebPMuxFrameInfo* info;
    WebPMuxError err;
    EncodedFrame* const curr = CacheGetFrame(cache, 0);
    // Pick frame or full canvas.
    if (cache->keyframe == 0) {
      info = &curr->key_frame;
      info->blend_method = WEBP_MUX_NO_BLEND;
      cache->keyframe = KEYFRAME_NONE;
      cache->best_delta = DELTA_INFINITY;
    } else {
      info = &curr->sub_frame;
      info->blend_method = WEBP_MUX_BLEND;
    }
    // Add to mux.
    err = WebPMuxPushFrame(mux, info, 1);
    if (err != WEBP_MUX_OK) return err;
    if (verbose) {
      printf("Added frame. offset:%d,%d duration:%d dispose:%d blend:%d\n",
             info->x_offset, info->y_offset, info->duration,
             info->dispose_method, info->blend_method);
    }
    FrameRelease(curr);
    ++cache->start;
    --cache->flush_count;
    --cache->count;
    if (cache->keyframe != KEYFRAME_NONE) --cache->keyframe;
  }

  if (cache->count == 0) CacheReset(cache);
  return WEBP_MUX_OK;
}

WebPMuxError WebPFrameCacheFlushAll(WebPFrameCache* const cache, int verbose,
                                    WebPMux* const mux) {
  cache->flush_count = cache->count;  // Force flushing of all frames.
  return WebPFrameCacheFlush(cache, verbose, mux);
}

int WebPFrameCacheShouldTryKeyFrame(const WebPFrameCache* const cache) {
  return cache->count_since_key_frame >= cache->kmin;
}

//------------------------------------------------------------------------------
// Frame rectangle and related utilities.

static void ClearRectangle(WebPPicture* const picture,
                           int left, int top, int width, int height) {
  int j;
  for (j = top; j < top + height; ++j) {
    uint32_t* const dst = picture->argb + j * picture->argb_stride;
    int i;
    for (i = left; i < left + width; ++i) {
      dst[i] = TRANSPARENT_COLOR;
    }
  }
}

// Clear pixels in 'picture' within given 'rect' to transparent color.
void WebPUtilClearPic(WebPPicture* const picture,
                      const WebPFrameRect* const rect) {
  if (rect != NULL) {
    ClearRectangle(picture, rect->x_offset, rect->y_offset,
                   rect->width, rect->height);
  } else {
    ClearRectangle(picture, 0, 0, picture->width, picture->height);
  }
}

// TODO: Also used in picture.c. Move to a common location?
// Copy width x height pixels from 'src' to 'dst' honoring the strides.
static void CopyPlane(const uint8_t* src, int src_stride,
                      uint8_t* dst, int dst_stride, int width, int height) {
  while (height-- > 0) {
    memcpy(dst, src, width);
    src += src_stride;
    dst += dst_stride;
  }
}

void WebPUtilCopyPixels(const WebPPicture* const src, WebPPicture* const dst) {
  assert(src->width == dst->width && src->height == dst->height);
  CopyPlane((uint8_t*)src->argb, 4 * src->argb_stride, (uint8_t*)dst->argb,
            4 * dst->argb_stride, 4 * src->width, src->height);
}

void WebPUtilBlendPixels(const WebPPicture* const src,
                         const WebPFrameRect* const rect,
                         WebPPicture* const dst) {
  int j;
  assert(src->width == dst->width && src->height == dst->height);
  for (j = rect->y_offset; j < rect->y_offset + rect->height; ++j) {
    int i;
    for (i = rect->x_offset; i < rect->x_offset + rect->width; ++i) {
      const uint32_t src_pixel = src->argb[j * src->argb_stride + i];
      const int src_alpha = src_pixel >> 24;
      if (src_alpha != 0) {
        dst->argb[j * dst->argb_stride + i] = src_pixel;
      }
    }
  }
}

void WebPUtilReduceTransparency(const WebPPicture* const src,
                                const WebPFrameRect* const rect,
                                WebPPicture* const dst) {
  int i, j;
  assert(src != NULL && dst != NULL && rect != NULL);
  assert(src->width == dst->width && src->height == dst->height);
  for (j = rect->y_offset; j < rect->y_offset + rect->height; ++j) {
    for (i = rect->x_offset; i < rect->x_offset + rect->width; ++i) {
      const uint32_t src_pixel = src->argb[j * src->argb_stride + i];
      const int src_alpha = src_pixel >> 24;
      const uint32_t dst_pixel = dst->argb[j * dst->argb_stride + i];
      const int dst_alpha = dst_pixel >> 24;
      if (dst_alpha == 0 && src_alpha == 0xff) {
        dst->argb[j * dst->argb_stride + i] = src_pixel;
      }
    }
  }
}

void WebPUtilFlattenSimilarBlocks(const WebPPicture* const src,
                                  const WebPFrameRect* const rect,
                                  WebPPicture* const dst) {
  int i, j;
  const int block_size = 8;
  const int y_start = (rect->y_offset + block_size) & ~(block_size - 1);
  const int y_end = (rect->y_offset + rect->height) & ~(block_size - 1);
  const int x_start = (rect->x_offset + block_size) & ~(block_size - 1);
  const int x_end = (rect->x_offset + rect->width) & ~(block_size - 1);
  assert(src != NULL && dst != NULL && rect != NULL);
  assert(src->width == dst->width && src->height == dst->height);
  assert((block_size & (block_size - 1)) == 0);  // must be a power of 2
  // Iterate over each block and count similar pixels.
  for (j = y_start; j < y_end; j += block_size) {
    for (i = x_start; i < x_end; i += block_size) {
      int cnt = 0;
      int avg_r = 0, avg_g = 0, avg_b = 0;
      int x, y;
      const uint32_t* const psrc = src->argb + j * src->argb_stride + i;
      uint32_t* const pdst = dst->argb + j * dst->argb_stride + i;
      for (y = 0; y < block_size; ++y) {
        for (x = 0; x < block_size; ++x) {
          const uint32_t src_pixel = psrc[x + y * src->argb_stride];
          const int alpha = src_pixel >> 24;
          if (alpha == 0xff &&
              src_pixel == pdst[x + y * dst->argb_stride]) {
              ++cnt;
              avg_r += (src_pixel >> 16) & 0xff;
              avg_g += (src_pixel >>  8) & 0xff;
              avg_b += (src_pixel >>  0) & 0xff;
          }
        }
      }
      // If we have a fully similar block, we replace it with an
      // average transparent block. This compresses better in lossy mode.
      if (cnt == block_size * block_size) {
        const uint32_t color = (0x00          << 24) |
                               ((avg_r / cnt) << 16) |
                               ((avg_g / cnt) <<  8) |
                               ((avg_b / cnt) <<  0);
        for (y = 0; y < block_size; ++y) {
          for (x = 0; x < block_size; ++x) {
            pdst[x + y * dst->argb_stride] = color;
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------
// Key frame related utilities.

int WebPUtilIsKeyFrame(const WebPPicture* const curr,
                       const WebPFrameRect* const curr_rect,
                       const WebPPicture* const prev) {
  int i, j;
  int is_key_frame = 1;

  // If previous canvas (with previous frame disposed) is all transparent,
  // current frame is a key frame.
  for (i = 0; i < prev->width; ++i) {
    for (j = 0; j < prev->height; ++j) {
      const uint32_t prev_alpha = (prev->argb[j * prev->argb_stride + i]) >> 24;
      if (prev_alpha != 0) {
        is_key_frame = 0;
        break;
      }
    }
    if (!is_key_frame) break;
  }
  if (is_key_frame) return 1;

  // If current frame covers the whole canvas and does not contain any
  // transparent pixels that depend on previous canvas, then current frame is
  // a key frame.
  if (curr_rect->width == curr->width && curr_rect->height == curr->height) {
    assert(curr_rect->x_offset == 0 && curr_rect->y_offset == 0);
    is_key_frame = 1;
    for (j = 0; j < prev->height; ++j) {
      for (i = 0; i < prev->width; ++i) {
        const uint32_t prev_alpha =
            (prev->argb[j * prev->argb_stride + i]) >> 24;
        const uint32_t curr_alpha =
            (curr->argb[j * curr->argb_stride + i]) >> 24;
        if (curr_alpha != 0xff && prev_alpha != 0) {
          is_key_frame = 0;
          break;
        }
      }
      if (!is_key_frame) break;
    }
    if (is_key_frame) return 1;
  }

  return 0;
}

void WebPUtilConvertToKeyFrame(const WebPPicture* const prev,
                               WebPFrameRect* const rect,
                               WebPPicture* const curr) {
  int j;
  assert(curr->width == prev->width && curr->height == prev->height);

  // Replace transparent pixels of current canvas with those from previous
  // canvas (with previous frame disposed).
  for (j = 0; j < curr->height; ++j) {
    int i;
    for (i = 0; i < curr->width; ++i) {
      uint32_t* const curr_pixel = curr->argb + j * curr->argb_stride + i;
      const int curr_alpha = *curr_pixel >> 24;
      if (curr_alpha == 0) {
        *curr_pixel = prev->argb[j * prev->argb_stride + i];
      }
    }
  }

  // Frame rectangle now covers the whole canvas.
  rect->x_offset = 0;
  rect->y_offset = 0;
  rect->width = curr->width;
  rect->height = curr->height;
}

//------------------------------------------------------------------------------
