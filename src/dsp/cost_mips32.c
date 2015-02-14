// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Author: Djordje Pesut (djordje.pesut@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MIPS32)

#include "../enc/cost.h"

static int GetResidualCost(int ctx0, const VP8Residual* const res) {
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  int p0 = res->prob[n][ctx0][0];
  const uint16_t* t = res->cost[n][ctx0];
  int cost;
  const int const_2 = 2;
  const int const_255 = 255;
  const int const_max_level = MAX_VARIABLE_LEVEL;
  int res_cost;
  int res_prob;
  int res_coeffs;
  int res_last;
  int v_reg;
  int b_reg;
  int ctx_reg;
  int cost_add, temp_1, temp_2, temp_3;

  if (res->last < 0) {
    return VP8BitCost(0, p0);
  }

  cost = (ctx0 == 0) ? VP8BitCost(1, p0) : 0;

  res_cost = (int)res->cost;
  res_prob = (int)res->prob;
  res_coeffs = (int)res->coeffs;
  res_last = (int)res->last;

  __asm__ volatile(
    ".set   push                                                           \n\t"
    ".set   noreorder                                                      \n\t"

    "sll    %[temp_1],     %[n],              1                            \n\t"
    "addu   %[res_coeffs], %[res_coeffs],     %[temp_1]                    \n\t"
    "slt    %[temp_2],     %[n],              %[res_last]                  \n\t"
    "bnez   %[temp_2],     1f                                              \n\t"
    " li    %[cost_add],   0                                               \n\t"
    "b      2f                                                             \n\t"
    " nop                                                                  \n\t"
  "1:                                                                      \n\t"
    "lh     %[v_reg],      0(%[res_coeffs])                                \n\t"
    "addu   %[b_reg],      %[n],              %[VP8EncBands]               \n\t"
    "move   %[temp_1],     %[const_max_level]                              \n\t"
    "addu   %[cost],       %[cost],           %[cost_add]                  \n\t"
    "negu   %[temp_2],     %[v_reg]                                        \n\t"
    "slti   %[temp_3],     %[v_reg],          0                            \n\t"
    "movn   %[v_reg],      %[temp_2],         %[temp_3]                    \n\t"
    "lbu    %[b_reg],      1(%[b_reg])                                     \n\t"
    "li     %[cost_add],   0                                               \n\t"

    "sltiu  %[temp_3],     %[v_reg],          2                            \n\t"
    "move   %[ctx_reg],    %[v_reg]                                        \n\t"
    "movz   %[ctx_reg],    %[const_2],        %[temp_3]                    \n\t"
    //  cost += VP8LevelCost(t, v);
    "slt    %[temp_3],     %[v_reg],          %[const_max_level]           \n\t"
    "movn   %[temp_1],     %[v_reg],          %[temp_3]                    \n\t"
    "sll    %[temp_2],     %[v_reg],          1                            \n\t"
    "addu   %[temp_2],     %[temp_2],         %[VP8LevelFixedCosts]        \n\t"
    "lhu    %[temp_2],     0(%[temp_2])                                    \n\t"
    "sll    %[temp_1],     %[temp_1],         1                            \n\t"
    "addu   %[temp_1],     %[temp_1],         %[t]                         \n\t"
    "lhu    %[temp_3],     0(%[temp_1])                                    \n\t"
    "addu   %[cost],       %[cost],           %[temp_2]                    \n\t"

    //  t = res->cost[b][ctx];
    "sll    %[temp_1],     %[ctx_reg],        7                            \n\t"
    "sll    %[temp_2],     %[ctx_reg],        3                            \n\t"
    "addu   %[cost],       %[cost],           %[temp_3]                    \n\t"
    "addu   %[temp_1],     %[temp_1],         %[temp_2]                    \n\t"
    "sll    %[temp_2],     %[b_reg],          3                            \n\t"
    "sll    %[temp_3],     %[b_reg],          5                            \n\t"
    "sub    %[temp_2],     %[temp_3],         %[temp_2]                    \n\t"
    "sll    %[temp_3],     %[temp_2],         4                            \n\t"
    "addu   %[temp_1],     %[temp_1],         %[temp_3]                    \n\t"
    "addu   %[temp_2],     %[temp_2],         %[res_cost]                  \n\t"
    "addiu  %[n],          %[n],              1                            \n\t"
    "addu   %[t],          %[temp_1],         %[temp_2]                    \n\t"
    "slt    %[temp_1],     %[n],              %[res_last]                  \n\t"
    "bnez   %[temp_1],     1b                                              \n\t"
    " addiu %[res_coeffs], %[res_coeffs],     2                            \n\t"
   "2:                                                                     \n\t"

    ".set   pop                                                            \n\t"
    : [cost]"+r"(cost), [t]"+r"(t), [n]"+r"(n), [v_reg]"=&r"(v_reg),
      [ctx_reg]"=&r"(ctx_reg), [b_reg]"=&r"(b_reg), [cost_add]"=&r"(cost_add),
      [temp_1]"=&r"(temp_1), [temp_2]"=&r"(temp_2), [temp_3]"=&r"(temp_3)
    : [const_2]"r"(const_2), [const_255]"r"(const_255), [res_last]"r"(res_last),
      [VP8EntropyCost]"r"(VP8EntropyCost), [VP8EncBands]"r"(VP8EncBands),
      [const_max_level]"r"(const_max_level), [res_prob]"r"(res_prob),
      [VP8LevelFixedCosts]"r"(VP8LevelFixedCosts), [res_coeffs]"r"(res_coeffs),
      [res_cost]"r"(res_cost)
    : "memory"
  );

  // Last coefficient is always non-zero
  {
    const int v = abs(res->coeffs[n]);
    assert(v != 0);
    cost += VP8LevelCost(t, v);
    if (n < 15) {
      const int b = VP8EncBands[n + 1];
      const int ctx = (v == 1) ? 1 : 2;
      const int last_p0 = res->prob[b][ctx][0];
      cost += VP8BitCost(0, last_p0);
    }
  }
  return cost;
}

