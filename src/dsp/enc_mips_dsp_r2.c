// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of speed-critical encoding functions.
//
// Author(s): Darko Laus (darko.laus@imgtec.com)
//            Mirko Raus (mirko.raus@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "./mips_macro.h"
#include "../enc/cost.h"
#include "../enc/vp8enci.h"

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;

#define LOAD_REF(O0, O1, O2, O3)                                               \
  "ulw              %["#O0"],  0(%[ref])                      \n\t"            \
  "ulw              %["#O1"],  16(%[ref])                     \n\t"            \
  "ulw              %["#O2"],  32(%[ref])                     \n\t"            \
  "ulw              %["#O3"],  48(%[ref])                     \n\t"

// temp0[31..16 | 15..0] = temp0[31..16 | 15..0] + temp8[31..16 | 15..0]
// temp0[31..16 | 15..0] = temp0[31..16 <<(s) 7 | 15..0 <<(s) 7]
// temp1..temp7 same as temp0
// precrqu_s.qb.ph temp0, temp1, temp0:
//   temp0 = temp1[31..24] | temp1[15..8] | temp0[31..24] | temp0[15..8]
// store temp0 to dst
// IO - input/output
// I - input (macro doesn't change it)
#define STORE_SAT_SUM_X2(IO0, IO1, IO2, IO3, IO4, IO5, IO6, IO7,               \
                         I0, I1, I2, I3, I4, I5, I6, I7)                       \
  "addq.ph          %["#IO0"],  %["#IO0"],  %["#I0"]          \n\t"            \
  "addq.ph          %["#IO1"],  %["#IO1"],  %["#I1"]          \n\t"            \
  "addq.ph          %["#IO2"],  %["#IO2"],  %["#I2"]          \n\t"            \
  "addq.ph          %["#IO3"],  %["#IO3"],  %["#I3"]          \n\t"            \
  "addq.ph          %["#IO4"],  %["#IO4"],  %["#I4"]          \n\t"            \
  "addq.ph          %["#IO5"],  %["#IO5"],  %["#I5"]          \n\t"            \
  "addq.ph          %["#IO6"],  %["#IO6"],  %["#I6"]          \n\t"            \
  "addq.ph          %["#IO7"],  %["#IO7"],  %["#I7"]          \n\t"            \
  "shll_s.ph        %["#IO0"],  %["#IO0"],  7                 \n\t"            \
  "shll_s.ph        %["#IO1"],  %["#IO1"],  7                 \n\t"            \
  "shll_s.ph        %["#IO2"],  %["#IO2"],  7                 \n\t"            \
  "shll_s.ph        %["#IO3"],  %["#IO3"],  7                 \n\t"            \
  "shll_s.ph        %["#IO4"],  %["#IO4"],  7                 \n\t"            \
  "shll_s.ph        %["#IO5"],  %["#IO5"],  7                 \n\t"            \
  "shll_s.ph        %["#IO6"],  %["#IO6"],  7                 \n\t"            \
  "shll_s.ph        %["#IO7"],  %["#IO7"],  7                 \n\t"            \
  "precrqu_s.qb.ph  %["#IO0"],  %["#IO1"],  %["#IO0"]         \n\t"            \
  "precrqu_s.qb.ph  %["#IO2"],  %["#IO3"],  %["#IO2"]         \n\t"            \
  "precrqu_s.qb.ph  %["#IO4"],  %["#IO5"],  %["#IO4"]         \n\t"            \
  "precrqu_s.qb.ph  %["#IO6"],  %["#IO7"],  %["#IO6"]         \n\t"            \
  "usw              %["#IO0"],  0(%[dst])                     \n\t"            \
  "usw              %["#IO2"],  16(%[dst])                    \n\t"            \
  "usw              %["#IO4"],  32(%[dst])                    \n\t"            \
  "usw              %["#IO6"],  48(%[dst])                    \n\t"

