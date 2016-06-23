// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA common macros
//
// Author(s):  Prashant Patil   (prashant.patil@imgtec.com)

#ifndef WEBP_DSP_MSA_MACRO_H_
#define WEBP_DSP_MSA_MACRO_H_

#include <stdint.h>
#include <msa.h>

#if defined(__clang__)
  #define CLANG_BUILD
#endif

#ifdef CLANG_BUILD
  #define ADDVI_H(a, b)  __msa_addvi_h((v8i16)a, b)
  #define SRAI_B(a, b)  __msa_srai_b((v16i8)a, b)
  #define SRAI_H(a, b)  __msa_srai_h((v8i16)a, b)
  #define SRAI_W(a, b)  __msa_srai_w((v4i32)a, b)
#else
  #define ADDVI_H(a, b)  (a + b)
  #define SRAI_B(a, b)  (a >> b)
  #define SRAI_H(a, b)  (a >> b)
  #define SRAI_W(a, b)  (a >> b)
#endif

#define LD_B(RTYPE, psrc) *((RTYPE*)(psrc))
#define LD_UB(...) LD_B(v16u8, __VA_ARGS__)
#define LD_SB(...) LD_B(v16i8, __VA_ARGS__)

#define LD_H(RTYPE, psrc) *((RTYPE*)(psrc))
#define LD_UH(...) LD_H(v8u16, __VA_ARGS__)
#define LD_SH(...) LD_H(v8i16, __VA_ARGS__)

#define LD_W(RTYPE, psrc) *((RTYPE*)(psrc))
#define LD_UW(...) LD_W(v4u32, __VA_ARGS__)
#define LD_SW(...) LD_W(v4i32, __VA_ARGS__)

#define ST_B(RTYPE, in, pdst) *((RTYPE*)(pdst)) = in
#define ST_UB(...) ST_B(v16u8, __VA_ARGS__)
#define ST_SB(...) ST_B(v16i8, __VA_ARGS__)

#define ST_H(RTYPE, in, pdst) *((RTYPE*)(pdst)) = in
#define ST_UH(...) ST_H(v8u16, __VA_ARGS__)
#define ST_SH(...) ST_H(v8i16, __VA_ARGS__)

#define ST_W(RTYPE, in, pdst) *((RTYPE*)(pdst)) = in
#define ST_UW(...) ST_W(v4u32, __VA_ARGS__)
#define ST_SW(...) ST_W(v4i32, __VA_ARGS__)

#define MSA_LOAD_FUNC(TYPE, INSTR, FUNC_NAME)             \
  static inline TYPE FUNC_NAME(const void* const psrc) {  \
    const uint8_t* const psrc_m = (const uint8_t*)psrc;   \
    TYPE val_m;                                           \
    asm volatile (                                        \
      "" #INSTR " %[val_m], %[psrc_m]  \n\t"              \
      : [val_m] "=r" (val_m)                              \
      : [psrc_m] "m" (*psrc_m));                          \
    return val_m;                                         \
  }

#define MSA_LOAD(psrc, FUNC_NAME)  FUNC_NAME(psrc)

#define MSA_STORE_FUNC(TYPE, INSTR, FUNC_NAME)               \
  static inline void FUNC_NAME(TYPE val, void* const pdst) { \
    uint8_t* const pdst_m = (uint8_t*)pdst;                  \
    TYPE val_m = val;                                        \
    asm volatile (                                           \
      " " #INSTR "  %[val_m],  %[pdst_m]  \n\t"              \
      : [pdst_m] "=m" (*pdst_m)                              \
      : [val_m] "r" (val_m));                                \
  }

#define MSA_STORE(val, pdst, FUNC_NAME)  FUNC_NAME(val, pdst)

#if (__mips_isa_rev >= 6)
  MSA_LOAD_FUNC(uint16_t, lh, msa_lh);
  #define LH(psrc)  MSA_LOAD(psrc, msa_lh)
  MSA_LOAD_FUNC(uint32_t, lw, msa_lw);
  #define LW(psrc)  MSA_LOAD(psrc, msa_lw)
  #if (__mips == 64)
    MSA_LOAD_FUNC(uint64_t, ld, msa_ld);
    #define LD(psrc)  MSA_LOAD(psrc, msa_ld)
  #else  // !(__mips == 64)
    #define LD(psrc)  ((((uint64_t)MSA_LOAD(psrc + 4, msa_lw)) << 32) | \
                       MSA_LOAD(psrc, msa_lw))
  #endif  // (__mips == 64)

  MSA_STORE_FUNC(uint16_t, sh, msa_sh);
  #define SH(val, pdst)  MSA_STORE(val, pdst, msa_sh)
  MSA_STORE_FUNC(uint32_t, sw, msa_sw);
  #define SW(val, pdst)  MSA_STORE(val, pdst, msa_sw)
  MSA_STORE_FUNC(uint64_t, sd, msa_sd);
  #define SD(val, pdst)  MSA_STORE(val, pdst, msa_sd)
#else  // !(__mips_isa_rev >= 6)
  MSA_LOAD_FUNC(uint16_t, ulh, msa_ulh);
  #define LH(psrc)  MSA_LOAD(psrc, msa_ulh)
  MSA_LOAD_FUNC(uint32_t, ulw, msa_ulw);
  #define LW(psrc)  MSA_LOAD(psrc, msa_ulw)
  #if (__mips == 64)
    MSA_LOAD_FUNC(uint64_t, uld, msa_uld);
    #define LD(psrc)  MSA_LOAD(psrc, msa_uld)
  #else  // !(__mips == 64)
    #define LD(psrc)  ((((uint64_t)MSA_LOAD(psrc + 4, msa_ulw)) << 32) | \
                        MSA_LOAD(psrc, msa_ulw))
  #endif  // (__mips == 64)

  MSA_STORE_FUNC(uint16_t, ush, msa_ush);
  #define SH(val, pdst)  MSA_STORE(val, pdst, msa_ush)
  MSA_STORE_FUNC(uint32_t, usw, msa_usw);
  #define SW(val, pdst)  MSA_STORE(val, pdst, msa_usw)
  #define SD(val, pdst) {                                                  \
    uint8_t* const pdst_sd_m = (uint8_t*)(pdst);                           \
    const uint32_t val0_m = (uint32_t)(val & 0x00000000FFFFFFFF);          \
    const uint32_t val1_m = (uint32_t)((val >> 32) & 0x00000000FFFFFFFF);  \
    SW(val0_m, pdst_sd_m);                                                 \
    SW(val1_m, pdst_sd_m + 4);                                             \
  }
