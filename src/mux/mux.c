// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// RIFF container manipulation
//
// Author: Urvang (urvang@google.com)

#include <assert.h>
#include <stdlib.h>
#include "webp/mux.h"
#include "dec/vp8i.h"
#include "dec/webpi.h"    // for chunk-size constants

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// Object to store metadata about images.
typedef struct {
  uint32_t    x_offset_;
  uint32_t    y_offset_;
  uint32_t    duration_;
  uint32_t    width_;
  uint32_t    height_;
} WebPImageInfo;

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

// Main mux object. Stores data chunks.
struct WebPMux {
  WebPChunk*  images_;
  WebPChunk*  iccp_;
  WebPChunk*  meta_;
  WebPChunk*  loop_;
  WebPChunk*  vp8x_;

  WebPChunk*  unknown_;

  WebPChunk   mux_;        // Final assembly of chunks.
};

//------------------------------------------------------------------------------
// Internal struct management.

#define CHUNKS_PER_FRAME  2
#define CHUNKS_PER_TILE   2

typedef enum {
  VP8X_ID = 0,
  ICCP_ID,
  LOOP_ID,
  FRAME_ID,
  TILE_ID,
  IMAGE_ID,
  META_ID,
  UNKNOWN_ID,

  NIL_ID,
  LIST_ID
} TAG_ID;

// Maximum chunk payload (data) size such that adding the header and padding
// won't overflow an uint32.
static const uint32_t MAX_CHUNK_PAYLOAD = ~0U - CHUNK_HEADER_SIZE - 1;

#define NIL_TAG 0x00000000u  // To signal void chunk.

#define mktag(c1, c2, c3, c4) \
  ((uint32_t)c1 | (c2 << 8) | (c3 << 16) | (c4 << 24))

typedef struct ChunkInfo ChunkInfo;
struct ChunkInfo {
  const char*   chunkName;
  uint32_t      chunkTag;
  TAG_ID        chunkId;
  uint32_t      chunkSize;  // Negative value denotes that size is NOT fixed.
};

static const ChunkInfo kChunks[] = {
  {"vp8x",    mktag('V', 'P', '8', 'X'),  VP8X_ID,    VP8X_CHUNK_SIZE},
  {"iccp",    mktag('I', 'C', 'C', 'P'),  ICCP_ID,    -1},
  {"loop",    mktag('L', 'O', 'O', 'P'),  LOOP_ID,    LOOP_CHUNK_SIZE},
  {"frame",   mktag('F', 'R', 'M', ' '),  FRAME_ID,   FRAME_CHUNK_SIZE},
  {"tile",    mktag('T', 'I', 'L', 'E'),  TILE_ID,    TILE_CHUNK_SIZE},
  {"image",   mktag('V', 'P', '8', ' '),  IMAGE_ID,   -1},
  {"meta",    mktag('M', 'E', 'T', 'A'),  META_ID,    -1},
  {"unknown", mktag('U', 'N', 'K', 'N'),  UNKNOWN_ID, -1},

  {NULL,      NIL_TAG,                    NIL_ID,     -1},
  {"list",    mktag('L', 'I', 'S', 'T'),  LIST_ID,    -1}
};

static TAG_ID GetChunkIdFromName(const char* const what) {
  int i;
  if (what == NULL) return -1;
  for (i = 0; kChunks[i].chunkName != NULL; ++i) {
    if (!strcmp(what, kChunks[i].chunkName)) return i;
  }
  return NIL_ID;
}

static TAG_ID GetChunkIdFromTag(uint32_t tag) {
  int i;
  for (i = 0; kChunks[i].chunkTag != NIL_TAG; ++i) {
    if (tag == kChunks[i].chunkTag) return i;
  }
  return NIL_ID;
}

static WebPChunk** GetListFromId(const WebPMux* mux, TAG_ID id) {
  assert(mux != NULL);
  switch(id) {
    case VP8X_ID: return (WebPChunk**)&mux->vp8x_;
    case ICCP_ID: return (WebPChunk**)&mux->iccp_;
    case LOOP_ID: return (WebPChunk**)&mux->loop_;
    case FRAME_ID: return (WebPChunk**)&mux->images_;
    case TILE_ID: return (WebPChunk**)&mux->images_;
    case IMAGE_ID: return (WebPChunk**)&mux->images_;
    case META_ID: return (WebPChunk**)&mux->meta_;
    case UNKNOWN_ID: return (WebPChunk**)&mux->unknown_;
    default: return NULL;
  }
}

//------------------------------------------------------------------------------
// ImageInfo object management.

static void InitImageInfo(WebPImageInfo* const image_info) {
  assert(image_info);
  memset(image_info, 0, sizeof(*image_info));
}

// Creates WebPImageInfo object and sets offsets, dimensions and duration.
// Dimensions calculated from passed VP8 image data.
static WebPImageInfo* CreateImageInfo(uint32_t x_offset, uint32_t y_offset,
                                      uint32_t duration, const uint8_t* data,
                                      uint32_t size) {
  int width;
  int height;
  WebPImageInfo* image_info = NULL;

  if (!VP8GetInfo(data, size, size, &width, &height, NULL)) {
    return NULL;
  }

  image_info = (WebPImageInfo*)malloc(sizeof(WebPImageInfo));
  if (image_info != NULL) {
    InitImageInfo(image_info);
    image_info->x_offset_ = x_offset;
    image_info->y_offset_ = y_offset;
    image_info->duration_ = duration;
    image_info->width_ = width;
    image_info->height_ = height;
  }

  return image_info;
}

//------------------------------------------------------------------------------
// Chunks management.

