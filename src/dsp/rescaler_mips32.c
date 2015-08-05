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

#if defined(WEBP_USE_MIPS32)

#include "../utils/rescaler.h"

static void ExportRow(WebPRescaler* const wrk, int x_out) {
  if (wrk->y_accum <= 0) {
    uint8_t* const dst = wrk->dst;
    int32_t* const irow = wrk->irow;
    const int32_t* const frow = wrk->frow;
    const int yscale = wrk->fy_scale * (-wrk->y_accum);
    const int x_out_max = wrk->dst_width * wrk->num_channels;
    // if wrk->fxy_scale can fit into 32 bits use optimized code,
    // otherwise use C code
    if ((wrk->fxy_scale >> 32) == 0) {
      int temp0, temp1, temp3, temp4, temp5, temp6, temp7, loop_end;
      const int temp2 = (int)(wrk->fxy_scale);
      const int temp8 = x_out_max << 2;
      uint8_t* dst_t = (uint8_t*)dst;
      int32_t* irow_t = (int32_t*)irow;
      const int32_t* frow_t = (const int32_t*)frow;

      __asm__ volatile(
        "addiu    %[temp6],    $zero,       -256          \n\t"
        "addiu    %[temp7],    $zero,       255           \n\t"
        "li       %[temp3],    0x10000                    \n\t"
        "li       %[temp4],    0x8000                     \n\t"
        "addu     %[loop_end], %[frow_t],   %[temp8]      \n\t"
      "1:                                                 \n\t"
        "lw       %[temp0],    0(%[frow_t])               \n\t"
        "mult     %[temp3],    %[temp4]                   \n\t"
        "addiu    %[frow_t],   %[frow_t],   4             \n\t"
        "sll      %[temp0],    %[temp0],    2             \n\t"
        "madd     %[temp0],    %[yscale]                  \n\t"
        "mfhi     %[temp1]                                \n\t"
        "lw       %[temp0],    0(%[irow_t])               \n\t"
        "addiu    %[dst_t],    %[dst_t],    1             \n\t"
        "addiu    %[irow_t],   %[irow_t],   4             \n\t"
        "subu     %[temp0],    %[temp0],    %[temp1]      \n\t"
        "mult     %[temp3],    %[temp4]                   \n\t"
        "sll      %[temp0],    %[temp0],    2             \n\t"
        "madd     %[temp0],    %[temp2]                   \n\t"
        "mfhi     %[temp5]                                \n\t"
        "sw       %[temp1],    -4(%[irow_t])              \n\t"
        "and      %[temp0],    %[temp5],    %[temp6]      \n\t"
        "slti     %[temp1],    %[temp5],    0             \n\t"
        "beqz     %[temp0],    2f                         \n\t"
        "xor      %[temp5],    %[temp5],    %[temp5]      \n\t"
        "movz     %[temp5],    %[temp7],    %[temp1]      \n\t"
      "2:                                                 \n\t"
        "sb       %[temp5],    -1(%[dst_t])               \n\t"
        "bne      %[frow_t],   %[loop_end], 1b            \n\t"

        : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp3]"=&r"(temp3),
          [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),
          [temp7]"=&r"(temp7), [frow_t]"+r"(frow_t), [irow_t]"+r"(irow_t),
          [dst_t]"+r"(dst_t), [loop_end]"=&r"(loop_end)
        : [temp2]"r"(temp2), [yscale]"r"(yscale), [temp8]"r"(temp8)
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

extern void WebPRescalerDspInitMIPS32(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitMIPS32(void) {
  WebPRescalerExportRow = ExportRow;
}

#else  // !WEBP_USE_MIPS32

WEBP_DSP_INIT_STUB(WebPRescalerDspInitMIPS32)

#endif  // WEBP_USE_MIPS32