#endif  // (__mips_isa_rev >= 6)

/* Description : Load 4 words with stride
 * Arguments   : Inputs  - psrc, stride
 *               Outputs - out0, out1, out2, out3
 * Details     : Load word in 'out0' from (psrc)
 *               Load word in 'out1' from (psrc + stride)
 *               Load word in 'out2' from (psrc + 2 * stride)
 *               Load word in 'out3' from (psrc + 3 * stride)
 */
#define LW4(psrc, stride, out0, out1, out2, out3) {  \
  const uint8_t* ptmp = (const uint8_t*)psrc;        \
  out0 = LW(ptmp);                                   \
  ptmp += stride;                                    \
  out1 = LW(ptmp);                                   \
  ptmp += stride;                                    \
  out2 = LW(ptmp);                                   \
  ptmp += stride;                                    \
  out3 = LW(ptmp);                                   \
}

/* Description : Store 4 words with stride
 * Arguments   : Inputs - in0, in1, in2, in3, pdst, stride
 * Details     : Store word from 'in0' to (pdst)
 *               Store word from 'in1' to (pdst + stride)
 *               Store word from 'in2' to (pdst + 2 * stride)
 *               Store word from 'in3' to (pdst + 3 * stride)
 */
#define SW4(in0, in1, in2, in3, pdst, stride) {  \
  uint8_t* ptmp = (uint8_t*)pdst;                \
  SW(in0, ptmp);                                 \
  ptmp += stride;                                \
  SW(in1, ptmp);                                 \
  ptmp += stride;                                \
  SW(in2, ptmp);                                 \
  ptmp += stride;                                \
  SW(in3, ptmp);                                 \
}

/* Description : Store 4 double words with stride
 * Arguments   : Inputs - in0, in1, in2, in3, pdst, stride
 * Details     : Store double word from 'in0' to (pdst)
 *               Store double word from 'in1' to (pdst + stride)
 *               Store double word from 'in2' to (pdst + 2 * stride)
 *               Store double word from 'in3' to (pdst + 3 * stride)
 */
#define SD4(in0, in1, in2, in3, pdst, stride) {  \
  uint8_t* ptmp = (uint8_t*)pdst;                \
  SD(in0, ptmp);                                 \
  ptmp += stride;                                \
  SD(in1, ptmp);                                 \
  ptmp += stride;                                \
  SD(in2, ptmp);                                 \
  ptmp += stride;                                \
  SD(in3, ptmp);                                 \
}

/* Description : Load vectors with 16 byte elements with stride
 * Arguments   : Inputs  - psrc, stride
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Load 16 byte elements in 'out0' from (psrc)
 *               Load 16 byte elements in 'out1' from (psrc + stride)
 */
#define LD_B2(RTYPE, psrc, stride, out0, out1) {  \
  out0 = LD_B(RTYPE, psrc);                       \
  out1 = LD_B(RTYPE, psrc + stride);              \
}
#define LD_UB2(...) LD_B2(v16u8, __VA_ARGS__)
#define LD_SB2(...) LD_B2(v16i8, __VA_ARGS__)

#define LD_B4(RTYPE, psrc, stride, out0, out1, out2, out3) {  \
  LD_B2(RTYPE, psrc, stride, out0, out1);                     \
  LD_B2(RTYPE, psrc + 2 * stride , stride, out2, out3);       \
}
#define LD_UB4(...) LD_B4(v16u8, __VA_ARGS__)
#define LD_SB4(...) LD_B4(v16i8, __VA_ARGS__)

#define LD_B8(RTYPE, psrc, stride,                                  \
              out0, out1, out2, out3, out4, out5, out6, out7) {     \
  LD_B4(RTYPE, psrc, stride, out0, out1, out2, out3);               \
  LD_B4(RTYPE, psrc + 4 * stride, stride, out4, out5, out6, out7);  \
}
#define LD_UB8(...) LD_B8(v16u8, __VA_ARGS__)
#define LD_SB8(...) LD_B8(v16i8, __VA_ARGS__)

/* Description : Load vectors with 8 halfword elements with stride
 * Arguments   : Inputs  - psrc, stride
 *               Outputs - out0, out1
 * Details     : Load 8 halfword elements in 'out0' from (psrc)
 *               Load 8 halfword elements in 'out1' from (psrc + stride)
 */
#define LD_H2(RTYPE, psrc, stride, out0, out1) {  \
  out0 = LD_H(RTYPE, psrc);                       \
  out1 = LD_H(RTYPE, psrc + stride);              \
}
#define LD_UH2(...) LD_H2(v8u16, __VA_ARGS__)
#define LD_SH2(...) LD_H2(v8i16, __VA_ARGS__)

/* Description : Store vectors of 16 byte elements with stride
 * Arguments   : Inputs - in0, in1, pdst, stride
 * Details     : Store 16 byte elements from 'in0' to (pdst)
 *               Store 16 byte elements from 'in1' to (pdst + stride)
 */
