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

#define ALIGNMENT           16
#define ALIGNMENT_MINUS_1   (ALIGNMENT - 1)
#define ALLOC_ALIGNED(align) __attribute__ ((aligned((align) << 1)))
#define CLANG_BUILD

#ifdef CLANG_BUILD
  #define ADDVI_H(a, b)  __msa_addvi_h((v8i16)a, b)
  #define SRAI_H(a, b)   __msa_srai_h((v8i16)a, b)
  #define SRAI_W(a, b)   __msa_srai_w((v4i32)a, b)
#else
  #define ADDVI_H(a, b)  (a + b)
  #define SRAI_H(a, b)   (a >> b)
  #define SRAI_W(a, b)   (a >> b)
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

#define ST_B(RTYPE, in, pdst) *((RTYPE*)(pdst)) = (in)
#define ST_UB(...) ST_B(v16u8, __VA_ARGS__)
#define ST_SB(...) ST_B(v16i8, __VA_ARGS__)

#define ST_H(RTYPE, in, pdst) *((RTYPE*)(pdst)) = (in)
#define ST_UH(...) ST_H(v8u16, __VA_ARGS__)
#define ST_SH(...) ST_H(v8i16, __VA_ARGS__)

#define ST_W(RTYPE, in, pdst) *((RTYPE*)(pdst)) = (in)
#define ST_UW(...) ST_W(v4u32, __VA_ARGS__)
#define ST_SW(...) ST_W(v4i32, __VA_ARGS__)

#if (__mips_isa_rev >= 6)
  #define LH(psrc) ( {                       \
    uint8_t *psrc_lh_m = (uint8_t*)(psrc);   \
    uint16_t val_m;                          \
                                             \
    asm volatile (                           \
        "lh  %[val_m],  %[psrc_lh_m]  \n\t"  \
        : [val_m] "=r" (val_m)               \
        : [psrc_lh_m] "m" (*psrc_lh_m)       \
    );                                       \
    val_m;                                   \
  } )

  #define LW(psrc) ( {                       \
    uint8_t *psrc_lw_m = (uint8_t*)(psrc);   \
    uint32_t val_m;                          \
                                             \
    asm volatile (                           \
        "lw  %[val_m],  %[psrc_lw_m]  \n\t"  \
        : [val_m] "=r" (val_m)               \
        : [psrc_lw_m] "m" (*psrc_lw_m)       \
    );                                       \
    val_m;                                   \
  } )

  #if (__mips == 64)
    #define LD(psrc) ( {                       \
      uint8_t *psrc_ld_m = (uint8_t*)(psrc);   \
      uint64_t val_m = 0;                      \
                                               \
      asm volatile (                           \
          "ld  %[val_m],  %[psrc_ld_m]  \n\t"  \
          : [val_m] "=r" (val_m)               \
          : [psrc_ld_m] "m" (*psrc_ld_m)       \
      );                                       \
      val_m;                                   \
    } )
  #else  // !(__mips == 64)
    #define LD(psrc) ( {                                       \
      uint8_t *psrc_ld_m = (uint8_t*)(psrc);                   \
      uint32_t val0_m, val1_m;                                 \
      uint64_t val_m = 0;                                      \
                                                               \
      val0_m = LW(psrc_ld_m);                                  \
      val1_m = LW(psrc_ld_m + 4);                              \
      val_m = (uint64_t)(val1_m);                              \
      val_m = (uint64_t)((val_m << 32) & 0xFFFFFFFF00000000);  \
      val_m = (uint64_t)(val_m | (uint64_t) val0_m);           \
      val_m;                                                   \
    } )
  #endif  // (__mips == 64)

  #define SH(val, pdst) {                    \
    uint8_t *pdst_sh_m = (uint8_t*)(pdst);   \
    uint16_t val_m = (val);                  \
                                             \
    asm volatile (                           \
        "sh  %[val_m],  %[pdst_sh_m]  \n\t"  \
        : [pdst_sh_m] "=m" (*pdst_sh_m)      \
        : [val_m] "r" (val_m)                \
    );                                       \
  }

  #define SW(val, pdst) {                    \
    uint8_t *pdst_sw_m = (uint8_t*)(pdst);   \
    uint32_t val_m = (val);                  \
                                             \
    asm volatile (                           \
        "sw  %[val_m],  %[pdst_sw_m]  \n\t"  \
        : [pdst_sw_m] "=m" (*pdst_sw_m)      \
        : [val_m] "r" (val_m)                \
    );                                       \
  }

  #define SD(val, pdst) {                    \
    uint8_t *pdst_sd_m = (uint8_t*)(pdst);   \
    uint64_t val_m = (val);                  \
                                             \
    asm volatile (                           \
        "sd  %[val_m],  %[pdst_sd_m]  \n\t"  \
        : [pdst_sd_m] "=m" (*pdst_sd_m)      \
        : [val_m] "r" (val_m)                \
    );                                       \
  }

  #define SW_ZERO(pdst) {                \
    uint8_t *pdst_m = (uint8_t*)(pdst);  \
                                         \
    asm volatile (                       \
        "sw  $0,  %[pdst_m]  \n\t"       \
        : [pdst_m] "=m" (*pdst_m)        \
        :                                \
    );                                   \
  }
