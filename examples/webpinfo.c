// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Command-line tool to print out the chunk level structure of WebP files
//  along with basic integrity checks.
//
//  Author: Hui Su (huisu@google.com)

#include <assert.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

#include "../imageio/imageio_util.h"
#include "webp/decode.h"
#include "webp/format_constants.h"
#include "webp/mux_types.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

static const char* const kFormats[3] = {
  "Unknown",
  "Lossy",
  "Lossless"
};

typedef enum {
  WEBP_INFO_OK = 0,
  WEBP_INFO_TRUNCATED_DATA,
  WEBP_INFO_PARSE_ERROR,
  WEBP_INFO_INVALID_PARAM,
  WEBP_INFO_BITSTREAM_ERROR,
  WEBP_INFO_MISSING_DATA,
  WEBP_INFO_INVALID_COMMAND
} WebPInfoStatus;

typedef enum ChunkID {
  CHUNK_VP8,
  CHUNK_VP8L,
  CHUNK_VP8X,
  CHUNK_ALPHA,
  CHUNK_ANIM,
  CHUNK_ANMF,
  CHUNK_ICCP,
  CHUNK_EXIF,
  CHUNK_XMP,
  CHUNK_UNKNOWN,
  CHUNK_TYPES = CHUNK_UNKNOWN
} ChunkID;

typedef struct {
  size_t start_;
  size_t end_;
  const uint8_t* buf_;
} MemBuffer;

typedef struct {
  size_t offset_;
  size_t size_;
  const uint8_t* payload_;
  ChunkID id_;
} ChunkData;

typedef struct WebPInfo {
  int canvas_width_;
  int canvas_height_;
  int loop_count_;
  int num_frames_;
  int chunk_counts_[CHUNK_TYPES];
  int anmf_subchunk_counts_[3];  // 0 VP8; 1 VP8L; 2 ALPH.
  uint32_t bgcolor_;
  int feature_flags_;
  int has_alpha_;
  // Used for parsing ANMF chunks.
  int frame_width_, frame_height_;
  size_t anim_frame_data_size_;
  int is_processing_anim_frame_, seen_alpha_subchunk_, seen_image_subchunk_;
  // Print output control.
  int quiet_, show_diagnosis_, show_summary_;
} WebPInfo;

static void WebPInfoInit(WebPInfo* const webp_info) {
  memset(webp_info, 0, sizeof(*webp_info));
}

static const char kWebPChunkTags[CHUNK_TYPES][4] = {
  { 'V', 'P', '8', ' ' },
  { 'V', 'P', '8', 'L' },
  { 'V', 'P', '8', 'X' },
  { 'A', 'L', 'P', 'H' },
  { 'A', 'N', 'I', 'M' },
  { 'A', 'N', 'M', 'F' },
  { 'I', 'C', 'C', 'P' },
  { 'E', 'X', 'I', 'F' },
  { 'X', 'M', 'P', ' ' },
};

// -----------------------------------------------------------------------------
// Data reading.

static int GetLE16(const uint8_t* const data) {
  return (data[0] << 0) | (data[1] << 8);
}

static int GetLE24(const uint8_t* const data) {
  return GetLE16(data) | (data[2] << 16);
}

static uint32_t GetLE32(const uint8_t* const data) {
  return GetLE16(data) | ((uint32_t)GetLE16(data + 2) << 16);
}

static int ReadLE16(const uint8_t** data) {
  const int val = GetLE16(*data);
  *data += 2;
  return val;
}

static int ReadLE24(const uint8_t** data) {
  const int val = GetLE24(*data);
  *data += 3;
  return val;
}

static uint32_t ReadLE32(const uint8_t** data) {
  const uint32_t val = GetLE32(*data);
  *data += 4;
  return val;
}

static int ReadFileToWebPData(const char* const filename,
                              WebPData* const webp_data) {
  const uint8_t* data;
  size_t size;
  if (!ImgIoUtilReadFile(filename, &data, &size)) return 0;
  webp_data->bytes = data;
  webp_data->size = size;
  return 1;
}

