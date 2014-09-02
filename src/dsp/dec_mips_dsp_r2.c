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
  "usw              %["#IO2"],  32(%[dst])                    \n\t"            \
  "usw              %["#IO4"],  64(%[dst])                    \n\t"            \
  "usw              %["#IO6"],  96(%[dst])                    \n\t"

// temp0[31..16 | 15..0] = temp8[31..16 | 15..0] + temp12[31..16 | 15..0]
// temp1[31..16 | 15..0] = temp8[31..16 | 15..0] - temp12[31..16 | 15..0]
// temp0[31..16 | 15..0] = temp0[31..16 >> 3 | 15..0 >> 3]
// temp1[31..16 | 15..0] = temp1[31..16 >> 3 | 15..0 >> 3]
// O - output
// I - input (macro doesn't change it)
#define SHIFT_R_SUM_X2(O0, O1, O2, O3, O4, O5, O6, O7,                         \
                       I0, I1, I2, I3, I4, I5, I6, I7)                         \
  "addq.ph          %["#O0"],   %["#I0"],   %["#I4"]          \n\t"            \
  "subq.ph          %["#O1"],   %["#I0"],   %["#I4"]          \n\t"            \
  "addq.ph          %["#O2"],   %["#I1"],   %["#I5"]          \n\t"            \
  "subq.ph          %["#O3"],   %["#I1"],   %["#I5"]          \n\t"            \
  "addq.ph          %["#O4"],   %["#I2"],   %["#I6"]          \n\t"            \
  "subq.ph          %["#O5"],   %["#I2"],   %["#I6"]          \n\t"            \
  "addq.ph          %["#O6"],   %["#I3"],   %["#I7"]          \n\t"            \
  "subq.ph          %["#O7"],   %["#I3"],   %["#I7"]          \n\t"            \
  "shra.ph          %["#O0"],   %["#O0"],   3                 \n\t"            \
  "shra.ph          %["#O1"],   %["#O1"],   3                 \n\t"            \
  "shra.ph          %["#O2"],   %["#O2"],   3                 \n\t"            \
  "shra.ph          %["#O3"],   %["#O3"],   3                 \n\t"            \
  "shra.ph          %["#O4"],   %["#O4"],   3                 \n\t"            \
  "shra.ph          %["#O5"],   %["#O5"],   3                 \n\t"            \
  "shra.ph          %["#O6"],   %["#O6"],   3                 \n\t"            \
  "shra.ph          %["#O7"],   %["#O7"],   3                 \n\t"

// preceu.ph.qbr temp0, temp8
//   temp0 = 0 | 0 | temp8[23..16] | temp8[7..0]
// preceu.ph.qbl temp1, temp8
//   temp1 = temp8[23..16] | temp8[7..0] | 0 | 0
// O - output
// I - input (macro doesn't change it)
#define CONVERT_2_BYTES_TO_HALF(O0, O1, O2, O3, O4, O5, O6, O7,                \
                                I0, I1, I2, I3)                                \
  "preceu.ph.qbr    %["#O0"],   %["#I0"]                      \n\t"            \
  "preceu.ph.qbl    %["#O1"],   %["#I0"]                      \n\t"            \
  "preceu.ph.qbr    %["#O2"],   %["#I1"]                      \n\t"            \
  "preceu.ph.qbl    %["#O3"],   %["#I1"]                      \n\t"            \
  "preceu.ph.qbr    %["#O4"],   %["#I2"]                      \n\t"            \
  "preceu.ph.qbl    %["#O5"],   %["#I2"]                      \n\t"            \
  "preceu.ph.qbr    %["#O6"],   %["#I3"]                      \n\t"            \
  "preceu.ph.qbl    %["#O7"],   %["#I3"]                      \n\t"

// O - output
#define LOAD_DST(O0, O1, O2, O3)                                               \
  "ulw              %["#O0"],  0(%[dst])                      \n\t"            \
  "ulw              %["#O1"],  32(%[dst])                     \n\t"            \
  "ulw              %["#O2"],  64(%[dst])                     \n\t"            \
  "ulw              %["#O3"],  96(%[dst])                     \n\t"

