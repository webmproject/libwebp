// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// WebP encoder: main entry point
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vp8enci.h"

// #define PRINT_MEMORY_INFO

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#ifdef PRINT_MEMORY_INFO
#include <stdio.h>
#endif

#define MAX_DIMENSION 16384   // maximum width/height allowed by the spec

//-----------------------------------------------------------------------------
// WebPPicture
//-----------------------------------------------------------------------------

static int DummyWriter(const uint8_t* data, size_t data_size,
                       const WebPPicture* const picture) {
  return 1;
}

int WebPPictureInitInternal(WebPPicture* const picture, int version) {
  if (version != WEBP_ENCODER_ABI_VERSION) {
    return 0;   // caller/system version mismatch!
  }
  if (picture) {
    memset(picture, 0, sizeof(*picture));
    picture->writer = DummyWriter;
  }
  return 1;
}

//-----------------------------------------------------------------------------
// VP8Encoder
//-----------------------------------------------------------------------------

static void ResetSegmentHeader(VP8Encoder* const enc) {
  VP8SegmentHeader* const hdr = &enc->segment_hdr_;
  hdr->num_segments_ = enc->config_->segments;
  hdr->update_map_  = (hdr->num_segments_ > 1);
  hdr->size_ = 0;
}

static void ResetFilterHeader(VP8Encoder* const enc) {
  VP8FilterHeader* const hdr = &enc->filter_hdr_;
  hdr->simple_ = 1;
  hdr->level_ = 0;
  hdr->sharpness_ = 0;
  hdr->i4x4_lf_delta_ = 0;
}

static void ResetBoundaryPredictions(VP8Encoder* const enc) {
  // init boundary values once for all
  // Note: actually, initializing the preds_[] is only needed for intra4.
  int i;
  uint8_t* const top = enc->preds_ - enc->preds_w_;
  uint8_t* const left = enc->preds_ - 1;
  for (i = -1; i < 4 * enc->mb_w_; ++i) {
    top[i] = B_DC_PRED;
  }
  for (i = 0; i < 4 * enc->mb_h_; ++i) {
    left[i * enc->preds_w_] = B_DC_PRED;
  }
  enc->nz_[-1] = 0;   // constant
}

// Map configured quality level to coding tools used.
//-------------+---+---+---+---+---+---+
//   Quality   | 0 | 1 | 2 | 3 | 4 | 5 +
//-------------+---+---+---+---+---+---+
// dynamic prob| ~ | x | x | x | x | x |
//-------------+---+---+---+---+---+---+
// rd-opt modes|   |   | x | x | x | x |
//-------------+---+---+---+---+---+---+
// fast i4/i16 | x | x |   |   |   |   |
//-------------+---+---+---+---+---+---+
// rd-opt i4/16|   |   | x | x | x | x |
//-------------+---+---+---+---+---+---+
// Trellis     |   | x |   |   | x | x |
//-------------+---+---+---+---+---+---+
// full-SNS    |   |   |   |   |   | x |
//-------------+---+---+---+---+---+---+

static void MapConfigToTools(VP8Encoder* const enc) {
  const int method = enc->config_->method;
  enc->method_ = method;
  enc->rd_opt_level_ = (method >= 6) ? 3
                     : (method >= 5) ? 2
                     : (method >= 3) ? 1
                     : 0;
}

// Memory scaling with dimensions:
//  memory (bytes) ~= 2.25 * w + 0.0625 * w * h
//
// Typical memory footprint (768x510 picture)
// Memory used:
//              encoder: 33919
//          block cache: 2880
//                 info: 3072
//                preds: 24897
//          top samples: 1623
//             non-zero: 196
//             lf-stats: 2048
//                total: 68635
// Transcient object sizes:
//       VP8EncIterator: 352
//         VP8ModeScore: 912
//       VP8SegmentInfo: 532
//             VP8Proba: 31032
//              LFStats: 2048
// Picture size (yuv): 589824