// -----------------------------------------------------------------------------
// MemBuffer object.

static void InitMemBuffer(MemBuffer* const mem, const WebPData* webp_data) {
  mem->buf_ = webp_data->bytes;
  mem->start_ = 0;
  mem->end_ = webp_data->size;
}

static size_t MemDataSize(const MemBuffer* const mem) {
  return (mem->end_ - mem->start_);
}

static const uint8_t* GetBuffer(MemBuffer* const mem) {
  return mem->buf_ + mem->start_;
}

static void Skip(MemBuffer* const mem, size_t size) {
  mem->start_ += size;
}

static uint32_t ReadMemBufLE32(MemBuffer* const mem) {
  const uint8_t* const data = mem->buf_ + mem->start_;
  const uint32_t val = GetLE32(data);
  assert(MemDataSize(mem) >= 4);
  Skip(mem, 4);
  return val;
}

// -----------------------------------------------------------------------------
// Chunk parsing.

#define LOG_ERROR(MESSAGE)                     \
  if (webp_info->show_diagnosis_) {            \
    fprintf(stderr, "Error: %s\n", MESSAGE);   \
  }

#define LOG_WARN(MESSAGE)                      \
  if (webp_info->show_diagnosis_) {            \
    fprintf(stderr, "Warning: %s\n", MESSAGE); \
  }

static WebPInfoStatus ParseRIFFHeader(const WebPInfo* const webp_info,
                                      MemBuffer* const mem) {
  const size_t min_size = RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE;
  size_t riff_size;

  if (MemDataSize(mem) < min_size) {
    LOG_ERROR("Truncated data detected when parsing RIFF header.");
    return WEBP_INFO_TRUNCATED_DATA;
  }
  if (memcmp(GetBuffer(mem), "RIFF", CHUNK_SIZE_BYTES) ||
      memcmp(GetBuffer(mem) + CHUNK_HEADER_SIZE, "WEBP", CHUNK_SIZE_BYTES)) {
    LOG_ERROR("Corrupted RIFF header.");
    return WEBP_INFO_PARSE_ERROR;
  }
  riff_size = GetLE32(GetBuffer(mem) + TAG_SIZE);
  if (riff_size < CHUNK_HEADER_SIZE) {
    LOG_ERROR("RIFF size is too small.");
    return WEBP_INFO_PARSE_ERROR;
  }
  if (riff_size > MAX_CHUNK_PAYLOAD) {
    LOG_ERROR("RIFF size is over limit.");
    return WEBP_INFO_PARSE_ERROR;
  }
  riff_size += CHUNK_HEADER_SIZE;
  if (!webp_info->quiet_) {
    printf("RIFF HEADER:\n");
    printf("  RIFF file size: %6d\n", (int)riff_size);
  }
  if (riff_size < mem->end_) {
    LOG_WARN("RIFF size is smaller than the file size.");
    mem->end_ = riff_size;
  } else if (riff_size > mem->end_) {
    LOG_ERROR("Truncated data detected when parsing RIFF payload.");
    return WEBP_INFO_TRUNCATED_DATA;
  }
  Skip(mem, RIFF_HEADER_SIZE);
  return WEBP_INFO_OK;
}