#else  // !(__mips_isa_rev >= 6)
  #define LH(psrc) ( {                        \
    uint8_t *psrc_lh_m = (uint8_t*)(psrc);    \
    uint16_t val_m;                           \
                                              \
    asm volatile (                            \
        "ulh  %[val_m],  %[psrc_lh_m]  \n\t"  \
        : [val_m] "=r" (val_m)                \
        : [psrc_lh_m] "m" (*psrc_lh_m)        \
    );                                        \
    val_m;                                    \
  } )

  #define LW(psrc) ( {                        \
    uint8_t *psrc_lw_m = (uint8_t*)(psrc);    \
    uint32_t val_m;                           \
                                              \
    asm volatile (                            \
        "ulw  %[val_m],  %[psrc_lw_m]  \n\t"  \
        : [val_m] "=r" (val_m)                \
        : [psrc_lw_m] "m" (*psrc_lw_m)        \
    );                                        \
    val_m;                                    \
  } )

  #if (__mips == 64)
    #define LD(psrc) ( {                        \
      uint8_t *psrc_ld_m = (uint8_t*)(psrc);    \
      uint64_t val_m = 0;                       \
                                                \
      asm volatile (                            \
          "uld  %[val_m],  %[psrc_ld_m]  \n\t"  \
          : [val_m] "=r" (val_m)                \
          : [psrc_ld_m] "m" (*psrc_ld_m)        \
      );                                        \
      val_m;                                    \
    } )
  #else  // !(__mips == 64)
    #define LD(psrc) ( {                                       \
      uint8_t *psrc_ld_m = (uint8_t*)(psrc);                   \
      uint32_t val0_m, val1_m;                                 \
      uint64_t val_m = 0;                                      \
                                                               \
      val0_m = LW(psrc_ld_m);                                  \
      val1_m = LW(psrc_ld_m + 4);                              \
      val_m = (uint64_t)(val1_m);                              \
      val_m = (uint64_t)((val_m << 32) & 0xFFFFFFFF00000000);  \
      val_m = (uint64_t)(val_m | (uint64_t)val0_m);            \
      val_m;                                                   \
    } )
  #endif  // (__mips == 64)

  #define SH(val, pdst) {                     \
    uint8_t *pdst_sh_m = (uint8_t*)(pdst);    \
    uint16_t val_m = (val);                   \
                                              \
    asm volatile (                            \
        "ush  %[val_m],  %[pdst_sh_m]  \n\t"  \
        : [pdst_sh_m] "=m" (*pdst_sh_m)       \
        : [val_m] "r" (val_m)                 \
    );                                        \
  }

  #define SW(val, pdst) {                     \
    uint8_t *pdst_sw_m = (uint8_t*)(pdst);    \
    uint32_t val_m = (val);                   \
                                              \
    asm volatile (                            \
        "usw  %[val_m],  %[pdst_sw_m]  \n\t"  \
        : [pdst_sw_m] "=m" (*pdst_sw_m)       \
        : [val_m] "r" (val_m)                 \
    );                                        \
  }

  #define SD(val, pdst) {                                     \
    uint8_t *pdst_sd_m = (uint8_t*)(pdst);                    \
    uint32_t val0_m, val1_m;                                  \
                                                              \
    val0_m = (uint32_t)((val) & 0x00000000FFFFFFFF);          \
    val1_m = (uint32_t)(((val) >> 32) & 0x00000000FFFFFFFF);  \
    SW(val0_m, pdst_sd_m);                                    \
    SW(val1_m, pdst_sd_m + 4);                                \
  }

  #define SW_ZERO(pdst) {                \
    uint8_t *pdst_m = (uint8_t*)(pdst);  \
                                         \
    asm volatile (                       \
        "usw  $0,  %[pdst_m]  \n\t"      \
        : [pdst_m] "=m" (*pdst_m)        \
        :                                \
    );                                   \
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
  uint8_t *ptmp = (uint8_t*)(psrc);                  \
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
  uint8_t *ptmp = (uint8_t*)(pdst);              \
  SW(in0, ptmp);                                 \
  ptmp += stride;                                \
  SW(in1, ptmp);                                 \
  ptmp += stride;                                \
  SW(in2, ptmp);                                 \
  ptmp += stride;                                \
  SW(in3, ptmp);                                 \
}

