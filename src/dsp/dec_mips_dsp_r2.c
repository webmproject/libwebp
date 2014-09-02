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

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;

#define MUL(a, b) (((a) * (b)) >> 16)

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

static void TransformAC3(const int16_t* in, uint8_t* dst) {
  const int a = in[0] + 4;
  int c4 = MUL(in[4], kC2);
  const int d4 = MUL(in[4], kC1);
  const int c1 = MUL(in[1], kC2);
  const int d1 = MUL(in[1], kC1);
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15, temp16, temp17, temp18;

  __asm__ volatile (
    "ins              %[c4],      %[d4],     16,       16    \n\t"
    "replv.ph         %[temp1],   %[a]                       \n\t"
    "replv.ph         %[temp4],   %[d1]                      \n\t"
    "addq.ph          %[temp2],   %[temp1],  %[c4]           \n\t"
    "subq.ph          %[temp3],   %[temp1],  %[c4]           \n\t"
    "replv.ph         %[temp5],   %[c1]                      \n\t"
    "addq.ph          %[temp1],   %[temp2],  %[temp4]        \n\t"
    "subq.ph          %[temp6],   %[temp2],  %[temp4]        \n\t"
    "addq.ph          %[temp7],   %[temp2],  %[temp5]        \n\t"
    "subq.ph          %[temp8],   %[temp2],  %[temp5]        \n\t"
    "addq.ph          %[temp2],   %[temp3],  %[temp4]        \n\t"
    "subq.ph          %[temp9],   %[temp3],  %[temp4]        \n\t"
    "addq.ph          %[temp10],  %[temp3],  %[temp5]        \n\t"
    "subq.ph          %[temp4],   %[temp3],  %[temp5]        \n\t"
    "shra.ph          %[temp1],   %[temp1],  3               \n\t"
    "shra.ph          %[temp6],   %[temp6],  3               \n\t"
    "shra.ph          %[temp7],   %[temp7],  3               \n\t"
    "shra.ph          %[temp8],   %[temp8],  3               \n\t"
    "shra.ph          %[temp2],   %[temp2],  3               \n\t"
    "shra.ph          %[temp9],   %[temp9],  3               \n\t"
    "shra.ph          %[temp10],  %[temp10], 3               \n\t"
    "shra.ph          %[temp4],   %[temp4],  3               \n\t"
    "ulw              %[temp3],   0(%[dst])                  \n\t"
    "ulw              %[temp5],   32(%[dst])                 \n\t"
    "ulw              %[temp11],  64(%[dst])                 \n\t"
    "ulw              %[temp12],  96(%[dst])                 \n\t"
    "preceu.ph.qbr    %[temp13],  %[temp3]                   \n\t"
    "preceu.ph.qbl    %[temp14],  %[temp3]                   \n\t"
    "preceu.ph.qbr    %[temp3],   %[temp5]                   \n\t"
    "preceu.ph.qbl    %[temp15],  %[temp5]                   \n\t"
    "preceu.ph.qbr    %[temp5],   %[temp11]                  \n\t"
    "preceu.ph.qbl    %[temp16],  %[temp11]                  \n\t"
    "preceu.ph.qbr    %[temp11],  %[temp12]                  \n\t"
    "preceu.ph.qbl    %[temp17],  %[temp12]                  \n\t"
    "precrq.ph.w      %[temp12],  %[temp7],  %[temp1]        \n\t"
    "precrq.ph.w      %[temp18],  %[temp6],  %[temp8]        \n\t"
    "ins              %[temp1],   %[temp7],  16,       16    \n\t"
    "ins              %[temp8],   %[temp6],  16,       16    \n\t"
    "precrq.ph.w      %[temp7],   %[temp10], %[temp2]        \n\t"
    "precrq.ph.w      %[temp6],   %[temp9],  %[temp4]        \n\t"
    "ins              %[temp2],   %[temp10], 16,       16    \n\t"
    "ins              %[temp4],   %[temp9],  16,       16    \n\t"
    "addq.ph          %[temp13],  %[temp13], %[temp12]       \n\t"
    "addq.ph          %[temp14],  %[temp14], %[temp18]       \n\t"
    "addq.ph          %[temp3],   %[temp3],  %[temp1]        \n\t"
    "addq.ph          %[temp15],  %[temp15], %[temp8]        \n\t"
    "addq.ph          %[temp5],   %[temp5],  %[temp2]        \n\t"
    "addq.ph          %[temp16],  %[temp16], %[temp4]        \n\t"
    "addq.ph          %[temp11],  %[temp11], %[temp7]        \n\t"
    "addq.ph          %[temp17],  %[temp17], %[temp6]        \n\t"
    "shll_s.ph        %[temp13],  %[temp13], 7               \n\t"
    "shll_s.ph        %[temp14],  %[temp14], 7               \n\t"
    "shll_s.ph        %[temp3],   %[temp3],  7               \n\t"
    "shll_s.ph        %[temp15],  %[temp15], 7               \n\t"
    "shll_s.ph        %[temp5],   %[temp5],  7               \n\t"
    "shll_s.ph        %[temp16],  %[temp16], 7               \n\t"
    "shll_s.ph        %[temp11],  %[temp11], 7               \n\t"
    "shll_s.ph        %[temp17],  %[temp17], 7               \n\t"
    "precrqu_s.qb.ph  %[temp13],  %[temp14], %[temp13]       \n\t"
    "precrqu_s.qb.ph  %[temp3],   %[temp15], %[temp3]        \n\t"
    "precrqu_s.qb.ph  %[temp5],   %[temp16], %[temp5]        \n\t"
    "precrqu_s.qb.ph  %[temp11],  %[temp17], %[temp11]       \n\t"
    "usw              %[temp13],  0(%[dst])                  \n\t"
    "usw              %[temp3],   32(%[dst])                 \n\t"
    "usw              %[temp5],   64(%[dst])                 \n\t"
    "usw              %[temp11],  96(%[dst])                 \n\t"
    : [temp1]"=&r"(temp1), [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),
      [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),
      [temp7]"=&r"(temp7), [temp8]"=&r"(temp8), [temp9]"=&r"(temp9),
      [temp10]"=&r"(temp10), [temp11]"=&r"(temp11), [temp12]"=&r"(temp12),
      [temp13]"=&r"(temp13), [temp14]"=&r"(temp14), [temp15]"=&r"(temp15),
      [temp16]"=&r"(temp16), [temp17]"=&r"(temp17), [temp18]"=&r"(temp18),
      [c4]"+&r"(c4)
    : [dst]"r"(dst), [a]"r"(a), [d1]"r"(d1), [d4]"r"(d4), [c1]"r"(c1)
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
  VP8TransformAC3 = TransformAC3;
#endif
}
