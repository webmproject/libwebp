// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Set and delete APIs for mux.
//
// Authors: Urvang (urvang@google.com)
//          Vikas (vikasa@google.com)

#include <assert.h>
#include "./muxi.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Life of a mux object.

static void MuxInit(WebPMux* const mux) {
  if (mux == NULL) return;
  memset(mux, 0, sizeof(*mux));
  mux->state_ = WEBP_MUX_STATE_PARTIAL;
}

WebPMux* WebPMuxNew(void) {
  WebPMux* const mux = (WebPMux*)malloc(sizeof(WebPMux));
  if (mux) MuxInit(mux);
  return mux;
}

static void DeleteAllChunks(WebPChunk** const chunk_list) {
  while (*chunk_list) {
    *chunk_list = ChunkDelete(*chunk_list);
  }
}

static void MuxRelease(WebPMux* const mux) {
  if (mux == NULL) return;
  MuxImageDeleteAll(&mux->images_);
  DeleteAllChunks(&mux->vp8x_);
  DeleteAllChunks(&mux->iccp_);
  DeleteAllChunks(&mux->loop_);
  DeleteAllChunks(&mux->meta_);
  DeleteAllChunks(&mux->unknown_);
}

void WebPMuxDelete(WebPMux* const mux) {
  // If mux is NULL MuxRelease is a noop.
  MuxRelease(mux);
  free(mux);
}

//------------------------------------------------------------------------------
// Helper method(s).

// Handy MACRO, makes MuxSet() very symmetric to MuxGet().
#define SWITCH_ID_LIST(ID, LIST)                                               \
  if (id == (ID)) {                                                            \
    err = ChunkAssignDataImageInfo(&chunk, data, size,                         \
                                   image_info,                                 \
                                   copy_data, kChunks[(ID)].chunkTag);         \
    if (err == WEBP_MUX_OK) {                                                  \
      err = ChunkSetNth(&chunk, (LIST), nth);                                  \
    }                                                                          \
    return err;                                                                \
  }

static WebPMuxError MuxSet(WebPMux* const mux, TAG_ID id, uint32_t nth,
                           const uint8_t* data, uint32_t size,
                           WebPImageInfo* image_info, int copy_data) {
  WebPChunk chunk;
  WebPMuxError err = WEBP_MUX_NOT_FOUND;
  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;
  assert(!IsWPI(id));

  ChunkInit(&chunk);
  SWITCH_ID_LIST(VP8X_ID, &mux->vp8x_);
  SWITCH_ID_LIST(ICCP_ID, &mux->iccp_);
  SWITCH_ID_LIST(LOOP_ID, &mux->loop_);
  SWITCH_ID_LIST(META_ID, &mux->meta_);
  if (id == UNKNOWN_ID && size > TAG_SIZE) {
    // For raw-data unknown chunk, the first four bytes should be the tag to be
    // used for the chunk.
    err = ChunkAssignDataImageInfo(&chunk, data + TAG_SIZE, size - TAG_SIZE,
                                   image_info, copy_data, GetLE32(data + 0));
    if (err == WEBP_MUX_OK)
      err = ChunkSetNth(&chunk, &mux->unknown_, nth);
  }
  return err;
}
#undef SWITCH_ID_LIST

static WebPMuxError MuxAddChunk(WebPMux* const mux, uint32_t nth, uint32_t tag,
                                const uint8_t* data, uint32_t size,
                                WebPImageInfo* image_info, int copy_data) {
  const TAG_ID id = ChunkGetIdFromTag(tag);
  assert(mux != NULL);
  assert(size <= MAX_CHUNK_PAYLOAD);

  if (id == NIL_ID) return WEBP_MUX_INVALID_PARAMETER;
  return MuxSet(mux, id, nth, data, size, image_info, copy_data);
}

static void InitImageInfo(WebPImageInfo* const image_info) {
  assert(image_info);
  memset(image_info, 0, sizeof(*image_info));
}

