// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Incremental decoding
//
// Author: somnath@google.com (Somnath Banerjee)

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "webpi.h"
#include "vp8i.h"
#include "yuv.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define RIFF_HEADER_SIZE 20
#define VP8_HEADER_SIZE 10
#define WEBP_HEADER_SIZE (RIFF_HEADER_SIZE + VP8_HEADER_SIZE)
#define CHUNK_SIZE 4096
#define MAX_MB_SIZE 4096

//------------------------------------------------------------------------------
// Data structures for memory and states

// Decoding states. State normally flows like HEADER->PARTS0->DATA->DONE.
// If there is any error the decoder goes into state ERROR.
typedef enum { STATE_HEADER = 0, STATE_PARTS0 = 1,
               STATE_DATA = 2, STATE_DONE = 3,
               STATE_ERROR = 4
} DecState;

// Operating state for the MemBuffer
typedef enum { MEM_MODE_NONE = 0,
               MEM_MODE_APPEND, MEM_MODE_MAP
} MemBufferMode;

// storage for partition #0 and partial data (in a rolling fashion)
typedef struct {
  MemBufferMode mode_;  // Operation mode
  uint32_t start_;      // start location of the data to be decoded
  uint32_t end_;        // end location
  size_t buf_size_;     // size of the allocated buffer
  uint8_t* buf_;        // We don't own this buffer in case WebPIUpdate()

  size_t part0_size_;         // size of partition #0
  const uint8_t* part0_buf_;  // buffer to store partition #0
} MemBuffer;

struct WebPIDecoder {
  DecState state_;         // current decoding state
  int w_, h_;              // width and height
  WebPDecParams params_;   // Params to store output info
  VP8Decoder* dec_;
  VP8Io io_;

  MemBuffer mem_;          // memory buffer
};

// MB context to restore in case VP8DecodeMB() fails
typedef struct {
  VP8MB left_;
  VP8MB info_;
  uint8_t intra_t_[4];
  uint8_t intra_l_[4];
  VP8BitReader br_;
  VP8BitReader token_br_;
} MBContext;

//------------------------------------------------------------------------------
// MemBuffer: incoming data handling

#define REMAP(PTR, OLD_BASE, NEW_BASE) (PTR) = (NEW_BASE) + ((PTR) - OLD_BASE)

static inline size_t MemDataSize(const MemBuffer* mem) {
  return (mem->end_ - mem->start_);
}

// Appends data to the end of MemBuffer->buf_. It expands the allocated memory
// size if required and also updates VP8BitReader's if new memory is allocated.
static int AppendToMemBuffer(WebPIDecoder* const idec,
                             const uint8_t* const data, size_t data_size) {
  MemBuffer* const mem = &idec->mem_;
  VP8Decoder* const dec = idec->dec_;
  const int last_part = dec->num_parts_ - 1;
  assert(mem->mode_ == MEM_MODE_APPEND);

  if (mem->end_ + data_size > mem->buf_size_) {  // Need some free memory
    int p;
    uint8_t* new_buf = NULL;
    const int num_chunks = (MemDataSize(mem) + data_size + CHUNK_SIZE - 1)
        / CHUNK_SIZE;
    const size_t new_size = num_chunks * CHUNK_SIZE;
    const uint8_t* const base = mem->buf_ + mem->start_;

    new_buf = (uint8_t*)malloc(new_size);
    if (!new_buf) return 0;
    memcpy(new_buf, base, MemDataSize(mem));

    // adjust VP8BitReader pointers
    for (p = 0; p <= last_part; ++p) {
      if (dec->parts_[p].buf_) {
        REMAP(dec->parts_[p].buf_, base, new_buf);
        REMAP(dec->parts_[p].buf_end_, base, new_buf);
      }
    }

    // adjust memory pointers
    free(mem->buf_);
    mem->buf_ = new_buf;
    mem->buf_size_ = new_size;

    mem->end_ = MemDataSize(mem);
    mem->start_ = 0;
  }

  memcpy(mem->buf_ + mem->end_, data, data_size);
  mem->end_ += data_size;
  assert(mem->end_ <= mem->buf_size_);
  dec->parts_[last_part].buf_end_ = mem->buf_ + mem->end_;

  // note: setting up idec->io_ is only really needed at the beginning
  // of the decoding, till partition #0 is complete.
  idec->io_.data = mem->buf_ + mem->start_;
  idec->io_.data_size = MemDataSize(mem);
  return 1;
}

