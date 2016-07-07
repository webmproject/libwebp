// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA version of encoder dsp functions.
//
// Author:  Prashant Patil   (prashant.patil@imgtec.com)

#include "./dsp.h"

#if defined(WEBP_USE_MSA)

#include <stdlib.h>
#include "./msa_macro.h"

//------------------------------------------------------------------------------
// Transforms

#define IDCT_1D_W(in0, in1, in2, in3, out0, out1, out2, out3) do {  \
  v4i32 a1_m, b1_m, c1_m, d1_m;                                     \
  const v4i32 cospi8sqrt2minus1 = __msa_fill_w(20091);              \
  const v4i32 sinpi8sqrt2 = __msa_fill_w(35468);                    \
  v4i32 c_tmp1_m = in1 * sinpi8sqrt2;                               \
  v4i32 c_tmp2_m = in3 * cospi8sqrt2minus1;                         \
  v4i32 d_tmp1_m = in1 * cospi8sqrt2minus1;                         \
  v4i32 d_tmp2_m = in3 * sinpi8sqrt2;                               \
                                                                    \
  ADDSUB2(in0, in2, a1_m, b1_m);                                    \
  SRAI_W2_SW(c_tmp1_m, c_tmp2_m, 16);                               \
  c_tmp2_m = c_tmp2_m + in3;                                        \
  c1_m = c_tmp1_m - c_tmp2_m;                                       \
  SRAI_W2_SW(d_tmp1_m, d_tmp2_m, 16);                               \
  d_tmp1_m = d_tmp1_m + in1;                                        \
  d1_m = d_tmp1_m + d_tmp2_m;                                       \
  BUTTERFLY_4(a1_m, b1_m, c1_m, d1_m, out0, out1, out2, out3);      \
} while (0)

static WEBP_INLINE void ITransformOne(const uint8_t* ref, const int16_t* in,
                                      uint8_t* dst) {
  v8i16 input0, input1;
  v4i32 in0, in1, in2, in3, hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3;
  v4i32 res0, res1, res2, res3;
  v16i8 dest0, dest1, dest2, dest3;
  const v16i8 zero = { 0 };

  LD_SH2(in, 8, input0, input1);
  UNPCK_SH_SW(input0, in0, in1);
  UNPCK_SH_SW(input1, in2, in3);
  IDCT_1D_W(in0, in1, in2, in3, hz0, hz1, hz2, hz3);
  TRANSPOSE4x4_SW_SW(hz0, hz1, hz2, hz3, hz0, hz1, hz2, hz3);
  IDCT_1D_W(hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3);
  SRARI_W4_SW(vt0, vt1, vt2, vt3, 3);
  TRANSPOSE4x4_SW_SW(vt0, vt1, vt2, vt3, vt0, vt1, vt2, vt3);
  LD_SB4(ref, BPS, dest0, dest1, dest2, dest3);
  ILVR_B4_SW(zero, dest0, zero, dest1, zero, dest2, zero, dest3,
             res0, res1, res2, res3);
  ILVR_H4_SW(zero, res0, zero, res1, zero, res2, zero, res3,
             res0, res1, res2, res3);
  ADD4(res0, vt0, res1, vt1, res2, vt2, res3, vt3, res0, res1, res2, res3);
  CLIP_SW4_0_255(res0, res1, res2, res3);
  PCKEV_B2_SW(res0, res1, res2, res3, vt0, vt1);
  res0 = (v4i32)__msa_pckev_b((v16i8)vt0, (v16i8)vt1);
  ST4x4_UB(res0, res0, 3, 2, 1, 0, dst, BPS);
}