/* Description : Load vectors with 16 byte elements with stride
 * Arguments   : Inputs  - psrc, stride
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Load 16 byte elements in 'out0' from (psrc)
 *               Load 16 byte elements in 'out1' from (psrc + stride)
 */
#define LD_B2(RTYPE, psrc, stride, out0, out1) {  \
  out0 = LD_B(RTYPE, (psrc));                     \
  out1 = LD_B(RTYPE, (psrc) + stride);            \
}
#define LD_UB2(...) LD_B2(v16u8, __VA_ARGS__)
#define LD_SB2(...) LD_B2(v16i8, __VA_ARGS__)

#define LD_B4(RTYPE, psrc, stride, out0, out1, out2, out3) {  \
  LD_B2(RTYPE, (psrc), stride, out0, out1);                   \
  LD_B2(RTYPE, (psrc) + 2 * stride , stride, out2, out3);     \
}
#define LD_UB4(...) LD_B4(v16u8, __VA_ARGS__)
#define LD_SB4(...) LD_B4(v16i8, __VA_ARGS__)

/* Description : Load vectors with 8 halfword elements with stride
 * Arguments   : Inputs  - psrc, stride
 *               Outputs - out0, out1
 * Details     : Load 8 halfword elements in 'out0' from (psrc)
 *               Load 8 halfword elements in 'out1' from (psrc + stride)
 */
#define LD_H2(RTYPE, psrc, stride, out0, out1) {  \
  out0 = LD_H(RTYPE, (psrc));                     \
  out1 = LD_H(RTYPE, (psrc) + (stride));          \
}
#define LD_UH2(...) LD_H2(v8u16, __VA_ARGS__)
#define LD_SH2(...) LD_H2(v8i16, __VA_ARGS__)

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
  uint32_t out0_m, out1_m, out2_m, out3_m;                          \
  uint8_t *pblk_4x4_m = (uint8_t*)(pdst);                           \
                                                                    \
  out0_m = __msa_copy_s_w((v4i32)in0, idx0);                        \
  out1_m = __msa_copy_s_w((v4i32)in0, idx1);                        \
  out2_m = __msa_copy_s_w((v4i32)in1, idx2);                        \
  out3_m = __msa_copy_s_w((v4i32)in1, idx3);                        \
  SW4(out0_m, out1_m, out2_m, out3_m, pblk_4x4_m, stride);          \
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
 * Arguments   : Input  - in
 *               Output - out_m
 *               Return Type - signed halfword
 */
#define CLIP_SH_0_255(in) ( {                         \
  v8i16 max_m = __msa_ldi_h(255);                     \
  v8i16 out_m;                                        \
                                                      \
  out_m = __msa_maxi_s_h((v8i16)in, 0);               \
  out_m = __msa_min_s_h((v8i16)max_m, (v8i16)out_m);  \
  out_m;                                              \
} )
#define CLIP_SH2_0_255(in0, in1) {  \
  in0 = CLIP_SH_0_255(in0);         \
  in1 = CLIP_SH_0_255(in1);         \
}

/* Description : Clips all signed word elements of input vector
 *               between 0 & 255
 * Arguments   : Input  - in
 *               Output - out_m
 *               Return Type - signed word
 */
#define CLIP_SW_0_255(in) ( {                         \
  v4i32 max_m = __msa_ldi_w(255);                     \
  v4i32 out_m;                                        \
                                                      \
  out_m = __msa_maxi_s_w((v4i32)in, 0);               \
  out_m = __msa_min_s_w((v4i32)max_m, (v4i32)out_m);  \
  out_m;                                              \
} )

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

/* Description : Arithmetic immegiate shift right all elements of word vector
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

/* Description : Arithmetic rounded shift right all elements of
 *               half-word vector
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
#define UNPCK_SH_SW(in, out0, out1) {     \
  v8i16 tmp_m;                            \
  tmp_m = __msa_clti_s_h((v8i16)in, 0);   \
  ILVRL_H2_SW(tmp_m, in, out0, out1);     \
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

/* Description : Transpose 4x4 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *                Outputs - out0, out1, out2, out3
 *                Return Type - as per RTYPE
 */
#define TRANSPOSE4x4_W(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3) {  \
  v4i32 s0_m, s1_m, s2_m, s3_m;                                              \
                                                                             \
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
                                                                \
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