static int RemapMemBuffer(WebPIDecoder* const idec,
                          const uint8_t* const data, size_t data_size) {
  int p;
  MemBuffer* const mem = &idec->mem_;
  VP8Decoder* const dec = idec->dec_;
  const int last_part = dec->num_parts_ - 1;
  const uint8_t* base = mem->buf_;

  assert(mem->mode_ == MEM_MODE_MAP);
  if (data_size < mem->buf_size_) {
    return 0;  // we cannot remap to a shorter buffer!
  }

  for (p = 0; p <= last_part; ++p) {
    if (dec->parts_[p].buf_) {
      REMAP(dec->parts_[p].buf_, base, data);
      REMAP(dec->parts_[p].buf_end_, base, data);
    }
  }
  dec->parts_[last_part].buf_end_ = data + data_size;

  // Remap partition #0 data pointer to new offset.
  if (dec->br_.buf_) {
    REMAP(dec->br_.buf_, base, data);
    REMAP(dec->br_.buf_end_, base, data);
  }

  mem->buf_ = (uint8_t*)data;
  mem->end_ = mem->buf_size_ = data_size;

  idec->io_.data = data;
  idec->io_.data_size = data_size;
  return 1;
}

static void InitMemBuffer(MemBuffer* const mem) {
  mem->mode_       = MEM_MODE_NONE;
  mem->buf_        = 0;
  mem->buf_size_   = 0;
  mem->part0_buf_  = 0;
  mem->part0_size_ = 0;
}

static void ClearMemBuffer(MemBuffer* const mem) {
  assert(mem);
  if (mem->mode_ == MEM_MODE_APPEND) {
    free(mem->buf_);
    free((void*)mem->part0_buf_);
  }
}

static int CheckMemBufferMode(MemBuffer* const mem, MemBufferMode expected) {
  if (mem->mode_ == MEM_MODE_NONE) {
    mem->mode_ = expected;    // switch to the expected mode
  } else if (mem->mode_ != expected) {
    return 0;         // we mixed the modes => error
  }
  assert(mem->mode_ == expected);   // mode is ok
  return 1;
}

#undef REMAP

//------------------------------------------------------------------------------
// Macroblock-decoding contexts

static void SaveContext(const VP8Decoder* dec, const VP8BitReader* token_br,
                        MBContext* const context) {
  const VP8BitReader* const br = &dec->br_;
  const VP8MB* const left = dec->mb_info_ - 1;
  const VP8MB* const info = dec->mb_info_ + dec->mb_x_;

  context->left_ = *left;
  context->info_ = *info;
  context->br_ = *br;
  context->token_br_ = *token_br;
  memcpy(context->intra_t_, dec->intra_t_ + 4 * dec->mb_x_, 4);
  memcpy(context->intra_l_, dec->intra_l_, 4);
}

static void RestoreContext(const MBContext* context, VP8Decoder* const dec,
                           VP8BitReader* const token_br) {
  VP8BitReader* const br = &dec->br_;
  VP8MB* const left = dec->mb_info_ - 1;
  VP8MB* const info = dec->mb_info_ + dec->mb_x_;

  *left = context->left_;
  *info = context->info_;
  *br = context->br_;
  *token_br = context->token_br_;
  memcpy(dec->intra_t_ + 4 * dec->mb_x_, context->intra_t_, 4);
  memcpy(dec->intra_l_, context->intra_l_, 4);
}

//------------------------------------------------------------------------------

static VP8StatusCode IDecError(WebPIDecoder* idec, VP8StatusCode error) {
  idec->state_ = STATE_ERROR;
  return error;
}