static void ITransform(const uint8_t* ref, const int16_t* in, uint8_t* dst,
                       int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

static void FTransform(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  uint64_t out0, out1, out2, out3;
  uint32_t in0, in1, in2, in3;
  v4i32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  v8i16 t0, t1, t2, t3;
  v16u8 srcl0, srcl1, src0, src1;
  const v8i16 mask0 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask1 = { 3, 7, 11, 15, 2, 6, 10, 14 };
  const v8i16 mask2 = { 4, 0, 5, 1, 6, 2, 7, 3 };
  const v8i16 mask3 = { 0, 4, 1, 5, 2, 6, 3, 7 };
  const v8i16 cnst0 = { 2217, -5352, 2217, -5352, 2217, -5352, 2217, -5352 };
  const v8i16 cnst1 = { 5352, 2217, 5352, 2217, 5352, 2217, 5352, 2217 };

  LW4(src, BPS, in0, in1, in2, in3);
  INSERT_W4_UB(in0, in1, in2, in3, src0);
  LW4(ref, BPS, in0, in1, in2, in3);
  INSERT_W4_UB(in0, in1, in2, in3, src1);
  ILVRL_B2_UB(src0, src1, srcl0, srcl1);
  HSUB_UB2_SH(srcl0, srcl1, t0, t1);
  VSHF_H2_SH(t0, t1, t0, t1, mask0, mask1, t2, t3);
  ADDSUB2(t2, t3, t0, t1);
  t0 = SRLI_H(t0, 3);
  VSHF_H2_SH(t0, t0, t1, t1, mask2, mask3, t3, t2);
  tmp0 = __msa_hadd_s_w(t3, t3);
  tmp2 = __msa_hsub_s_w(t3, t3);
  FILL_W2_SW(1812, 937, tmp1, tmp3);
  DPADD_SH2_SW(t2, t2, cnst0, cnst1, tmp3, tmp1);
  SRAI_W2_SW(tmp1, tmp3, 9);
  PCKEV_H2_SH(tmp1, tmp0, tmp3, tmp2, t0, t1);
  VSHF_H2_SH(t0, t1, t0, t1, mask0, mask1, t2, t3);
  ADDSUB2(t2, t3, t0, t1);
  VSHF_H2_SH(t0, t0, t1, t1, mask2, mask3, t3, t2);
  tmp0 = __msa_hadd_s_w(t3, t3);
  tmp2 = __msa_hsub_s_w(t3, t3);
  ADDVI_W2_SW(tmp0, 7, tmp2, 7, tmp0, tmp2);
  SRAI_W2_SW(tmp0, tmp2, 4);
  FILL_W2_SW(12000, 51000, tmp1, tmp3);
  DPADD_SH2_SW(t2, t2, cnst0, cnst1, tmp3, tmp1);
  SRAI_W2_SW(tmp1, tmp3, 16);
  UNPCK_R_SH_SW(t1, tmp4);
  tmp5 = __msa_ceqi_w(tmp4, 0);
  tmp4 = (v4i32)__msa_nor_v((v16u8)tmp5, (v16u8)tmp5);
  tmp5 = __msa_fill_w(1);
  tmp5 = (v4i32)__msa_and_v((v16u8)tmp5, (v16u8)tmp4);
  tmp1 += tmp5;
  PCKEV_H2_SH(tmp1, tmp0, tmp3, tmp2, t0, t1);
  out0 = __msa_copy_s_d((v2i64)t0, 0);
  out1 = __msa_copy_s_d((v2i64)t0, 1);
  out2 = __msa_copy_s_d((v2i64)t1, 0);
  out3 = __msa_copy_s_d((v2i64)t1, 1);
  SD4(out0, out1, out2, out3, out, 8);
}

static void FTransformWHT(const int16_t* in, int16_t* out) {
  v8i16 in0 = { 0 };
  v8i16 in1 = { 0 };
  v8i16 tmp0, tmp1, tmp2, tmp3;
  v8i16 out0, out1;
  const v8i16 mask0 = { 0, 1, 2, 3, 8, 9, 10, 11 };
  const v8i16 mask1 = { 4, 5, 6, 7, 12, 13, 14, 15 };
  const v8i16 mask2 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask3 = { 3, 7, 11, 15, 2, 6, 10, 14 };

  in0 = __msa_insert_h(in0, 0, in[  0]);
  in0 = __msa_insert_h(in0, 1, in[ 64]);
  in0 = __msa_insert_h(in0, 2, in[128]);
  in0 = __msa_insert_h(in0, 3, in[192]);
  in0 = __msa_insert_h(in0, 4, in[ 16]);
  in0 = __msa_insert_h(in0, 5, in[ 80]);
  in0 = __msa_insert_h(in0, 6, in[144]);
  in0 = __msa_insert_h(in0, 7, in[208]);
  in1 = __msa_insert_h(in1, 0, in[ 48]);
  in1 = __msa_insert_h(in1, 1, in[112]);
  in1 = __msa_insert_h(in1, 2, in[176]);
  in1 = __msa_insert_h(in1, 3, in[240]);
  in1 = __msa_insert_h(in1, 4, in[ 32]);
  in1 = __msa_insert_h(in1, 5, in[ 96]);
  in1 = __msa_insert_h(in1, 6, in[160]);
  in1 = __msa_insert_h(in1, 7, in[224]);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask2, mask3, in0, in1);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, out0, out1);
  SRAI_H2_SH(out0, out1, 1);
  ST_SH2(out0, out1, out, 8);
}

static int TTransform(const uint8_t* in, const uint16_t* w) {
  int sum;
  uint32_t in0_m, in1_m, in2_m, in3_m;
  v16i8 src0;
  v8i16 in0, in1, tmp0, tmp1, tmp2, tmp3;
  v4i32 dst0, dst1;
  const v16i8 zero = { 0 };
  const v8i16 mask0 = { 0, 1, 2, 3, 8, 9, 10, 11 };
  const v8i16 mask1 = { 4, 5, 6, 7, 12, 13, 14, 15 };
  const v8i16 mask2 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask3 = { 3, 7, 11, 15, 2, 6, 10, 14 };

  LW4(in, BPS, in0_m, in1_m, in2_m, in3_m);
  INSERT_W4_SB(in0_m, in1_m, in2_m, in3_m, src0);
  ILVRL_B2_SH(zero, src0, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask2, mask3, in0, in1);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask2, mask3, in0, in1);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, tmp0, tmp1);
  tmp0 = __msa_add_a_h(tmp0, (v8i16)zero);
  tmp1 = __msa_add_a_h(tmp1, (v8i16)zero);
  LD_SH2(w, 8, tmp2, tmp3);
  DOTP_SH2_SW(tmp0, tmp1, tmp2, tmp3, dst0, dst1);
  dst0 = dst0 + dst1;
  sum = HADD_SW_S32(dst0);
  return sum;
}

static int Disto4x4(const uint8_t* const a, const uint8_t* const b,
                    const uint16_t* const w) {
  const int sum1 = TTransform(a, w);
  const int sum2 = TTransform(b, w);
  return abs(sum2 - sum1) >> 5;
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

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMSA(void) {
  VP8ITransform = ITransform;
  VP8FTransform = FTransform;
  VP8FTransformWHT = FTransformWHT;

  VP8TDisto4x4 = Disto4x4;
  VP8TDisto16x16 = Disto16x16;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8EncDspInitMSA)

#endif  // WEBP_USE_MSA
