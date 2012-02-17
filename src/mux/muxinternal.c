// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Internal objects and utils for mux.
//
// Authors: Urvang (urvang@google.com)
//          Vikas (vikasa@google.com)

#include <assert.h>
#include "./muxi.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define UNDEFINED_CHUNK_SIZE (-1)

const ChunkInfo kChunks[] = {
  {"vp8x",    mktag('V', 'P', '8', 'X'),  VP8X_ID,    VP8X_CHUNK_SIZE},
  {"iccp",    mktag('I', 'C', 'C', 'P'),  ICCP_ID,    UNDEFINED_CHUNK_SIZE},
  {"loop",    mktag('L', 'O', 'O', 'P'),  LOOP_ID,    LOOP_CHUNK_SIZE},
  {"frame",   mktag('F', 'R', 'M', ' '),  FRAME_ID,   FRAME_CHUNK_SIZE},
  {"tile",    mktag('T', 'I', 'L', 'E'),  TILE_ID,    TILE_CHUNK_SIZE},
  {"alpha",   mktag('A', 'L', 'P', 'H'),  ALPHA_ID,   UNDEFINED_CHUNK_SIZE},
  {"image",   mktag('V', 'P', '8', ' '),  IMAGE_ID,   UNDEFINED_CHUNK_SIZE},
  {"meta",    mktag('M', 'E', 'T', 'A'),  META_ID,    UNDEFINED_CHUNK_SIZE},
  {"unknown", mktag('U', 'N', 'K', 'N'),  UNKNOWN_ID, UNDEFINED_CHUNK_SIZE},

  {NULL,      NIL_TAG,                    NIL_ID,     UNDEFINED_CHUNK_SIZE},
  {"list",    mktag('L', 'I', 'S', 'T'),  LIST_ID,    UNDEFINED_CHUNK_SIZE}
};

//------------------------------------------------------------------------------
// Life of a chunk object.

void ChunkInit(WebPChunk* const chunk) {
  assert(chunk);
  memset(chunk, 0, sizeof(*chunk));
  chunk->tag_ = NIL_TAG;
}

WebPChunk* ChunkRelease(WebPChunk* const chunk) {
  WebPChunk* next;
  if (chunk == NULL) return NULL;
  free(chunk->image_info_);
  if (chunk->owner_) {
    free((void*)chunk->data_);
  }
  next = chunk->next_;
  ChunkInit(chunk);
  return next;
}

//------------------------------------------------------------------------------
// Chunk misc methods.

TAG_ID ChunkGetIdFromName(const char* const what) {
  int i;
  if (what == NULL) return -1;
  for (i = 0; kChunks[i].chunkName != NULL; ++i) {
    if (!strcmp(what, kChunks[i].chunkName)) return i;
  }
  return NIL_ID;
}

TAG_ID ChunkGetIdFromTag(uint32_t tag) {
  int i;
  for (i = 0; kChunks[i].chunkTag != NIL_TAG; ++i) {
    if (tag == kChunks[i].chunkTag) return i;
  }
  return NIL_ID;
}

//------------------------------------------------------------------------------
// Chunk search methods.

// Returns next chunk in the chunk list with the given tag.
static WebPChunk* ChunkSearchNextInList(WebPChunk* chunk, uint32_t tag) {
  while (chunk && chunk->tag_ != tag) {
    chunk = chunk->next_;
  }
  return chunk;
}

WebPChunk* ChunkSearchList(WebPChunk* first, uint32_t nth, uint32_t tag) {
  uint32_t iter = nth;
  first = ChunkSearchNextInList(first, tag);
  if (!first) return NULL;

  while (--iter != 0) {
    WebPChunk* next_chunk = ChunkSearchNextInList(first->next_, tag);
    if (next_chunk == NULL) break;
    first = next_chunk;
  }
  return ((nth > 0) && (iter > 0)) ? NULL : first;
}