static WebPInfoStatus ParseChunk(const WebPInfo* const webp_info,
                                 MemBuffer* const mem,
                                 ChunkData* const chunk_data) {
  memset(chunk_data, 0, sizeof(*chunk_data));
  if (MemDataSize(mem) < CHUNK_HEADER_SIZE) {
    LOG_ERROR("Truncated data detected when parsing chunk header.");
    return WEBP_INFO_TRUNCATED_DATA;
  } else {
    const size_t chunk_start_offset = mem->start_;
    const uint32_t fourcc = ReadMemBufLE32(mem);
    const uint32_t payload_size = ReadMemBufLE32(mem);
    const uint32_t payload_size_padded = payload_size + (payload_size & 1);
    const size_t chunk_size = CHUNK_HEADER_SIZE + payload_size_padded;
    int i;
    if (payload_size > MAX_CHUNK_PAYLOAD) {
      LOG_ERROR("Size of chunk payload is over limit.");
      return WEBP_INFO_INVALID_PARAM;
    }
    if (payload_size_padded > MemDataSize(mem)){
      LOG_ERROR("Truncated data detected when parsing chunk payload.");
      return WEBP_INFO_TRUNCATED_DATA;
    }
    for (i = 0; i < CHUNK_TYPES; ++i) {
      if (!memcmp(kWebPChunkTags[i], &fourcc, TAG_SIZE)) break;
    }
    chunk_data->offset_ = chunk_start_offset;
    chunk_data->size_ = chunk_size;
    chunk_data->id_ = (ChunkID)i;
    chunk_data->payload_ = GetBuffer(mem);
    if (chunk_data->id_ == CHUNK_ANMF) {
      if (payload_size != payload_size_padded) {
        LOG_ERROR("ANMF chunk size should always be even.");
        return WEBP_INFO_PARSE_ERROR;
      }
      // There are sub-chunks to be parsed in an ANMF chunk.
      Skip(mem, ANMF_CHUNK_SIZE);
    } else {
      Skip(mem, payload_size_padded);
    }
    return WEBP_INFO_OK;
  }
}

// -----------------------------------------------------------------------------
// Chunk analysis.