static WEBP_INLINE void ITransformOne(const uint8_t* ref, const int16_t* in,
                                      uint8_t* dst) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15, temp16, temp17, temp18;

  __asm__ volatile (
    "ulw              %[temp1],   0(%[in])                 \n\t"
    "ulw              %[temp2],   16(%[in])                \n\t"
    LOAD_IN_X2(temp5, temp6, 24, 26)
    ADD_SUB_HALVES(temp3, temp4, temp1, temp2)
    LOAD_IN_X2(temp1, temp2, 8, 10)
    MUL_SHIFT_SUM(temp7, temp8, temp9, temp10, temp11, temp12, temp13, temp14,
                  temp10, temp8, temp9, temp7, temp1, temp2, temp5, temp6,
                  temp13, temp11, temp14, temp12)
    INSERT_HALF_X2(temp8, temp7, temp10, temp9)
    "ulw              %[temp17],  4(%[in])                 \n\t"
    "ulw              %[temp18],  20(%[in])                \n\t"
    ADD_SUB_HALVES(temp1, temp2, temp3, temp8)
    ADD_SUB_HALVES(temp5, temp6, temp4, temp7)
    ADD_SUB_HALVES(temp7, temp8, temp17, temp18)
    LOAD_IN_X2(temp17, temp18, 12, 14)
    LOAD_IN_X2(temp9, temp10, 28, 30)
    MUL_SHIFT_SUM(temp11, temp12, temp13, temp14, temp15, temp16, temp4, temp17,
                  temp12, temp14, temp11, temp13, temp17, temp18, temp9, temp10,
                  temp15, temp4, temp16, temp17)
    INSERT_HALF_X2(temp11, temp12, temp13, temp14)
    ADD_SUB_HALVES(temp17, temp8, temp8, temp11)
    ADD_SUB_HALVES(temp3, temp4, temp7, temp12)

    // horizontal
    SRA_16(temp9, temp10, temp11, temp12, temp1, temp2, temp5, temp6)
    INSERT_HALF_X2(temp1, temp6, temp5, temp2)
    SRA_16(temp13, temp14, temp15, temp16, temp3, temp4, temp17, temp8)
    "repl.ph          %[temp2],   0x4                      \n\t"
    INSERT_HALF_X2(temp3, temp8, temp17, temp4)
    "addq.ph          %[temp1],   %[temp1],  %[temp2]      \n\t"
    "addq.ph          %[temp6],   %[temp6],  %[temp2]      \n\t"
    ADD_SUB_HALVES(temp2, temp4, temp1, temp3)
    ADD_SUB_HALVES(temp5, temp7, temp6, temp8)
    MUL_SHIFT_SUM(temp1, temp3, temp6, temp8, temp9, temp13, temp17, temp18,
                  temp3, temp13, temp1, temp9, temp9, temp13, temp11, temp15,
                  temp6, temp17, temp8, temp18)
    MUL_SHIFT_SUM(temp6, temp8, temp18, temp17, temp11, temp15, temp12, temp16,
                  temp8, temp15, temp6, temp11, temp12, temp16, temp10, temp14,
                  temp18, temp12, temp17, temp16)
    INSERT_HALF_X2(temp1, temp3, temp9, temp13)
    INSERT_HALF_X2(temp6, temp8, temp11, temp15)
    SHIFT_R_SUM_X2(temp9, temp10, temp11, temp12, temp13, temp14, temp15,
                   temp16, temp2, temp4, temp5, temp7, temp3, temp1, temp8,
                   temp6)
    PACK_2_HALVES_TO_WORD(temp1, temp2, temp3, temp4, temp9, temp12, temp13,
                          temp16, temp11, temp10, temp15, temp14)
    LOAD_REF(temp10, temp11, temp14, temp15)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp17, temp18, temp10,
                            temp11, temp10, temp11, temp14, temp15)
    STORE_SAT_SUM_X2(temp5, temp6, temp7, temp8, temp17, temp18, temp10, temp11,
                     temp9, temp12, temp1, temp2, temp13, temp16, temp3, temp4)

    OUTPUT_EARLY_CLOBBER_REGS_18()
    : [dst]"r"(dst), [in]"r"(in), [kC1]"r"(kC1), [kC2]"r"(kC2), [ref]"r"(ref)
    : "memory", "hi", "lo"
  );
}

static void ITransform(const uint8_t* ref, const int16_t* in, uint8_t* dst,
                       int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

#undef LOAD_REF
#undef STORE_SAT_SUM_X2

#endif  // WEBP_USE_MIPS_DSP_R2

//------------------------------------------------------------------------------
// Entry point

extern WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMIPSdspR2(void) {
#if defined(WEBP_USE_MIPS_DSP_R2)
  VP8ITransform = ITransform;
#endif  // WEBP_USE_MIPS_DSP_R2
}