#define ST_B2(RTYPE, in0, in1, pdst, stride) {  \
  ST_B(RTYPE, in0, pdst);                       \
  ST_B(RTYPE, in1, pdst + stride);              \
}
#define ST_UB2(...) ST_B2(v16u8, __VA_ARGS__)
#define ST_SB2(...) ST_B2(v16i8, __VA_ARGS__)

#define ST_B4(RTYPE, in0, in1, in2, in3, pdst, stride) {  \
  ST_B2(RTYPE, in0, in1, pdst, stride);                   \
  ST_B2(RTYPE, in2, in3, pdst + 2 * stride, stride);      \
}
#define ST_UB4(...) ST_B4(v16u8, __VA_ARGS__)
#define ST_SB4(...) ST_B4(v16i8, __VA_ARGS__)

/* Description : Store 2x4 byte block to destination memory from input vector
 * Arguments   : Inputs - in, stidx, pdst, stride
 * Details     : Index 'stidx' halfword element from 'in' vector is copied to
 *               the GP register and stored to (pdst)
 *               Index 'stidx+1' halfword element from 'in' vector is copied to
 *               the GP register and stored to (pdst + stride)
 *               Index 'stidx+2' halfword element from 'in' vector is copied to
 *               the GP register and stored to (pdst + 2 * stride)
 *               Index 'stidx+3' halfword element from 'in' vector is copied to
 *               the GP register and stored to (pdst + 3 * stride)
 */
#define ST2x4_UB(in, stidx, pdst, stride) {                      \
  uint8_t* pblk_2x4_m = (uint8_t*)pdst;                          \
  const uint16_t out0_m = __msa_copy_s_h((v8i16)in, stidx);      \
  const uint16_t out1_m = __msa_copy_s_h((v8i16)in, stidx + 1);  \
  const uint16_t out2_m = __msa_copy_s_h((v8i16)in, stidx + 2);  \
  const uint16_t out3_m = __msa_copy_s_h((v8i16)in, stidx + 3);  \
  SH(out0_m, pblk_2x4_m);                                        \
  pblk_2x4_m += stride;                                          \
  SH(out1_m, pblk_2x4_m);                                        \
  pblk_2x4_m += stride;                                          \
  SH(out2_m, pblk_2x4_m);                                        \
  pblk_2x4_m += stride;                                          \
  SH(out3_m, pblk_2x4_m);                                        \
}

/* Description : Store 4x4 byte block to destination memory from input vector
 * Arguments   : Inputs - in0, in1, pdst, stride
 * Details     : 'Idx0' word element from input vector 'in0' is copied to the
 *               GP register and stored to (pdst)
 *               'Idx1' word element from input vector 'in0' is copied to the
 *               GP register and stored to (pdst + stride)
 *               'Idx2' word element from input vector 'in0' is copied to the
 *               GP register and stored to (pdst + 2 * stride)
 *               'Idx3' word element from input vector 'in0' is copied to the
 *               GP register and stored to (pdst + 3 * stride)
 */
#define ST4x4_UB(in0, in1, idx0, idx1, idx2, idx3, pdst, stride) {  \
  uint8_t* const pblk_4x4_m = (uint8_t*)pdst;                       \
  const uint32_t out0_m = __msa_copy_s_w((v4i32)in0, idx0);         \
  const uint32_t out1_m = __msa_copy_s_w((v4i32)in0, idx1);         \
  const uint32_t out2_m = __msa_copy_s_w((v4i32)in1, idx2);         \
  const uint32_t out3_m = __msa_copy_s_w((v4i32)in1, idx3);         \
  SW4(out0_m, out1_m, out2_m, out3_m, pblk_4x4_m, stride);          \
}

#define ST4x8_UB(in0, in1, pdst, stride) {                        \
  uint8_t* const pblk_4x8 = (uint8_t*)pdst;                       \
  ST4x4_UB(in0, in0, 0, 1, 2, 3, pblk_4x8, stride);               \
  ST4x4_UB(in1, in1, 0, 1, 2, 3, pblk_4x8 + 4 * stride, stride);  \
}

/* Description : Immediate number of elements to slide
 * Arguments   : Inputs  - in0, in1, slide_val
 *               Outputs - out
 *               Return Type - as per RTYPE
 * Details     : Byte elements from 'in1' vector are slid into 'in0' by
 *               value specified in the 'slide_val'
 */
#define SLDI_B(RTYPE, in0, in1, slide_val)                      \
        (RTYPE)__msa_sldi_b((v16i8)in0, (v16i8)in1, slide_val)  \

#define SLDI_UB(...) SLDI_B(v16u8, __VA_ARGS__)
#define SLDI_SB(...) SLDI_B(v16i8, __VA_ARGS__)
#define SLDI_SH(...) SLDI_B(v8i16, __VA_ARGS__)

/* Description : Shuffle halfword vector elements as per mask vector
 * Arguments   : Inputs  - in0, in1, in2, in3, mask0, mask1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : halfword elements from 'in0' & 'in1' are copied selectively to
 *               'out0' as per control vector 'mask0'
 */
#define VSHF_H2(RTYPE, in0, in1, in2, in3, mask0, mask1, out0, out1) {  \
  out0 = (RTYPE)__msa_vshf_h((v8i16)mask0, (v8i16)in1, (v8i16)in0);     \
  out1 = (RTYPE)__msa_vshf_h((v8i16)mask1, (v8i16)in3, (v8i16)in2);     \
}
#define VSHF_H2_UH(...) VSHF_H2(v8u16, __VA_ARGS__)
#define VSHF_H2_SH(...) VSHF_H2(v8i16, __VA_ARGS__)

/* Description : Clips all signed halfword elements of input vector
 *               between 0 & 255
 * Arguments   : Input/output  - val
 *               Return Type - signed halfword
 */
