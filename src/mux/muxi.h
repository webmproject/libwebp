// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Internal header for mux library.
//
// Author: Urvang (urvang@google.com)

#ifndef WEBP_MUX_MUXI_H_
#define WEBP_MUX_MUXI_H_

#include <stdlib.h>
#include "../dec/vp8i.h"
#include "../dec/webpi.h"    // For chunk-size constants.
#include "../webp/mux.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Defines and constants.

// Object to store metadata about images.
typedef struct {
  uint32_t    x_offset_;
  uint32_t    y_offset_;
  uint32_t    duration_;
  uint32_t    width_;
  uint32_t    height_;
} WebPImageInfo;

// Chunk object.
typedef struct WebPChunk WebPChunk;
struct WebPChunk {
  uint32_t        tag_;
  uint32_t        payload_size_;
  WebPImageInfo*  image_info_;
  int             owner_;  // True if *data_ memory is owned internally.
                           // VP8X, Loop, and other internally created chunks
                           // like frame/tile are always owned.
  const           uint8_t* data_;
  WebPChunk*      next_;
};

// MuxImage object. Store a full webp image (including frame/tile chunk, alpha
// chunk and VP8 chunk),
typedef struct WebPMuxImage WebPMuxImage;
struct WebPMuxImage {
  WebPChunk*  header_;      // Corresponds to FRAME_ID/TILE_ID.
  WebPChunk*  alpha_;       // Corresponds to ALPHA_ID.
  WebPChunk*  vp8_;         // Corresponds to IMAGE_ID.
  int         is_partial_;  // True if only some of the chunks are filled.
  WebPMuxImage* next_;
};

// Main mux object. Stores data chunks.
struct WebPMux {
  WebPMuxState    state_;
  WebPMuxImage*   images_;
  WebPChunk*      iccp_;
  WebPChunk*      meta_;
  WebPChunk*      loop_;
  WebPChunk*      vp8x_;

  WebPChunk*  unknown_;
};

#define CHUNKS_PER_FRAME  2
#define CHUNKS_PER_TILE   2

typedef enum {
  VP8X_ID = 0,
  ICCP_ID,
  LOOP_ID,
  FRAME_ID,
  TILE_ID,
  ALPHA_ID,
  IMAGE_ID,
  META_ID,
  UNKNOWN_ID,

  NIL_ID,
  LIST_ID
} TAG_ID;

// Maximum chunk payload (data) size such that adding the header and padding
// won't overflow an uint32.
#define MAX_CHUNK_PAYLOAD (~0U - CHUNK_HEADER_SIZE - 1)

#define NIL_TAG 0x00000000u  // To signal void chunk.

#define mktag(c1, c2, c3, c4) \
  ((uint32_t)c1 | (c2 << 8) | (c3 << 16) | (c4 << 24))

typedef struct {
  const char*   chunkName;
  uint32_t      chunkTag;
  TAG_ID        chunkId;
  uint32_t      chunkSize;
} ChunkInfo;

extern const ChunkInfo kChunks[LIST_ID + 1];

//------------------------------------------------------------------------------
// Helper functions.

