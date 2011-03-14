// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// VP8 decoder: internal header.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_DEC_VP8I_H_
#define WEBP_DEC_VP8I_H_

#include <string.h>     // for memcpy()
#include "bits.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Various defines and enums

#define ONLY_KEYFRAME_CODE      // to remove any code related to P-Frames

// intra prediction modes
enum { B_DC_PRED = 0,   // 4x4 modes
       B_TM_PRED,
       B_VE_PRED,
       B_HE_PRED,
       B_RD_PRED,
       B_VR_PRED,
       B_LD_PRED,
       B_VL_PRED,
       B_HD_PRED,
       B_HU_PRED,
       NUM_BMODES = B_HU_PRED + 1 - B_DC_PRED,  // = 10

       // Luma16 or UV modes
       DC_PRED = B_DC_PRED, V_PRED = B_VE_PRED,
       H_PRED = B_HE_PRED, TM_PRED = B_TM_PRED,
       B_PRED = NUM_BMODES,   // refined I4x4 mode

       // special modes
       B_DC_PRED_NOTOP = 4,
       B_DC_PRED_NOLEFT = 5,
       B_DC_PRED_NOTOPLEFT = 6,
       NUM_B_DC_MODES = 7 };

enum { MB_FEATURE_TREE_PROBS = 3,
       NUM_MB_SEGMENTS = 4,
       NUM_REF_LF_DELTAS = 4,
       NUM_MODE_LF_DELTAS = 4,    // I4x4, ZERO, *, SPLIT
       MAX_NUM_PARTITIONS = 8,
       // Probabilities
       NUM_TYPES = 4,
       NUM_BANDS = 8,
       NUM_CTX = 3,
       NUM_PROBAS = 11,
       NUM_MV_PROBAS = 19 };

// YUV-cache parameters.
// Constraints are: We need to store one 16x16 block of luma samples (y),
// and two 8x8 chroma blocks (u/v). These are better be 16-bytes aligned,
// in order to be SIMD-friendly. We also need to store the top, left and
// top-left samples (from previously decoded blocks), along with four
// extra top-right samples for luma (intra4x4 prediction only).
// One possible layout is, using 32 * (17 + 9) bytes:
//
//   .+------   <- only 1 pixel high
//   .|yyyyt.
//   .|yyyyt.
//   .|yyyyt.
//   .|yyyy..
//   .+--.+--   <- only 1 pixel high
//   .|uu.|vv
//   .|uu.|vv
//
// Every character is a 4x4 block, with legend:
//  '.' = unused
//  'y' = y-samples   'u' = u-samples     'v' = u-samples
//  '|' = left sample,   '-' = top sample,    '+' = top-left sample
//  't' = extra top-right sample for 4x4 modes
// With this layout, BPS (=Bytes Per Scan-line) is one cacheline size.
#define BPS       32    // this is the common stride used by yuv[]
#define YUV_SIZE (BPS * 17 + BPS * 9)
#define Y_SIZE   (BPS * 17)
#define Y_OFF    (BPS * 1 + 8)
#define U_OFF    (Y_OFF + BPS * 16 + BPS)
#define V_OFF    (U_OFF + 16)

//-----------------------------------------------------------------------------
// Headers

typedef struct {
  uint8_t key_frame_;
  uint8_t profile_;
  uint8_t show_;
  uint32_t partition_length_;
} VP8FrameHeader;

typedef struct {
  uint16_t width_;
  uint16_t height_;
  uint8_t xscale_;
  uint8_t yscale_;
  uint8_t colorspace_;   // 0 = YCbCr
  uint8_t clamp_type_;
} VP8PictureHeader;

// segment features
typedef struct {
  int use_segment_;
  int update_map_;        // whether to update the segment map or not
  int absolute_delta_;    // absolute or delta values for quantizer and filter
  int8_t quantizer_[NUM_MB_SEGMENTS];        // quantization changes
  int8_t filter_strength_[NUM_MB_SEGMENTS];  // filter strength for segments
} VP8SegmentHeader;

