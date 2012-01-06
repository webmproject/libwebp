// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Read APIs for mux.
//
// Authors: Urvang (urvang@google.com)
//          Vikas (vikasa@google.com)

#include <assert.h>
#include "./muxi.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Helper method(s).

// Handy MACRO.
#define SWITCH_ID_LIST(ID, LIST)                                              \
  if (id == (ID)) {                                                           \
    const WebPChunk* const chunk = ChunkSearchList((LIST), nth,               \
                                                   kChunks[(ID)].chunkTag);   \
    if (chunk) {                                                              \
      *data = chunk->data_;                                                   \
      *data_size = chunk->payload_size_;                                      \
      return WEBP_MUX_OK;                                                     \
    } else {                                                                  \
      return WEBP_MUX_NOT_FOUND;                                              \
    }                                                                         \
  }

static WebPMuxError MuxGet(const WebPMux* const mux, TAG_ID id, uint32_t nth,
                           const uint8_t** data, uint32_t* data_size) {
  assert(mux != NULL);
  *data = NULL;
  *data_size = 0;
  assert(!IsWPI(id));

  SWITCH_ID_LIST(VP8X_ID, mux->vp8x_);
  SWITCH_ID_LIST(ICCP_ID, mux->iccp_);
  SWITCH_ID_LIST(LOOP_ID, mux->loop_);
  SWITCH_ID_LIST(META_ID, mux->meta_);
  SWITCH_ID_LIST(UNKNOWN_ID, mux->unknown_);
  return WEBP_MUX_NOT_FOUND;
}
#undef SWITCH_ID_LIST

// Fill the chunk with the given data, after verifying that the data size
// doesn't exceed 'max_size'.
static WebPMuxError ChunkAssignData(WebPChunk* chunk, const uint8_t* data,
                                    uint32_t max_size, int copy_data) {
  uint32_t size = 0;
  assert(max_size >= CHUNK_HEADER_SIZE);

  size = GetLE32(data + 4);
  assert(size <= MAX_CHUNK_PAYLOAD);
  if (size + CHUNK_HEADER_SIZE > max_size) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return ChunkAssignDataImageInfo(chunk, data + CHUNK_HEADER_SIZE, size, NULL,
                                  copy_data, GetLE32(data + 0));
}

//------------------------------------------------------------------------------
// Create a mux object from WebP-RIFF data.

WebPMux* WebPMuxCreate(const uint8_t* data, uint32_t size, int copy_data) {
  uint32_t mux_size;
  uint32_t tag;
  const uint8_t* end;
  TAG_ID id;
  WebPMux* mux;
  WebPMuxImage wpi;

  // Sanity checks on size and leading bytes.
  if (data == NULL) return NULL;
  if (size < RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE) {
    return NULL;
  }
  if (GetLE32(data + 0) != mktag('R', 'I', 'F', 'F') ||
      GetLE32(data + 8) != mktag('W', 'E', 'B', 'P')) {
    return NULL;
  }
  mux_size = CHUNK_HEADER_SIZE + GetLE32(data + TAG_SIZE);
  if (mux_size > size) {
    return NULL;
  }
  tag = GetLE32(data + RIFF_HEADER_SIZE);
  if (tag != kChunks[IMAGE_ID].chunkTag && tag != kChunks[VP8X_ID].chunkTag) {
    // First chunk should be either VP8X or VP8.
    return NULL;
  }
  end = data + mux_size;
  data += RIFF_HEADER_SIZE;
  mux_size -= RIFF_HEADER_SIZE;

  mux = WebPMuxNew();
  if (mux == NULL) return NULL;

  MuxImageInit(&wpi);

  // Loop over chunks.
  while (data != end) {
    WebPChunk chunk;
    uint32_t data_size;

    ChunkInit(&chunk);
    if (ChunkAssignData(&chunk, data, mux_size, copy_data) != WEBP_MUX_OK) {
      goto Err;
    }

    data_size = ChunkDiskSize(&chunk);
    id = ChunkGetIdFromTag(chunk.tag_);

    if (IsWPI(id)) {  // An image chunk (frame/tile/alpha/vp8).
      WebPChunk** wpi_chunk_ptr;
      wpi_chunk_ptr = MuxImageGetListFromId(&wpi, id);  // Image chunk to set.
      assert(wpi_chunk_ptr != NULL);
      if (*wpi_chunk_ptr != NULL) goto Err;  // Consecutive alpha chunks or
                                             // consecutive frame/tile chunks.
      if (ChunkSetNth(&chunk, wpi_chunk_ptr, 1) != WEBP_MUX_OK) goto Err;
      if (id == IMAGE_ID) {
        wpi.is_partial_ = 0;  // wpi is completely filled.
        // Add this to mux->images_ list.
        if (MuxImageSetNth(&wpi, &mux->images_, 0) != WEBP_MUX_OK) goto Err;
        MuxImageInit(&wpi);  // Reset for reading next image.
      } else {
        wpi.is_partial_ = 1;  // wpi is only partially filled.
      }
    } else {  // A non-image chunk.
      WebPChunk** chunk_list;
      if (wpi.is_partial_) goto Err;  // Encountered a non-image chunk before
                                      // getting all chunks of an image.
      chunk_list = GetChunkListFromId(mux, id);  // List for adding this chunk.
      if (chunk_list == NULL) chunk_list = (WebPChunk**)&mux->unknown_;
      if (ChunkSetNth(&chunk, chunk_list, 0) != WEBP_MUX_OK) goto Err;
    }

    data += data_size;
    mux_size -= data_size;
  }

  // Validate mux.
  if (WebPMuxValidate(mux) != WEBP_MUX_OK) goto Err;

  return mux;  // All OK;

 Err:  // Something bad happened.
   WebPMuxDelete(mux);
   return NULL;
}