// Outputs a pointer to 'prev_chunk->next_',
//   where 'prev_chunk' is the pointer to the chunk at position (nth - 1).
// Returns 1 if nth chunk was found, 0 otherwise.
static int ChunkSearchListToSet(WebPChunk** chunk_list, uint32_t nth,
                                WebPChunk*** const location) {
  uint32_t count = 0;
  assert(chunk_list);
  *location = chunk_list;

  while (*chunk_list) {
    WebPChunk* const cur_chunk = *chunk_list;
    ++count;
    if (count == nth) return 1;  // Found.
    chunk_list = &cur_chunk->next_;
    *location = chunk_list;
  }

  // *chunk_list is ok to be NULL if adding at last location.
  return (nth == 0 || (count == nth - 1)) ? 1 : 0;
}

//------------------------------------------------------------------------------
// Chunk writer methods.

WebPMuxError ChunkAssignDataImageInfo(WebPChunk* chunk,
                                      const uint8_t* data, uint32_t data_size,
                                      WebPImageInfo* image_info,
                                      int copy_data, uint32_t tag) {
  // For internally allocated chunks, always copy data & make it owner of data.
  if (tag == kChunks[VP8X_ID].chunkTag || tag == kChunks[LOOP_ID].chunkTag) {
    copy_data = 1;
  }

  ChunkRelease(chunk);
  if (data == NULL) {
    data_size = 0;
  } else if (data_size == 0) {
    data = NULL;
  }

  if (data != NULL) {
    if (copy_data) {
      // Copy data.
      chunk->data_ = (uint8_t*)malloc(data_size);
      if (chunk->data_ == NULL) return WEBP_MUX_MEMORY_ERROR;
      memcpy((uint8_t*)chunk->data_, data, data_size);
      chunk->payload_size_ = data_size;

      // Chunk is owner of data.
      chunk->owner_ = 1;
    } else {
      // Don't copy data.
      chunk->data_ = data;
      chunk->payload_size_ = data_size;
    }
  }

  if (tag == kChunks[IMAGE_ID].chunkTag) {
    chunk->image_info_ = image_info;
  }

  chunk->tag_ = tag;

  return WEBP_MUX_OK;
}