// precrq.ph.w temp0, temp8, temp2
//   temp0 = temp8[31..16] | temp2[31..16]
// ins temp2, temp8, 16, 16
//   temp2 = temp8[31..16] | temp2[15..0]
// O - output
// IO - input/output
// I - input (macro doesn't change it)
#define PACK_2_HALVES_TO_WORD(O0, O1, O2, O3,                                  \
                              IO0, IO1, IO2, IO3,                              \
                              I0, I1, I2, I3)                                  \
  "precrq.ph.w      %["#O0"],    %["#I0"],  %["#IO0"]         \n\t"            \
  "precrq.ph.w      %["#O1"],    %["#I1"],  %["#IO1"]         \n\t"            \
  "ins              %["#IO0"],   %["#I0"],  16,    16         \n\t"            \
  "ins              %["#IO1"],   %["#I1"],  16,    16         \n\t"            \
  "precrq.ph.w      %["#O2"],    %["#I2"],  %["#IO2"]         \n\t"            \
  "precrq.ph.w      %["#O3"],    %["#I3"],  %["#IO3"]         \n\t"            \
  "ins              %["#IO2"],   %["#I2"],  16,    16         \n\t"            \
  "ins              %["#IO3"],   %["#I3"],  16,    16         \n\t"

// O - output
// IO - input/output
// I - input (macro doesn't change it)
#define MUL_SHIFT_SUM(O0, O1, O2, O3, O4, O5, O6, O7,                          \
                      IO0, IO1, IO2, IO3,                                      \
                      I0, I1, I2, I3, I4, I5, I6, I7)                          \
  "mul              %["#O0"],   %["#I0"],   %[kC2]            \n\t"            \
  "mul              %["#O1"],   %["#I0"],   %[kC1]            \n\t"            \
  "mul              %["#O2"],   %["#I1"],   %[kC2]            \n\t"            \
  "mul              %["#O3"],   %["#I1"],   %[kC1]            \n\t"            \
  "mul              %["#O4"],   %["#I2"],   %[kC2]            \n\t"            \
  "mul              %["#O5"],   %["#I2"],   %[kC1]            \n\t"            \
  "mul              %["#O6"],   %["#I3"],   %[kC2]            \n\t"            \
  "mul              %["#O7"],   %["#I3"],   %[kC1]            \n\t"            \
  "sra              %["#O0"],   %["#O0"],   16                \n\t"            \
  "sra              %["#O1"],   %["#O1"],   16                \n\t"            \
  "sra              %["#O2"],   %["#O2"],   16                \n\t"            \
  "sra              %["#O3"],   %["#O3"],   16                \n\t"            \
  "sra              %["#O4"],   %["#O4"],   16                \n\t"            \
  "sra              %["#O5"],   %["#O5"],   16                \n\t"            \
  "sra              %["#O6"],   %["#O6"],   16                \n\t"            \
  "sra              %["#O7"],   %["#O7"],   16                \n\t"            \
  "addu             %["#IO0"],  %["#IO0"],  %["#I4"]          \n\t"            \
  "addu             %["#IO1"],  %["#IO1"],  %["#I5"]          \n\t"            \
  "subu             %["#IO2"],  %["#IO2"],  %["#I6"]          \n\t"            \
  "subu             %["#IO3"],  %["#IO3"],  %["#I7"]          \n\t"

// O - output
// I - input (macro doesn't change it)
#define ADD_SUB_HALVES(O0, O1,                                                 \
                       I0, I1)                                                 \
  "addq.ph          %["#O0"],   %["#I0"],  %["#I1"]           \n\t"            \
  "subq.ph          %["#O1"],   %["#I0"],  %["#I1"]           \n\t"

// O - output
// I - input (macro doesn't change it)
// I[0/1] - offset in bytes
#define LOAD_IN_X2(O0, O1,                                                     \
                   I0, I1)                                                     \
  "lh               %["#O0"],   "#I0"(%[in])                  \n\t"            \
  "lh               %["#O1"],   "#I1"(%[in])                  \n\t"

// O - output
// I - input (macro doesn't change it)
#define SRA_16(O0, O1, O2, O3,                                                 \
               I0, I1, I2, I3)                                                 \
  "sra              %["#O0"],  %["#I0"],  16                  \n\t"            \
  "sra              %["#O1"],  %["#I1"],  16                  \n\t"            \
  "sra              %["#O2"],  %["#I2"],  16                  \n\t"            \
  "sra              %["#O3"],  %["#I3"],  16                  \n\t"

// O - output
// I - input (macro doesn't change it)
#define INSERT_HALF_X2(O0, O1,                                                 \
                       I0, I1)                                                 \
  "ins              %["#O0"],   %["#I0"], 16,    16           \n\t"            \
  "ins              %["#O1"],   %["#I1"], 16,    16           \n\t"

#define OUTPUT_EARLY_CLOBBER_REGS_10()                                         \
  : [temp1]"=&r"(temp1), [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),             \
    [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),             \
    [temp7]"=&r"(temp7), [temp8]"=&r"(temp8), [temp9]"=&r"(temp9),             \
    [temp10]"=&r"(temp10)