//------------------------------------------------------------------------------
// Get API(s).

WebPMuxError WebPMuxGetFeatures(const WebPMux* const mux, uint32_t* flags) {
  const uint8_t* data;
  uint32_t data_size;
  WebPMuxError err;

  if (mux == NULL || flags == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = MuxGet(mux, VP8X_ID, 1, &data, &data_size);
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

WebPMuxError WebPMuxGetImage(const WebPMux* const mux,
                             const uint8_t** data, uint32_t* size,
                             const uint8_t** alpha_data, uint32_t* alpha_size) {
  WebPMuxError err;
  WebPMuxImage* wpi = NULL;

  if (mux == NULL || data == NULL || size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  *data = NULL;
  *size = 0;

  err = ValidateForImage(mux);
  if (err != WEBP_MUX_OK) return err;

  // All well. Get the image.
  err = MuxImageGetNth((const WebPMuxImage**)&mux->images_, 1, IMAGE_ID, &wpi);
  assert(err == WEBP_MUX_OK);  // Already tested above.

  // Get alpha chunk (if present & requested).
  if (alpha_data != NULL && alpha_size != NULL) {
      *alpha_data = NULL;
      *alpha_size = 0;
      if (wpi->alpha_ != NULL) {
        *alpha_data = wpi->alpha_->data_;
        *alpha_size = wpi->alpha_->payload_size_;
      }
  }

  // Get image chunk.
  if (wpi->vp8_ != NULL) {
    *data = wpi->vp8_->data_;
    *size = wpi->vp8_->payload_size_;
  }
  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxGetMetadata(const WebPMux* const mux, const uint8_t** data,
                                uint32_t* size) {
  if (mux == NULL || data == NULL || size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return MuxGet(mux, META_ID, 1, data, size);
}

WebPMuxError WebPMuxGetColorProfile(const WebPMux* const mux,
                                    const uint8_t** data, uint32_t* size) {
  if (mux == NULL || data == NULL || size == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  return MuxGet(mux, ICCP_ID, 1, data, size);
}

WebPMuxError WebPMuxGetLoopCount(const WebPMux* const mux,
                                 uint32_t* loop_count) {
  const uint8_t* data;
  uint32_t data_size;
  WebPMuxError err;

  if (mux == NULL || loop_count == NULL) return WEBP_MUX_INVALID_ARGUMENT;

  err = MuxGet(mux, LOOP_ID, 1, &data, &data_size);
  if (err != WEBP_MUX_OK) return err;
  if (data_size < kChunks[LOOP_ID].chunkSize) return WEBP_MUX_BAD_DATA;
  *loop_count = GetLE32(data);

  return WEBP_MUX_OK;
}

static WebPMuxError MuxGetFrameTileInternal(const WebPMux* const mux,
                                            uint32_t nth,
                                            const uint8_t** data,
                                            uint32_t* size,
                                            const uint8_t** alpha_data,
                                            uint32_t* alpha_size,
                                            uint32_t* x_offset,
                                            uint32_t* y_offset,
                                            uint32_t* duration, uint32_t tag) {
  const uint8_t* frame_tile_data;
  uint32_t frame_tile_size;
  WebPMuxError err;
  WebPMuxImage* wpi;

  const int is_frame = (tag == kChunks[FRAME_ID].chunkTag) ? 1 : 0;
  const TAG_ID id = is_frame ? FRAME_ID : TILE_ID;

  if (mux == NULL || data == NULL || size == NULL ||
      x_offset == NULL || y_offset == NULL || (is_frame && duration == NULL)) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  // Get the nth WebPMuxImage.
  err = MuxImageGetNth((const WebPMuxImage**)&mux->images_, nth, id, &wpi);
  if (err != WEBP_MUX_OK) return err;

  // Get frame chunk.
  assert(wpi->header_ != NULL);  // As GetNthImage() already checked header_.
  frame_tile_data = wpi->header_->data_;
  frame_tile_size = wpi->header_->payload_size_;

  if (frame_tile_size < kChunks[id].chunkSize) return WEBP_MUX_BAD_DATA;
  *x_offset = GetLE32(frame_tile_data);
  *y_offset = GetLE32(frame_tile_data + 4);
  if (is_frame) *duration = GetLE32(frame_tile_data + 16);

  // Get alpha chunk (if present & requested).
  if (alpha_data != NULL && alpha_size != NULL) {
    *alpha_data = NULL;
    *alpha_size = 0;
    if (wpi->alpha_ != NULL) {
      *alpha_data = wpi->alpha_->data_;
      *alpha_size = wpi->alpha_->payload_size_;
    }
  }

  // Get image chunk.
  *data = NULL;
  *size = 0;
  if (wpi->vp8_ != NULL) {
    *data = wpi->vp8_->data_;
    *size = wpi->vp8_->payload_size_;
  }

  return WEBP_MUX_OK;
}

WebPMuxError WebPMuxGetFrame(const WebPMux* const mux, uint32_t nth,
                             const uint8_t** data, uint32_t* size,
                             const uint8_t** alpha_data, uint32_t* alpha_size,
                             uint32_t* x_offset, uint32_t* y_offset,
                             uint32_t* duration) {
  return MuxGetFrameTileInternal(mux, nth, data, size, alpha_data, alpha_size,
                                 x_offset, y_offset, duration,
                                 kChunks[FRAME_ID].chunkTag);
}

WebPMuxError WebPMuxGetTile(const WebPMux* const mux, uint32_t nth,
                            const uint8_t** data, uint32_t* size,
                            const uint8_t** alpha_data, uint32_t* alpha_size,
                            uint32_t* x_offset, uint32_t* y_offset) {
  return MuxGetFrameTileInternal(mux, nth, data, size, alpha_data, alpha_size,
                                 x_offset, y_offset, NULL,
                                 kChunks[TILE_ID].chunkTag);
}

// Count number of chunks matching 'tag' in the 'chunk_list'.
// If tag == NIL_TAG, any tag will be matched.
static int CountChunks(WebPChunk* const chunk_list, uint32_t tag) {
  int count = 0;
  WebPChunk* current;
  for (current = chunk_list; current != NULL; current = current->next_) {
    if ((tag == NIL_TAG) || (current->tag_ == tag)) {
      count++;  // Count chunks whose tags match.
    }
  }
  return count;
}

WebPMuxError WebPMuxNumNamedElements(const WebPMux* const mux, const char* tag,
                                     int* num_elements) {
  TAG_ID id;
  WebPChunk** chunk_list;

  if (mux == NULL || tag == NULL || num_elements == NULL) {
    return WEBP_MUX_INVALID_ARGUMENT;
  }

  id = ChunkGetIdFromName(tag);
  if (IsWPI(id)) {
    *num_elements = MuxImageCount(mux->images_, id);
  } else {
    chunk_list = GetChunkListFromId(mux, id);
    if (chunk_list == NULL) {
      *num_elements = 0;
    } else {
      *num_elements = CountChunks(*chunk_list, kChunks[id].chunkTag);
    }
  }

  return WEBP_MUX_OK;
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