WebPMuxError ChunkSetNth(const WebPChunk* chunk, WebPChunk** chunk_list,
                         uint32_t nth) {
  WebPChunk* new_chunk;

  if (!ChunkSearchListToSet(chunk_list, nth, &chunk_list)) {
    return WEBP_MUX_NOT_FOUND;
  }

  new_chunk = (WebPChunk*)malloc(sizeof(*new_chunk));
  if (new_chunk == NULL) return WEBP_MUX_MEMORY_ERROR;
  *new_chunk = *chunk;
  new_chunk->next_ = *chunk_list;
  *chunk_list = new_chunk;
  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------
// Chunk deletion method(s).

WebPChunk* ChunkDelete(WebPChunk* const chunk) {
  WebPChunk* const next = ChunkRelease(chunk);
  free(chunk);
  return next;
}

//------------------------------------------------------------------------------
// Chunk serialization methods.

uint32_t ChunksListDiskSize(const WebPChunk* chunk_list) {
  uint32_t size = 0;
  while (chunk_list) {
    size += ChunkDiskSize(chunk_list);
    chunk_list = chunk_list->next_;
  }
  return size;
}

static uint8_t* ChunkEmit(const WebPChunk* const chunk, uint8_t* dst) {
  assert(chunk);
  assert(chunk->tag_ != NIL_TAG);
  PutLE32(dst + 0, chunk->tag_);
  PutLE32(dst + TAG_SIZE, chunk->payload_size_);
  memcpy(dst + CHUNK_HEADER_SIZE, chunk->data_, chunk->payload_size_);
  if (chunk->payload_size_ & 1)
    dst[CHUNK_HEADER_SIZE + chunk->payload_size_] = 0;  // Add padding.
  return dst + ChunkDiskSize(chunk);
}

uint8_t* ChunkListEmit(const WebPChunk* chunk_list, uint8_t* dst) {
  while (chunk_list) {
    dst = ChunkEmit(chunk_list, dst);
    chunk_list = chunk_list->next_;
  }
  return dst;
}

//------------------------------------------------------------------------------
// Life of a MuxImage object.

void MuxImageInit(WebPMuxImage* const wpi) {
  assert(wpi);
  memset(wpi, 0, sizeof(*wpi));
}

WebPMuxImage* MuxImageRelease(WebPMuxImage* const wpi) {
  WebPMuxImage* next;
  if (wpi == NULL) return NULL;
  ChunkDelete(wpi->header_);
  ChunkDelete(wpi->alpha_);
  ChunkDelete(wpi->vp8_);

  next = wpi->next_;
  MuxImageInit(wpi);
  return next;
}

//------------------------------------------------------------------------------
// MuxImage search methods.

int MuxImageCount(WebPMuxImage* const wpi_list, TAG_ID id) {
  int count = 0;
  WebPMuxImage* current;
  for (current = wpi_list; current != NULL; current = current->next_) {
    WebPChunk** const wpi_chunk_ptr = MuxImageGetListFromId(current, id);
    assert(wpi_chunk_ptr != NULL);

    if (*wpi_chunk_ptr != NULL &&
        (*wpi_chunk_ptr)->tag_ == kChunks[id].chunkTag) {
      ++count;
    }
  }
  return count;
}

// Outputs a pointer to 'prev_wpi->next_',
//   where 'prev_wpi' is the pointer to the image at position (nth - 1).
// Returns 1 if nth image was found, 0 otherwise.
static int SearchImageToSet(WebPMuxImage** wpi_list, uint32_t nth,
                            WebPMuxImage*** const location) {
  uint32_t count = 0;
  assert(wpi_list);
  *location = wpi_list;

  while (*wpi_list) {
    WebPMuxImage* const cur_wpi = *wpi_list;
    ++count;
    if (count == nth) return 1;  // Found.
    wpi_list = &cur_wpi->next_;
    *location = wpi_list;
  }

  // *chunk_list is ok to be NULL if adding at last location.
  return (nth == 0 || (count == nth - 1)) ? 1 : 0;
}

// Outputs a pointer to 'prev_wpi->next_',
//   where 'prev_wpi' is the pointer to the image at position (nth - 1).
// Returns 1 if nth image with given id was found, 0 otherwise.
static int SearchImageToGetOrDelete(WebPMuxImage** wpi_list, uint32_t nth,
                                    TAG_ID id, WebPMuxImage*** const location) {
  uint32_t count = 0;
  assert(wpi_list);
  *location = wpi_list;

  // Search makes sense only for the following.
  assert(id == FRAME_ID || id == TILE_ID || id == IMAGE_ID);
  assert(id != IMAGE_ID || nth == 1);

  if (nth == 0) {
    nth = MuxImageCount(*wpi_list, id);
    if (nth == 0) return 0;  // Not found.
  }

  while (*wpi_list) {
    WebPMuxImage* const cur_wpi = *wpi_list;
    WebPChunk** const wpi_chunk_ptr = MuxImageGetListFromId(cur_wpi, id);
    assert(wpi_chunk_ptr != NULL);
    if ((*wpi_chunk_ptr)->tag_ == kChunks[id].chunkTag) {
      ++count;
      if (count == nth) return 1;  // Found.
    }
    wpi_list = &cur_wpi->next_;
    *location = wpi_list;
  }
  return 0;  // Not found.
}

//------------------------------------------------------------------------------
// MuxImage writer methods.

WebPMuxError MuxImageSetNth(const WebPMuxImage* wpi, WebPMuxImage** wpi_list,
                            uint32_t nth) {
  WebPMuxImage* new_wpi;

  if (!SearchImageToSet(wpi_list, nth, &wpi_list)) {
    return WEBP_MUX_NOT_FOUND;
  }

  new_wpi = (WebPMuxImage*)malloc(sizeof(*new_wpi));
  if (new_wpi == NULL) return WEBP_MUX_MEMORY_ERROR;
  *new_wpi = *wpi;
  new_wpi->next_ = *wpi_list;
  *wpi_list = new_wpi;
  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------
// MuxImage deletion methods.

WebPMuxImage* MuxImageDelete(WebPMuxImage* const wpi) {
  // Delete the components of wpi. If wpi is NULL this is a noop.
  WebPMuxImage* const next = MuxImageRelease(wpi);
  free(wpi);
  return next;
}

void MuxImageDeleteAll(WebPMuxImage** const wpi_list) {
  while (*wpi_list) {
    *wpi_list = MuxImageDelete(*wpi_list);
  }
}

WebPMuxError MuxImageDeleteNth(WebPMuxImage** wpi_list, uint32_t nth,
                               TAG_ID id) {
  assert(wpi_list);
  if (!SearchImageToGetOrDelete(wpi_list, nth, id, &wpi_list)) {
    return WEBP_MUX_NOT_FOUND;
  }
  *wpi_list = MuxImageDelete(*wpi_list);
  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------
// MuxImage reader methods.

WebPMuxError MuxImageGetNth(const WebPMuxImage** wpi_list, uint32_t nth,
                            TAG_ID id, WebPMuxImage** wpi) {
  assert(wpi_list);
  assert(wpi);
  if (!SearchImageToGetOrDelete((WebPMuxImage**)wpi_list, nth, id,
                                (WebPMuxImage***)&wpi_list)) {
    return WEBP_MUX_NOT_FOUND;
  }
  *wpi = (WebPMuxImage*)*wpi_list;
  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------
// MuxImage serialization methods.

// Size of an image.
static uint32_t MuxImageDiskSize(const WebPMuxImage* wpi) {
  uint32_t size = 0;
  if (wpi->header_ != NULL) size += ChunkDiskSize(wpi->header_);
  if (wpi->alpha_ != NULL) size += ChunkDiskSize(wpi->alpha_);
  if (wpi->vp8_ != NULL) size += ChunkDiskSize(wpi->vp8_);
  return size;
}

uint32_t MuxImageListDiskSize(const WebPMuxImage* wpi_list) {
  uint32_t size = 0;
  while (wpi_list) {
    size += MuxImageDiskSize(wpi_list);
    wpi_list = wpi_list->next_;
  }
  return size;
}

static uint8_t* MuxImageEmit(const WebPMuxImage* const wpi, uint8_t* dst) {
  // Ordering of chunks to be emitted is strictly as follows:
  // 1. Frame/Tile chunk (if present).
  // 2. Alpha chunk (if present).
  // 3. VP8 chunk.
  assert(wpi);
  if (wpi->header_ != NULL) dst = ChunkEmit(wpi->header_, dst);
  if (wpi->alpha_ != NULL) dst = ChunkEmit(wpi->alpha_, dst);
  if (wpi->vp8_ != NULL) dst = ChunkEmit(wpi->vp8_, dst);
  return dst;
}

uint8_t* MuxImageListEmit(const WebPMuxImage* wpi_list, uint8_t* dst) {
  while (wpi_list) {
    dst = MuxImageEmit(wpi_list, dst);
    wpi_list = wpi_list->next_;
  }
  return dst;
}

//------------------------------------------------------------------------------
// Helper methods for mux.

WebPChunk** GetChunkListFromId(const WebPMux* mux, TAG_ID id) {
  assert(mux != NULL);
  switch(id) {
    case VP8X_ID: return (WebPChunk**)&mux->vp8x_;
    case ICCP_ID: return (WebPChunk**)&mux->iccp_;
    case LOOP_ID: return (WebPChunk**)&mux->loop_;
    case META_ID: return (WebPChunk**)&mux->meta_;
    case UNKNOWN_ID: return (WebPChunk**)&mux->unknown_;
    default: return NULL;
  }
}

WebPMuxError ValidateForImage(const WebPMux* const mux) {
  const int num_vp8 = MuxImageCount(mux->images_, IMAGE_ID);
  const int num_frames = MuxImageCount(mux->images_, FRAME_ID);
  const int num_tiles = MuxImageCount(mux->images_, TILE_ID);

  if (num_vp8 == 0) {
    // No images in mux.
    return WEBP_MUX_NOT_FOUND;
  } else if (num_vp8 == 1 && num_frames == 0 && num_tiles == 0) {
    // Valid case (single image).
    return WEBP_MUX_OK;
  } else {
    // Frame/Tile case OR an invalid mux.
    return WEBP_MUX_INVALID_ARGUMENT;
  }
}

static int IsNotCompatible(int feature, int num_items) {
  return (feature != 0) != (num_items > 0);
}

#define NO_FLAG 0

// Test basic constraints:
// retrieval, maximum number of chunks by id (use -1 to skip)
// and feature incompatibility (use NO_FLAG to skip).
// On success returns WEBP_MUX_OK and stores the chunk count in *num.
static WebPMuxError ValidateChunk(const WebPMux* const mux, TAG_ID id,
                                  FeatureFlags feature, FeatureFlags vp8x_flags,
                                  int max, int* num) {
  const WebPMuxError err =
      WebPMuxNumNamedElements(mux, kChunks[id].chunkName, num);
  assert(id == kChunks[id].chunkId);

  if (err != WEBP_MUX_OK) return err;
  if (max > -1 && *num > max) return WEBP_MUX_INVALID_ARGUMENT;
  if (feature != NO_FLAG && IsNotCompatible(vp8x_flags & feature, *num)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }
  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxValidate(const WebPMux* const mux) {
  int num_iccp;
  int num_meta;
  int num_loop_chunks;
  int num_frames;
  int num_tiles;
  int num_vp8x;
  int num_images;
  int num_alpha;
  uint32_t flags;
  WebPMuxError err;

  // Verify mux is not NULL.
  if (mux == NULL || mux->state_ == WEBP_MUX_STATE_ERROR) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // No further checks if mux is partial.
  if (mux->state_ == WEBP_MUX_STATE_PARTIAL) return WEBP_MUX_OK;

  // Verify mux has at least one image.
  if (mux->images_ == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = WebPMuxGetFeatures(mux, &flags);
  if (err != WEBP_MUX_OK) return err;

  // At most one color profile chunk.
  err = ValidateChunk(mux, ICCP_ID, ICCP_FLAG, flags, 1, &num_iccp);
  if (err != WEBP_MUX_OK) return err;

  // At most one XMP metadata.
  err = ValidateChunk(mux, META_ID, META_FLAG, flags, 1, &num_meta);
  if (err != WEBP_MUX_OK) return err;

  // Animation: ANIMATION_FLAG, loop chunk and frame chunk(s) are consistent.
  // At most one loop chunk.
  err = ValidateChunk(mux, LOOP_ID, NO_FLAG, flags, 1, &num_loop_chunks);
  if (err != WEBP_MUX_OK) return err;
  err = ValidateChunk(mux, FRAME_ID, NO_FLAG, flags, -1, &num_frames);
  if (err != WEBP_MUX_OK) return err;

  {
    const int has_animation = !!(flags & ANIMATION_FLAG);
    if (has_animation && (num_loop_chunks == 0 || num_frames == 0)) {
      return WEBP_MUX_INVALID_ARGUMENT;
    }
    if (!has_animation && (num_loop_chunks == 1 || num_frames > 0)) {
      return WEBP_MUX_INVALID_ARGUMENT;
    }
  }

  // Tiling: TILE_FLAG and tile chunk(s) are consistent.
  err = ValidateChunk(mux, TILE_ID, TILE_FLAG, flags, -1, &num_tiles);
  if (err != WEBP_MUX_OK) return err;

  // Verify either VP8X chunk is present OR there is only one elem in
  // mux->images_.
  err = ValidateChunk(mux, VP8X_ID, NO_FLAG, flags, 1, &num_vp8x);
  if (err != WEBP_MUX_OK) return err;
  err = ValidateChunk(mux, IMAGE_ID, NO_FLAG, flags, -1, &num_images);
  if (err != WEBP_MUX_OK) return err;
  if (num_vp8x == 0 && num_images != 1) return WEBP_MUX_INVALID_ARGUMENT;

  // ALPHA_FLAG & alpha chunk(s) are consistent.
  err = ValidateChunk(mux, ALPHA_ID, ALPHA_FLAG, flags, -1, &num_alpha);
  if (err != WEBP_MUX_OK) return err;

  // num_images & num_alpha_chunks are consistent.
  if (num_alpha > 0 && num_alpha != num_images) {
    // Note that "num_alpha > 0" is the correct check but "flags && ALPHA_FLAG"
    // is NOT, because ALPHA_FLAG is based on first image only.
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return WEBP_MUX_OK;
}

#undef NO_FLAG

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
