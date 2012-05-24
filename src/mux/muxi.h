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
#include "../dec/vp8li.h"
#include "../webp/format_constants.h"
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
  size_t          payload_size_;
  WebPImageInfo*  image_info_;
  int             owner_;  // True if *data_ memory is owned internally.
                           // VP8X, Loop, and other internally created chunks
                           // like frame/tile are always owned.
  const           uint8_t* data_;
  WebPChunk*      next_;
};

// MuxImage object. Store a full webp image (including frame/tile chunk, alpha
// chunk and VP8/VP8L chunk),
typedef struct WebPMuxImage WebPMuxImage;
struct WebPMuxImage {
  WebPChunk*  header_;      // Corresponds to FRAME_ID/TILE_ID.
  WebPChunk*  alpha_;       // Corresponds to ALPHA_ID.
  WebPChunk*  img_;         // Corresponds to IMAGE_ID.
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

// TAG_ID enum: used to assign an ID to each type of chunk.
typedef enum {
  VP8X_ID,
  ICCP_ID,
  LOOP_ID,
  FRAME_ID,
  TILE_ID,
  ALPHA_ID,
  IMAGE_ID,
  META_ID,
  UNKNOWN_ID,
  NIL_ID
} TAG_ID;

// CHUNK_INDEX enum: used for indexing within 'kChunks' (defined below) only.
// Note: the reason for having two enums ('TAG_ID' and 'CHUNK_INDEX') is to
// allow two different chunks to have the same id (e.g. TAG_ID 'IMAGE_ID' can
// correspond to CHUNK_INDEX 'IDX_VP8' or 'IDX_VP8L').
typedef enum {
  IDX_VP8X = 0,
  IDX_ICCP,
  IDX_LOOP,
  IDX_FRAME,
  IDX_TILE,
  IDX_ALPHA,
  IDX_VP8,
  IDX_VP8L,
  IDX_META,
  IDX_UNKNOWN,

  IDX_NIL,
  IDX_LAST_CHUNK
} CHUNK_INDEX;

#define NIL_TAG 0x00000000u  // To signal void chunk.

#define mktag(c1, c2, c3, c4) \
  ((uint32_t)c1 | (c2 << 8) | (c3 << 16) | (c4 << 24))

typedef struct {
  const char*   name;
  uint32_t      tag;
  TAG_ID        id;
  uint32_t      size;
} ChunkInfo;

extern const ChunkInfo kChunks[IDX_LAST_CHUNK];

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

static WEBP_INLINE size_t SizeWithPadding(size_t chunk_size) {
  return CHUNK_HEADER_SIZE + ((chunk_size + 1) & ~1U);
}

//------------------------------------------------------------------------------
// Chunk object management.

// Initialize.
void ChunkInit(WebPChunk* const chunk);

// Get chunk index from chunk name.
// Returns IDX_NIL if chunk name is NULL or not found.
CHUNK_INDEX ChunkGetIndexFromName(const char* const name);

// Get chunk index from chunk tag.
// Returns IDX_NIL if not found.
CHUNK_INDEX ChunkGetIndexFromTag(uint32_t tag);

// Get chunk id from chunk tag.
// Returns NIL_ID if not found.
TAG_ID ChunkGetIdFromTag(uint32_t tag);

// Search for nth chunk with given 'tag' in the chunk list.
// nth = 0 means "last of the list".
WebPChunk* ChunkSearchList(WebPChunk* first, uint32_t nth, uint32_t tag);

// Fill the chunk with the given data & image_info.
WebPMuxError ChunkAssignDataImageInfo(WebPChunk* chunk,
                                      const uint8_t* data, size_t data_size,
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
static WEBP_INLINE size_t ChunkDiskSize(const WebPChunk* chunk) {
  assert(chunk->payload_size_ < MAX_CHUNK_PAYLOAD);
  return SizeWithPadding(chunk->payload_size_);
}

// Total size of a list of chunks.
size_t ChunksListDiskSize(const WebPChunk* chunk_list);

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

// Count number of images matching the given tag id in the 'wpi_list'.
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
    case TILE_ID:  return &wpi->header_;
    case ALPHA_ID: return &wpi->alpha_;
    case IMAGE_ID: return &wpi->img_;
    default: return NULL;
  }
}

// Sets 'wpi' at nth position in the 'wpi_list'.
// nth = 0 has the special meaning "last of the list".
WebPMuxError MuxImageSetNth(const WebPMuxImage* wpi, WebPMuxImage** wpi_list,
                            uint32_t nth);

// Delete nth image in the image list with given tag id.
WebPMuxError MuxImageDeleteNth(WebPMuxImage** wpi_list, uint32_t nth,
                               TAG_ID id);

// Get nth image in the image list with given tag id.
WebPMuxError MuxImageGetNth(const WebPMuxImage** wpi_list, uint32_t nth,
                            TAG_ID id, WebPMuxImage** wpi);

// Total size of a list of images.
size_t MuxImageListDiskSize(const WebPMuxImage* wpi_list);

// Write out the given list of images into 'dst'.
uint8_t* MuxImageListEmit(const WebPMuxImage* wpi_list, uint8_t* dst);

// Checks if the given image list contains at least one lossless image.
int MuxHasLosslessImages(const WebPMuxImage* images);

//------------------------------------------------------------------------------
// Helper methods for mux.

// Returns the list where chunk with given ID is to be inserted in mux.
// Return value is NULL if this chunk should be inserted in mux->images_ list
// or if 'id' is not known.
WebPChunk** MuxGetChunkListFromId(const WebPMux* mux, TAG_ID id);

// Validates that the given mux has a single image.
WebPMuxError MuxValidateForImage(const WebPMux* const mux);

// Validates the given mux object.
WebPMuxError MuxValidate(const WebPMux* const mux);

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  /* WEBP_MUX_MUXI_H_ */