static void InitChunk(WebPChunk* const chunk) {
  assert(chunk);
  chunk->tag_ = NIL_TAG;
  chunk->data_ = NULL;
  chunk->payload_size_ = 0;
  chunk->owner_ = 0;
  chunk->image_info_ = NULL;
  chunk->next_ = NULL;
}

// Releases chunk and returns chunk->next_.
static WebPChunk* ReleaseChunk(WebPChunk* const chunk) {
  WebPChunk* next;
  if (chunk == NULL) return NULL;
  free(chunk->image_info_);
  if (chunk->owner_) {
    free((void*)chunk->data_);
  }
  next = chunk->next_;
  InitChunk(chunk);
  return next;
}

// If tag == NIL_TAG, any tag in the chunk list will be matched.
static int ListCountChunks(WebPChunk* const chunk_list, uint32_t tag) {
  int count = 0;
  WebPChunk* current;
  for (current = chunk_list; current != NULL; current = current->next_) {
    if ((tag == NIL_TAG) || (current->tag_ == tag)) {
      count++;  // Count chunks whose tags match.
    }
  }
  return count;
}

// Returns next chunk in the chunk list with the given tag.
static WebPChunk* SearchNextChunkList(WebPChunk* chunk, uint32_t tag) {
  while (chunk && chunk->tag_ != tag) {
    chunk = chunk->next_;
  }
  return chunk;
}

static WebPChunk* SearchChunkList(WebPChunk* first, uint32_t nth,
                                  uint32_t tag) {
  // nth = 0 means "last of the list".
  uint32_t iter = nth;
  first = SearchNextChunkList(first, tag);
  if (!first) return NULL;

  while (--iter != 0) {
    WebPChunk* next_chunk = SearchNextChunkList(first->next_, tag);
    if (next_chunk == NULL) break;
    first = next_chunk;
  }
  return ((nth > 0) && (iter > 0)) ? NULL : first;
}

// Outputs a pointer to 'prev_chunk->next_',
//   where 'prev_chunk' is the pointer to the chunk at position (nth - 1).
// If tag == NIL_TAG, any chunk will be matched.
// Returns 1 if nth chunk was found, 0 otherwise.
static int SearchChunkToDelete(WebPChunk** chunk_list, uint32_t nth,
                               uint32_t tag, WebPChunk*** const location) {
  uint32_t count = 0;
  assert(chunk_list);
  *location = chunk_list;

  if (nth == 0) {
    nth = ListCountChunks(*chunk_list, tag);
    if (nth == 0) return 0;  // Not found.
  }

  while (*chunk_list) {
    WebPChunk* const cur_chunk = *chunk_list;
    if ((tag == NIL_TAG) || (cur_chunk->tag_ == tag)) {
      ++count;
      if (count == nth) return 1;  // Found.
    }
    chunk_list = &cur_chunk->next_;
    *location = chunk_list;
  }
  return 0;  // Not found.
}

