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

#include <stdlib.h>
#include <string.h>

#include "../webp/decode.h"  // WebPGetInfo
#include "../webp/format_constants.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define MKFOURCC(a, b, c, d) ((uint32_t)(a) | (b) << 8 | (c) << 16 | (d) << 24)

typedef struct {
  size_t start_;        // start location of the data
  size_t end_;          // end location
  size_t riff_end_;     // riff chunk end location, can be > end_.
  size_t buf_size_;     // size of the buffer
  const uint8_t* buf_;
} MemBuffer;

typedef struct {
  size_t offset_;
  size_t size_;
} ChunkData;

typedef struct Frame {
  int width_, height_;
  int frame_num_;  // the referent frame number for use in assembling tiles.
  int complete_;   // img_components_ contains a full image.
  ChunkData img_components_[2];  // 0=VP8{,L} 1=ALPH
  struct Frame* next_;
} Frame;

struct WebPDemuxer {
  MemBuffer mem_;
  WebPDemuxState state_;
  int canvas_width_, canvas_height_;
  int num_frames_;
  Frame* frames_;
};

typedef enum {
  PARSE_OK,
  PARSE_NEED_MORE_DATA,
  PARSE_ERROR
} ParseStatus;

typedef struct ChunkParser {
  uint8_t id[4];
  ParseStatus (*parse)(WebPDemuxer* const dmux);
  int (*valid)(const WebPDemuxer* const dmux);
} ChunkParser;

static int IsValidSimpleFormat(const WebPDemuxer* const dmux);
static ParseStatus ParseSingleImage(WebPDemuxer* const dmux);

static const ChunkParser kMasterChunks[] = {
  { { 'V', 'P', '8', ' ' }, ParseSingleImage, IsValidSimpleFormat },
  { { 'V', 'P', '8', 'L' }, ParseSingleImage, IsValidSimpleFormat },
  { { '0', '0', '0', '0' }, NULL, NULL },
};

// -----------------------------------------------------------------------------
// MemBuffer

static int RemapMemBuffer(MemBuffer* const mem,
                          const uint8_t* data, size_t size) {
  if (size < mem->buf_size_) return 0;  // can't remap to a shorter buffer!

  mem->buf_ = data;
  mem->end_ = mem->buf_size_ = size;
  return 1;
}

static int InitMemBuffer(MemBuffer* const mem,
                         const uint8_t* data, size_t size) {
  memset(mem, 0, sizeof(*mem));
  return RemapMemBuffer(mem, data, size);
}

// Return the remaining data size available in 'mem'.
static WEBP_INLINE size_t MemDataSize(const MemBuffer* const mem) {
  return (mem->end_ - mem->start_);
}

// Return true if 'size' exceeds the end of the RIFF chunk.
static WEBP_INLINE int SizeIsInvalid(const MemBuffer* const mem, size_t size) {
  return (size > mem->riff_end_ - mem->start_);
}

static WEBP_INLINE void Skip(MemBuffer* const mem, size_t size) {
  mem->start_ += size;
}

static WEBP_INLINE void Rewind(MemBuffer* const mem, size_t size) {
  mem->start_ -= size;
}

static WEBP_INLINE const uint8_t* GetBuffer(MemBuffer* const mem) {
  return mem->buf_ + mem->start_;
}

static WEBP_INLINE uint32_t ReadLE32(const uint8_t* const data) {
  return (uint32_t)(data[0] << 0   | (data[1] << 8) |
                   (data[2] << 16) | (data[3] << 24));
}

static WEBP_INLINE uint32_t GetLE32(MemBuffer* const mem) {
  const uint8_t* const data = mem->buf_ + mem->start_;
  const uint32_t val = ReadLE32(data);
  Skip(mem, 4);
  return val;
}

// -----------------------------------------------------------------------------
// Secondary chunk parsing

