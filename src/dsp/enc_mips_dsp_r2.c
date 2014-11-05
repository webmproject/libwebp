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

// O - output
// I - input (macro doesn't change it)
#define ADD_SUB_HALVES_X4(O0, O1, O2, O3, O4, O5, O6, O7,                      \
                          I0, I1, I2, I3, I4, I5, I6, I7)                      \
  "addq.ph          %["#O0"],   %["#I0"],  %["#I1"]           \n\t"            \
  "subq.ph          %["#O1"],   %["#I0"],  %["#I1"]           \n\t"            \
  "addq.ph          %["#O2"],   %["#I2"],  %["#I3"]           \n\t"            \
  "subq.ph          %["#O3"],   %["#I2"],  %["#I3"]           \n\t"            \
  "addq.ph          %["#O4"],   %["#I4"],  %["#I5"]           \n\t"            \
  "subq.ph          %["#O5"],   %["#I4"],  %["#I5"]           \n\t"            \
  "addq.ph          %["#O6"],   %["#I6"],  %["#I7"]           \n\t"            \
  "subq.ph          %["#O7"],   %["#I6"],  %["#I7"]           \n\t"

// IO - input/output
#define ABS_X8(IO0, IO1, IO2, IO3, IO4, IO5, IO6, IO7)                         \
  "absq_s.ph        %["#IO0"],   %["#IO0"]                    \n\t"            \
  "absq_s.ph        %["#IO1"],   %["#IO1"]                    \n\t"            \
  "absq_s.ph        %["#IO2"],   %["#IO2"]                    \n\t"            \
  "absq_s.ph        %["#IO3"],   %["#IO3"]                    \n\t"            \
  "absq_s.ph        %["#IO4"],   %["#IO4"]                    \n\t"            \
  "absq_s.ph        %["#IO5"],   %["#IO5"]                    \n\t"            \
  "absq_s.ph        %["#IO6"],   %["#IO6"]                    \n\t"            \
  "absq_s.ph        %["#IO7"],   %["#IO7"]                    \n\t"

// dpa.w.ph $ac0 temp0 ,temp1
//  $ac += temp0[31..16] * temp1[31..16] + temp0[15..0] * temp1[15..0]
// dpax.w.ph $ac0 temp0 ,temp1
//  $ac += temp0[31..16] * temp1[15..0] + temp0[15..0] * temp1[31..16]
// O - output
// I - input (macro doesn't change it)
#define MUL_HALF(O0, I0, I1, I2, I3, I4, I5, I6, I7,                           \
                 I8, I9, I10, I11, I12, I13, I14, I15)                         \
    "mult            $ac0,      $zero,     $zero              \n\t"            \
    "dpa.w.ph        $ac0,      %["#I2"],  %["#I0"]           \n\t"            \
    "dpax.w.ph       $ac0,      %["#I5"],  %["#I6"]           \n\t"            \
    "dpa.w.ph        $ac0,      %["#I8"],  %["#I9"]           \n\t"            \
    "dpax.w.ph       $ac0,      %["#I11"], %["#I4"]           \n\t"            \
    "dpa.w.ph        $ac0,      %["#I12"], %["#I7"]           \n\t"            \
    "dpax.w.ph       $ac0,      %["#I13"], %["#I1"]           \n\t"            \
    "dpa.w.ph        $ac0,      %["#I14"], %["#I3"]           \n\t"            \
    "dpax.w.ph       $ac0,      %["#I15"], %["#I10"]          \n\t"            \
    "mflo            %["#O0"],  $ac0                          \n\t"

#define OUTPUT_EARLY_CLOBBER_REGS_17()                                         \
  OUTPUT_EARLY_CLOBBER_REGS_10(),                                              \
  [temp11]"=&r"(temp11), [temp12]"=&r"(temp12), [temp13]"=&r"(temp13),         \
  [temp14]"=&r"(temp14), [temp15]"=&r"(temp15), [temp16]"=&r"(temp16),         \
  [temp17]"=&r"(temp17)

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
    LOAD_WITH_OFFSET_X4(temp10, temp11, temp14, temp15, ref, 0, 16, 32, 48)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp17, temp18, temp10,
                            temp11, temp10, temp11, temp14, temp15)
    STORE_SAT_SUM_X2(temp5, temp6, temp7, temp8, temp17, temp18, temp10, temp11,
                     temp9, temp12, temp1, temp2, temp13, temp16, temp3, temp4,
                     dst, 0, 16, 32, 48)

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