static void SetResidualCoeffs(const int16_t* const coeffs,
                              VP8Residual* const res) {
  const int16_t* p_coeffs = (int16_t*)coeffs;
  int temp0, temp1, temp2, n, n1;
  assert(res->first == 0 || coeffs[0] == 0);

  __asm__ volatile (
    ".set     push                                      \n\t"
    ".set     noreorder                                 \n\t"
    "addiu    %[p_coeffs],   %[p_coeffs],    28         \n\t"
    "li       %[n],          15                         \n\t"
    "li       %[temp2],      -1                         \n\t"
  "0:                                                   \n\t"
    "ulw      %[temp0],      0(%[p_coeffs])             \n\t"
    "beqz     %[temp0],      1f                         \n\t"
#if defined(WORDS_BIGENDIAN)
    " sll     %[temp1],      %[temp0],       16         \n\t"
#else
    " srl     %[temp1],      %[temp0],       16         \n\t"
#endif
    "addiu    %[n1],         %[n],           -1         \n\t"
    "movz     %[temp0],      %[n1],          %[temp1]   \n\t"
    "movn     %[temp0],      %[n],           %[temp1]   \n\t"
    "j        2f                                        \n\t"
    " addiu   %[temp2],      %[temp0],       0          \n\t"
  "1:                                                   \n\t"
    "addiu    %[n],          %[n],           -2         \n\t"
    "bgtz     %[n],          0b                         \n\t"
    " addiu   %[p_coeffs],   %[p_coeffs],    -4         \n\t"
  "2:                                                   \n\t"
    ".set     pop                                       \n\t"
    : [p_coeffs]"+&r"(p_coeffs), [temp0]"=&r"(temp0),
      [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [n]"=&r"(n), [n1]"=&r"(n1)
    :
    : "memory"
  );
  res->last = temp2;
  res->coeffs = coeffs;
}

#endif  // WEBP_USE_MIPS32

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspCostInitMIPS32(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspCostInitMIPS32(void) {
#if defined(WEBP_USE_MIPS32)
  VP8GetResidualCost = GetResidualCost;
  VP8SetResidualCoeffs = SetResidualCoeffs;
#endif  // WEBP_USE_MIPS32
}

//------------------------------------------------------------------------------