// Struct collecting all frame-persistent probabilities.
typedef struct {
  uint8_t segments_[MB_FEATURE_TREE_PROBS];
  // Type: 0:Intra16-AC  1:Intra16-DC   2:Chroma   3:Intra4
  uint8_t coeffs_[NUM_TYPES][NUM_BANDS][NUM_CTX][NUM_PROBAS];
#ifndef ONLY_KEYFRAME_CODE
  uint8_t ymode_[4], uvmode_[3];
  uint8_t mv_[2][NUM_MV_PROBAS];
#endif
} VP8Proba;

// Filter parameters
typedef struct {
  int simple_;                  // 0=complex, 1=simple
  int level_;                   // [0..63]
  int sharpness_;               // [0..7]
  int use_lf_delta_;
  int ref_lf_delta_[NUM_REF_LF_DELTAS];
  int mode_lf_delta_[NUM_MODE_LF_DELTAS];
} VP8FilterHeader;

//-----------------------------------------------------------------------------
// Informations about the macroblocks.

typedef struct {
  // block type
  uint8_t skip_:1;
  // filter specs
  uint8_t f_level_:6;      // filter strength: 0..63
  uint8_t f_ilevel_:6;     // inner limit: 1..63
  uint8_t f_inner_:1;      // do inner filtering?
  // cbp
  uint8_t nz_;        // non-zero AC/DC coeffs
  uint8_t dc_nz_;     // non-zero DC coeffs
} VP8MB;

// Dequantization matrices
typedef struct {
  uint16_t y1_mat_[2], y2_mat_[2], uv_mat_[2];    // [DC / AC]
} VP8QuantMatrix;

//-----------------------------------------------------------------------------
// VP8Decoder: the main opaque structure handed over to user

struct VP8Decoder {
  VP8StatusCode status_;
  int ready_;     // true if ready to decode a picture with VP8Decode()
  const char* error_msg_;  // set when status_ is not OK.

  // Main data source
  VP8BitReader br_;

  // headers
  VP8FrameHeader   frm_hdr_;
  VP8PictureHeader pic_hdr_;
  VP8FilterHeader  filter_hdr_;
  VP8SegmentHeader segment_hdr_;

  // dimension, in macroblock units.
  int mb_w_, mb_h_;

  // number of partitions.
  int num_parts_;
  // per-partition boolean decoders.
  VP8BitReader parts_[MAX_NUM_PARTITIONS];

  // buffer refresh flags
  //   bit 0: refresh Gold, bit 1: refresh Alt
  //   bit 2-3: copy to Gold, bit 4-5: copy to Alt
  //   bit 6: Gold sign bias, bit 7: Alt sign bias
  //   bit 8: refresh last frame
  uint32_t buffer_flags_;

  // dequantization (one set of DC/AC dequant factor per segment)
  VP8QuantMatrix dqm_[NUM_MB_SEGMENTS];

  // probabilities
  VP8Proba proba_;
  int use_skip_proba_;
  uint8_t skip_p_;
#ifndef ONLY_KEYFRAME_CODE
  uint8_t intra_p_, last_p_, golden_p_;
  VP8Proba proba_saved_;
  int update_proba_;
#endif

  // Boundary data cache and persistent buffers.
  uint8_t* intra_t_;     // top intra modes values: 4 * mb_w_
  uint8_t  intra_l_[4];  // left intra modes values
  uint8_t *y_t_;         // top luma samples: 16 * mb_w_
  uint8_t *u_t_, *v_t_;  // top u/v samples: 8 * mb_w_ each

  VP8MB* mb_info_;       // contextual macroblock infos (mb_w_ + 1)
  uint8_t* yuv_b_;       // main block for Y/U/V (size = YUV_SIZE)
  int16_t* coeffs_;      // 384 coeffs = (16+8+8) * 4*4

  uint8_t* cache_y_;     // macroblock row for storing unfiltered samples
  uint8_t* cache_u_;
  uint8_t* cache_v_;
  int cache_y_stride_;
  int cache_uv_stride_;

  // main memory chunk for the above data. Persistent.
  void* mem_;
  int mem_size_;

  // Per macroblock non-persistent infos.
  int mb_x_, mb_y_;       // current position, in macroblock units
  uint8_t is_i4x4_;       // true if intra4x4
  uint8_t imodes_[16];    // one 16x16 mode (#0) or sixteen 4x4 modes
  uint8_t uvmode_;        // chroma prediction mode
  uint8_t segment_;       // block's segment

