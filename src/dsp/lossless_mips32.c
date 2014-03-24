// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of lossless functions
//
// Author(s):  Jovan Zelincevic (jovan.zelincevic@imgtec.com)

#include "./dsp.h"
#include "./lossless.h"

#if defined(WEBP_USE_MIPS32)

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#define APPROX_LOG_WITH_CORRECTION_MAX  65536
#define APPROX_LOG_MAX                   4096
#define LOG_2_RECIPROCAL 1.44269504088896338700465094007086

static float FastSLog2SlowMIPS32(int v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    int log_cnt, y, correction;
    const int c24 = 24;
    const float v_f = (float)v;
    int temp;

    // Xf = 256 = 2^8
    // log_cnt is index of leading one in upper 24 bits
    __asm__ volatile(
      "clz      %[log_cnt], %[v]                      \n\t"
      "addiu    %[y],       $zero,        1           \n\t"
      "subu     %[log_cnt], %[c24],       %[log_cnt]  \n\t"
      "sllv     %[y],       %[y],         %[log_cnt]  \n\t"
      "srlv     %[temp],    %[v],         %[log_cnt]  \n\t"
      : [log_cnt]"=&r"(log_cnt), [y]"=&r"(y),
        [temp]"=r"(temp)
      : [c24]"r"(c24), [v]"r"(v)
    );

    // vf = (2^log_cnt) * Xf; where y = 2^log_cnt and Xf < 256
    // Xf = floor(Xf) * (1 + (v % y) / v)
    // log2(Xf) = log2(floor(Xf)) + log2(1 + (v % y) / v)
    // The correction factor: log(1 + d) ~ d; for very small d values, so
    // log2(1 + (v % y) / v) ~ LOG_2_RECIPROCAL * (v % y)/v
    // LOG_2_RECIPROCAL ~ 23/16

    // (v % y) = (v % 2^log_cnt) = v & (2^log_cnt - 1)
    correction = (23 * (v & (y - 1))) >> 4;
    return v_f * (kLog2Table[temp] + log_cnt) + correction;
  } else {
    return (float)(LOG_2_RECIPROCAL * v * log((double)v));
  }
}

static float FastLog2SlowMIPS32(int v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    int log_cnt, y;
    const int c24 = 24;
    double log_2;
    int temp;

    __asm__ volatile(
      "clz      %[log_cnt], %[v]                      \n\t"
      "addiu    %[y],       $zero,        1           \n\t"
      "subu     %[log_cnt], %[c24],       %[log_cnt]  \n\t"
      "sllv     %[y],       %[y],         %[log_cnt]  \n\t"
      "srlv     %[temp],    %[v],         %[log_cnt]  \n\t"
      : [log_cnt]"=&r"(log_cnt), [y]"=&r"(y),
        [temp]"=r"(temp)
      : [c24]"r"(c24), [v]"r"(v)
    );

    log_2 = kLog2Table[temp] + log_cnt;
    if (v >= APPROX_LOG_MAX) {
      // Since the division is still expensive, add this correction factor only
      // for large values of 'v'.

      const int correction = (23 * (v & (y - 1))) >> 4;
      log_2 += (double)correction / v;
    }
    return (float)log_2;
  } else {
    return (float)(LOG_2_RECIPROCAL * log((double)v));
  }
}

#endif  // WEBP_USE_MIPS32

//------------------------------------------------------------------------------
// Entry point

extern void VP8LDspInitMIPS32(void);

void VP8LDspInitMIPS32(void) {
#if defined(WEBP_USE_MIPS32)
  VP8LFastSLog2Slow = FastSLog2SlowMIPS32;
  VP8LFastLog2Slow = FastLog2SlowMIPS32;
#endif  // WEBP_USE_MIPS32
}