#define CLIP_SH_0_255(val) {                      \
  const v8i16 max_m = __msa_ldi_h(255);           \
  val = __msa_maxi_s_h((v8i16)val, 0);            \
  val = __msa_min_s_h(max_m, (v8i16)val);         \
}
#define CLIP_SH2_0_255(in0, in1) {  \
  CLIP_SH_0_255(in0);               \
  CLIP_SH_0_255(in1);               \
}

/* Description : Clips all signed word elements of input vector
 *               between 0 & 255
 * Arguments   : Input/output  - val
 *               Return Type - signed word
 */
#define CLIP_SW_0_255(val) {                      \
  const v4i32 max_m = __msa_ldi_w(255);           \
  val = __msa_maxi_s_w((v4i32)val, 0);            \
  val = __msa_min_s_w(max_m, (v4i32)val);         \
}
#define CLIP_SW4_0_255(in0, in1, in2, in3) {  \
  CLIP_SW_0_255(in0);                         \
  CLIP_SW_0_255(in1);                         \
  CLIP_SW_0_255(in2);                         \
  CLIP_SW_0_255(in3);                         \
}

/* Description : Set element n input vector to GPR value
 * Arguments   : Inputs - in0, in1, in2, in3
 *               Output - out
 *               Return Type - as per RTYPE
 * Details     : Set element 0 in vector 'out' to value specified in 'in0'
 */
#define INSERT_W2(RTYPE, in0, in1, out) {           \
  out = (RTYPE)__msa_insert_w((v4i32)out, 0, in0);  \
  out = (RTYPE)__msa_insert_w((v4i32)out, 1, in1);  \
}
#define INSERT_W2_UB(...) INSERT_W2(v16u8, __VA_ARGS__)
#define INSERT_W2_SB(...) INSERT_W2(v16i8, __VA_ARGS__)

#define INSERT_W4(RTYPE, in0, in1, in2, in3, out) {  \
  out = (RTYPE)__msa_insert_w((v4i32)out, 0, in0);   \
  out = (RTYPE)__msa_insert_w((v4i32)out, 1, in1);   \
  out = (RTYPE)__msa_insert_w((v4i32)out, 2, in2);   \
  out = (RTYPE)__msa_insert_w((v4i32)out, 3, in3);   \
}
#define INSERT_W4_UB(...) INSERT_W4(v16u8, __VA_ARGS__)
#define INSERT_W4_SB(...) INSERT_W4(v16i8, __VA_ARGS__)
#define INSERT_W4_SW(...) INSERT_W4(v4i32, __VA_ARGS__)

/* Description : Interleave even byte elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Even byte elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 */
#define ILVEV_B2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvev_b((v16i8)in1, (v16i8)in0);     \
  out1 = (RTYPE)__msa_ilvev_b((v16i8)in3, (v16i8)in2);     \
}
#define ILVEV_B2_UB(...) ILVEV_B2(v16u8, __VA_ARGS__)
#define ILVEV_B2_SB(...) ILVEV_B2(v16i8, __VA_ARGS__)
#define ILVEV_B2_UH(...) ILVEV_B2(v8u16, __VA_ARGS__)
#define ILVEV_B2_SH(...) ILVEV_B2(v8i16, __VA_ARGS__)
#define ILVEV_B2_SD(...) ILVEV_B2(v2i64, __VA_ARGS__)

/* Description : Interleave odd byte elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Odd byte elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 */
#define ILVOD_B2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvod_b((v16i8)in1, (v16i8)in0);     \
  out1 = (RTYPE)__msa_ilvod_b((v16i8)in3, (v16i8)in2);     \
}
#define ILVOD_B2_UB(...) ILVOD_B2(v16u8, __VA_ARGS__)
#define ILVOD_B2_SB(...) ILVOD_B2(v16i8, __VA_ARGS__)
#define ILVOD_B2_UH(...) ILVOD_B2(v8u16, __VA_ARGS__)
#define ILVOD_B2_SH(...) ILVOD_B2(v8i16, __VA_ARGS__)
#define ILVOD_B2_SD(...) ILVOD_B2(v2i64, __VA_ARGS__)

/* Description : Interleave even halfword elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Even halfword elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 */
#define ILVEV_H2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvev_h((v8i16)in1, (v8i16)in0);     \
  out1 = (RTYPE)__msa_ilvev_h((v8i16)in3, (v8i16)in2);     \
}
#define ILVEV_H2_UB(...) ILVEV_H2(v16u8, __VA_ARGS__)
#define ILVEV_H2_UH(...) ILVEV_H2(v8u16, __VA_ARGS__)
#define ILVEV_H2_SH(...) ILVEV_H2(v8i16, __VA_ARGS__)
#define ILVEV_H2_SW(...) ILVEV_H2(v4i32, __VA_ARGS__)

/* Description : Interleave odd halfword elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Odd halfword elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 */
#define ILVOD_H2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvod_h((v8i16)in1, (v8i16)in0);     \
  out1 = (RTYPE)__msa_ilvod_h((v8i16)in3, (v8i16)in2);     \
}
#define ILVOD_H2_UB(...) ILVOD_H2(v16u8, __VA_ARGS__)
#define ILVOD_H2_UH(...) ILVOD_H2(v8u16, __VA_ARGS__)
#define ILVOD_H2_SH(...) ILVOD_H2(v8i16, __VA_ARGS__)
#define ILVOD_H2_SW(...) ILVOD_H2(v4i32, __VA_ARGS__)

/* Description : Interleave even word elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Even word elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 */
#define ILVEV_W2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvev_w((v4i32)in1, (v4i32)in0);     \
  out1 = (RTYPE)__msa_ilvev_w((v4i32)in3, (v4i32)in2);     \
}
#define ILVEV_W2_UB(...) ILVEV_W2(v16u8, __VA_ARGS__)
#define ILVEV_W2_SB(...) ILVEV_W2(v16i8, __VA_ARGS__)
#define ILVEV_W2_UH(...) ILVEV_W2(v8u16, __VA_ARGS__)
#define ILVEV_W2_SD(...) ILVEV_W2(v2i64, __VA_ARGS__)