// Header
static VP8StatusCode DecodeHeader(WebPIDecoder* const idec) {
  int width, height;
  uint32_t curr_size, riff_header_size, bits;
  WebPDecParams* params = &idec->params_;
  const uint8_t* data = idec->mem_.buf_ + idec->mem_.start_;

  if (MemDataSize(&idec->mem_) < WEBP_HEADER_SIZE) {
    return VP8_STATUS_SUSPENDED;
  }

  if (!WebPInitDecParams(data, idec->mem_.end_, &width, &height, params)) {
    return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
  }

  // Validate and Skip over RIFF header
  curr_size = MemDataSize(&idec->mem_);
  if (!WebPCheckRIFFHeader(&data, &curr_size)) {
    return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
  }
  riff_header_size = idec->mem_.end_ - curr_size;
  bits = data[0] | (data[1] << 8) | (data[2] << 16);

  idec->mem_.part0_size_ = (bits >> 5) + VP8_HEADER_SIZE;
  idec->mem_.start_ += riff_header_size;
  assert(idec->mem_.start_ <= idec->mem_.end_);

  idec->w_ = width;
  idec->h_ = height;
  idec->io_.data_size -= riff_header_size;
  idec->io_.data = data;
  idec->state_ = STATE_PARTS0;
  return VP8_STATUS_OK;
}

// Partition #0
static int CopyParts0Data(WebPIDecoder* idec) {
  VP8BitReader* const br = &idec->dec_->br_;
  const size_t psize = br->buf_end_ - br->buf_;
  MemBuffer* const mem = &idec->mem_;
  assert(!mem->part0_buf_);
  assert(psize > 0);
  assert(psize <= mem->part0_size_);
  if (mem->mode_ == MEM_MODE_APPEND) {
    // We copy and grab ownership of the partition #0 data.
    uint8_t* const part0_buf = (uint8_t*)malloc(psize);
    if (!part0_buf) {
      return 0;
    }
    memcpy(part0_buf, br->buf_, psize);
    mem->part0_buf_ = part0_buf;
    mem->start_ += psize;
    br->buf_ = part0_buf;
    br->buf_end_ = part0_buf + psize;
  } else {
    // Else: just keep pointers to the partition #0's data in dec_->br_.
  }
  return 1;
}

static VP8StatusCode DecodePartition0(WebPIDecoder* const idec) {
  VP8Decoder* const dec = idec->dec_;
  VP8Io* const io = &idec->io_;
  const WebPDecParams* const params = &idec->params_;
  const WEBP_CSP_MODE mode = params->mode;

  // Wait till we have enough data for the whole partition #0
  if (MemDataSize(&idec->mem_) < idec->mem_.part0_size_) {
    return VP8_STATUS_SUSPENDED;
  }

  io->opaque = &idec->params_;
  if (!VP8GetHeaders(dec, io)) {
    const VP8StatusCode status = dec->status_;
    if (status == VP8_STATUS_SUSPENDED ||
        status == VP8_STATUS_NOT_ENOUGH_DATA) {
      // treating NOT_ENOUGH_DATA as SUSPENDED state
      return VP8_STATUS_SUSPENDED;
    }
    return IDecError(idec, status);
  }

  if (!WebPCheckDecParams(io, params)) {
    return IDecError(idec, VP8_STATUS_INVALID_PARAM);
  }

  if (mode != MODE_YUV) {
    VP8YUVInit();
  }

  // allocate memory and prepare everything.
  if (!VP8InitFrame(dec, io)) {
    return IDecError(idec, VP8_STATUS_OUT_OF_MEMORY);
  }
  if (io->setup && !io->setup(io)) {
    return IDecError(idec, VP8_STATUS_USER_ABORT);
  }

  // disable filtering per user request (_after_ setup() is called)
  if (io->bypass_filtering) dec->filter_type_ = 0;

  if (!CopyParts0Data(idec)) {
    return IDecError(idec, VP8_STATUS_OUT_OF_MEMORY);
  }

  idec->state_ = STATE_DATA;
  return VP8_STATUS_OK;
}

