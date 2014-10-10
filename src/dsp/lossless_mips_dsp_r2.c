// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Image transforms and color space conversion methods for lossless decoder.
//
// Author(s):  Djordje Pesut    (djordje.pesut@imgtec.com)
//             Jovan Zelincevic (jovan.zelincevic@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "./lossless.h"

#define MAP_COLOR_FUNCS(FUNC_NAME, TYPE, GET_INDEX, GET_VALUE)                 \
static void FUNC_NAME(const TYPE* src,                                         \
                      const uint32_t* const color_map,                         \
                      TYPE* dst, int y_start, int y_end,                       \
                      int width) {                                             \
  int y;                                                                       \
  for (y = y_start; y < y_end; ++y) {                                          \
    int x;                                                                     \
    for (x = 0; x < (width >> 2); ++x) {                                       \
      int tmp1, tmp2, tmp3, tmp4;                                              \
      __asm__ volatile (                                                       \
      ".ifc        "#TYPE",  uint8_t                    \n\t"                  \
        "lbu       %[tmp1],  0(%[src])                  \n\t"                  \
        "lbu       %[tmp2],  1(%[src])                  \n\t"                  \
        "lbu       %[tmp3],  2(%[src])                  \n\t"                  \
        "lbu       %[tmp4],  3(%[src])                  \n\t"                  \
        "addiu     %[src],   %[src],      4             \n\t"                  \
      ".endif                                           \n\t"                  \
      ".ifc        "#TYPE",  uint32_t                   \n\t"                  \
        "lw        %[tmp1],  0(%[src])                  \n\t"                  \
        "lw        %[tmp2],  4(%[src])                  \n\t"                  \
        "lw        %[tmp3],  8(%[src])                  \n\t"                  \
        "lw        %[tmp4],  12(%[src])                 \n\t"                  \
        "ext       %[tmp1],  %[tmp1],     8,        8   \n\t"                  \
        "ext       %[tmp2],  %[tmp2],     8,        8   \n\t"                  \
        "ext       %[tmp3],  %[tmp3],     8,        8   \n\t"                  \
        "ext       %[tmp4],  %[tmp4],     8,        8   \n\t"                  \
        "addiu     %[src],   %[src],      16            \n\t"                  \
      ".endif                                           \n\t"                  \
        "sll       %[tmp1],  %[tmp1],     2             \n\t"                  \
        "sll       %[tmp2],  %[tmp2],     2             \n\t"                  \
        "sll       %[tmp3],  %[tmp3],     2             \n\t"                  \
        "sll       %[tmp4],  %[tmp4],     2             \n\t"                  \
        "lwx       %[tmp1],  %[tmp1](%[color_map])      \n\t"                  \
        "lwx       %[tmp2],  %[tmp2](%[color_map])      \n\t"                  \
        "lwx       %[tmp3],  %[tmp3](%[color_map])      \n\t"                  \
        "lwx       %[tmp4],  %[tmp4](%[color_map])      \n\t"                  \
      ".ifc        "#TYPE",  uint8_t                    \n\t"                  \
        "ext       %[tmp1],  %[tmp1],     8,        8   \n\t"                  \
        "ext       %[tmp2],  %[tmp2],     8,        8   \n\t"                  \
        "ext       %[tmp3],  %[tmp3],     8,        8   \n\t"                  \
        "ext       %[tmp4],  %[tmp4],     8,        8   \n\t"                  \
        "sb        %[tmp1],  0(%[dst])                  \n\t"                  \
        "sb        %[tmp2],  1(%[dst])                  \n\t"                  \
        "sb        %[tmp3],  2(%[dst])                  \n\t"                  \
        "sb        %[tmp4],  3(%[dst])                  \n\t"                  \
        "addiu     %[dst],   %[dst],      4             \n\t"                  \
      ".endif                                           \n\t"                  \
      ".ifc        "#TYPE",  uint32_t                   \n\t"                  \
        "sw        %[tmp1],  0(%[dst])                  \n\t"                  \
        "sw        %[tmp2],  4(%[dst])                  \n\t"                  \
        "sw        %[tmp3],  8(%[dst])                  \n\t"                  \
        "sw        %[tmp4],  12(%[dst])                 \n\t"                  \
        "addiu     %[dst],   %[dst],      16            \n\t"                  \
      ".endif                                           \n\t"                  \
        : [tmp1]"=&r"(tmp1), [tmp2]"=&r"(tmp2), [tmp3]"=&r"(tmp3),             \
          [tmp4]"=&r"(tmp4), [src]"+&r"(src), [dst]"+r"(dst)                   \
        : [color_map]"r"(color_map)                                            \
        : "memory"                                                             \
      );                                                                       \
    }                                                                          \
    for (x = 0; x < (width & 3); ++x) {                                        \
      *dst++ = GET_VALUE(color_map[GET_INDEX(*src++)]);                        \
    }                                                                          \
  }                                                                            \
}