/* Description : Interleave even-odd word elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Even word elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 *               Odd word elements of 'in2' and 'in3' are interleaved
 *               and written to 'out1'
 */
#define ILVEVOD_W2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvev_w((v4i32)in1, (v4i32)in0);       \
  out1 = (RTYPE)__msa_ilvod_w((v4i32)in3, (v4i32)in2);       \
}
#define ILVEVOD_W2_UB(...) ILVEVOD_W2(v16u8, __VA_ARGS__)
#define ILVEVOD_W2_UH(...) ILVEVOD_W2(v8u16, __VA_ARGS__)
#define ILVEVOD_W2_SH(...) ILVEVOD_W2(v8i16, __VA_ARGS__)
#define ILVEVOD_W2_SW(...) ILVEVOD_W2(v4i32, __VA_ARGS__)

/* Description : Interleave even-odd half-word elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Even half-word elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 *               Odd half-word elements of 'in2' and 'in3' are interleaved
 *               and written to 'out1'
 */
#define ILVEVOD_H2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvev_h((v8i16)in1, (v8i16)in0);       \
  out1 = (RTYPE)__msa_ilvod_h((v8i16)in3, (v8i16)in2);       \
}
#define ILVEVOD_H2_UB(...) ILVEVOD_H2(v16u8, __VA_ARGS__)
#define ILVEVOD_H2_UH(...) ILVEVOD_H2(v8u16, __VA_ARGS__)
#define ILVEVOD_H2_SH(...) ILVEVOD_H2(v8i16, __VA_ARGS__)
#define ILVEVOD_H2_SW(...) ILVEVOD_H2(v4i32, __VA_ARGS__)

/* Description : Interleave even double word elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Even double word elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'
 */
#define ILVEV_D2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvev_d((v2i64)in1, (v2i64)in0);     \
  out1 = (RTYPE)__msa_ilvev_d((v2i64)in3, (v2i64)in2);     \
}
#define ILVEV_D2_UB(...) ILVEV_D2(v16u8, __VA_ARGS__)
#define ILVEV_D2_SB(...) ILVEV_D2(v16i8, __VA_ARGS__)
#define ILVEV_D2_SW(...) ILVEV_D2(v4i32, __VA_ARGS__)
#define ILVEV_D2_SD(...) ILVEV_D2(v2i64, __VA_ARGS__)

/* Description : Interleave left half of byte elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Left half of byte elements of 'in0' and 'in1' are interleaved
 *               and written to 'out0'.
 */
#define ILVL_B2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvl_b((v16i8)in0, (v16i8)in1);     \
  out1 = (RTYPE)__msa_ilvl_b((v16i8)in2, (v16i8)in3);     \
}
#define ILVL_B2_UB(...) ILVL_B2(v16u8, __VA_ARGS__)
#define ILVL_B2_SB(...) ILVL_B2(v16i8, __VA_ARGS__)
#define ILVL_B2_UH(...) ILVL_B2(v8u16, __VA_ARGS__)
#define ILVL_B2_SH(...) ILVL_B2(v8i16, __VA_ARGS__)
#define ILVL_B2_SW(...) ILVL_B2(v4i32, __VA_ARGS__)

/* Description : Interleave right half of byte elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Right half of byte elements of 'in0' and 'in1' are interleaved
 *               and written to out0.
 */
#define ILVR_B2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvr_b((v16i8)in0, (v16i8)in1);     \
  out1 = (RTYPE)__msa_ilvr_b((v16i8)in2, (v16i8)in3);     \
}
#define ILVR_B2_UB(...) ILVR_B2(v16u8, __VA_ARGS__)
#define ILVR_B2_SB(...) ILVR_B2(v16i8, __VA_ARGS__)
#define ILVR_B2_UH(...) ILVR_B2(v8u16, __VA_ARGS__)
#define ILVR_B2_SH(...) ILVR_B2(v8i16, __VA_ARGS__)
#define ILVR_B2_SW(...) ILVR_B2(v4i32, __VA_ARGS__)

#define ILVR_B4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,  \
                out0, out1, out2, out3) {                       \
  ILVR_B2(RTYPE, in0, in1, in2, in3, out0, out1);               \
  ILVR_B2(RTYPE, in4, in5, in6, in7, out2, out3);               \
}
#define ILVR_B4_UB(...) ILVR_B4(v16u8, __VA_ARGS__)
#define ILVR_B4_SB(...) ILVR_B4(v16i8, __VA_ARGS__)
#define ILVR_B4_UH(...) ILVR_B4(v8u16, __VA_ARGS__)
#define ILVR_B4_SH(...) ILVR_B4(v8i16, __VA_ARGS__)
#define ILVR_B4_SW(...) ILVR_B4(v4i32, __VA_ARGS__)

/* Description : Interleave right half of halfword elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Right half of halfword elements of 'in0' and 'in1' are
 *               interleaved and written to 'out0'.
 */
#define ILVR_H2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvr_h((v8i16)in0, (v8i16)in1);     \
  out1 = (RTYPE)__msa_ilvr_h((v8i16)in2, (v8i16)in3);     \
}
#define ILVR_H2_UB(...) ILVR_H2(v16u8, __VA_ARGS__)
#define ILVR_H2_SH(...) ILVR_H2(v8i16, __VA_ARGS__)
#define ILVR_H2_SW(...) ILVR_H2(v4i32, __VA_ARGS__)