// Remaining partitions
static VP8StatusCode DecodeRemaining(WebPIDecoder* const idec) {
  VP8BitReader*  br;
  VP8Decoder* const dec = idec->dec_;
  VP8Io* const io = &idec->io_;

  assert(dec->ready_);

  br = &dec->br_;
  for (; dec->mb_y_ < dec->mb_h_; ++dec->mb_y_) {
    VP8BitReader* token_br = &dec->parts_[dec->mb_y_ & (dec->num_parts_ - 1)];
    if (dec->mb_x_ == 0) {
      VP8MB* const left = dec->mb_info_ - 1;
      left->nz_ = 0;
      left->dc_nz_ = 0;
      memset(dec->intra_l_, B_DC_PRED, sizeof(dec->intra_l_));
    }

    for (; dec->mb_x_ < dec->mb_w_;  dec->mb_x_++) {
      MBContext context;
      SaveContext(dec, token_br, &context);

      if (!VP8DecodeMB(dec, token_br)) {
        RestoreContext(&context, dec, token_br);
        // We shouldn't fail when MAX_MB data was available
        if (dec->num_parts_ == 1 && MemDataSize(&idec->mem_) > MAX_MB_SIZE) {
          return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
        }
        return VP8_STATUS_SUSPENDED;
      }
      VP8ReconstructBlock(dec);
      // Store data and save block's filtering params
      VP8StoreBlock(dec);

      // Release buffer only if there is only one partition
      if (dec->num_parts_ == 1) {
        idec->mem_.start_ = token_br->buf_ - idec->mem_.buf_;
        assert(idec->mem_.start_ <= idec->mem_.end_);
      }
    }
    if (!VP8FinishRow(dec, io)) {
      return IDecError(idec, VP8_STATUS_USER_ABORT);
    }
    dec->mb_x_ = 0;
  }

  if (io->teardown) {
    io->teardown(io);
  }
  dec->ready_ = 0;
  idec->state_ = STATE_DONE;

  return VP8_STATUS_OK;
}

  // Main decoding loop
static VP8StatusCode IDecode(WebPIDecoder* idec) {
  VP8StatusCode status = VP8_STATUS_SUSPENDED;
  assert(idec->dec_);

  if (idec->state_ == STATE_HEADER) {
    status = DecodeHeader(idec);
  }
  if (idec->state_ == STATE_PARTS0) {
    status = DecodePartition0(idec);
  }
  if (idec->state_ == STATE_DATA) {
    return DecodeRemaining(idec);
  }
  return status;
}

//------------------------------------------------------------------------------
// Public functions

WebPIDecoder* WebPINew(WEBP_CSP_MODE mode) {
  WebPIDecoder* idec = (WebPIDecoder*)calloc(1, sizeof(WebPIDecoder));
  if (!idec) return NULL;

  idec->dec_ = VP8New();
  if (idec->dec_ == NULL) {
    free(idec);
    return NULL;
  }

  idec->state_ = STATE_HEADER;
  idec->params_.mode = mode;

  InitMemBuffer(&idec->mem_);
  VP8InitIo(&idec->io_);
  WebPInitCustomIo(&idec->io_);
  return idec;
}

void WebPIDelete(WebPIDecoder* const idec) {
  if (!idec) return;
  VP8Delete(idec->dec_);
  WebPClearDecParams(&idec->params_);
  ClearMemBuffer(&idec->mem_);
  free(idec);
}

//------------------------------------------------------------------------------

WebPIDecoder* WebPINewRGB(WEBP_CSP_MODE mode, uint8_t* output_buffer,
                          int output_buffer_size, int output_stride) {
  WebPIDecoder* idec;
  if (mode == MODE_YUV) return NULL;
  idec = WebPINew(mode);
  if (idec == NULL) return NULL;
  idec->params_.output = output_buffer;
  idec->params_.stride = output_stride;
  idec->params_.output_size = output_buffer_size;
  idec->params_.external_buffer = 1;
  return idec;
}