// Outputs a pointer to 'prev_chunk->next_',
//   where 'prev_chunk' is the pointer to the chunk at position (nth - 1).
// Returns 1 if nth chunk was found, 0 otherwise.
static int SearchChunkToSet(WebPChunk** chunk_list, uint32_t nth,
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

// Deletes given chunk & returns chunk->next_.
static WebPChunk* DeleteChunk(WebPChunk* const chunk) {
  WebPChunk* const next = ReleaseChunk(chunk);
  free(chunk);
  return next;
}

// Deletes all chunks in the chunk list.
static void DeleteAllChunks(WebPChunk** const chunk_list) {
  while (*chunk_list) {
    *chunk_list = DeleteChunk(*chunk_list);
  }
}

// Delete all chunks in the chunk list with given tag.
static WebPMuxError DeleteChunks(WebPChunk** chunk_list, uint32_t tag) {
  WebPMuxError err = WEBP_MUX_NOT_FOUND;
  assert(chunk_list);
  while (*chunk_list) {
    WebPChunk* const chunk = *chunk_list;
    if (chunk->tag_ == tag) {
      *chunk_list = DeleteChunk(chunk);
      err = WEBP_MUX_OK;
    } else {
      chunk_list = &chunk->next_;
    }
  }
  return err;
}

// Delete nth chunk in the chunk list with given tag.
// If tag == NIL_TAG, any chunk will be matched.
static WebPMuxError DeleteNthChunk(WebPChunk** chunk_list, uint32_t nth,
                                   uint32_t tag) {
  if (!SearchChunkToDelete(chunk_list, nth, tag, &chunk_list)) {
    return WEBP_MUX_NOT_FOUND;
  }
  *chunk_list = DeleteChunk(*chunk_list);
  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------
// Mux object management.

static int WebPMuxInit(WebPMux* const mux) {
  if (mux == NULL) return 0;
  memset(mux, 0, sizeof(*mux));
  return 1;
}

WebPMux* WebPMuxNew(void) {
  WebPMux* const mux = (WebPMux*)malloc(sizeof(WebPMux));
  if (mux) WebPMuxInit(mux);
  return mux;
}

static int WebPMuxRelease(WebPMux* const mux) {
  if (mux == NULL) return 0;
  DeleteAllChunks(&mux->vp8x_);
  DeleteAllChunks(&mux->iccp_);
  DeleteAllChunks(&mux->loop_);
  DeleteAllChunks(&mux->images_);
  DeleteAllChunks(&mux->meta_);
  DeleteAllChunks(&mux->unknown_);
  ReleaseChunk(&mux->mux_);
  return 1;
}

void WebPMuxDelete(WebPMux* const mux) {
  if (mux) {
    WebPMuxRelease(mux);
    free(mux);
  }
}

//------------------------------------------------------------------------------
// Helper functions.

static uint32_t GetLE32(const uint8_t* const data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Size of a chunk including header and padding.
static uint32_t ChunkDiskSize(const WebPChunk* chunk) {
  assert(chunk->payload_size_ < MAX_CHUNK_PAYLOAD);
  return CHUNK_HEADER_SIZE + ((chunk->payload_size_ + 1) & ~1U);
}

static void PutLE16(uint8_t* const data, uint16_t val) {
  data[0] = (val >> 0) & 0xff;
  data[1] = (val >> 8) & 0xff;
}

static void PutLE32(uint8_t* const data, uint32_t val) {
  PutLE16(data, val);
  PutLE16(data + 2, val >> 16);
}

static WebPMuxError AssignData(WebPChunk* chunk, const uint8_t* data,
                               uint32_t data_size, WebPImageInfo* image_info,
                               int copy_data, uint32_t tag) {
  WebPMuxError err = WEBP_MUX_OK;

  // For internally allocated chunks, always copy data & make it owner of data.
  if ((tag == kChunks[VP8X_ID].chunkTag) ||
      (tag == kChunks[LOOP_ID].chunkTag)) {
    copy_data = 1;
  }

  ReleaseChunk(chunk);
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

  return err;
}

static WebPMuxError RecordChunk(WebPChunk* chunk, const uint8_t* data,
                                uint32_t max_size, int copy_data) {
  uint32_t size = 0;
  assert(max_size >= CHUNK_HEADER_SIZE);

  size = GetLE32(data + 4);
  assert(size <= MAX_CHUNK_PAYLOAD);
  if (size + CHUNK_HEADER_SIZE > max_size) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return AssignData(chunk, data + CHUNK_HEADER_SIZE, size, NULL, copy_data,
                    GetLE32(data + 0));
}

// Sets 'chunk' at nth position in the 'chunk_list'.
// nth = 0 has the special meaning "last of the list".
static WebPMuxError SetNthChunk(const WebPChunk* chunk,
                                WebPChunk** chunk_list, uint32_t nth) {
  WebPChunk* new_chunk;

  if (!SearchChunkToSet(chunk_list, nth, &chunk_list)) {
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
// Writing.

// Handy MACRO, makes MuxSet() very symmetric to MuxGet().
#define SWITCH_ID_LIST(ID, LIST)                                    \
    if (id == (ID)) {                                               \
      err = AssignData(&chunk, data, size,                          \
                       image_info,                                  \
                       copy_data, kChunks[(ID)].chunkTag);          \
      if (err == WEBP_MUX_OK) {                                     \
        err = SetNthChunk(&chunk, (LIST), nth);                     \
      }                                                             \
      return err;                                                   \
    }

static WebPMuxError MuxSet(WebPMux* const mux, TAG_ID id, uint32_t nth,
                           const uint8_t* data, uint32_t size,
                           WebPImageInfo* image_info, int copy_data) {
  WebPChunk chunk;
  WebPMuxError err = WEBP_MUX_NOT_FOUND;
  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;
  InitChunk(&chunk);
  SWITCH_ID_LIST(VP8X_ID, &mux->vp8x_);
  SWITCH_ID_LIST(ICCP_ID, &mux->iccp_);
  SWITCH_ID_LIST(LOOP_ID, &mux->loop_);
  SWITCH_ID_LIST(FRAME_ID, &mux->images_);
  SWITCH_ID_LIST(TILE_ID, &mux->images_);
  SWITCH_ID_LIST(IMAGE_ID, &mux->images_);
  SWITCH_ID_LIST(META_ID, &mux->meta_);
  if (id == UNKNOWN_ID && size > 4) {
    // For raw-data unknown chunk, the first four bytes should be the tag to be
    // used for the chunk.
    err = AssignData(&chunk, data + 4, size - 4, image_info, copy_data,
                     GetLE32(data + 0));
    if (err == WEBP_MUX_OK)
      err = SetNthChunk(&chunk, &mux->unknown_, nth);
  }
  return err;
}
#undef SWITCH_ID_LIST

static WebPMuxError WebPMuxAddChunk(WebPMux* const mux, uint32_t nth,
                                    uint32_t tag, const uint8_t* data,
                                    uint32_t size, WebPImageInfo* image_info,
                                    int copy_data) {
  TAG_ID id;
  assert(mux != NULL);
  assert(size <= MAX_CHUNK_PAYLOAD);

  id = GetChunkIdFromTag(tag);
  if (id == NIL_ID) return WEBP_MUX_INVALID_PARAMETER;

  return MuxSet(mux, id, nth, data, size, image_info, copy_data);
}

WebPMuxError WebPMuxCreate(WebPMux* const mux, const uint8_t* data,
                           uint32_t size, int copy_data) {
  uint32_t mux_size;
  uint32_t tag;
  const uint8_t* end;
  TAG_ID id;

  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  // Sanity checks on size and leading bytes.
  if (size < RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }
  if (GetLE32(data + 0) != mktag('R', 'I', 'F', 'F') ||
      GetLE32(data + 8) != mktag('W', 'E', 'B', 'P')) {
    return WEBP_MUX_BAD_DATA;
  }
  mux_size = CHUNK_HEADER_SIZE + GetLE32(data + 4);
  if (mux_size > size) {
    return WEBP_MUX_BAD_DATA;
  }
  tag = GetLE32(data + RIFF_HEADER_SIZE);
  if (tag != kChunks[IMAGE_ID].chunkTag && tag != kChunks[VP8X_ID].chunkTag) {
    // First chunk should be either VP8X or VP8.
    return WEBP_MUX_BAD_DATA;
  }
  end = data + mux_size;
  data += RIFF_HEADER_SIZE;
  mux_size -= RIFF_HEADER_SIZE;

  // TODO(urvang): Add a call to WebPMuxInit() here to empty mux (next CL).

  // Loop over chunks.
  while (data != end) {
    WebPMuxError err = WEBP_MUX_OK;
    WebPChunk chunk;
    WebPChunk** chunk_list;
    uint32_t data_size;

    InitChunk(&chunk);
    err = RecordChunk(&chunk, data, mux_size, copy_data);
    if (err != WEBP_MUX_OK) return err;

    data_size = ChunkDiskSize(&chunk);

    id = GetChunkIdFromTag(chunk.tag_);
    chunk_list = GetListFromId(mux, id);

    if (chunk_list == NULL) chunk_list = (WebPChunk**)&mux->unknown_;
    err = SetNthChunk(&chunk, chunk_list, 0);
    if (err != WEBP_MUX_OK) return err;

    data += data_size;
    mux_size -= data_size;
  }
  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxSetMetadata(WebPMux* const mux, const uint8_t* data,
                                uint32_t size, int copy_data) {
  WebPMuxError err;

  if ((mux == NULL) || (data == NULL) || (size > MAX_CHUNK_PAYLOAD)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Delete the existing metadata chunk(s).
  err = WebPMuxDeleteMetadata(mux);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Add the given metadata chunk.
  return MuxSet(mux, META_ID, 1, data, size, NULL, copy_data);
}

WebPMuxError WebPMuxSetColorProfile(WebPMux* const mux, const uint8_t* data,
                                    uint32_t size, int copy_data) {
  WebPMuxError err;

  if ((mux == NULL) || (data == NULL) || (size > MAX_CHUNK_PAYLOAD)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Delete the existing ICCP chunk(s).
  err = WebPMuxDeleteColorProfile(mux);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Add the given ICCP chunk.
  return MuxSet(mux, ICCP_ID, 1, data, size, NULL, copy_data);
}

static WebPMuxError WebPMuxDeleteLoopCount(WebPMux* const mux);

WebPMuxError WebPMuxSetLoopCount(WebPMux* const mux, uint32_t loop_count) {
  WebPMuxError err;
  uint8_t* data = NULL;

  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  // Delete the existing LOOP chunk(s).
  err = WebPMuxDeleteLoopCount(mux);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Add the given loop count.
  data = (uint8_t *)malloc(kChunks[LOOP_ID].chunkSize);
  if (data == NULL) return WEBP_MUX_MEMORY_ERROR;

  PutLE32(data, loop_count);
  err = WebPMuxAddChunk(mux, 1, kChunks[LOOP_ID].chunkTag, data,
                        kChunks[LOOP_ID].chunkSize, NULL, 1);
  free(data);
  return err;
}

// Find out absolute indices or frame/chunk and image chunks
// given overall index 'nth'.
// isFrame = true -> This is for a frame.
// isFrame = false -> This is for a tile.
// Returns 0 if error
//         1 on success
static int GetFrameTileAndImageIndices(uint32_t nth, int isFrame,
                                       uint32_t* frame_tile_idx,
                                       uint32_t* image_idx) {
  const int multiplier = isFrame ? CHUNKS_PER_FRAME : CHUNKS_PER_TILE;

  assert(frame_tile_idx != NULL);
  assert(image_idx != NULL);

  if (nth == 0) {  // Both chunks are at the end.
    *frame_tile_idx = 0;
    *image_idx = 0;
  } else {  // Position given.
    // Frame/Tile chunk is in (multiplier * nth - 1) position in
    // mux->images_ list because prior (nth-1) chunks occupy positions
    // 1 through multiplier * (nth - 1).
    *frame_tile_idx = multiplier * nth - 1;

    // Image chunk is in (multiplier * nth) position in mux->images_ list.
    *image_idx = *frame_tile_idx + 1;
  }
  return 1;
}

static WebPMuxError CreateDataFromImageInfo(WebPImageInfo* image_info,
                                            int isFrame, uint8_t** data,
                                            uint32_t* size) {
  assert(data);
  assert(size);
  assert(image_info);

  *size = isFrame ? kChunks[FRAME_ID].chunkSize : kChunks[TILE_ID].chunkSize;
  *data = (uint8_t*)malloc(*size);
  if (*data == NULL) return WEBP_MUX_MEMORY_ERROR;

  // Fill in data according to frame/tile chunk format.
  PutLE32(*data, image_info->x_offset_);
  PutLE32(*data + 4, image_info->y_offset_);

  if (isFrame) {
    PutLE32(*data + 8, image_info->width_);
    PutLE32(*data + 12, image_info->height_);
    PutLE32(*data + 16, image_info->duration_);
  }
  return WEBP_MUX_OK;
}

static WebPMuxError WebPMuxDeleteNamedData(WebPMux* const mux, uint32_t nth,
                                           const char* const tag);

static WebPMuxError WebPMuxAddFrameTileInternal(WebPMux* const mux,
                                                uint32_t nth,
                                                const uint8_t* data,
                                                uint32_t size,
                                                uint32_t x_offset,
                                                uint32_t y_offset,
                                                uint32_t duration,
                                                int copy_data,
                                                uint32_t tag) {
  WebPMuxError err;
  WebPImageInfo* image_info = NULL;
  uint8_t* frame_tile_data = NULL;
  uint32_t frame_tile_data_size = 0;
  uint32_t pos1, pos2;
  int ok;
  const int isFrame = (tag == kChunks[FRAME_ID].chunkTag) ? 1 : 0;
  const char* tag_name = isFrame ? "frame" : "tile";

  if (mux == NULL || data == NULL || size > MAX_CHUNK_PAYLOAD) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Get absolute indexes for frame/title and image chunks
  // (because MuxSet() assumes 'nth' is absolute position in a chunk list).
  ok = GetFrameTileAndImageIndices(nth, isFrame, &pos1, &pos2);
  if (!ok) return WEBP_MUX_MEMORY_ERROR;

  // Create image_info object.
  image_info = CreateImageInfo(x_offset, y_offset, duration, data, size);
  if (image_info == NULL) return WEBP_MUX_MEMORY_ERROR;

  // Create frame/tile data from image_info.
  err = CreateDataFromImageInfo(image_info, isFrame, &frame_tile_data,
                                &frame_tile_data_size);
  if (err != WEBP_MUX_OK) {
    free(image_info);
    return err;
  }

  // Add frame/tile chunk (with copy_data = 1).
  err = WebPMuxAddChunk(mux, pos1, tag, frame_tile_data, frame_tile_data_size,
                        NULL, 1);
  if (err != WEBP_MUX_OK) {
    free(image_info);
    free(frame_tile_data);
    return err;
  }

  // Add image chunk.
  err = WebPMuxAddChunk(mux, pos2, kChunks[IMAGE_ID].chunkTag, data, size,
                        image_info, copy_data);
  if (err != WEBP_MUX_OK) {
    WebPMuxDeleteNamedData(mux, pos1, tag_name);  // Delete frame/tile chunk.
    free(image_info);
  }

  free(frame_tile_data);
  return err;
}

WebPMuxError WebPMuxAddFrame(WebPMux* const mux, uint32_t nth,
                             const uint8_t* data, uint32_t size,
                             uint32_t x_offset, uint32_t y_offset,
                             uint32_t duration, int copy_data) {
  return WebPMuxAddFrameTileInternal(mux, nth, data, size, x_offset, y_offset,
                                     duration, copy_data,
                                     kChunks[FRAME_ID].chunkTag);
}

WebPMuxError WebPMuxAddTile(WebPMux* const mux, uint32_t nth,
                            const uint8_t* data, uint32_t size,
                            uint32_t x_offset, uint32_t y_offset,
                            int copy_data) {
  return WebPMuxAddFrameTileInternal(mux, nth, data, size, x_offset, y_offset,
                                     1, copy_data, kChunks[TILE_ID].chunkTag);
}

WebPMuxError WebPMuxAddNamedData(WebPMux* const mux, uint32_t nth,
                                 const char* const tag, const uint8_t* data,
                                 uint32_t size, int copy_data) {
  TAG_ID id;
  if (mux == NULL || tag == NULL || data == NULL || size > MAX_CHUNK_PAYLOAD) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }
  id = GetChunkIdFromName(tag);
  if (id == NIL_ID) return WEBP_MUX_INVALID_PARAMETER;

  return WebPMuxAddChunk(mux, nth, kChunks[id].chunkTag, data, size, NULL,
                         copy_data);
}

static WebPMuxError WebPMuxDeleteNamedData(WebPMux* const mux, uint32_t nth,
                                           const char* const tag) {
  TAG_ID id;
  WebPChunk** chunk_list;
  if ((mux == NULL) || (tag == NULL)) return WEBP_MUX_INVALID_ARGUMENT;

  id = GetChunkIdFromName(tag);
  chunk_list = GetListFromId(mux, id);
  if (chunk_list == NULL) return WEBP_MUX_INVALID_PARAMETER;

  return DeleteNthChunk(chunk_list, nth, kChunks[id].chunkTag);
}

static WebPMuxError WebPMuxDeleteAllNamedData(WebPMux* const mux,
                                              const char* const tag) {
  TAG_ID id;
  WebPChunk** chunk_list;
  if ((mux == NULL) || (tag == NULL)) return WEBP_MUX_INVALID_ARGUMENT;

  id = GetChunkIdFromName(tag);
  chunk_list = GetListFromId(mux, id);
  if (chunk_list == NULL) return WEBP_MUX_INVALID_PARAMETER;

  return DeleteChunks(chunk_list, kChunks[id].chunkTag);
}

WebPMuxError WebPMuxDeleteMetadata(WebPMux* const mux) {
  return WebPMuxDeleteAllNamedData(mux, kChunks[META_ID].chunkName);
}

WebPMuxError WebPMuxDeleteColorProfile(WebPMux* const mux) {
  return WebPMuxDeleteAllNamedData(mux, kChunks[ICCP_ID].chunkName);
}

static WebPMuxError WebPMuxDeleteLoopCount(WebPMux* const mux) {
  return WebPMuxDeleteAllNamedData(mux, kChunks[LOOP_ID].chunkName);
}

static WebPMuxError WebPMuxDeleteFrameTileInternal(WebPMux* const mux,
                                                   uint32_t nth,
                                                   const char* const tag) {
  WebPMuxError err;

  // Delete image chunk.
  err = WebPMuxDeleteNamedData(mux, nth, kChunks[IMAGE_ID].chunkName);
  if (err != WEBP_MUX_OK) return err;

  // Delete frame chunk.
  return WebPMuxDeleteNamedData(mux, nth, tag);
}

WebPMuxError WebPMuxDeleteFrame(WebPMux* const mux, uint32_t nth) {
  return WebPMuxDeleteFrameTileInternal(mux, nth, kChunks[FRAME_ID].chunkName);
}

WebPMuxError WebPMuxDeleteTile(WebPMux* const mux, uint32_t nth) {
  return WebPMuxDeleteFrameTileInternal(mux, nth, kChunks[TILE_ID].chunkName);
}

static int IsNotCompatible(int feature, int num_items) {
  return (feature != 0) != (num_items > 0);
}

static WebPMuxError WebPMuxValidate(const WebPMux* mux) {
  int num_iccp;
  int num_meta;
  int num_loop_chunks;
  int num_frames;
  int num_tiles;
  int num_vp8x;
  int num_images;
  uint32_t flags;
  WebPMuxError err;

  // Verify mux is not NULL.
  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  // Verify mux has at least one image.
  if (mux->images_ == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = WebPMuxGetFeatures(mux, &flags);
  if (err != WEBP_MUX_OK) return err;

  // At most one color profile chunk.
  err = WebPMuxNumNamedElements(mux, kChunks[ICCP_ID].chunkName, &num_iccp);
  if (err != WEBP_MUX_OK) return err;
  if (num_iccp > 1) return WEBP_MUX_INVALID_ARGUMENT;

  // ICCP_FLAG and color profile chunk is consistent.
  if (IsNotCompatible(flags & ICCP_FLAG, num_iccp)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // At most one XMP metadata.
  err = WebPMuxNumNamedElements(mux, kChunks[META_ID].chunkName, &num_meta);
  if (err != WEBP_MUX_OK) return err;
  if (num_meta > 1) return WEBP_MUX_INVALID_ARGUMENT;

  // META_FLAG and XMP metadata chunk is consistent.
  if (IsNotCompatible(flags & META_FLAG, num_meta)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // At most one loop chunk.
  err = WebPMuxNumNamedElements(mux, kChunks[LOOP_ID].chunkName,
                                &num_loop_chunks);
  if (err != WEBP_MUX_OK) return err;
  if (num_loop_chunks > 1) return WEBP_MUX_INVALID_ARGUMENT;

  // Animation: ANIMATION_FLAG, loop chunk and frame chunk(s) are consistent.
  err = WebPMuxNumNamedElements(mux, kChunks[FRAME_ID].chunkName, &num_frames);
  if (err != WEBP_MUX_OK) return err;
  if ((flags & ANIMATION_FLAG) &&
      ((num_loop_chunks == 0) || (num_frames == 0))) {
    return WEBP_MUX_INVALID_ARGUMENT;
  } else if (((num_loop_chunks == 1) || (num_frames > 0)) &&
      !(flags & ANIMATION_FLAG)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Tiling: TILE_FLAG and tile chunk(s) are consistent.
  err = WebPMuxNumNamedElements(mux, kChunks[TILE_ID].chunkName, &num_tiles);
  if (err != WEBP_MUX_OK) return err;
  if (IsNotCompatible(flags & TILE_FLAG, num_tiles)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Verify either VP8X chunk is present OR there is only one elem in
  // mux->images_.
  err = WebPMuxNumNamedElements(mux, kChunks[VP8X_ID].chunkName, &num_vp8x);
  if (err != WEBP_MUX_OK) return err;
  err = WebPMuxNumNamedElements(mux, kChunks[IMAGE_ID].chunkName, &num_images);
  if (err != WEBP_MUX_OK) return err;

  if (num_vp8x > 1) {
    return WEBP_MUX_INVALID_ARGUMENT;
  } else if ((num_vp8x == 0) && (num_images != 1)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------
// Assembly of the WebP RIFF file.

static uint32_t ChunksListDiskSize(const WebPChunk* chunk_list) {
  uint32_t size = 0;
  while (chunk_list) {
    size += ChunkDiskSize(chunk_list);
    chunk_list = chunk_list->next_;
  }
  return size;
}

static uint8_t* EmitChunk(const WebPChunk* const chunk, uint8_t* dst) {
  assert(chunk);
  assert(chunk->tag_ != NIL_TAG);
  PutLE32(dst + 0, chunk->tag_);
  PutLE32(dst + 4, chunk->payload_size_);
  memcpy(dst + CHUNK_HEADER_SIZE, chunk->data_, chunk->payload_size_);
  if (chunk->payload_size_ & 1)
    dst[CHUNK_HEADER_SIZE + chunk->payload_size_] = 0;  // Add padding.
  return dst + ChunkDiskSize(chunk);
}

static uint8_t* EmitChunks(const WebPChunk* chunk_list, uint8_t* dst) {
  while (chunk_list) {
    dst = EmitChunk(chunk_list, dst);
    chunk_list = chunk_list->next_;
  }
  return dst;
}

static WebPMuxError GetImageCanvasHeightWidth(WebPMux* const mux,
                                              uint32_t flags,
                                              uint32_t* width,
                                              uint32_t* height) {
  uint32_t max_x = 0;
  uint32_t max_y = 0;
  uint64_t image_area = 0;
  WebPChunk* image_chunk = NULL;

  assert(mux != NULL);
  assert(mux->images_ != NULL);
  assert(width && height);

  image_chunk = mux->images_;

  if (image_chunk->next_) {
    // Aggregate the bounding box for Animation frames & Tiled images.
    for (; image_chunk != NULL; image_chunk = image_chunk->next_) {
      const WebPImageInfo* const image_info = image_chunk->image_info_;

      if (image_chunk->tag_ != kChunks[IMAGE_ID].chunkTag) continue;

      if (image_info != NULL) {
        const uint32_t max_x_pos = image_info->x_offset_ + image_info->width_;
        const uint32_t max_y_pos = image_info->y_offset_ + image_info->height_;
        if (max_x_pos < image_info->x_offset_) {  // Overflow occurred.
          return WEBP_MUX_INVALID_ARGUMENT;
        }
        if (max_y_pos < image_info->y_offset_) {  // Overflow occurred.
          return WEBP_MUX_INVALID_ARGUMENT;
        }
        if (max_x_pos > max_x) {
          max_x = max_x_pos;
        }
        if (max_y_pos > max_y) {
          max_y = max_y_pos;
        }
        image_area += (image_info->width_ * image_info->height_);
      }
    }
    *width = max_x;
    *height = max_y;
    // Crude check to validate that there are no image overlaps/holes for Tile
    // images. Check that the aggregated image area for individual tiles exactly
    // matches the image area of the constructed Canvas. However, the area-match
    // is necessary but not sufficient condition.
    if ((flags & TILE_FLAG) && (image_area != (max_x * max_y))) {
      *width = 0;
      *height = 0;
      return WEBP_MUX_INVALID_ARGUMENT;
    }
  } else {
    // For a single image, extract the width & height from VP8 image-data.
    int w, h;
    if (VP8GetInfo(image_chunk->data_, image_chunk->payload_size_,
                   image_chunk->payload_size_, &w, &h, NULL)) {
      *width = w;
      *height = h;
    }
  }
  return WEBP_MUX_OK;
}

// Following VP8X format followed:
// Total Size : 12,
// Flags : 4 bytes,
// Width  : 4 bytes,
// Height : 4 bytes.
static WebPMuxError CreateVp8xChunk(WebPMux* const mux) {
  WebPMuxError err = WEBP_MUX_OK;
  uint32_t flags = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t data[VP8X_CHUNK_SIZE];
  const size_t data_size = VP8X_CHUNK_SIZE;

  assert(mux != NULL);
  if (mux->images_ == NULL || mux->images_->data_ == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // If VP8X chunk(s) is(are) already present, remove them (and later add new
  // VP8X chunk with updated flags).
  err = WebPMuxDeleteAllNamedData(mux, kChunks[VP8X_ID].chunkName);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Set flags.
  if (mux->iccp_ != NULL && mux->iccp_->data_ != NULL) {
    flags |= ICCP_FLAG;
  }

  if (mux->meta_ != NULL && mux->meta_->data_ != NULL) {
    flags |= META_FLAG;
  }

  if (mux->images_->tag_ == kChunks[TILE_ID].chunkTag) {
    // This is a tiled image.
    flags |= TILE_FLAG;
  } else if (mux->images_->tag_ == kChunks[FRAME_ID].chunkTag) {
    // This is a simple animation image.
    flags |= ANIMATION_FLAG;
  }

  if (flags == 0) {
    // For Simple Image, VP8X chunk should not be added.
    return WEBP_MUX_OK;
  }

  err = GetImageCanvasHeightWidth(mux, flags, &width, &height);
  if (err != WEBP_MUX_OK) return err;

  PutLE32(data + 0, flags);   // Put VP8X Chunk Flags.
  PutLE32(data + 4, width);   // Put canvasWidth.
  PutLE32(data + 8, height);  // Put canvasHeight.

  err = WebPMuxAddChunk(mux, 1, kChunks[VP8X_ID].chunkTag, data, data_size,
                        NULL, 1);
  return err;
}

WebPMuxError WebPMuxAssemble(WebPMux* const mux, uint8_t** output_data,
                             uint32_t* output_size) {
  uint32_t size = 0;
  uint8_t* data = NULL;
  uint8_t* dst = NULL;
  int num_frames;
  int num_loop_chunks;
  WebPMuxError err;

  if (mux == NULL || output_data == NULL || output_size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  *output_data = NULL;
  *output_size = 0;

  // Remove LOOP chunk if unnecessary.
  err = WebPMuxNumNamedElements(mux, kChunks[LOOP_ID].chunkName,
                                &num_loop_chunks);
  if (err != WEBP_MUX_OK) return err;
  if (num_loop_chunks >= 1) {
    err = WebPMuxNumNamedElements(mux, kChunks[FRAME_ID].chunkName,
                                  &num_frames);
    if (err != WEBP_MUX_OK) return err;
    if (num_frames == 0) {
      err = WebPMuxDeleteLoopCount(mux);
      if (err != WEBP_MUX_OK) return err;
    }
  }

  // Create VP8X chunk.
  err = CreateVp8xChunk(mux);
  if (err != WEBP_MUX_OK) return err;

  // Allocate data.
  ReleaseChunk(&mux->mux_);
  size = ChunksListDiskSize(mux->vp8x_) + ChunksListDiskSize(mux->iccp_)
      + ChunksListDiskSize(mux->loop_) + ChunksListDiskSize(mux->images_)
      + ChunksListDiskSize(mux->meta_) + ChunksListDiskSize(mux->unknown_)
      + RIFF_HEADER_SIZE;

  data = (uint8_t*)malloc(size);
  if (data == NULL) return WEBP_MUX_MEMORY_ERROR;

  // Main RIFF header.
  PutLE32(data + 0, mktag('R', 'I', 'F', 'F'));
  PutLE32(data + 4, size - CHUNK_HEADER_SIZE);
  PutLE32(data + 8, mktag('W', 'E', 'B', 'P'));

  // Chunks.
  dst = data + RIFF_HEADER_SIZE;
  dst = EmitChunks(mux->vp8x_, dst);
  dst = EmitChunks(mux->iccp_, dst);
  dst = EmitChunks(mux->loop_, dst);
  dst = EmitChunks(mux->images_, dst);
  dst = EmitChunks(mux->meta_, dst);
  dst = EmitChunks(mux->unknown_, dst);
  assert(dst == data + size);

  // Validate mux.
  err = WebPMuxValidate(mux);
  if (err != WEBP_MUX_OK) {
    free(data);
    data = NULL;
    size = 0;
  }

  // Finalize.
  mux->mux_.data_ = data;
  mux->mux_.payload_size_ = size;
  *output_data = data;
  *output_size = size;

  return err;
}

//------------------------------------------------------------------------------
// Reading.

// Handy MACRO.
#define SWITCH_ID_LIST(ID, LIST)                                              \
  if (id == (ID)) {                                                           \
    const WebPChunk* const chunk = SearchChunkList((LIST), nth,               \
                                                   kChunks[(ID)].chunkTag);   \
    if (chunk) {                                                              \
      *data = chunk->data_;                                                   \
      *data_size = chunk->payload_size_;                                      \
      return WEBP_MUX_OK;                                                     \
    } else {                                                                  \
      return WEBP_MUX_NOT_FOUND;                                              \
    }                                                                         \
  }

static WebPMuxError MuxGet(const WebPMux* mux, TAG_ID id, uint32_t nth,
                           const uint8_t** data, uint32_t* data_size) {
  assert(mux != NULL);
  *data = NULL;
  *data_size = 0;
  SWITCH_ID_LIST(VP8X_ID, mux->vp8x_);
  SWITCH_ID_LIST(ICCP_ID, mux->iccp_);
  SWITCH_ID_LIST(LOOP_ID, mux->loop_);
  SWITCH_ID_LIST(FRAME_ID, mux->images_);
  SWITCH_ID_LIST(TILE_ID, mux->images_);
  SWITCH_ID_LIST(IMAGE_ID, mux->images_);
  SWITCH_ID_LIST(META_ID, mux->meta_);
  SWITCH_ID_LIST(UNKNOWN_ID, mux->unknown_);
  return WEBP_MUX_NOT_FOUND;
}
#undef SWITCH_ID_LIST

WebPMuxError WebPMuxGetFeatures(const WebPMux* mux, uint32_t* flags) {
  const uint8_t* data;
  uint32_t data_size;
  WebPMuxError err;

  if (mux == NULL || flags == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = WebPMuxGetNamedData(mux, kChunks[VP8X_ID].chunkName, 1, &data,
                            &data_size);
  if (err == WEBP_MUX_NOT_FOUND) {  // Single image case.
    *flags = 0;
    return WEBP_MUX_OK;
  }

  // Multiple image case.
  if (err != WEBP_MUX_OK) return err;
  if (data_size < 4) return WEBP_MUX_BAD_DATA;
  *flags = GetLE32(data);

  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxGetMetadata(const WebPMux* mux, const uint8_t** data,
                                uint32_t* size) {
  if (mux == NULL || data == NULL || size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return MuxGet(mux, META_ID, 1, data, size);
}

WebPMuxError WebPMuxGetColorProfile(const WebPMux* mux, const uint8_t** data,
                                    uint32_t* size) {
  if (mux == NULL || data == NULL || size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return MuxGet(mux, ICCP_ID, 1, data, size);
}

WebPMuxError WebPMuxGetLoopCount(const WebPMux* mux, uint32_t* loop_count) {
  const uint8_t* data;
  uint32_t data_size;
  WebPMuxError err;

  if (mux == NULL || loop_count == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = WebPMuxGetNamedData(mux, kChunks[LOOP_ID].chunkName, 1, &data,
                            &data_size);
  if (err != WEBP_MUX_OK) return err;
  if (data_size < kChunks[LOOP_ID].chunkSize) return WEBP_MUX_BAD_DATA;
  *loop_count = GetLE32(data);

  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxGetFrame(const WebPMux* mux, uint32_t nth,
                             const uint8_t** data, uint32_t* size,
                             uint32_t* x_offset, uint32_t* y_offset,
                             uint32_t* duration) {
  const uint8_t* frame_data;
  uint32_t frame_size;
  WebPMuxError err;

  if (mux == NULL || data == NULL || size == NULL || x_offset == NULL ||
      y_offset == NULL || duration == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Get frame chunk.
  err = WebPMuxGetNamedData(mux, kChunks[FRAME_ID].chunkName, nth, &frame_data,
                            &frame_size);
  if (err != WEBP_MUX_OK) return err;
  if (frame_size < kChunks[FRAME_ID].chunkSize) return WEBP_MUX_BAD_DATA;
  *x_offset = GetLE32(frame_data);
  *y_offset = GetLE32(frame_data + 4);
  *duration = GetLE32(frame_data + 16);

  // Get image chunk.
  return WebPMuxGetNamedData(mux, kChunks[IMAGE_ID].chunkName, nth, data, size);
}

WebPMuxError WebPMuxGetTile(const WebPMux* mux, uint32_t nth,
                            const uint8_t** data, uint32_t* size,
                            uint32_t* x_offset, uint32_t* y_offset) {
  const uint8_t* tile_data;
  uint32_t tile_size;
  WebPMuxError err;

  if (mux == NULL || data == NULL || size == NULL || x_offset == NULL ||
      y_offset == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Get frame chunk.
  err = WebPMuxGetNamedData(mux, kChunks[TILE_ID].chunkName, nth, &tile_data,
                            &tile_size);
  if (err != WEBP_MUX_OK) return err;
  if (tile_size < kChunks[TILE_ID].chunkSize) return WEBP_MUX_BAD_DATA;
  *x_offset = GetLE32(tile_data);
  *y_offset = GetLE32(tile_data + 4);

  // Get image chunk.
  return WebPMuxGetNamedData(mux, kChunks[IMAGE_ID].chunkName, nth, data, size);
}

WebPMuxError WebPMuxNumNamedElements(const WebPMux* mux, const char* tag,
                                     int* num_elements) {
  TAG_ID id;
  WebPChunk** chunk_list;

  if (mux == NULL || tag == NULL || num_elements == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  id = GetChunkIdFromName(tag);
  chunk_list = GetListFromId(mux, id);

  if (chunk_list == NULL) {
    *num_elements = 0;
  } else {
    *num_elements = ListCountChunks(*chunk_list, kChunks[id].chunkTag);
  }

  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxGetNamedData(const WebPMux* mux, const char* tag,
                                 uint32_t nth, const uint8_t** data,
                                 uint32_t* size) {
  TAG_ID id;
  if (mux == NULL || tag == NULL || data == NULL || size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  id = GetChunkIdFromName(tag);
  if (id == NIL_ID) return WEBP_MUX_INVALID_PARAMETER;

  return MuxGet(mux, id, nth, data, size);
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