// Creates WebPImageInfo object and sets offsets, dimensions and duration.
// Dimensions calculated from passed VP8 image data.
static WebPImageInfo* CreateImageInfo(uint32_t x_offset, uint32_t y_offset,
                                      uint32_t duration,
                                      const uint8_t* data, uint32_t size) {
  int width;
  int height;
  WebPImageInfo* image_info = NULL;

  if (!VP8GetInfo(data, size, size, &width, &height)) {
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

// Create data for frame/tile given image_info.
static WebPMuxError CreateDataFromImageInfo(const WebPImageInfo* image_info,
                                            int is_frame,
                                            uint8_t** data, uint32_t* size) {
  assert(data);
  assert(size);
  assert(image_info);

  *size = kChunks[is_frame ? FRAME_ID : TILE_ID].chunkSize;
  *data = (uint8_t*)malloc(*size);
  if (*data == NULL) return WEBP_MUX_MEMORY_ERROR;

  // Fill in data according to frame/tile chunk format.
  PutLE32(*data + 0, image_info->x_offset_);
  PutLE32(*data + 4, image_info->y_offset_);

  if (is_frame) {
    PutLE32(*data + 8, image_info->width_);
    PutLE32(*data + 12, image_info->height_);
    PutLE32(*data + 16, image_info->duration_);
  }
  return WEBP_MUX_OK;
}

// Outputs image data given data from a webp file (including RIFF header).
static WebPMuxError GetImageData(const uint8_t* data, uint32_t size,
                                 WebPData* const image, WebPData* const alpha) {
  if (size < TAG_SIZE || memcmp(data, "RIFF", TAG_SIZE)) {
    // It is NOT webp file data. Return input data as is.
    image->bytes_ = data;
    image->size_ = size;
    return WEBP_MUX_OK;
  } else {
    // It is webp file data. Extract image data from it.
    WebPMuxError err;
    WebPMuxState mux_state;
    WebPMux* const mux = WebPMuxCreate(data, size, 0, &mux_state);
    if (mux == NULL || mux_state != WEBP_MUX_STATE_COMPLETE) {
      return WEBP_MUX_BAD_DATA;
    }

    err = WebPMuxGetImage(mux, image, alpha);
    WebPMuxDelete(mux);
    return err;
  }
}

static WebPMuxError DeleteChunks(WebPChunk** chunk_list, uint32_t tag) {
  WebPMuxError err = WEBP_MUX_NOT_FOUND;
  assert(chunk_list);
  while (*chunk_list) {
    WebPChunk* const chunk = *chunk_list;
    if (chunk->tag_ == tag) {
      *chunk_list = ChunkDelete(chunk);
      err = WEBP_MUX_OK;
    } else {
      chunk_list = &chunk->next_;
    }
  }
  return err;
}

static WebPMuxError MuxDeleteAllNamedData(WebPMux* const mux,
                                          const char* const tag) {
  const TAG_ID id = ChunkGetIdFromName(tag);
  WebPChunk** chunk_list;

  if (mux == NULL || tag == NULL) return WEBP_MUX_INVALID_ARGUMENT;
  if (IsWPI(id)) return WEBP_MUX_INVALID_ARGUMENT;

  chunk_list = GetChunkListFromId(mux, id);
  if (chunk_list == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  return DeleteChunks(chunk_list, kChunks[id].chunkTag);
}

static WebPMuxError DeleteLoopCount(WebPMux* const mux) {
  return MuxDeleteAllNamedData(mux, kChunks[LOOP_ID].chunkName);
}

//------------------------------------------------------------------------------
// Set API(s).

WebPMuxError WebPMuxSetImage(WebPMux* const mux,
                             const uint8_t* data, uint32_t size,
                             const uint8_t* alpha_data, uint32_t alpha_size,
                             int copy_data) {
  WebPMuxError err;
  WebPChunk chunk;
  WebPMuxImage wpi;
  WebPData image;
  const int has_alpha = (alpha_data != NULL && alpha_size != 0);

  if (mux == NULL || data == NULL || size > MAX_CHUNK_PAYLOAD) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // If given data is for a whole webp file, extract only the VP8 data from it.
  err = GetImageData(data, size, &image, NULL);
  if (err != WEBP_MUX_OK) return err;

  // Delete the existing images.
  MuxImageDeleteAll(&mux->images_);

  MuxImageInit(&wpi);

  if (has_alpha) {  // Add alpha chunk.
    ChunkInit(&chunk);
    err = ChunkAssignDataImageInfo(&chunk, alpha_data, alpha_size, NULL,
                                   copy_data, kChunks[ALPHA_ID].chunkTag);
    if (err != WEBP_MUX_OK) return err;
    err = ChunkSetNth(&chunk, &wpi.alpha_, 1);
    if (err != WEBP_MUX_OK) return err;
  }

  // Add image chunk.
  ChunkInit(&chunk);
  err = ChunkAssignDataImageInfo(&chunk, image.bytes_, image.size_, NULL,
                                 copy_data, kChunks[IMAGE_ID].chunkTag);
  if (err != WEBP_MUX_OK) return err;
  err = ChunkSetNth(&chunk, &wpi.vp8_, 1);
  if (err != WEBP_MUX_OK) return err;

  // Add this image to mux.
  return MuxImageSetNth(&wpi, &mux->images_, 1);
}

WebPMuxError WebPMuxSetMetadata(WebPMux* const mux,
                                const uint8_t* data, uint32_t size,
                                int copy_data) {
  WebPMuxError err;

  if (mux == NULL || data == NULL || size > MAX_CHUNK_PAYLOAD) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Delete the existing metadata chunk(s).
  err = WebPMuxDeleteMetadata(mux);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Add the given metadata chunk.
  return MuxSet(mux, META_ID, 1, data, size, NULL, copy_data);
}

WebPMuxError WebPMuxSetColorProfile(WebPMux* const mux,
                                    const uint8_t* data, uint32_t size,
                                    int copy_data) {
  WebPMuxError err;

  if (mux == NULL || data == NULL || size > MAX_CHUNK_PAYLOAD) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Delete the existing ICCP chunk(s).
  err = WebPMuxDeleteColorProfile(mux);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Add the given ICCP chunk.
  return MuxSet(mux, ICCP_ID, 1, data, size, NULL, copy_data);
}

WebPMuxError WebPMuxSetLoopCount(WebPMux* const mux, uint32_t loop_count) {
  WebPMuxError err;
  uint8_t* data = NULL;

  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  // Delete the existing LOOP chunk(s).
  err = DeleteLoopCount(mux);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Add the given loop count.
  data = (uint8_t*)malloc(kChunks[LOOP_ID].chunkSize);
  if (data == NULL) return WEBP_MUX_MEMORY_ERROR;

  PutLE32(data, loop_count);
  err = MuxAddChunk(mux, 1, kChunks[LOOP_ID].chunkTag, data,
                    kChunks[LOOP_ID].chunkSize, NULL, 1);
  free(data);
  return err;
}

static WebPMuxError MuxAddFrameTileInternal(
    WebPMux* const mux, uint32_t nth,
    const uint8_t* data, uint32_t size,
    const uint8_t* alpha_data, uint32_t alpha_size,
    uint32_t x_offset, uint32_t y_offset, uint32_t duration,
    int copy_data, uint32_t tag) {
  WebPChunk chunk;
  WebPData image;
  WebPMuxImage wpi;
  WebPMuxError err;
  WebPImageInfo* image_info = NULL;
  uint8_t* frame_tile_data = NULL;
  uint32_t frame_tile_data_size = 0;
  const int is_frame = (tag == kChunks[FRAME_ID].chunkTag) ? 1 : 0;
  const int has_alpha = (alpha_data != NULL && alpha_size != 0);

  if (mux == NULL || data == NULL || size > MAX_CHUNK_PAYLOAD) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // If given data is for a whole webp file, extract only the VP8 data from it.
  err = GetImageData(data, size, &image, NULL);
  if (err != WEBP_MUX_OK) return err;

  ChunkInit(&chunk);
  MuxImageInit(&wpi);

  if (has_alpha) {
    // Add alpha chunk.
    err = ChunkAssignDataImageInfo(&chunk, alpha_data, alpha_size, NULL,
                                   copy_data, kChunks[ALPHA_ID].chunkTag);
    if (err != WEBP_MUX_OK) return err;
    err = ChunkSetNth(&chunk, &wpi.alpha_, 1);
    if (err != WEBP_MUX_OK) return err;
    ChunkInit(&chunk);  // chunk owned by wpi.alpha_ now.
  }

  // Create image_info object.
  image_info = CreateImageInfo(x_offset, y_offset, duration,
                               image.bytes_, image.size_);
  if (image_info == NULL) {
    MuxImageRelease(&wpi);
    return WEBP_MUX_MEMORY_ERROR;
  }

  // Add image chunk.
  err = ChunkAssignDataImageInfo(&chunk, image.bytes_, image.size_, image_info,
                                 copy_data, kChunks[IMAGE_ID].chunkTag);
  if (err != WEBP_MUX_OK) goto Err;
  image_info = NULL;  // Owned by 'chunk' now.
  err = ChunkSetNth(&chunk, &wpi.vp8_, 1);
  if (err != WEBP_MUX_OK) goto Err;
  ChunkInit(&chunk);  // chunk owned by wpi.vp8_ now.

  // Create frame/tile data from image_info.
  err = CreateDataFromImageInfo(wpi.vp8_->image_info_, is_frame,
                                &frame_tile_data, &frame_tile_data_size);
  if (err != WEBP_MUX_OK) goto Err;

  // Add frame/tile chunk (with copy_data = 1).
  err = ChunkAssignDataImageInfo(&chunk, frame_tile_data, frame_tile_data_size,
                                 NULL, 1, tag);
  if (err != WEBP_MUX_OK) goto Err;
  free(frame_tile_data);
  frame_tile_data = NULL;
  err = ChunkSetNth(&chunk, &wpi.header_, 1);
  if (err != WEBP_MUX_OK) goto Err;
  ChunkInit(&chunk);  // chunk owned by wpi.header_ now.

  // Add this WebPMuxImage to mux.
  err = MuxImageSetNth(&wpi, &mux->images_, nth);
  if (err != WEBP_MUX_OK) goto Err;

  // All is well.
  return WEBP_MUX_OK;

 Err:  // Something bad happened.
  free(image_info);
  free(frame_tile_data);
  ChunkRelease(&chunk);
  MuxImageRelease(&wpi);
  return err;
}

// TODO(urvang): Think about whether we need 'nth' while adding a frame or tile.

WebPMuxError WebPMuxAddFrame(WebPMux* const mux, uint32_t nth,
                             const uint8_t* data, uint32_t size,
                             const uint8_t* alpha_data, uint32_t alpha_size,
                             uint32_t x_offset, uint32_t y_offset,
                             uint32_t duration, int copy_data) {
  return MuxAddFrameTileInternal(mux, nth, data, size, alpha_data, alpha_size,
                                 x_offset, y_offset, duration,
                                 copy_data, kChunks[FRAME_ID].chunkTag);
}

WebPMuxError WebPMuxAddTile(WebPMux* const mux, uint32_t nth,
                            const uint8_t* data, uint32_t size,
                            const uint8_t* alpha_data, uint32_t alpha_size,
                            uint32_t x_offset, uint32_t y_offset,
                            int copy_data) {
  return MuxAddFrameTileInternal(mux, nth, data, size, alpha_data, alpha_size,
                                 x_offset, y_offset, 1,
                                 copy_data, kChunks[TILE_ID].chunkTag);
}

//------------------------------------------------------------------------------
// Delete API(s).

WebPMuxError WebPMuxDeleteImage(WebPMux* const mux) {
  WebPMuxError err;

  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = ValidateForImage(mux);
  if (err != WEBP_MUX_OK) return err;

  // All well, delete image.
  MuxImageDeleteAll(&mux->images_);
  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxDeleteMetadata(WebPMux* const mux) {
  return MuxDeleteAllNamedData(mux, kChunks[META_ID].chunkName);
}

WebPMuxError WebPMuxDeleteColorProfile(WebPMux* const mux) {
  return MuxDeleteAllNamedData(mux, kChunks[ICCP_ID].chunkName);
}

static WebPMuxError DeleteFrameTileInternal(WebPMux* const mux,
                                            uint32_t nth,
                                            const char* const tag) {
  const TAG_ID id = ChunkGetIdFromName(tag);
  if (mux == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  assert(id == FRAME_ID || id == TILE_ID);
  return MuxImageDeleteNth(&mux->images_, nth, id);
}

WebPMuxError WebPMuxDeleteFrame(WebPMux* const mux, uint32_t nth) {
  return DeleteFrameTileInternal(mux, nth, kChunks[FRAME_ID].chunkName);
}

WebPMuxError WebPMuxDeleteTile(WebPMux* const mux, uint32_t nth) {
  return DeleteFrameTileInternal(mux, nth, kChunks[TILE_ID].chunkName);
}

//------------------------------------------------------------------------------
// Assembly of the WebP RIFF file.

static WebPMuxError GetImageCanvasHeightWidth(
    const WebPMux* const mux,
    uint32_t flags, uint32_t* width, uint32_t* height) {
  WebPMuxImage* wpi = NULL;
  assert(mux != NULL);
  assert(width && height);

  wpi = mux->images_;
  assert(wpi != NULL);
  assert(wpi->vp8_ != NULL);

  if (wpi->next_) {
    uint32_t max_x = 0;
    uint32_t max_y = 0;
    uint64_t image_area = 0;
    // Aggregate the bounding box for animation frames & tiled images.
    for (; wpi != NULL; wpi = wpi->next_) {
      const WebPImageInfo* image_info = wpi->vp8_->image_info_;

      if (image_info != NULL) {
        const uint32_t max_x_pos = image_info->x_offset_ + image_info->width_;
        const uint32_t max_y_pos = image_info->y_offset_ + image_info->height_;
        if (max_x_pos < image_info->x_offset_) {  // Overflow occurred.
          return WEBP_MUX_INVALID_ARGUMENT;
        }
        if (max_y_pos < image_info->y_offset_) {  // Overflow occurred.
          return WEBP_MUX_INVALID_ARGUMENT;
        }
        if (max_x_pos > max_x) max_x = max_x_pos;
        if (max_y_pos > max_y) max_y = max_y_pos;
        image_area += (image_info->width_ * image_info->height_);
      }
    }
    *width = max_x;
    *height = max_y;
    // Crude check to validate that there are no image overlaps/holes for tile
    // images. Check that the aggregated image area for individual tiles exactly
    // matches the image area of the constructed canvas. However, the area-match
    // is necessary but not sufficient condition.
    if ((flags & TILE_FLAG) && (image_area != (max_x * max_y))) {
      *width = 0;
      *height = 0;
      return WEBP_MUX_INVALID_ARGUMENT;
    }
  } else {
    // For a single image, extract the width & height from VP8 image-data.
    int w, h;
    const WebPChunk* const image_chunk = wpi->vp8_;
    assert(image_chunk != NULL);
    if (VP8GetInfo(image_chunk->data_, image_chunk->payload_size_,
                   image_chunk->payload_size_, &w, &h)) {
      *width = w;
      *height = h;
    }
  }
  return WEBP_MUX_OK;
}

// VP8X format:
// Total Size : 12,
// Flags  : 4 bytes,
// Width  : 4 bytes,
// Height : 4 bytes.
static WebPMuxError CreateVP8XChunk(WebPMux* const mux) {
  WebPMuxError err = WEBP_MUX_OK;
  uint32_t flags = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t data[VP8X_CHUNK_SIZE];
  const uint32_t data_size = VP8X_CHUNK_SIZE;
  const WebPMuxImage* images = NULL;

  assert(mux != NULL);
  images = mux->images_;  // First image.
  if (images == NULL || images->vp8_ == NULL || images->vp8_->data_ == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // If VP8X chunk(s) is(are) already present, remove them (and later add new
  // VP8X chunk with updated flags).
  err = MuxDeleteAllNamedData(mux, kChunks[VP8X_ID].chunkName);
  if (err != WEBP_MUX_OK && err != WEBP_MUX_NOT_FOUND) return err;

  // Set flags.
  if (mux->iccp_ != NULL && mux->iccp_->data_ != NULL) {
    flags |= ICCP_FLAG;
  }

  if (mux->meta_ != NULL && mux->meta_->data_ != NULL) {
    flags |= META_FLAG;
  }

  if (images->header_ != NULL) {
    if (images->header_->tag_ == kChunks[TILE_ID].chunkTag) {
      // This is a tiled image.
      flags |= TILE_FLAG;
    } else if (images->header_->tag_ == kChunks[FRAME_ID].chunkTag) {
      // This is an image with animation.
      flags |= ANIMATION_FLAG;
    }
  }

  if (images->alpha_ != NULL && images->alpha_->data_ != NULL) {
    // This is an image with alpha channel.
    flags |= ALPHA_FLAG;
  }

  if (flags == 0) {
    // For Simple Image, VP8X chunk should not be added.
    return WEBP_MUX_OK;
  }

  err = GetImageCanvasHeightWidth(mux, flags, &width, &height);
  if (err != WEBP_MUX_OK) return err;

  PutLE32(data + 0, flags);   // VP8X chunk flags.
  PutLE32(data + 4, width);   // canvas width.
  PutLE32(data + 8, height);  // canvas height.

  err = MuxAddChunk(mux, 1, kChunks[VP8X_ID].chunkTag, data, data_size,
                    NULL, 1);
  return err;
}

WebPMuxError WebPMuxAssemble(WebPMux* const mux,
                             uint8_t** output_data, uint32_t* output_size) {
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
      err = DeleteLoopCount(mux);
      if (err != WEBP_MUX_OK) return err;
    }
  }

  // Create VP8X chunk.
  err = CreateVP8XChunk(mux);
  if (err != WEBP_MUX_OK) return err;

  // Mark mux as complete.
  mux->state_ = WEBP_MUX_STATE_COMPLETE;

  // Allocate data.
  size = ChunksListDiskSize(mux->vp8x_) + ChunksListDiskSize(mux->iccp_)
       + ChunksListDiskSize(mux->loop_) + MuxImageListDiskSize(mux->images_)
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
  dst = ChunkListEmit(mux->vp8x_, dst);
  dst = ChunkListEmit(mux->iccp_, dst);
  dst = ChunkListEmit(mux->loop_, dst);
  dst = MuxImageListEmit(mux->images_, dst);
  dst = ChunkListEmit(mux->meta_, dst);
  dst = ChunkListEmit(mux->unknown_, dst);
  assert(dst == data + size);

  // Validate mux.
  err = WebPMuxValidate(mux);
  if (err != WEBP_MUX_OK) {
    free(data);
    data = NULL;
    size = 0;
  }

  // Finalize.
  *output_data = data;
  *output_size = size;

  return err;
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
