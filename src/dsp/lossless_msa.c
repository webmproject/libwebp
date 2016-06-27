// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA variant of methods for lossless decoder
//
// Author: Prashant Patil (prashant.patil@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MSA)

#include "./lossless.h"
#include "./msa_macro.h"

//------------------------------------------------------------------------------
// Colorspace conversion functions

#define CONVERT16_BGRA_XXX(psrc, pdst, m0, m1, m2) do {    \
  v16u8 src0, src1, src2, src3, dst0, dst1, dst2;          \
  LD_UB4(psrc, 16, src0, src1, src2, src3);                \
  VSHF_B2_UB(src0, src1, src1, src2, m0, m1, dst0, dst1);  \
  dst2 = VSHF_UB(src2, src3, m2);                          \
  ST_UB2(dst0, dst1, pdst, 16);                            \
  ST_UB(dst2, pdst + 32);                                  \
} while (0)

#define CONVERT12_BGRA_XXX(psrc, pdst, m0, m1, m2) do {    \
  uint32_t pix_w;                                          \
  v16u8 src0, src1, src2, dst0, dst1, dst2;                \
  LD_UB3(psrc, 16, src0, src1, src2);                      \
  VSHF_B2_UB(src0, src1, src1, src2, m0, m1, dst0, dst1);  \
  dst2 = VSHF_UB(src2, src2, m2);                          \
  ST_UB2(dst0, dst1, pdst, 16);                            \
  pix_w = __msa_copy_s_w((v4i32)dst2, 0);                  \
  SW(pix_w, pdst + 32);                                    \
} while (0)

#define CONVERT8_BGRA_XXX(psrc, pdst, m0, m1) do {         \
  uint64_t pix_d;                                          \
  v16u8 src0, src1, src2, dst0, dst1;                      \
  LD_UB2(psrc, 16, src0, src1);                            \
  VSHF_B2_UB(src0, src1, src1, src2, m0, m1, dst0, dst1);  \
  ST_UB(dst0, pdst);                                       \
  pix_d = __msa_copy_s_d((v2i64)dst1, 0);                  \
  SD(pix_d, pdst + 16);                                    \
} while (0)

#define CONVERT4_BGRA_XXX(psrc, pdst, m) do {       \
  const v16u8 src0 = LD_UB(psrc);                   \
  const v16u8 dst0 = VSHF_UB(src0, src0, m);        \
  uint64_t pix_d = __msa_copy_s_d((v2i64)dst0, 0);  \
  uint32_t pix_w = __msa_copy_s_w((v4i32)dst0, 2);  \
  SD(pix_d, pdst + 0);                              \
  SW(pix_w, pdst + 8);                              \
} while (0)

#define CONVERT1_BGRA_BGR(psrc, pdst) do {  \
  const int32_t b = (psrc)[0];              \
  const int32_t g = (psrc)[1];              \
  const int32_t r = (psrc)[2];              \
  (pdst)[0] = b;                            \
  (pdst)[1] = g;                            \
  (pdst)[2] = r;                            \
} while (0)

#define CONVERT1_BGRA_RGB(psrc, pdst) do {  \
  const int32_t b = (psrc)[0];              \
  const int32_t g = (psrc)[1];              \
  const int32_t r = (psrc)[2];              \
  (pdst)[0] = r;                            \
  (pdst)[1] = g;                            \
  (pdst)[2] = b;                            \
} while (0)

static void ConvertBGRAToRGBA(const uint32_t* src,
                              int num_pixels, uint8_t* dst) {
  int i;
  const uint8_t* ptemp_src = (const uint8_t*)src;
  uint8_t* ptemp_dst = (uint8_t*)dst;
  v16u8 src0, dst0;
  const v16u8 mask = { 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15 };

  while (num_pixels >= 8) {
    v16u8 src1, dst1;
    LD_UB2(ptemp_src, 16, src0, src1);
    VSHF_B2_UB(src0, src0, src1, src1, mask, mask, dst0, dst1);
    ST_UB2(dst0, dst1, ptemp_dst, 16);
    ptemp_src += 32;
    ptemp_dst += 32;
    num_pixels -= 8;
  }
  if (num_pixels > 0) {
    if (num_pixels >= 4) {
      src0 = LD_UB(ptemp_src);
      dst0 = VSHF_UB(src0, src0, mask);
      ST_UB(dst0, ptemp_dst);
      ptemp_src += 16;
      ptemp_dst += 16;
      num_pixels -= 4;
    }
    for (i = 0; i < num_pixels; i++) {
      const uint8_t b = ptemp_src[2];
      const uint8_t g = ptemp_src[1];
      const uint8_t r = ptemp_src[0];
      const uint8_t a = ptemp_src[3];
      ptemp_dst[0] = b;
      ptemp_dst[1] = g;
      ptemp_dst[2] = r;
      ptemp_dst[3] = a;
      ptemp_src += 4;
      ptemp_dst += 4;
    }
  }
}