static WEBP_INLINE uint32_t GetLE32(const uint8_t* const data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static WEBP_INLINE void PutLE16(uint8_t* const data, uint16_t val) {
  data[0] = (val >> 0) & 0xff;
  data[1] = (val >> 8) & 0xff;
}

static WEBP_INLINE void PutLE32(uint8_t* const data, uint32_t val) {
  PutLE16(data, val);
  PutLE16(data + 2, val >> 16);
}

static WEBP_INLINE uint32_t SizeWithPadding(uint32_t chunk_size) {
  return CHUNK_HEADER_SIZE + ((chunk_size + 1) & ~1U);
}

//------------------------------------------------------------------------------
// Chunk object management.

// Initialize.
void ChunkInit(WebPChunk* const chunk);

// Get chunk id from chunk name.
TAG_ID ChunkGetIdFromName(const char* const what);

// Get chunk id from chunk tag.
TAG_ID ChunkGetIdFromTag(uint32_t tag);

// Search for nth chunk with given 'tag' in the chunk list.
// nth = 0 means "last of the list".
WebPChunk* ChunkSearchList(WebPChunk* first, uint32_t nth, uint32_t tag);

// Fill the chunk with the given data & image_info.
WebPMuxError ChunkAssignDataImageInfo(WebPChunk* chunk,
                                      const uint8_t* data, uint32_t data_size,
                                      WebPImageInfo* image_info,
                                      int copy_data, uint32_t tag);

// Sets 'chunk' at nth position in the 'chunk_list'.
// nth = 0 has the special meaning "last of the list".
WebPMuxError ChunkSetNth(const WebPChunk* chunk, WebPChunk** chunk_list,
                         uint32_t nth);

// Releases chunk and returns chunk->next_.
WebPChunk* ChunkRelease(WebPChunk* const chunk);

// Deletes given chunk & returns chunk->next_.
WebPChunk* ChunkDelete(WebPChunk* const chunk);

// Size of a chunk including header and padding.
static WEBP_INLINE uint32_t ChunkDiskSize(const WebPChunk* chunk) {
  assert(chunk->payload_size_ < MAX_CHUNK_PAYLOAD);
  return SizeWithPadding(chunk->payload_size_);
}

// Total size of a list of chunks.
uint32_t ChunksListDiskSize(const WebPChunk* chunk_list);

// Write out the given list of chunks into 'dst'.
uint8_t* ChunkListEmit(const WebPChunk* chunk_list, uint8_t* dst);

//------------------------------------------------------------------------------
// MuxImage object management.

// Initialize.
void MuxImageInit(WebPMuxImage* const wpi);

// Releases image 'wpi' and returns wpi->next.
WebPMuxImage* MuxImageRelease(WebPMuxImage* const wpi);

// Delete image 'wpi' and return the next image in the list or NULL.
// 'wpi' can be NULL.
WebPMuxImage* MuxImageDelete(WebPMuxImage* const wpi);

// Delete all images in 'wpi_list'.
void MuxImageDeleteAll(WebPMuxImage** const wpi_list);

// Count number of images matching 'tag' in the 'wpi_list'.
// If tag == NIL_TAG, any tag will be matched.
int MuxImageCount(WebPMuxImage* const wpi_list, TAG_ID id);

// Check if given ID corresponds to an image related chunk.
static WEBP_INLINE int IsWPI(TAG_ID id) {
  switch (id) {
    case FRAME_ID:
    case TILE_ID:
    case ALPHA_ID:
    case IMAGE_ID:  return 1;
    default:        return 0;
  }
}

// Get a reference to appropriate chunk list within an image given chunk tag.
static WEBP_INLINE WebPChunk** MuxImageGetListFromId(WebPMuxImage* wpi,
                                                     TAG_ID id) {
  assert(wpi != NULL);
  switch (id) {
    case FRAME_ID:
    case TILE_ID: return &wpi->header_;
    case ALPHA_ID: return &wpi->alpha_;
    case IMAGE_ID: return &wpi->vp8_;
    default: return NULL;
  }
}

// Sets 'wpi' at nth position in the 'wpi_list'.
// nth = 0 has the special meaning "last of the list".
WebPMuxError MuxImageSetNth(const WebPMuxImage* wpi, WebPMuxImage** wpi_list,
                            uint32_t nth);

// Delete nth image in the image list with given tag.
WebPMuxError MuxImageDeleteNth(WebPMuxImage** wpi_list, uint32_t nth,
                               TAG_ID id);

// Get nth image in the image list with given tag.
WebPMuxError MuxImageGetNth(const WebPMuxImage** wpi_list, uint32_t nth,
                            TAG_ID id, WebPMuxImage** wpi);

// Total size of a list of images.
uint32_t MuxImageListDiskSize(const WebPMuxImage* wpi_list);

// Write out the given list of images into 'dst'.
uint8_t* MuxImageListEmit(const WebPMuxImage* wpi_list, uint8_t* dst);

//------------------------------------------------------------------------------
// Helper methods for mux.

// Returns the list where chunk with given ID is to be inserted in mux.
// Return value is NULL if this chunk should be inserted in mux->images_ list
// or if 'id' is not known.
WebPChunk** GetChunkListFromId(const WebPMux* mux, TAG_ID id);

// Validates that the given mux has a single image.
WebPMuxError ValidateForImage(const WebPMux* const mux);

// Validates the given mux object.
WebPMuxError WebPMuxValidate(const WebPMux* const mux);

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  /* WEBP_MUX_MUXI_H_ */