#define ILVR_H4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,  \
                out0, out1, out2, out3) {                       \
  ILVR_H2(RTYPE, in0, in1, in2, in3, out0, out1);               \
  ILVR_H2(RTYPE, in4, in5, in6, in7, out2, out3);               \
}
#define ILVR_H4_UB(...) ILVR_H4(v16u8, __VA_ARGS__)
#define ILVR_H4_SH(...) ILVR_H4(v8i16, __VA_ARGS__)
#define ILVR_H4_SW(...) ILVR_H4(v4i32, __VA_ARGS__)

/* Description : Interleave right half of double word elements from vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Right half of double word elements of 'in0' and 'in1' are
 *               interleaved and written to 'out0'.
 */
#define ILVR_D2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_ilvr_d((v2i64)in0, (v2i64)in1);     \
  out1 = (RTYPE)__msa_ilvr_d((v2i64)in2, (v2i64)in3);     \
}
#define ILVR_D2_UB(...) ILVR_D2(v16u8, __VA_ARGS__)
#define ILVR_D2_SB(...) ILVR_D2(v16i8, __VA_ARGS__)
#define ILVR_D2_SH(...) ILVR_D2(v8i16, __VA_ARGS__)

#define ILVR_D4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,  \
                out0, out1, out2, out3) {                       \
  ILVR_D2(RTYPE, in0, in1, in2, in3, out0, out1);               \
  ILVR_D2(RTYPE, in4, in5, in6, in7, out2, out3);               \
}
#define ILVR_D4_SB(...) ILVR_D4(v16i8, __VA_ARGS__)
#define ILVR_D4_UB(...) ILVR_D4(v16u8, __VA_ARGS__)

/* Description : Interleave both left and right half of input vectors
 * Arguments   : Inputs  - in0, in1
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Right half of byte elements from 'in0' and 'in1' are
 *               interleaved and written to 'out0'
 */
#define ILVRL_B2(RTYPE, in0, in1, out0, out1) {        \
  out0 = (RTYPE)__msa_ilvr_b((v16i8)in0, (v16i8)in1);  \
  out1 = (RTYPE)__msa_ilvl_b((v16i8)in0, (v16i8)in1);  \
}
#define ILVRL_B2_UB(...) ILVRL_B2(v16u8, __VA_ARGS__)
#define ILVRL_B2_SB(...) ILVRL_B2(v16i8, __VA_ARGS__)
#define ILVRL_B2_UH(...) ILVRL_B2(v8u16, __VA_ARGS__)
#define ILVRL_B2_SH(...) ILVRL_B2(v8i16, __VA_ARGS__)
#define ILVRL_B2_SW(...) ILVRL_B2(v4i32, __VA_ARGS__)

#define ILVRL_H2(RTYPE, in0, in1, out0, out1) {        \
  out0 = (RTYPE)__msa_ilvr_h((v8i16)in0, (v8i16)in1);  \
  out1 = (RTYPE)__msa_ilvl_h((v8i16)in0, (v8i16)in1);  \
}
#define ILVRL_H2_UB(...) ILVRL_H2(v16u8, __VA_ARGS__)
#define ILVRL_H2_SB(...) ILVRL_H2(v16i8, __VA_ARGS__)
#define ILVRL_H2_SH(...) ILVRL_H2(v8i16, __VA_ARGS__)
#define ILVRL_H2_SW(...) ILVRL_H2(v4i32, __VA_ARGS__)
#define ILVRL_H2_UW(...) ILVRL_H2(v4u32, __VA_ARGS__)

#define ILVRL_W2(RTYPE, in0, in1, out0, out1) {        \
  out0 = (RTYPE)__msa_ilvr_w((v4i32)in0, (v4i32)in1);  \
  out1 = (RTYPE)__msa_ilvl_w((v4i32)in0, (v4i32)in1);  \
}
#define ILVRL_W2_UB(...) ILVRL_W2(v16u8, __VA_ARGS__)
#define ILVRL_W2_SH(...) ILVRL_W2(v8i16, __VA_ARGS__)
#define ILVRL_W2_SW(...) ILVRL_W2(v4i32, __VA_ARGS__)

/* Description : Pack even byte elements of vector pairs
 *  Arguments   : Inputs  - in0, in1, in2, in3
 *                Outputs - out0, out1
 *                Return Type - as per RTYPE
 *  Details     : Even byte elements of 'in0' are copied to the left half of
 *                'out0' & even byte elements of 'in1' are copied to the right
 *                half of 'out0'.
 */
#define PCKEV_B2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)__msa_pckev_b((v16i8)in0, (v16i8)in1);     \
  out1 = (RTYPE)__msa_pckev_b((v16i8)in2, (v16i8)in3);     \
}
#define PCKEV_B2_SB(...) PCKEV_B2(v16i8, __VA_ARGS__)
#define PCKEV_B2_UB(...) PCKEV_B2(v16u8, __VA_ARGS__)
#define PCKEV_B2_SH(...) PCKEV_B2(v8i16, __VA_ARGS__)
#define PCKEV_B2_SW(...) PCKEV_B2(v4i32, __VA_ARGS__)

/* Description : Arithmetic immediate shift right all elements of word vector
 * Arguments   : Inputs  - in0, in1, shift
 *               Outputs - in place operation
 *               Return Type - as per input vector RTYPE
 * Details     : Each element of vector 'in0' is right shifted by 'shift' and
 *               the result is written in-place. 'shift' is a GP variable.
 */
#define SRAI_W2(RTYPE, in0, in1, shift_val) {  \
  in0 = (RTYPE)SRAI_W(in0, shift_val);         \
  in1 = (RTYPE)SRAI_W(in1, shift_val);         \
}
#define SRAI_W2_SW(...) SRAI_W2(v4i32, __VA_ARGS__)
#define SRAI_W2_UW(...) SRAI_W2(v4u32, __VA_ARGS__)