MAP_COLOR_FUNCS(MapARGB, uint32_t, VP8GetARGBIndex, VP8GetARGBValue)
MAP_COLOR_FUNCS(MapAlpha, uint8_t, VP8GetAlphaIndex, VP8GetAlphaValue)

#undef MAP_COLOR_FUNCS

static WEBP_INLINE uint32_t ClampedAddSubtractHalf(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
  __asm__ volatile (
   "adduh.qb         %[tmp5], %[c0],   %[c1]       \n\t"
   "preceu.ph.qbr    %[tmp3], %[c2]                \n\t"
   "preceu.ph.qbr    %[tmp1], %[tmp5]              \n\t"
   "preceu.ph.qbl    %[tmp2], %[tmp5]              \n\t"
   "preceu.ph.qbl    %[tmp4], %[c2]                \n\t"
   "subq.ph          %[tmp3], %[tmp1], %[tmp3]     \n\t"
   "subq.ph          %[tmp4], %[tmp2], %[tmp4]     \n\t"
   "shrl.ph          %[tmp5], %[tmp3], 15          \n\t"
   "shrl.ph          %[tmp6], %[tmp4], 15          \n\t"
   "addq.ph          %[tmp3], %[tmp3], %[tmp5]     \n\t"
   "addq.ph          %[tmp4], %[tmp6], %[tmp4]     \n\t"
   "shra.ph          %[tmp3], %[tmp3], 1           \n\t"
   "shra.ph          %[tmp4], %[tmp4], 1           \n\t"
   "addq.ph          %[tmp1], %[tmp1], %[tmp3]     \n\t"
   "addq.ph          %[tmp2], %[tmp2], %[tmp4]     \n\t"
   "shll_s.ph        %[tmp1], %[tmp1], 7           \n\t"
   "shll_s.ph        %[tmp2], %[tmp2], 7           \n\t"
   "precrqu_s.qb.ph  %[tmp1], %[tmp2], %[tmp1]     \n\t"
   : [tmp1]"=&r"(tmp1), [tmp2]"=&r"(tmp2), [tmp3]"=&r"(tmp3),
     [tmp4]"=&r"(tmp4), [tmp5]"=&r"(tmp5), [tmp6]"=r"(tmp6)
   : [c0]"r"(c0), [c1]"r"(c1), [c2]"r"(c2)
   : "memory"
  );
  return tmp1;
}

static uint32_t Predictor13(uint32_t left, const uint32_t* const top) {
  return ClampedAddSubtractHalf(left, top[0], top[-1]);
}

#endif  // WEBP_USE_MIPS_DSP_R2

//------------------------------------------------------------------------------

extern void VP8LDspInitMIPSdspR2(void);

void VP8LDspInitMIPSdspR2(void) {
#if defined(WEBP_USE_MIPS_DSP_R2)
  VP8LMapColor32b = MapARGB;
  VP8LMapColor8b = MapAlpha;
  VP8LPredictors[13] = Predictor13;
#endif  // WEBP_USE_MIPS_DSP_R2
}

//------------------------------------------------------------------------------