static VP8Encoder* InitEncoder(const WebPConfig* const config,
                               WebPPicture* const picture) {
  const int use_filter =
      (config->filter_strength > 0) || (config->autofilter > 0);
  const int mb_w = (picture->width + 15) >> 4;
  const int mb_h = (picture->height + 15) >> 4;
  const int preds_w = 4 * mb_w + 1;
  const int preds_h = 4 * mb_h + 1;
  const size_t preds_size = preds_w * preds_h * sizeof(uint8_t);
  const int top_stride = mb_w * 16;
  const size_t nz_size = (mb_w + 1) * sizeof(uint32_t);
  const size_t cache_size = (3 * YUV_SIZE + PRED_SIZE) * sizeof(uint8_t);
  const size_t info_size = mb_w * mb_h * sizeof(VP8MBInfo);
  const size_t samples_size = (2 * top_stride +         // top-luma/u/v
                               16 + 16 + 16 + 8 + 1 +   // left y/u/v
                               2 * ALIGN_CST)           // align all
                               * sizeof(uint8_t);
  const size_t lf_stats_size = config->autofilter ? sizeof(LFStats) : 0;
  VP8Encoder* enc;
  uint8_t* mem;
  size_t size = sizeof(VP8Encoder) + ALIGN_CST  // main struct
              + cache_size                      // working caches
              + info_size                       // modes info
              + preds_size                      // prediction modes
              + samples_size                    // top/left samples
              + nz_size                         // coeff context bits
              + lf_stats_size;                  // autofilter stats

#ifdef PRINT_MEMORY_INFO
  printf("===================================\n");
  printf("Memory used:\n"
         "             encoder: %ld\n"
         "         block cache: %ld\n"
         "                info: %ld\n"
         "               preds: %ld\n"
         "         top samples: %ld\n"
         "            non-zero: %ld\n"
         "            lf-stats: %ld\n"
         "               total: %ld\n",
         sizeof(VP8Encoder) + ALIGN_CST, cache_size, info_size,
         preds_size, samples_size, nz_size, lf_stats_size, size);
  printf("Transcient object sizes:\n"
         "      VP8EncIterator: %ld\n"
         "        VP8ModeScore: %ld\n"
         "      VP8SegmentInfo: %ld\n"
         "            VP8Proba: %ld\n"
         "             LFStats: %ld\n",
         sizeof(VP8EncIterator), sizeof(VP8ModeScore),
         sizeof(VP8SegmentInfo), sizeof(VP8Proba),
         sizeof(LFStats));
  printf("Picture size (yuv): %ld\n",
         mb_w * mb_h * 384 * sizeof(uint8_t));
  printf("===================================\n");
#endif
  mem = (uint8_t*)malloc(size);
  if (mem == NULL) return NULL;
  enc = (VP8Encoder*)mem;
  mem = (uint8_t*)DO_ALIGN(mem + sizeof(*enc));
  memset(enc, 0, sizeof(*enc));
  enc->num_parts_ = 1 << config->partitions;
  enc->mb_w_ = mb_w;
  enc->mb_h_ = mb_h;
  enc->preds_w_ = preds_w;
  enc->yuv_in_ = (uint8_t*)mem;
  mem += YUV_SIZE;
  enc->yuv_out_ = (uint8_t*)mem;
  mem += YUV_SIZE;
  enc->yuv_out2_ = (uint8_t*)mem;
  mem += YUV_SIZE;
  enc->yuv_p_ = (uint8_t*)mem;
  mem += PRED_SIZE;
  enc->mb_info_ = (VP8MBInfo*)mem;
  mem += info_size;
  enc->preds_ = ((uint8_t*)mem) + 1 + enc->preds_w_;
  mem += preds_w * preds_h * sizeof(uint8_t);
  enc->nz_ = 1 + (uint32_t*)mem;
  mem += nz_size;
  enc->lf_stats_ = lf_stats_size ? (LFStats*)mem : NULL;
  mem += lf_stats_size;

  // top samples (all 16-aligned)
  mem = (uint8_t*)DO_ALIGN(mem);
  enc->y_top_ = (uint8_t*)mem;
  enc->uv_top_ = enc->y_top_ + top_stride;
  mem += 2 * top_stride;
  mem = (uint8_t*)DO_ALIGN(mem + 1);
  enc->y_left_ = (uint8_t*)mem;
  mem += 16 + 16;
  enc->u_left_ = (uint8_t*)mem;
  mem += 16;
  enc->v_left_ = (uint8_t*)mem;
  mem += 8;

  enc->config_ = config;
  enc->profile_ = use_filter ? ((config->filter_type == 1) ? 0 : 1) : 2;
  enc->pic_ = picture;

  MapConfigToTools(enc);
  VP8EncDspInit();
  VP8DefaultProbas(enc);
  ResetSegmentHeader(enc);
  ResetFilterHeader(enc);
  ResetBoundaryPredictions(enc);

  return enc;
}

static void DeleteEncoder(VP8Encoder* enc) {
  free(enc);
}

//-----------------------------------------------------------------------------

static double GetPSNR(uint64_t err, uint64_t size) {
  return err ? 10. * log10(255. * 255. * size / err) : 99.;
}

static void FinalizePSNR(const VP8Encoder* const enc) {
  WebPAuxStats* stats = enc->pic_->stats;
  const uint64_t size = enc->sse_count_;
  const uint64_t* const sse = enc->sse_;
  stats->PSNR[0] = (float)GetPSNR(sse[0], size);
  stats->PSNR[1] = (float)GetPSNR(sse[1], size / 4);
  stats->PSNR[2] = (float)GetPSNR(sse[2], size / 4);
  stats->PSNR[3] = (float)GetPSNR(sse[0] + sse[1] + sse[2], size * 3 / 2);
}

static void StoreStats(VP8Encoder* const enc) {
  WebPAuxStats* const stats = enc->pic_->stats;
  if (stats) {
    int i, s;
    for (i = 0; i < NUM_MB_SEGMENTS; ++i) {
      stats->segment_level[i] = enc->dqm_[i].fstrength_;
      stats->segment_quant[i] = enc->dqm_[i].quant_;
      for (s = 0; s <= 2; ++s) {
        stats->residual_bytes[s][i] = enc->residual_bytes_[s][i];
      }
    }
    FinalizePSNR(enc);
    stats->coded_size = enc->coded_size_;
    for (i = 0; i < 3; ++i) {
      stats->block_count[i] = enc->block_count_[i];
    }
  }
}

//-----------------------------------------------------------------------------

int WebPEncode(const WebPConfig* const config, WebPPicture* const pic) {
  VP8Encoder* enc;
  int ok;

  if (config == NULL || pic == NULL)
    return 0;   // bad params
  if (!WebPValidateConfig(config))
    return 0;   // invalid config.
  if (pic->width <= 0 || pic->height <= 0)
    return 0;   // invalid parameters
  if (pic->y == NULL || pic->u == NULL || pic->v == NULL)
    return 0;   // invalid parameters
  if (pic->width >= MAX_DIMENSION || pic->height >= MAX_DIMENSION)
    return 0;   // image is too big

  enc = InitEncoder(config, pic);
  if (enc == NULL) return 0;
  ok = VP8EncAnalyze(enc)
    && VP8StatLoop(enc)
    && VP8EncLoop(enc)
    && VP8EncWrite(enc);
  StoreStats(enc);
  DeleteEncoder(enc);
  return ok;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