// Add a frame to the end of the list, ensuring the last frame is complete.
// Returns true on success, false otherwise.
static int AddFrame(WebPDemuxer* const dmux, Frame* const frame) {
  const Frame* last_frame = NULL;
  Frame** f = &dmux->frames_;
  while (*f != NULL) {
    last_frame = *f;
    f = &(*f)->next_;
  }
  if (last_frame != NULL && !last_frame->complete_) return 0;
  *f = frame;
  frame->next_ = NULL;
  return 1;
}

// Store image bearing chunks to 'frame'.
static ParseStatus StoreFrame(int frame_num, MemBuffer* const mem,
                              Frame* const frame) {
  int image_chunks = 0;
  int done = (MemDataSize(mem) < CHUNK_HEADER_SIZE);
  ParseStatus status = PARSE_OK;

  if (done) return PARSE_NEED_MORE_DATA;

  do {
    const size_t chunk_start_offset = mem->start_;
    const uint32_t fourcc = GetLE32(mem);
    const uint32_t payload_size = GetLE32(mem);
    const uint32_t payload_size_padded = payload_size + (payload_size & 1);
    const size_t payload_available = (payload_size_padded > MemDataSize(mem))
                                   ? MemDataSize(mem) : payload_size_padded;
    const size_t chunk_size = CHUNK_HEADER_SIZE + payload_available;

    if (payload_size > MAX_CHUNK_PAYLOAD) return PARSE_ERROR;
    if (SizeIsInvalid(mem, payload_size_padded)) return PARSE_ERROR;
    if (payload_size_padded > MemDataSize(mem)) status = PARSE_NEED_MORE_DATA;

    switch (fourcc) {
      case MKFOURCC('V', 'P', '8', ' '):
      case MKFOURCC('V', 'P', '8', 'L'):
        if (image_chunks == 0) {
          int width = 0, height = 0;
          ++image_chunks;
          frame->img_components_[0].offset_ = chunk_start_offset;
          frame->img_components_[0].size_ = chunk_size;
          // Extract the width and height from the bitstream, tolerating
          // failures when the data is incomplete.
          if (!WebPGetInfo(mem->buf_ + frame->img_components_[0].offset_,
                           frame->img_components_[0].size_, &width, &height) &&
              status != PARSE_NEED_MORE_DATA) {
            return PARSE_ERROR;
          }

          frame->width_ = width;
          frame->height_ = height;
          frame->frame_num_ = frame_num;
          frame->complete_ = (status == PARSE_OK);
          Skip(mem, payload_available);
        } else {
          goto Done;
        }
        break;
 Done:
      default:
        // Restore fourcc/size when moving up one level in parsing.
        Rewind(mem, CHUNK_HEADER_SIZE);
        done = 1;
        break;
    }

    if (mem->start_ == mem->riff_end_) {
      done = 1;
    } else if (MemDataSize(mem) < CHUNK_HEADER_SIZE) {
      status = PARSE_NEED_MORE_DATA;
    }
  } while (!done && status == PARSE_OK);

  return status;
}

// -----------------------------------------------------------------------------
// Primary chunk parsing

static int ReadHeader(MemBuffer* const mem) {
  const size_t min_size = RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE;
  uint32_t riff_size;

  // Basic file level validation.
  if (MemDataSize(mem) < min_size) return 0;
  if (memcmp(GetBuffer(mem), "RIFF", CHUNK_SIZE_BYTES) ||
      memcmp(GetBuffer(mem) + CHUNK_HEADER_SIZE, "WEBP", CHUNK_SIZE_BYTES)) {
    return 0;
  }

  riff_size = ReadLE32(GetBuffer(mem) + TAG_SIZE);
  if (riff_size < CHUNK_HEADER_SIZE) return 0;
  if (riff_size > MAX_CHUNK_PAYLOAD) return 0;

  // There's no point in reading past the end of the RIFF chunk
  mem->riff_end_ = riff_size + CHUNK_HEADER_SIZE;
  if (mem->buf_size_ > mem->riff_end_) {
    mem->buf_size_ = mem->end_ = mem->riff_end_;
  }

  Skip(mem, RIFF_HEADER_SIZE);
  return 1;
}