WebPIDecoder* WebPINewYUV(uint8_t* luma, int luma_size, int luma_stride,
                          uint8_t* u, int u_size, int u_stride,
                          uint8_t* v, int v_size, int v_stride) {
  WebPIDecoder* idec = WebPINew(MODE_YUV);
  if (idec == NULL) return NULL;
  idec->params_.output = luma;
  idec->params_.stride = luma_stride;
  idec->params_.output_size = luma_size;
  idec->params_.u = u;
  idec->params_.u_stride = u_stride;
  idec->params_.output_u_size = u_size;
  idec->params_.v = v;
  idec->params_.v_stride = v_stride;
  idec->params_.output_v_size = v_size;
  idec->params_.external_buffer = 1;
  return idec;
}

//------------------------------------------------------------------------------

static VP8StatusCode IDecCheckStatus(const WebPIDecoder* const idec) {
  assert(idec);
  if (idec->dec_ == NULL) {
    return VP8_STATUS_USER_ABORT;
  }
  if (idec->state_ == STATE_ERROR) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }
  if (idec->state_ == STATE_DONE) {
    return VP8_STATUS_OK;
  }
  return VP8_STATUS_SUSPENDED;
}

VP8StatusCode WebPIAppend(WebPIDecoder* const idec, const uint8_t* data,
                          uint32_t data_size) {
  VP8StatusCode status;
  if (idec == NULL || data == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  status = IDecCheckStatus(idec);
  if (status != VP8_STATUS_SUSPENDED) {
    return status;
  }
  // Check mixed calls between RemapMemBuffer and AppendToMemBuffer.
  if (!CheckMemBufferMode(&idec->mem_, MEM_MODE_APPEND)) {
    return VP8_STATUS_INVALID_PARAM;
  }
  // Append data to memory buffer
  if (!AppendToMemBuffer(idec, data, data_size)) {
    return VP8_STATUS_OUT_OF_MEMORY;
  }
  return IDecode(idec);
}

VP8StatusCode WebPIUpdate(WebPIDecoder* const idec, const uint8_t* data,
                          uint32_t data_size) {
  VP8StatusCode status;
  if (idec == NULL || data == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  status = IDecCheckStatus(idec);
  if (status != VP8_STATUS_SUSPENDED) {
    return status;
  }
  // Check mixed calls between RemapMemBuffer and AppendToMemBuffer.
  if (!CheckMemBufferMode(&idec->mem_, MEM_MODE_MAP)) {
    return VP8_STATUS_INVALID_PARAM;
  }
  // Make the memory buffer point to the new buffer
  if (!RemapMemBuffer(idec, data, data_size)) {
    return VP8_STATUS_INVALID_PARAM;
  }
  return IDecode(idec);
}

//------------------------------------------------------------------------------

uint8_t* WebPIDecGetRGB(const WebPIDecoder* const idec, int *last_y,
                        int* width, int* height, int* stride) {
  if (!idec || !idec->dec_ || idec->params_.mode != MODE_RGB ||
      idec->state_ <= STATE_PARTS0) {
    return NULL;
  }

  if (last_y) *last_y = idec->params_.last_y;
  if (width) *width = idec->w_;
  if (height) *height = idec->h_;
  if (stride) *stride = idec->params_.stride;

  return idec->params_.output;
}

uint8_t* WebPIDecGetYUV(const WebPIDecoder* const idec, int *last_y,
                        uint8_t** u, uint8_t** v, int* width, int* height,
                        int *stride, int* uv_stride) {
  if (!idec || !idec->dec_ || idec->params_.mode != MODE_YUV ||
      idec->state_ <= STATE_PARTS0) {
    return NULL;
  }

  if (last_y) *last_y = idec->params_.last_y;
  if (u) *u = idec->params_.u;
  if (v) *v = idec->params_.v;
  if (width) *width = idec->w_;
  if (height) *height = idec->h_;
  if (stride) *stride = idec->params_.stride;
  if (uv_stride) *uv_stride = idec->params_.u_stride;

  return idec->params_.output;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
