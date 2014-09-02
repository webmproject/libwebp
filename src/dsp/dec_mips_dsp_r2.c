// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of dsp functions
//
// Author(s):  Djordje Pesut    (djordje.pesut@imgtec.com)
//             Jovan Zelincevic (jovan.zelincevic@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

static void TransformDC(const int16_t* in, uint8_t* dst) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9, temp10;

  __asm__ volatile (
    "ulw              %[temp1],  0(%[dst])              \n\t"
    "ulw              %[temp2],  32(%[dst])             \n\t"
    "ulw              %[temp3],  64(%[dst])             \n\t"
    "ulw              %[temp4],  96(%[dst])             \n\t"
    "lh               %[temp5],  0(%[in])               \n\t"
    "addiu            %[temp5],  %[temp5],  4           \n\t"
    "ins              %[temp5],  %[temp5],  16, 16      \n\t"
    "shra.ph          %[temp5],  %[temp5],  3           \n\t"
    "preceu.ph.qbr    %[temp6],  %[temp1]               \n\t"
    "preceu.ph.qbl    %[temp7],  %[temp1]               \n\t"
    "preceu.ph.qbr    %[temp8],  %[temp2]               \n\t"
    "preceu.ph.qbl    %[temp9],  %[temp2]               \n\t"
    "preceu.ph.qbr    %[temp10], %[temp3]               \n\t"
    "preceu.ph.qbl    %[temp1],  %[temp3]               \n\t"
    "preceu.ph.qbr    %[temp2],  %[temp4]               \n\t"
    "preceu.ph.qbl    %[temp3],  %[temp4]               \n\t"
    "addq.ph          %[temp6],  %[temp6],  %[temp5]    \n\t"
    "addq.ph          %[temp7],  %[temp7],  %[temp5]    \n\t"
    "addq.ph          %[temp8],  %[temp8],  %[temp5]    \n\t"
    "addq.ph          %[temp9],  %[temp9],  %[temp5]    \n\t"
    "addq.ph          %[temp10], %[temp10], %[temp5]    \n\t"
    "addq.ph          %[temp1],  %[temp1],  %[temp5]    \n\t"
    "addq.ph          %[temp2],  %[temp2],  %[temp5]    \n\t"
    "addq.ph          %[temp3],  %[temp3],  %[temp5]    \n\t"
    "shll_s.ph        %[temp6],  %[temp6],  7           \n\t"
    "shll_s.ph        %[temp7],  %[temp7],  7           \n\t"
    "shll_s.ph        %[temp8],  %[temp8],  7           \n\t"
    "shll_s.ph        %[temp9],  %[temp9],  7           \n\t"
    "shll_s.ph        %[temp10], %[temp10], 7           \n\t"
    "shll_s.ph        %[temp1],  %[temp1],  7           \n\t"
    "shll_s.ph        %[temp2],  %[temp2],  7           \n\t"
    "shll_s.ph        %[temp3],  %[temp3],  7           \n\t"
    "precrqu_s.qb.ph  %[temp6],  %[temp7],  %[temp6]    \n\t"
    "precrqu_s.qb.ph  %[temp8],  %[temp9],  %[temp8]    \n\t"
    "precrqu_s.qb.ph  %[temp10], %[temp1],  %[temp10]   \n\t"
    "precrqu_s.qb.ph  %[temp2],  %[temp3],  %[temp2]    \n\t"
    "usw              %[temp6],  0(%[dst])              \n\t"
    "usw              %[temp8],  32(%[dst])             \n\t"
    "usw              %[temp10], 64(%[dst])             \n\t"
    "usw              %[temp2],  96(%[dst])             \n\t"
    : [temp1]"=&r"(temp1), [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),
      [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),
      [temp7]"=&r"(temp7), [temp8]"=&r"(temp8), [temp9]"=&r"(temp9),
      [temp10]"=&r"(temp10)
    : [in]"r"(in), [dst]"r"(dst)
    : "memory"
  );
}

#endif  // WEBP_USE_MIPS_DSP_R2

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitMIPSdspR2(void);

void VP8DspInitMIPSdspR2(void) {
#if defined(WEBP_USE_MIPS_DSP_R2)
  VP8TransformDC = TransformDC;
#endif
}