static WebPInfoStatus ProcessVP8XChunk(const ChunkData* const chunk_data,
                                       WebPInfo* const webp_info) {
  const uint8_t* data = chunk_data->payload_;
  if (webp_info->chunk_counts_[CHUNK_VP8] ||
      webp_info->chunk_counts_[CHUNK_VP8L] ||
      webp_info->chunk_counts_[CHUNK_VP8X]) {
    LOG_ERROR("Already seen a VP8/VP8L/VP8X chunk when parsing VP8X chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  if (chunk_data->size_ != VP8X_CHUNK_SIZE + CHUNK_HEADER_SIZE) {
    LOG_ERROR("Corrupted VP8X chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  ++webp_info->chunk_counts_[CHUNK_VP8X];
  webp_info->feature_flags_ = *data;
  data += 4;
  webp_info->canvas_width_ = 1 + ReadLE24(&data);
  webp_info->canvas_height_ = 1 + ReadLE24(&data);
  if (!webp_info->quiet_) {
    printf("  ICCP %d\n  Alpha %d\n  EXIF %d\n  XMP %d\n  Animation %d\n",
           (webp_info->feature_flags_ & ICCP_FLAG) != 0,
           (webp_info->feature_flags_ & ALPHA_FLAG) != 0,
           (webp_info->feature_flags_ & EXIF_FLAG) != 0,
           (webp_info->feature_flags_ & XMP_FLAG) != 0,
           (webp_info->feature_flags_ & ANIMATION_FLAG) != 0);
    printf("  Canvas size %d x %d\n",
           webp_info->canvas_width_, webp_info->canvas_height_);
  }
  if (webp_info->canvas_width_ > MAX_CANVAS_SIZE) {
    LOG_WARN("Canvas width is out of range in VP8X chunk.");
  }
  if (webp_info->canvas_height_ > MAX_CANVAS_SIZE) {
    LOG_WARN("Canvas height is out of range in VP8X chunk.");
  }
  if ((uint64_t)webp_info->canvas_width_ * webp_info->canvas_height_ >
      MAX_IMAGE_AREA) {
    LOG_WARN("Canvas area is out of range in VP8X chunk.");
  }
  return WEBP_INFO_OK;
}

static WebPInfoStatus ProcessANIMChunk(const ChunkData* const chunk_data,
                                       WebPInfo* const webp_info) {
  const uint8_t* data = chunk_data->payload_;
  if (!webp_info->chunk_counts_[CHUNK_VP8X]) {
    LOG_ERROR("ANIM chunk detected before VP8X chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  if (chunk_data->size_ != ANIM_CHUNK_SIZE + CHUNK_HEADER_SIZE) {
    LOG_ERROR("Corrupted ANIM chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  webp_info->bgcolor_ = ReadLE32(&data);
  webp_info->loop_count_ = ReadLE16(&data);
  ++webp_info->chunk_counts_[CHUNK_ANIM];
  if (!webp_info->quiet_) {
    printf("  Background color:(BGRA) %02x %02x %02x %02x\n",
           (webp_info->bgcolor_ >> 24) & 0xff,
           (webp_info->bgcolor_ >> 16) & 0xff,
           (webp_info->bgcolor_ >> 8) & 0xff,
           webp_info->bgcolor_ & 0xff);
    printf("  Loop count      : %d\n", webp_info->loop_count_);
  }
  if (webp_info->loop_count_ > MAX_LOOP_COUNT) {
    LOG_WARN("Loop count is out of range in ANIM chunk.");
  }
  return WEBP_INFO_OK;
}

static WebPInfoStatus ProcessANMFChunk(const ChunkData* const chunk_data,
                                       WebPInfo* const webp_info) {
  const uint8_t* data = chunk_data->payload_;
  int offset_x, offset_y, width, height, duration, blend, dispose, temp;
  if (webp_info->is_processing_anim_frame_) {
    LOG_ERROR("ANMF chunk detected within another ANMF chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  if (!webp_info->chunk_counts_[CHUNK_ANIM]) {
    LOG_ERROR("ANMF chunk detected before ANIM chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  if (chunk_data->size_ <= CHUNK_HEADER_SIZE + ANMF_CHUNK_SIZE) {
    LOG_ERROR("Truncated data detected when parsing ANMF chunk.");
    return WEBP_INFO_TRUNCATED_DATA;
  }
  offset_x = 2 * ReadLE24(&data);
  offset_y = 2 * ReadLE24(&data);
  width = 1 + ReadLE24(&data);
  height = 1 + ReadLE24(&data);
  duration = ReadLE24(&data);
  temp = *data;
  dispose = temp & 1;
  blend = (temp >> 1) & 1;
  ++webp_info->chunk_counts_[CHUNK_ANMF];
  if (!webp_info->quiet_) {
    printf("  Offset_X %d\n  Offset_Y %d\n  Width %d\n  Height %d\n"
           "  Duration %d\n  Dispose %d\n  Blend %d\n",
           offset_x, offset_y, width, height, duration, dispose, blend);
  }
  if (duration > MAX_DURATION) {
    LOG_ERROR("Invalid duration parameter in ANMF chunk.");
    return WEBP_INFO_INVALID_PARAM;
  }
  if (offset_x > MAX_POSITION_OFFSET || offset_y > MAX_POSITION_OFFSET) {
    LOG_ERROR("Invalid offset parameters in ANMF chunk.");
    return WEBP_INFO_INVALID_PARAM;
  }
  if ((uint64_t)offset_x + width > (uint64_t)webp_info->canvas_width_ ||
      (uint64_t)offset_y + height > (uint64_t)webp_info->canvas_height_) {
    LOG_ERROR("Frame exceeds canvas in ANMF chunk.");
    return WEBP_INFO_INVALID_PARAM;
  }
  webp_info->is_processing_anim_frame_ = 1;
  webp_info->seen_alpha_subchunk_ = 0;
  webp_info->seen_image_subchunk_ = 0;
  webp_info->frame_width_ = width;
  webp_info->frame_height_ = height;
  webp_info->anim_frame_data_size_ =
      chunk_data->size_ - CHUNK_HEADER_SIZE - ANMF_CHUNK_SIZE;
  return WEBP_INFO_OK;
}

static WebPInfoStatus ProcessImageChunk(const ChunkData* const chunk_data,
                                        WebPInfo* const webp_info) {
  const uint8_t* data = chunk_data->payload_ - CHUNK_HEADER_SIZE;
  WebPBitstreamFeatures features;
  const VP8StatusCode vp8_status =
      WebPGetFeatures(data, chunk_data->size_, &features);
  if (vp8_status != VP8_STATUS_OK) {
    LOG_ERROR("VP8/VP8L bitstream error.");
    return WEBP_INFO_BITSTREAM_ERROR;
  }
  if (!webp_info->quiet_) {
    assert(features.format >= 0 && features.format <= 2);
    printf("  Width %d\n  Height %d\n  Alpha %d\n  Animation %d\n  Format %s\n",
           features.width, features.height, features.has_alpha,
           features.has_animation, kFormats[features.format]);
  }
  if (webp_info->is_processing_anim_frame_) {
    ++webp_info->anmf_subchunk_counts_[chunk_data->id_ == CHUNK_VP8 ? 0 : 1];
    if (chunk_data->id_ == CHUNK_VP8L && webp_info->seen_alpha_subchunk_) {
      LOG_ERROR("Both VP8L and ALPH sub-chunks are present in an ANMF chunk.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (webp_info->frame_width_ != features.width ||
        webp_info->frame_height_ != features.height) {
      LOG_ERROR("Frame size in VP8/VP8L sub-chunk differs from ANMF header.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (webp_info->seen_image_subchunk_) {
      LOG_ERROR("Consecutive VP8/VP8L sub-chunks in an ANMF chunk.");
      return WEBP_INFO_PARSE_ERROR;
    } else {
      webp_info->seen_image_subchunk_ = 1;
    }
  } else {
    if (webp_info->chunk_counts_[CHUNK_VP8] ||
        webp_info->chunk_counts_[CHUNK_VP8L]) {
      LOG_ERROR("Multiple VP8/VP8L chunks detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (chunk_data->id_ == CHUNK_VP8L &&
        webp_info->chunk_counts_[CHUNK_ALPHA]) {
      LOG_ERROR("Both VP8L and ALPH chunks are detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (webp_info->chunk_counts_[CHUNK_ANIM] ||
        webp_info->chunk_counts_[CHUNK_ANMF]) {
      LOG_ERROR("VP8/VP8L chunk and ANIM/ANMF chunk are both detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (webp_info->chunk_counts_[CHUNK_VP8X]) {
      if (webp_info->canvas_width_ != features.width ||
          webp_info->canvas_height_ != features.height) {
        LOG_ERROR("Image size in VP8/VP8L chunk differs from VP8X chunk.");
        return WEBP_INFO_PARSE_ERROR;
      }
    } else {
      webp_info->canvas_width_ = features.width;
      webp_info->canvas_height_ = features.height;
      if (webp_info->canvas_width_ < 1 || webp_info->canvas_height_ < 1 ||
          webp_info->canvas_width_ > MAX_CANVAS_SIZE ||
          webp_info->canvas_height_ > MAX_CANVAS_SIZE ||
          (uint64_t)webp_info->canvas_width_ * webp_info->canvas_height_ >
              MAX_IMAGE_AREA) {
        LOG_WARN("Invalid parameters in VP8/VP8L chunk.");
      }
    }
    ++webp_info->chunk_counts_[chunk_data->id_];
  }
  ++webp_info->num_frames_;
  webp_info->has_alpha_ |= features.has_alpha;
  return WEBP_INFO_OK;
}

static WebPInfoStatus ProcessALPHChunk(const ChunkData* const chunk_data,
                                       WebPInfo* const webp_info) {
  (void)chunk_data;
  if (webp_info->is_processing_anim_frame_) {
    ++webp_info->anmf_subchunk_counts_[2];
    if (webp_info->seen_alpha_subchunk_) {
      LOG_ERROR("Consecutive ALPH sub-chunks in an ANMF chunk.");
      return WEBP_INFO_PARSE_ERROR;
    } else {
      webp_info->seen_alpha_subchunk_ = 1;
    }
    if (webp_info->seen_image_subchunk_) {
      LOG_ERROR("ALPHA sub-chunk detected after VP8 sub-chunk "
                "in an ANMF chunk.");
      return WEBP_INFO_PARSE_ERROR;
    }
  } else {
    if (webp_info->chunk_counts_[CHUNK_ANIM] ||
        webp_info->chunk_counts_[CHUNK_ANMF]) {
      LOG_ERROR("ALPHA chunk and ANIM/ANMF chunk are both detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (!webp_info->chunk_counts_[CHUNK_VP8X]) {
      LOG_ERROR("ALPHA chunk detected before VP8X chunk.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (webp_info->chunk_counts_[CHUNK_VP8]) {
      LOG_ERROR("ALPHA chunk detected after VP8 chunk.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (webp_info->chunk_counts_[CHUNK_ALPHA]) {
      LOG_ERROR("Multiple ALPHA chunks detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    ++webp_info->chunk_counts_[CHUNK_ALPHA];
  }
  webp_info->has_alpha_ = 1;
  return WEBP_INFO_OK;
}

static WebPInfoStatus ProcessICCPChunk(const ChunkData* const chunk_data,
                                       WebPInfo* const webp_info) {
  (void)chunk_data;
  if (!webp_info->chunk_counts_[CHUNK_VP8X]) {
    LOG_ERROR("ICCP chunk detected before VP8X chunk.");
    return WEBP_INFO_PARSE_ERROR;
  }
  if (webp_info->chunk_counts_[CHUNK_VP8] ||
      webp_info->chunk_counts_[CHUNK_VP8L] ||
      webp_info->chunk_counts_[CHUNK_ANIM]) {
    LOG_ERROR("ICCP chunk detected after image data.");
    return WEBP_INFO_PARSE_ERROR;
  }
  ++webp_info->chunk_counts_[CHUNK_ICCP];
  return WEBP_INFO_OK;
}

static WebPInfoStatus ProcessChunk(const ChunkData* const chunk_data,
                                   WebPInfo* const webp_info) {
  WebPInfoStatus status = WEBP_INFO_OK;
  ChunkID id = chunk_data->id_;
  if (chunk_data->id_ == CHUNK_UNKNOWN) {
    char error_message[50];
    snprintf(error_message, 50, "Unknown chunk at offset %6d, length %6d",
            (int)chunk_data->offset_, (int)chunk_data->size_);
    LOG_WARN(error_message);
  } else {
    if (!webp_info->quiet_) {
      const char* tag = kWebPChunkTags[chunk_data->id_];
      printf("Chunk %c%c%c%c at offset %6d, length %6d\n",
             tag[0], tag[1], tag[2], tag[3], (int)chunk_data->offset_,
             (int)chunk_data->size_);
    }
  }
  switch (id) {
    case CHUNK_VP8:
    case CHUNK_VP8L:
      status = ProcessImageChunk(chunk_data, webp_info);
      break;
    case CHUNK_VP8X:
      status = ProcessVP8XChunk(chunk_data, webp_info);
      break;
    case CHUNK_ALPHA:
      status = ProcessALPHChunk(chunk_data, webp_info);
      break;
    case CHUNK_ANIM:
      status = ProcessANIMChunk(chunk_data, webp_info);
      break;
    case CHUNK_ANMF:
      status = ProcessANMFChunk(chunk_data, webp_info);
      break;
    case CHUNK_ICCP:
      status = ProcessICCPChunk(chunk_data, webp_info);
      break;
    case CHUNK_EXIF:
    case CHUNK_XMP:
      ++webp_info->chunk_counts_[id];
      break;
    case CHUNK_UNKNOWN:
    default:
      break;
  }
  if (webp_info->is_processing_anim_frame_ && id != CHUNK_ANMF) {
    if (webp_info->anim_frame_data_size_ == chunk_data->size_) {
      if (!webp_info->seen_image_subchunk_) {
        LOG_ERROR("No VP8/VP8L chunk detected in an ANMF chunk.");
        return WEBP_INFO_PARSE_ERROR;
      }
      webp_info->is_processing_anim_frame_ = 0;
    } else if (webp_info->anim_frame_data_size_ > chunk_data->size_) {
      webp_info->anim_frame_data_size_ -= chunk_data->size_;
    } else {
      LOG_ERROR("Truncated data detected when parsing ANMF chunk.");
      return WEBP_INFO_TRUNCATED_DATA;
    }
  }
  return status;
}

static WebPInfoStatus Validate(const WebPInfo* const webp_info) {
  if (webp_info->num_frames_ < 1) {
    LOG_ERROR("No image/frame detected.");
    return WEBP_INFO_MISSING_DATA;
  }
  if (webp_info->chunk_counts_[CHUNK_VP8X]) {
    const int iccp = !!(webp_info->feature_flags_ & ICCP_FLAG);
    const int exif = !!(webp_info->feature_flags_ & EXIF_FLAG);
    const int xmp = !!(webp_info->feature_flags_ & XMP_FLAG);
    const int animation = !!(webp_info->feature_flags_ & ANIMATION_FLAG);
    if (iccp && !webp_info->chunk_counts_[CHUNK_ICCP]) {
      LOG_ERROR("Missing ICCP chunk.");
      return WEBP_INFO_MISSING_DATA;
    }
    if (exif && !webp_info->chunk_counts_[CHUNK_EXIF]) {
      LOG_ERROR("Missing EXIF chunk.");
      return WEBP_INFO_MISSING_DATA;
    }
    if (xmp && !webp_info->chunk_counts_[CHUNK_XMP]) {
      LOG_ERROR("Missing XMP chunk.");
      return WEBP_INFO_MISSING_DATA;
    }
    if (!iccp && webp_info->chunk_counts_[CHUNK_ICCP]) {
      LOG_ERROR("Unexpected ICCP chunk detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (!exif && webp_info->chunk_counts_[CHUNK_EXIF]) {
      LOG_ERROR("Unexpected EXIF chunk detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (!xmp && webp_info->chunk_counts_[CHUNK_XMP]) {
      LOG_ERROR("Unexpected XMP chunk detected.");
      return WEBP_INFO_PARSE_ERROR;
    }
    // Incomplete animation frame.
    if (webp_info->is_processing_anim_frame_) return WEBP_INFO_MISSING_DATA;
    if (!animation && webp_info->num_frames_ > 1) {
      LOG_ERROR("More than 1 frame detected in non-animation file.");
      return WEBP_INFO_PARSE_ERROR;
    }
    if (animation && (!webp_info->chunk_counts_[CHUNK_ANIM] ||
        !webp_info->chunk_counts_[CHUNK_ANMF])) {
      LOG_ERROR("No ANIM/ANMF chunk detected in animation file.");
      return WEBP_INFO_PARSE_ERROR;
    }
  }
  return WEBP_INFO_OK;
}

static void ShowSummary(const WebPInfo* const webp_info) {
  int i;
  printf("Summary:\n");
  printf("Number of frames: %d\n", webp_info->num_frames_);
  printf("Chunk type  :  VP8 VP8L VP8X ALPH ANIM ANMF(VP8 /VP8L/ALPH) ICCP "
      "EXIF  XMP\n");
  printf("Chunk counts: ");
  for (i = 0; i < CHUNK_TYPES; ++i) {
    printf("%4d ", webp_info->chunk_counts_[i]);
    if (i == CHUNK_ANMF) {
      printf("%4d %4d %4d  ",
             webp_info->anmf_subchunk_counts_[0],
             webp_info->anmf_subchunk_counts_[1],
             webp_info->anmf_subchunk_counts_[2]);
    }
  }
  printf("\n");
}

static WebPInfoStatus AnalyzeWebP(WebPInfo* const webp_info,
                                  const WebPData* webp_data) {
  ChunkData chunk_data;
  MemBuffer mem_buffer;
  WebPInfoStatus webp_info_status = WEBP_INFO_OK;

  InitMemBuffer(&mem_buffer, webp_data);
  webp_info_status = ParseRIFFHeader(webp_info, &mem_buffer);
  if (webp_info_status != WEBP_INFO_OK) goto Error;

  //  Loop through all the chunks. Terminate immediately in case of error.
  while (webp_info_status == WEBP_INFO_OK && MemDataSize(&mem_buffer) > 0) {
    webp_info_status = ParseChunk(webp_info, &mem_buffer, &chunk_data);
    if (webp_info_status != WEBP_INFO_OK) goto Error;
    webp_info_status = ProcessChunk(&chunk_data, webp_info);
  }
  if (webp_info_status != WEBP_INFO_OK) goto Error;
  if (webp_info->show_summary_) ShowSummary(webp_info);

  //  Final check.
  webp_info_status = Validate(webp_info);

 Error:
  if (!webp_info->quiet_) {
    if (webp_info_status == WEBP_INFO_OK) {
      printf("No error detected.\n");
    } else {
      printf("Errors detected.\n");
    }
  }
  return webp_info_status;
}

static void HelpShort(void) {
  printf("Usage: webpinfo [options] in_files\n"
         "Try -longhelp for an exhaustive list of options.\n");
}

static void HelpLong(void) {
  printf("Usage: webpinfo [options] in_files\n"
         "Note: there could be multiple input files;\n"
         "      options must come before input files.\n"
         "Options:\n"
         "  -quiet ......... Do not show chunk parsing information.\n"
         "  -diag .......... Show parsing error diagnosis.\n"
         "  -summary ....... Show chunk stats summary.\n");
}

int main(int argc, const char* argv[]) {
  int c, quiet = 0, show_diag = 0, show_summary = 0;
  WebPInfoStatus webp_info_status = WEBP_INFO_OK;
  WebPInfo webp_info;

  if (argc == 1) {
    HelpShort();
    return WEBP_INFO_OK;
  }

  // Parse command-line input.
  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      HelpShort();
      return WEBP_INFO_OK;
    } else if (!strcmp(argv[c], "-H") || !strcmp(argv[c], "-longhelp")) {
      HelpLong();
      return WEBP_INFO_OK;
    } else if (!strcmp(argv[c], "-quiet")) {
      quiet = 1;
    } else if (!strcmp(argv[c], "-diag")) {
      show_diag = 1;
    } else if (!strcmp(argv[c], "-summary")) {
      show_summary = 1;
    } else {  // Assume the remaining are all input files.
      break;
    }
  }

  if (c == argc) {
    HelpShort();
    return WEBP_INFO_INVALID_COMMAND;
  }

  // Process input files one by one.
  for (; c < argc; ++c) {
    WebPData webp_data;
    const char* in_file = NULL;
    WebPInfoInit(&webp_info);
    webp_info.quiet_ = quiet;
    webp_info.show_diagnosis_ = show_diag;
    webp_info.show_summary_ = show_summary;
    in_file = argv[c];
    if (in_file == NULL || !ReadFileToWebPData(in_file, &webp_data)) {
      webp_info_status = WEBP_INFO_INVALID_COMMAND;
      fprintf(stderr, "Failed to open input file %s.\n", in_file);
      continue;
    }
    if (!webp_info.quiet_) printf("File %s.\n", in_file);
    webp_info_status = AnalyzeWebP(&webp_info, &webp_data);
    WebPDataClear(&webp_data);
  }
  return webp_info_status;
}