static void ConvertBGRAToBGR(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint8_t* ptemp_src = (const uint8_t*)src;
  uint8_t* ptemp_dst = (uint8_t*)dst;
  const v16u8 mask0 = { 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14,
                        16, 17, 18, 20 };
  const v16u8 mask1 = { 5, 6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 20,
                        21, 22, 24, 25 };
  const v16u8 mask2 = { 10, 12, 13, 14, 16, 17, 18, 20, 21, 22, 24, 25,
                        26, 28, 29, 30 };

  while (num_pixels >= 16) {
    CONVERT16_BGRA_XXX(ptemp_src, ptemp_dst, mask0, mask1, mask2);
    ptemp_src += 64;
    ptemp_dst += 48;
    num_pixels -= 16;
  }
  if (num_pixels > 0) {
    if (num_pixels >= 12) {
      CONVERT12_BGRA_XXX(ptemp_src, ptemp_dst, mask0, mask1, mask2);
      ptemp_src += 48;
      ptemp_dst += 36;
      num_pixels -= 12;
    } else if (num_pixels >= 8) {
      CONVERT8_BGRA_XXX(ptemp_src, ptemp_dst, mask0, mask1);
      ptemp_src += 32;
      ptemp_dst += 24;
      num_pixels -= 8;
    } else if (num_pixels >= 4) {
      CONVERT4_BGRA_XXX(ptemp_src, ptemp_dst, mask0);
      ptemp_src += 16;
      ptemp_dst += 12;
      num_pixels -= 4;
    }
    if (num_pixels == 3) {
      CONVERT1_BGRA_BGR(ptemp_src + 0, ptemp_dst + 0);
      CONVERT1_BGRA_BGR(ptemp_src + 4, ptemp_dst + 3);
      CONVERT1_BGRA_BGR(ptemp_src + 8, ptemp_dst + 6);
    } else if (num_pixels == 2) {
      CONVERT1_BGRA_BGR(ptemp_src + 0, ptemp_dst + 0);
      CONVERT1_BGRA_BGR(ptemp_src + 4, ptemp_dst + 3);
    } else if (num_pixels == 1) {
      CONVERT1_BGRA_BGR(ptemp_src, ptemp_dst);
    }
  }
}

static void ConvertBGRAToRGB(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint8_t* ptemp_src = (const uint8_t*)src;
  uint8_t* ptemp_dst = (uint8_t*)dst;
  const v16u8 mask0 = { 2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12,
                        18, 17, 16, 22 };
  const v16u8 mask1 = { 5, 4, 10, 9, 8, 14, 13, 12, 18, 17, 16, 22,
                        21, 20, 26, 25 };
  const v16u8 mask2 = { 8, 14, 13, 12, 18, 17, 16, 22, 21, 20, 26, 25,
                        24, 30, 29, 28 };

  while (num_pixels >= 16) {
    CONVERT16_BGRA_XXX(ptemp_src, ptemp_dst, mask0, mask1, mask2);
    ptemp_src += 64;
    ptemp_dst += 48;
    num_pixels -= 16;
  }
  if (num_pixels) {
    if (num_pixels >= 12) {
      CONVERT12_BGRA_XXX(ptemp_src, ptemp_dst, mask0, mask1, mask2);
      ptemp_src += 48;
      ptemp_dst += 36;
      num_pixels -= 12;
    } else if (num_pixels >= 8) {
      CONVERT8_BGRA_XXX(ptemp_src, ptemp_dst, mask0, mask1);
      ptemp_src += 32;
      ptemp_dst += 24;
      num_pixels -= 8;
    } else if (num_pixels >= 4) {
      CONVERT4_BGRA_XXX(ptemp_src, ptemp_dst, mask0);
      ptemp_src += 16;
      ptemp_dst += 12;
      num_pixels -= 4;
    }
    if (num_pixels == 3) {
      CONVERT1_BGRA_RGB(ptemp_src + 0, ptemp_dst + 0);
      CONVERT1_BGRA_RGB(ptemp_src + 4, ptemp_dst + 3);
      CONVERT1_BGRA_RGB(ptemp_src + 8, ptemp_dst + 6);
    } else if (num_pixels == 2) {
      CONVERT1_BGRA_RGB(ptemp_src + 0, ptemp_dst + 0);
      CONVERT1_BGRA_RGB(ptemp_src + 4, ptemp_dst + 3);
    } else if (num_pixels == 1) {
      CONVERT1_BGRA_RGB(ptemp_src, ptemp_dst);
    }
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LDspInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitMSA(void) {
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8LDspInitMSA)

#endif  // WEBP_USE_MSA