static int Disto4x4(const uint8_t* const a, const uint8_t* const b,
                    const uint16_t* const w) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15, temp16, temp17;

  __asm__ volatile (
    LOAD_WITH_OFFSET_X4(temp1, temp2, temp3, temp4, a, 0, 16, 32, 48)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp9,temp10, temp11,
                            temp12, temp1, temp2, temp3, temp4)
    ADD_SUB_HALVES_X4(temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
                      temp5, temp6, temp7, temp8, temp9, temp10, temp11, temp12)
    PACK_2_HALVES_TO_WORD(temp9, temp10, temp11, temp12, temp1, temp3, temp5,
                          temp7, temp2, temp4, temp6, temp8)
    ADD_SUB_HALVES_X4(temp2, temp4, temp6, temp8, temp9, temp1, temp3, temp10,
                      temp1, temp9, temp3, temp10, temp5, temp11, temp7, temp12)
    ADD_SUB_HALVES_X4(temp5, temp11, temp7, temp2, temp9, temp3, temp6, temp12,
                      temp2, temp9, temp6, temp3, temp4, temp1, temp8, temp10)
    ADD_SUB_HALVES_X4(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2,
                      temp5, temp7, temp11, temp2, temp9, temp6, temp3, temp12)
    ABS_X8(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2)
    LOAD_WITH_OFFSET_X4(temp3, temp6, temp9, temp12, w, 0, 4, 8, 12)
    LOAD_WITH_OFFSET_X4(temp13, temp14, temp15, temp16, w, 16, 20, 24, 28)
    MUL_HALF(temp17, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
             temp9, temp10, temp11, temp12, temp13, temp14, temp15, temp16)
    LOAD_WITH_OFFSET_X4(temp1, temp2, temp3, temp4, b, 0, 16, 32, 48)
    CONVERT_2_BYTES_TO_HALF(temp5,temp6, temp7, temp8, temp9,temp10, temp11,
                            temp12, temp1, temp2, temp3, temp4)
    ADD_SUB_HALVES_X4(temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
                      temp5, temp6, temp7, temp8, temp9, temp10, temp11, temp12)
    PACK_2_HALVES_TO_WORD(temp9, temp10, temp11, temp12, temp1, temp3, temp5,
                          temp7, temp2, temp4, temp6, temp8)
    ADD_SUB_HALVES_X4(temp2, temp4, temp6, temp8, temp9, temp1, temp3, temp10,
                      temp1, temp9, temp3, temp10, temp5, temp11, temp7, temp12)
    ADD_SUB_HALVES_X4(temp5, temp11, temp7, temp2, temp9, temp3, temp6, temp12,
                      temp2, temp9, temp6, temp3, temp4, temp1, temp8, temp10)
    ADD_SUB_HALVES_X4(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2,
                      temp5, temp7, temp11, temp2, temp9, temp6, temp3, temp12)
    ABS_X8(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2)
    LOAD_WITH_OFFSET_X4(temp3, temp6, temp9, temp12, w, 0, 4, 8, 12)
    LOAD_WITH_OFFSET_X4(temp13, temp14, temp15, temp16, w, 16, 20, 24, 28)
    MUL_HALF(temp3, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
             temp9, temp10, temp11, temp12, temp13, temp14, temp15, temp16)
    OUTPUT_EARLY_CLOBBER_REGS_17()
    : [a]"r"(a), [b]"r"(b), [w]"r"(w)
    : "memory", "hi", "lo"
  );
  return abs(temp3 - temp17) >> 5;
}

static int Disto16x16(const uint8_t* const a, const uint8_t* const b,
                      const uint16_t* const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4(a + x + y, b + x + y, w);
    }
  }
  return D;
}

#undef OUTPUT_EARLY_CLOBBER_REGS_17
#undef MUL_HALF
#undef ABS_X8
#undef ADD_SUB_HALVES_X4

#endif  // WEBP_USE_MIPS_DSP_R2

//------------------------------------------------------------------------------
// Entry point

extern WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMIPSdspR2(void) {
#if defined(WEBP_USE_MIPS_DSP_R2)
  VP8ITransform = ITransform;
  VP8TDisto4x4 = Disto4x4;
  VP8TDisto16x16 = Disto16x16;
#endif  // WEBP_USE_MIPS_DSP_R2
}