static int IsValidSimpleFormat(const WebPDemuxer* const dmux) {
  const Frame* const frame = dmux->frames_;
  if (dmux->state_ == WEBP_DEMUX_PARSING_HEADER) return 1;

  if (dmux->canvas_width_ <= 0 || dmux->canvas_height_ <= 0) return 0;
  if (dmux->state_ == WEBP_DEMUX_DONE && frame == NULL) return 0;

  if (frame->width_ <= 0 || frame->height_ <= 0) return 0;
  return 1;
}

static ParseStatus ParseSingleImage(WebPDemuxer* const dmux) {
  const size_t min_size = CHUNK_HEADER_SIZE;
  MemBuffer* const mem = &dmux->mem_;
  Frame* frame;
  ParseStatus status;

  if (dmux->frames_ != NULL) return PARSE_ERROR;
  if (MemDataSize(mem) < min_size) return PARSE_NEED_MORE_DATA;

  frame = (Frame*)calloc(1, sizeof(*frame));
  if (frame == NULL) return PARSE_ERROR;

  status = StoreFrame(1, &dmux->mem_, frame);
  if (status != PARSE_ERROR) {
    // Use the frame width/height as the canvas values for non-vp8x files.
    if (frame->width_ > 0 && frame->height_ > 0) {
      dmux->state_ = WEBP_DEMUX_PARSED_HEADER;
      dmux->canvas_width_ = frame->width_;
      dmux->canvas_height_ = frame->height_;
    }
    AddFrame(dmux, frame);
    dmux->num_frames_ = 1;
  } else {
    free(frame);
  }

  return status;
}

// -----------------------------------------------------------------------------
// WebPDemuxer object

static void InitDemux(WebPDemuxer* const dmux, const MemBuffer* const mem) {
  dmux->state_ = WEBP_DEMUX_PARSING_HEADER;
  dmux->canvas_width_ = -1;
  dmux->canvas_height_ = -1;
  dmux->mem_ = *mem;
}

WebPDemuxer* WebPDemuxInternal(
    const WebPData* const data, int allow_partial,
    WebPDemuxState* const state, int version) {
  const ChunkParser* parser;
  int partial;
  ParseStatus status = PARSE_ERROR;
  MemBuffer mem;
  WebPDemuxer* dmux;

  if (version != WEBP_DEMUX_ABI_VERSION) return NULL;
  if (data == NULL || data->bytes_ == NULL || data->size_ == 0) return NULL;

  if (!InitMemBuffer(&mem, data->bytes_, data->size_)) return NULL;
  if (!ReadHeader(&mem)) return NULL;

  partial = (mem.buf_size_ < mem.riff_end_);
  if (!allow_partial && partial) return NULL;

  dmux = (WebPDemuxer*)calloc(1, sizeof(*dmux));
  if (dmux == NULL) return NULL;
  InitDemux(dmux, &mem);

  for (parser = kMasterChunks; parser->parse != NULL; ++parser) {
    if (!memcmp(parser->id, GetBuffer(&dmux->mem_), TAG_SIZE)) {
      status = parser->parse(dmux);
      if (status == PARSE_OK) dmux->state_ = WEBP_DEMUX_DONE;
      if (status != PARSE_ERROR && !parser->valid(dmux)) status = PARSE_ERROR;
      break;
    }
  }
  if (state) *state = dmux->state_;

  if (status == PARSE_ERROR) {
    WebPDemuxDelete(dmux);
    return NULL;
  }
  return dmux;
}

void WebPDemuxDelete(WebPDemuxer* const dmux) {
  Frame* f;
  if (dmux == NULL) return;

  for (f = dmux->frames_; f != NULL;) {
    Frame* const cur_frame = f;
    f = f->next_;
    free(cur_frame);
  }
  free(dmux);
}

#if defined(__cplusplus) || defined(c_plusplus)
}  // extern "C"
#endif