#define OUTPUT_EARLY_CLOBBER_REGS_18()                                         \
  OUTPUT_EARLY_CLOBBER_REGS_10(),                                              \
  [temp11]"=&r"(temp11), [temp12]"=&r"(temp12), [temp13]"=&r"(temp13),         \
  [temp14]"=&r"(temp14), [temp15]"=&r"(temp15), [temp16]"=&r"(temp16),         \
  [temp17]"=&r"(temp17), [temp18]"=&r"(temp18)

static void TransformDC(const int16_t* in, uint8_t* dst) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9, temp10;

  __asm__ volatile (
    LOAD_DST(temp1, temp2, temp3, temp4)
    "lh               %[temp5],  0(%[in])               \n\t"
    "addiu            %[temp5],  %[temp5],  4           \n\t"
    "ins              %[temp5],  %[temp5],  16, 16      \n\t"
    "shra.ph          %[temp5],  %[temp5],  3           \n\t"
    CONVERT_2_BYTES_TO_HALF(temp6, temp7, temp8, temp9, temp10, temp1, temp2,
                            temp3, temp1, temp2, temp3, temp4)
    STORE_SAT_SUM_X2(temp6, temp7, temp8, temp9, temp10, temp1, temp2, temp3,
                     temp5, temp5, temp5, temp5, temp5, temp5, temp5, temp5)

    OUTPUT_EARLY_CLOBBER_REGS_10()
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
    ADD_SUB_HALVES(temp2, temp3, temp1, c4)
    "replv.ph         %[temp5],   %[c1]                      \n\t"
    SHIFT_R_SUM_X2(temp1, temp6, temp7, temp8, temp2, temp9, temp10, temp4,
                   temp2, temp2, temp3, temp3, temp4, temp5, temp4, temp5)
    LOAD_DST(temp3, temp5, temp11, temp12)
    CONVERT_2_BYTES_TO_HALF(temp13, temp14, temp3, temp15, temp5, temp16,
                            temp11, temp17, temp3, temp5, temp11, temp12)
    PACK_2_HALVES_TO_WORD(temp12, temp18, temp7, temp6, temp1, temp8, temp2,
                          temp4, temp7, temp6, temp10, temp9)
    STORE_SAT_SUM_X2(temp13, temp14, temp3, temp15, temp5, temp16, temp11,
                     temp17, temp12, temp18, temp1, temp8, temp2, temp4,
                     temp7, temp6)

    OUTPUT_EARLY_CLOBBER_REGS_18(),
      [c4]"+&r"(c4)
    : [dst]"r"(dst), [a]"r"(a), [d1]"r"(d1), [d4]"r"(d4), [c1]"r"(c1)
    : "memory"
  );
}

static void TransformOne(const int16_t* in, uint8_t* dst) {
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
    LOAD_DST(temp10, temp11, temp14, temp15)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp17, temp18, temp10,
                            temp11, temp10, temp11, temp14, temp15)
    STORE_SAT_SUM_X2(temp5, temp6, temp7, temp8, temp17, temp18, temp10, temp11,
                     temp9, temp12, temp1, temp2, temp13, temp16, temp3, temp4)

    OUTPUT_EARLY_CLOBBER_REGS_18()
    : [dst]"r"(dst), [in]"r"(in), [kC1]"r"(kC1), [kC2]"r"(kC2)
    : "memory", "hi", "lo"
  );
}

static void TransformTwo(const int16_t* in, uint8_t* dst, int do_two) {
  TransformOne(in, dst);
  if (do_two) {
    TransformOne(in + 16, dst + 4);
  }
}

#undef OUTPUT_EARLY_CLOBBER_REGS_18
#undef OUTPUT_EARLY_CLOBBER_REGS_10
#undef INSERT_HALF_X2
#undef SRA_16
#undef LOAD_IN_X2
#undef ADD_SUB_HALVES
#undef MUL_SHIFT_SUM
#undef PACK_2_HALVES_TO_WORD
#undef LOAD_DST
#undef CONVERT_BYTES_TO_HALF
#undef SHIFT_R_SUM_X2
#undef STORE_SAT_SUM_X2
#undef MUL

#endif  // WEBP_USE_MIPS_DSP_R2

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitMIPSdspR2(void);

void VP8DspInitMIPSdspR2(void) {
#if defined(WEBP_USE_MIPS_DSP_R2)
  VP8TransformDC = TransformDC;
  VP8TransformAC3 = TransformAC3;
  VP8Transform = TransformTwo;
#endif
}