  // bit-wise info about the content of each sub-4x4 blocks: there are 16 bits
  // for luma (bits #0->#15), then 4 bits for chroma-u (#16->#19) and 4 bits for
  // chroma-v (#20->#23), each corresponding to one 4x4 block in decoding order.
  // If the bit is set, the 4x4 block contains some non-zero coefficients.
  uint32_t non_zero_;
  uint32_t non_zero_ac_;

  // Filtering side-info
  int filter_type_;                       // 0=off, 1=simple, 2=complex
  uint8_t filter_levels_[NUM_MB_SEGMENTS];  // precalculated per-segment
};

//-----------------------------------------------------------------------------
// internal functions. Not public.

// in vp8.c
int VP8SetError(VP8Decoder* const dec,
                VP8StatusCode error, const char * const msg);

// in tree.c
void VP8ResetProba(VP8Proba* const proba);
void VP8ParseProba(VP8BitReader* const br, VP8Decoder* const dec);
void VP8ParseIntraMode(VP8BitReader* const br,  VP8Decoder* const dec);

// in quant.c
void VP8ParseQuant(VP8Decoder* const dec);

// in frame.c
int VP8InitFrame(VP8Decoder* const dec, VP8Io* io);
// Predict a block and add residual
void VP8ReconstructBlock(VP8Decoder* const dec);
// Store a block, along with filtering params
void VP8StoreBlock(VP8Decoder* const dec);
// Finalize and transmit a complete row. Return false in case of user-abort.
int VP8FinishRow(VP8Decoder* const dec, VP8Io* io);
// Decode one macroblock. Returns false if there is not enough data.
int VP8DecodeMB(VP8Decoder* const dec, VP8BitReader* const token_br);

// in dsp.c
typedef void (*VP8Idct)(const int16_t* coeffs, uint8_t* dst);
extern VP8Idct VP8Transform;
extern VP8Idct VP8TransformUV;
extern VP8Idct VP8TransformDC;
extern VP8Idct VP8TransformDCUV;
extern void (*VP8TransformWHT)(const int16_t* in, int16_t* out);

// *dst is the destination block, with stride BPS. Boundary samples are
// assumed accessible when needed.
typedef void (*VP8PredFunc)(uint8_t *dst);
extern VP8PredFunc VP8PredLuma16[NUM_B_DC_MODES];
extern VP8PredFunc VP8PredChroma8[NUM_B_DC_MODES];
extern VP8PredFunc VP8PredLuma4[NUM_BMODES];

void VP8DspInit();        // must be called before anything using the above
void VP8DspInitTables();  // needs to be called no matter what.

// simple filter (only for luma)
typedef void (*VP8SimpleFilterFunc)(uint8_t* p, int stride, int thresh);
extern VP8SimpleFilterFunc VP8SimpleVFilter16;
extern VP8SimpleFilterFunc VP8SimpleHFilter16;
extern VP8SimpleFilterFunc VP8SimpleVFilter16i;  // filter 3 inner edges
extern VP8SimpleFilterFunc VP8SimpleHFilter16i;

// regular filter (on both macroblock edges and inner edges)
typedef void (*VP8LumaFilterFunc)(uint8_t* luma, int stride,
                                  int thresh, int ithresh, int hev_t);
typedef void (*VP8ChromaFilterFunc)(uint8_t* u, uint8_t* v, int stride,
                                    int thresh, int ithresh, int hev_t);
// on outter edge
extern VP8LumaFilterFunc VP8VFilter16;
extern VP8LumaFilterFunc VP8HFilter16;
extern VP8ChromaFilterFunc VP8VFilter8;
extern VP8ChromaFilterFunc VP8HFilter8;

// on inner edge
extern VP8LumaFilterFunc VP8VFilter16i;   // filtering 3 inner edges altogether
extern VP8LumaFilterFunc VP8HFilter16i;
extern VP8ChromaFilterFunc VP8VFilter8i;  // filtering u and v altogether
extern VP8ChromaFilterFunc VP8HFilter8i;

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_DEC_VP8I_H_