#define SRAI_W4(RTYPE, in0, in1, in2, in3, shift_val) {  \
  SRAI_W2(RTYPE, in0, in1, shift_val);                   \
  SRAI_W2(RTYPE, in2, in3, shift_val);                   \
}
#define SRAI_W4_SW(...) SRAI_W4(v4i32, __VA_ARGS__)
#define SRAI_W4_UW(...) SRAI_W4(v4u32, __VA_ARGS__)

/* Description : Arithmetic shift right all elements of half-word vector
 * Arguments   : Inputs  - in0, in1, shift
 *               Outputs - in place operation
 *               Return Type - as per input vector RTYPE
 * Details     : Each element of vector 'in0' is right shifted by 'shift' and
 *               the result is written in-place. 'shift' is a GP variable.
 */
#define SRAI_H2(RTYPE, in0, in1, shift_val) {  \
  in0 = (RTYPE)SRAI_H(in0, shift_val);         \
  in1 = (RTYPE)SRAI_H(in1, shift_val);         \
}
#define SRAI_H2_SH(...) SRAI_H2(v8i16, __VA_ARGS__)
#define SRAI_H2_UH(...) SRAI_H2(v8u16, __VA_ARGS__)

/* Description : Arithmetic rounded shift right all elements of word vector
 * Arguments   : Inputs  - in0, in1, shift
 *               Outputs - in place operation
 *               Return Type - as per input vector RTYPE
 * Details     : Each element of vector 'in0' is right shifted by 'shift' and
 *               the result is written in-place. 'shift' is a GP variable.
 */
#define SRARI_W2(RTYPE, in0, in1, shift) {        \
  in0 = (RTYPE)__msa_srari_w((v4i32)in0, shift);  \
  in1 = (RTYPE)__msa_srari_w((v4i32)in1, shift);  \
}
#define SRARI_W2_SW(...) SRARI_W2(v4i32, __VA_ARGS__)

#define SRARI_W4(RTYPE, in0, in1, in2, in3, shift) {  \
  SRARI_W2(RTYPE, in0, in1, shift);                   \
  SRARI_W2(RTYPE, in2, in3, shift);                   \
}
#define SRARI_W4_SH(...) SRARI_W4(v8i16, __VA_ARGS__)
#define SRARI_W4_UW(...) SRARI_W4(v4u32, __VA_ARGS__)
#define SRARI_W4_SW(...) SRARI_W4(v4i32, __VA_ARGS__)

/* Description : Addition of 2 pairs of half-word vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 * Details     : Each element in 'in0' is added to 'in1' and result is written
 *               to 'out0'.
 */
#define ADDVI_H2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE)ADDVI_H(in0, in1);                         \
  out1 = (RTYPE)ADDVI_H(in2, in3);                         \
}
#define ADDVI_H2_SH(...) ADDVI_H2(v8i16, __VA_ARGS__)
#define ADDVI_H2_UH(...) ADDVI_H2(v8u16, __VA_ARGS__)

/* Description : Addition of 2 pairs of vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1
 * Details     : Each element in 'in0' is added to 'in1' and result is written
 *               to 'out0'.
 */
#define ADD2(in0, in1, in2, in3, out0, out1) {  \
  out0 = in0 + in1;                             \
  out1 = in2 + in3;                             \
}
#define ADD4(in0, in1, in2, in3, in4, in5, in6, in7,  \
             out0, out1, out2, out3) {                \
  ADD2(in0, in1, in2, in3, out0, out1);               \
  ADD2(in4, in5, in6, in7, out2, out3);               \
}

/* Description : Sign extend halfword elements from input vector and return
 *               the result in pair of vectors
 * Arguments   : Input   - in            (halfword vector)
 *               Outputs - out0, out1   (sign extended word vectors)
 *               Return Type - signed word
 * Details     : Sign bit of halfword elements from input vector 'in' is
 *               extracted and interleaved right with same vector 'in0' to
 *               generate 4 signed word elements in 'out0'
 *               Then interleaved left with same vector 'in0' to
 *               generate 4 signed word elements in 'out1'
 */
#define UNPCK_SH_SW(in, out0, out1) {                 \
  const v8i16 tmp_m = __msa_clti_s_h((v8i16)in, 0);   \
  ILVRL_H2_SW(tmp_m, in, out0, out1);                 \
}

/* Description : Butterfly of 4 input vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     : Butterfly operation
 */
#define BUTTERFLY_4(in0, in1, in2, in3, out0, out1, out2, out3) {  \
  out0 = in0 + in3;                                                \
  out1 = in1 + in2;                                                \
  out2 = in1 - in2;                                                \
  out3 = in0 - in3;                                                \
}

/* Description : Transpose 16x4 block into 4x16 with byte elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7,
 *                         in8, in9, in10, in11, in12, in13, in14, in15
 *               Outputs - out0, out1, out2, out3
 *               Return Type - unsigned byte
 */
#define TRANSPOSE16x4_UB_UB(in0, in1, in2, in3, in4, in5, in6, in7,        \
                            in8, in9, in10, in11, in12, in13, in14, in15,  \
                            out0, out1, out2, out3) {                      \
  v2i64 tmp0_m, tmp1_m, tmp2_m, tmp3_m, tmp4_m, tmp5_m;                    \
  ILVEV_W2_SD(in0, in4, in8, in12, tmp2_m, tmp3_m);                        \
  ILVEV_W2_SD(in1, in5, in9, in13, tmp0_m, tmp1_m);                        \
  ILVEV_D2_UB(tmp2_m, tmp3_m, tmp0_m, tmp1_m, out1, out3);                 \
  ILVEV_W2_SD(in2, in6, in10, in14, tmp4_m, tmp5_m);                       \
  ILVEV_W2_SD(in3, in7, in11, in15, tmp0_m, tmp1_m);                       \
  ILVEV_D2_SD(tmp4_m, tmp5_m, tmp0_m, tmp1_m, tmp2_m, tmp3_m);             \
  ILVEV_B2_SD(out1, out3, tmp2_m, tmp3_m, tmp0_m, tmp1_m);                 \
  ILVEVOD_H2_UB(tmp0_m, tmp1_m, tmp0_m, tmp1_m, out0, out2);               \
  ILVOD_B2_SD(out1, out3, tmp2_m, tmp3_m, tmp0_m, tmp1_m);                 \
  ILVEVOD_H2_UB(tmp0_m, tmp1_m, tmp0_m, tmp1_m, out1, out3);               \
}

