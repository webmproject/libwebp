// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of rescaling functions
//
// Author(s): Djordje Pesut (djordje.pesut@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "../utils/rescaler.h"

static void ExportRow(WebPRescaler* const wrk, int x_out) {
  if (wrk->y_accum <= 0) {
    // if wrk->fxy_scale can fit into 32 bits use optimized code,
    // otherwise use C code
    if ((wrk->fxy_scale >> 32) == 0) {
      uint8_t* dst = wrk->dst;
      int32_t* irow = wrk->irow;
      const int32_t* frow = wrk->frow;
      const int yscale = wrk->fy_scale * (-wrk->y_accum);
      const int x_out_max = wrk->dst_width * wrk->num_channels;
      int temp0, temp1, temp3, temp4, temp5, temp6, temp7;
      const int temp2 = (int)wrk->fxy_scale;
      const int rest = (x_out_max - x_out) & 1;
      const int32_t* const loop_end = frow + (x_out_max - x_out) - rest;

      __asm__ volatile (
        ".set             push                                    \n\t"
        ".set             noreorder                               \n\t"
        "beq              %[frow],   %[loop_end],   1f            \n\t"
        " nop                                                     \n\t"
      "0:                                                         \n\t"
        "lw               %[temp0],    0(%[frow])                 \n\t"
        "lw               %[temp1],    0(%[irow])                 \n\t"
        "lw               %[temp3],    4(%[frow])                 \n\t"
        "lw               %[temp4],    4(%[irow])                 \n\t"
        "sll              %[temp0],    %[temp0],      1           \n\t"
        "sll              %[temp3],    %[temp3],      1           \n\t"
        "mulq_rs.w        %[temp5],    %[temp0],      %[yscale]   \n\t"
        "mulq_rs.w        %[temp6],    %[temp3],      %[yscale]   \n\t"
        "addiu            %[frow],     %[frow],       8           \n\t"
        "addiu            %[dst],      %[dst],        2           \n\t"
        "addiu            %[irow],     %[irow],       8           \n\t"
        "subu             %[temp1],    %[temp1],      %[temp5]    \n\t"
        "subu             %[temp4],    %[temp4],      %[temp6]    \n\t"
        "sll              %[temp1],    %[temp1],      1           \n\t"
        "sll              %[temp4],    %[temp4],      1           \n\t"
        "mulq_rs.w        %[temp0],    %[temp1],      %[temp2]    \n\t"
        "mulq_rs.w        %[temp3],    %[temp4],      %[temp2]    \n\t"
        "sw               %[temp5],    -8(%[irow])                \n\t"
        "sw               %[temp6],    -4(%[irow])                \n\t"
        "shll_s.ph        %[temp0],    %[temp0],      7           \n\t"
        "shll_s.ph        %[temp3],    %[temp3],      7           \n\t"
        "precrqu_s.qb.ph  %[temp0],    %[temp0],      %[temp3]    \n\t"
        "sb               %[temp0],    -1(%[dst])                 \n\t"
        "srl              %[temp0],    %[temp0],      16          \n\t"
        "bne              %[frow],     %[loop_end],   0b          \n\t"
        " sb              %[temp0],    -2(%[dst])                 \n\t"
      "1:                                                         \n\t"
        "beqz             %[rest],     3f                         \n\t"
        " nop                                                     \n\t"
        "addiu            %[temp6],    $zero,         -256        \n\t"
        "addiu            %[temp7],    $zero,         255         \n\t"
        "lw               %[temp0],    0(%[frow])                 \n\t"
        "sll              %[temp0],    %[temp0],      1           \n\t"
        "mulq_rs.w        %[temp1],    %[temp0],      %[yscale]   \n\t"
        "lw               %[temp0],    0(%[irow])                 \n\t"
        "subu             %[temp0],    %[temp0],      %[temp1]    \n\t"
        "sll              %[temp0],    %[temp0],      1           \n\t"
        "mulq_rs.w        %[temp5],    %[temp0],      %[temp2]    \n\t"
        "sw               %[temp1],    0(%[irow])                 \n\t"
        "and              %[temp0],    %[temp5],      %[temp6]    \n\t"
        "beqz             %[temp0],    2f                         \n\t"
        " slti            %[temp1],    %[temp5],      0           \n\t"
        "xor              %[temp5],    %[temp5],      %[temp5]    \n\t"
        "movz             %[temp5],    %[temp7],      %[temp1]    \n\t"
      "2:                                                         \n\t"
        "sb               %[temp5],    0(%[dst])                  \n\t"
      "3:                                                         \n\t"
        ".set             pop                                     \n\t"
        : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp3]"=&r"(temp3),
          [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),
          [temp7]"=&r"(temp7), [frow]"+&r"(frow), [irow]"+&r"(irow),
          [dst]"+&r"(dst)
        : [temp2]"r"(temp2), [yscale]"r"(yscale), [loop_end]"r"(loop_end),
          [rest]"r"(rest)
        : "memory", "hi", "lo"
      );
      wrk->y_accum += wrk->y_add;
      wrk->dst += wrk->dst_stride;
    } else {
      WebPRescalerExportRowC(wrk, x_out);
    }
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void WebPRescalerDspInitMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitMIPSdspR2(void) {
  WebPRescalerExportRow = ExportRow;
}

#else  // !WEBP_USE_MIPS_DSP_R2

WEBP_DSP_INIT_STUB(WebPRescalerDspInitMIPSdspR2)

#endif  // WEBP_USE_MIPS_DSP_R2