/* Description : Transpose 16x8 block into 8x16 with byte elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7,
 *                         in8, in9, in10, in11, in12, in13, in14, in15
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 *               Return Type - unsigned byte
 */
#define TRANSPOSE16x8_UB_UB(in0, in1, in2, in3, in4, in5, in6, in7,        \
                            in8, in9, in10, in11, in12, in13, in14, in15,  \
                            out0, out1, out2, out3, out4, out5,            \
                            out6, out7) {                                  \
  v8i16 tmp0_m, tmp1_m, tmp4_m, tmp5_m, tmp6_m, tmp7_m;                    \
  v4i32 tmp2_m, tmp3_m;                                                    \
  ILVEV_D2_UB(in0, in8, in1, in9, out7, out6);                             \
  ILVEV_D2_UB(in2, in10, in3, in11, out5, out4);                           \
  ILVEV_D2_UB(in4, in12, in5, in13, out3, out2);                           \
  ILVEV_D2_UB(in6, in14, in7, in15, out1, out0);                           \
  ILVEV_B2_SH(out7, out6, out5, out4, tmp0_m, tmp1_m);                     \
  ILVOD_B2_SH(out7, out6, out5, out4, tmp4_m, tmp5_m);                     \
  ILVEV_B2_UB(out3, out2, out1, out0, out5, out7);                         \
  ILVOD_B2_SH(out3, out2, out1, out0, tmp6_m, tmp7_m);                     \
  ILVEV_H2_SW(tmp0_m, tmp1_m, out5, out7, tmp2_m, tmp3_m);                 \
  ILVEVOD_W2_UB(tmp2_m, tmp3_m, tmp2_m, tmp3_m, out0, out4);               \
  ILVOD_H2_SW(tmp0_m, tmp1_m, out5, out7, tmp2_m, tmp3_m);                 \
  ILVEVOD_W2_UB(tmp2_m, tmp3_m, tmp2_m, tmp3_m, out2, out6);               \
  ILVEV_H2_SW(tmp4_m, tmp5_m, tmp6_m, tmp7_m, tmp2_m, tmp3_m);             \
  ILVEVOD_W2_UB(tmp2_m, tmp3_m, tmp2_m, tmp3_m, out1, out5);               \
  ILVOD_H2_SW(tmp4_m, tmp5_m, tmp6_m, tmp7_m, tmp2_m, tmp3_m);             \
  ILVEVOD_W2_UB(tmp2_m, tmp3_m, tmp2_m, tmp3_m, out3, out7);               \
}

/* Description : Transpose 4x4 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *                Outputs - out0, out1, out2, out3
 *                Return Type - as per RTYPE
 */
#define TRANSPOSE4x4_W(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3) {  \
  v4i32 s0_m, s1_m, s2_m, s3_m;                                              \
  ILVRL_W2_SW(in1, in0, s0_m, s1_m);                                         \
  ILVRL_W2_SW(in3, in2, s2_m, s3_m);                                         \
  out0 = (RTYPE)__msa_ilvr_d((v2i64)s2_m, (v2i64)s0_m);                      \
  out1 = (RTYPE)__msa_ilvl_d((v2i64)s2_m, (v2i64)s0_m);                      \
  out2 = (RTYPE)__msa_ilvr_d((v2i64)s3_m, (v2i64)s1_m);                      \
  out3 = (RTYPE)__msa_ilvl_d((v2i64)s3_m, (v2i64)s1_m);                      \
}
#define TRANSPOSE4x4_SW_SW(...) TRANSPOSE4x4_W(v4i32, __VA_ARGS__)

/* Description : Add block 4x4
 * Arguments   : Inputs - in0, in1, in2, in3, pdst, stride
 * Details     : Least significant 4 bytes from each input vector are added to
 *               the destination bytes, clipped between 0-255 and stored.
 */
#define ADDBLK_ST4x4_UB(in0, in1, in2, in3, pdst, stride) {     \
  uint32_t src0_m, src1_m, src2_m, src3_m;                      \
  v8i16 inp0_m, inp1_m, res0_m, res1_m;                         \
  v16i8 dst0_m = { 0 };                                         \
  v16i8 dst1_m = { 0 };                                         \
  const v16i8 zero_m = { 0 };                                   \
  ILVR_D2_SH(in1, in0, in3, in2, inp0_m, inp1_m);               \
  LW4(pdst, stride, src0_m, src1_m, src2_m, src3_m);            \
  INSERT_W2_SB(src0_m, src1_m, dst0_m);                         \
  INSERT_W2_SB(src2_m, src3_m, dst1_m);                         \
  ILVR_B2_SH(zero_m, dst0_m, zero_m, dst1_m, res0_m, res1_m);   \
  ADD2(res0_m, inp0_m, res1_m, inp1_m, res0_m, res1_m);         \
  CLIP_SH2_0_255(res0_m, res1_m);                               \
  PCKEV_B2_SB(res0_m, res0_m, res1_m, res1_m, dst0_m, dst1_m);  \
  ST4x4_UB(dst0_m, dst1_m, 0, 1, 0, 1, pdst, stride);           \
}

#endif  /* WEBP_DSP_MSA_MACRO_H_ */
