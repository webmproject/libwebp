// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// speed-critical functions.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include "vp8enci.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------
// run-time tables (~4k)

static uint8_t clip1[255 + 510 + 1];    // clips [-255,510] to [0,255]

static int tables_ok = 0;

static void InitTables() {
  if (!tables_ok) {
    int i;
    for (i = -255; i <= 255 + 255; ++i) {
      clip1[255 + i] = (i < 0) ? 0 : (i > 255) ? 255 : i;
    }
    tables_ok = 1;
  }
}

static inline uint8_t clip_8b(int v) {
  return (!(v & ~0xff)) ? v : v < 0 ? 0 : 255;
}

//-----------------------------------------------------------------------------
// Transforms (Paragraph 14.4)

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(ref[(x) + (y) * BPS] + ((v) >> 3))

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)

static void ITransform(const uint8_t* ref, const int16_t* in, uint8_t* dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
    const int a = in[0] + in[8];
    const int b = in[0] - in[8];
    const int c = MUL(in[4], kC2) - MUL(in[12], kC1);
    const int d = MUL(in[4], kC1) + MUL(in[12], kC2);
    tmp[0] = a + d;
    tmp[1] = b + c;
    tmp[2] = b - c;
    tmp[3] = a - d;
    tmp += 4;
    in++;
  }

  tmp = C;
  for (i = 0; i < 4; ++i) {    // horizontal pass
    const int dc = tmp[0] + 4;
    const int a =  dc +  tmp[8];
    const int b =  dc -  tmp[8];
    const int c = MUL(tmp[4], kC2) - MUL(tmp[12], kC1);
    const int d = MUL(tmp[4], kC1) + MUL(tmp[12], kC2);
    STORE(0, i, a + d);
    STORE(1, i, b + c);
    STORE(2, i, b - c);
    STORE(3, i, a - d);
    tmp++;
  }
}

void FTransform(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  int i;
  int tmp[16];
  for (i = 0; i < 4; ++i, src += BPS, ref += BPS) {
    const int d0 = src[0] - ref[0];
    const int d1 = src[1] - ref[1];
    const int d2 = src[2] - ref[2];
    const int d3 = src[3] - ref[3];
    const int a0 = (d0 + d3) << 3;
    const int a1 = (d1 + d2) << 3;
    const int a2 = (d1 - d2) << 3;
    const int a3 = (d0 - d3) << 3;
    tmp[0 + i * 4] = (a0 + a1);
    tmp[1 + i * 4] = (a2 * 2217 + a3 * 5352 + 14500) >> 12;
    tmp[2 + i * 4] = (a0 - a1);
    tmp[3 + i * 4] = (a3 * 2217 - a2 * 5352 +  7500) >> 12;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[12 + i]);
    const int a1 = (tmp[4 + i] + tmp[ 8 + i]);
    const int a2 = (tmp[4 + i] - tmp[ 8 + i]);
    const int a3 = (tmp[0 + i] - tmp[12 + i]);
    out[0 + i] = (a0 + a1 + 7) >> 4;
    out[4 + i] = ((a2 * 2217 + a3 * 5352 + 12000) >> 16) + (a3 != 0);
    out[8 + i] = (a0 - a1 + 7) >> 4;
    out[12+ i] = ((a3 * 2217 - a2 * 5352 + 51000) >> 16);
  }
}

static void ITransformWHT(const int16_t* in, int16_t* out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
    const int a0 = in[0 + i] + in[12 + i];
    const int a1 = in[4 + i] + in[ 8 + i];
    const int a2 = in[4 + i] - in[ 8 + i];
    const int a3 = in[0 + i] - in[12 + i];
    tmp[0  + i] = a0 + a1;
    tmp[8  + i] = a0 - a1;
    tmp[4  + i] = a3 + a2;
    tmp[12 + i] = a3 - a2;
  }
  for (i = 0; i < 4; ++i) {
    const int dc = tmp[0 + i * 4] + 3;    // w/ rounder
    const int a0 = dc             + tmp[3 + i * 4];
    const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
    const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
    const int a3 = dc             - tmp[3 + i * 4];
    out[ 0] = (a0 + a1) >> 3;
    out[16] = (a3 + a2) >> 3;
    out[32] = (a0 - a1) >> 3;
    out[48] = (a3 - a2) >> 3;
    out += 64;
  }
}

static void FTransformWHT(const int16_t* in, int16_t* out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i, in += 64) {
    const int a0 = (in[0 * 16] + in[2 * 16]) << 2;
    const int a1 = (in[1 * 16] + in[3 * 16]) << 2;
    const int a2 = (in[1 * 16] - in[3 * 16]) << 2;
    const int a3 = (in[0 * 16] - in[2 * 16]) << 2;
    tmp[0 + i * 4] = (a0 + a1) + (a0 != 0);
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[8 + i]);
    const int a1 = (tmp[4 + i] + tmp[12+ i]);
    const int a2 = (tmp[4 + i] - tmp[12+ i]);
    const int a3 = (tmp[0 + i] - tmp[8 + i]);
    const int b0 = a0 + a1;
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;
    out[ 0 + i] = (b0 + (b0 > 0) + 3) >> 3;
    out[ 4 + i] = (b1 + (b1 > 0) + 3) >> 3;
    out[ 8 + i] = (b2 + (b2 > 0) + 3) >> 3;
    out[12 + i] = (b3 + (b3 > 0) + 3) >> 3;
  }
}

// default C implementations:
VP8Idct VP8ITransform = ITransform;
VP8Fdct VP8FTransform = FTransform;
VP8WHT VP8ITransformWHT = ITransformWHT;
VP8WHT VP8FTransformWHT = FTransformWHT;

#undef MUL
#undef STORE

//-----------------------------------------------------------------------------
// Intra predictions

#define OUT(x, y) dst[(x) + (y) * BPS]

static inline void Fill(uint8_t* dst, int value, int size) {
  int j;
  for (j = 0; j < size; ++j) {
    memset(dst + j * BPS, value, size);
  }
}

static inline void VerticalPred(uint8_t* dst, const uint8_t* top, int size) {
  int j;
  if (top) {
    for (j = 0; j < size; ++j) memcpy(dst + j * BPS, top, size);
  } else {
    Fill(dst, 127, size);
  }
}

static inline void HorizontalPred(uint8_t* dst, const uint8_t* left, int size) {
  if (left) {
    int j;
    for (j = 0; j < size; ++j) {
      memset(dst + j * BPS, left[j], size);
    }
  } else {
    Fill(dst, 129, size);
  }
}

static inline void TrueMotion(uint8_t* dst, const uint8_t* left,
                              const uint8_t* top, int size) {
  int y;
  if (left) {
    if (top) {
      const uint8_t* const clip = clip1 + 255 - left[-1];
      for (y = 0; y < size; ++y) {
        const uint8_t* const clip_table = clip + left[y];
        int x;
        for (x = 0; x < size; ++x) {
          dst[x] = clip_table[top[x]];
        }
        dst += BPS;
      }
    } else {
      HorizontalPred(dst, left, size);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (top) {
      VerticalPred(dst, top, size);
    } else {
      Fill(dst, 129, size);
    }
  }
}

static inline void DCMode(uint8_t* dst, const uint8_t* left,
                          const uint8_t* top,
                          int size, int round, int shift) {
  int DC = 0;
  int j;
  if (top) {
    for (j = 0; j < size; ++j) DC += top[j];
    if (left) {   // top and left present
      for (j = 0; j < size; ++j) DC += left[j];
    } else {      // top, but no left
      DC += DC;
    }
    DC = (DC + round) >> shift;
  } else if (left) {   // left but no top
    for (j = 0; j < size; ++j) DC += left[j];
    DC += DC;
    DC = (DC + round) >> shift;
  } else {   // no top, no left, nothing.
    DC = 0x80;
  }
  Fill(dst, DC, size);
}

//-----------------------------------------------------------------------------
// Chroma 8x8 prediction (paragraph 12.2)

static void IntraChromaPreds(uint8_t* dst, const uint8_t* left,
                             const uint8_t* top) {
  // U block
  DCMode(C8DC8 + dst, left, top, 8, 8, 4);
  VerticalPred(C8VE8 + dst, top, 8);
  HorizontalPred(C8HE8 + dst, left, 8);
  TrueMotion(C8TM8 + dst, left, top, 8);
  // V block
  dst += 8;
  if (top) top += 8;
  if (left) left += 16;
  DCMode(C8DC8 + dst, left, top, 8, 8, 4);
  VerticalPred(C8VE8 + dst, top, 8);
  HorizontalPred(C8HE8 + dst, left, 8);
  TrueMotion(C8TM8 + dst, left, top, 8);
}

//-----------------------------------------------------------------------------
// luma 16x16 prediction (paragraph 12.3)

static void Intra16Preds(uint8_t* dst,
                         const uint8_t* left, const uint8_t* top) {
  DCMode(I16DC16 + dst, left, top, 16, 16, 5);
  VerticalPred(I16VE16 + dst, top, 16);
  HorizontalPred(I16HE16 + dst, left, 16);
  TrueMotion(I16TM16 + dst, left, top, 16);
}

//-----------------------------------------------------------------------------
// luma 4x4 prediction

#define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

static void VE4(uint8_t* dst, const uint8_t* top) {    // vertical
  const uint8_t vals[4] = {
    AVG3(top[-1], top[0], top[1]),
    AVG3(top[ 0], top[1], top[2]),
    AVG3(top[ 1], top[2], top[3]),
    AVG3(top[ 2], top[3], top[4])
  };
  int i;
  for (i = 0; i < 4; ++i) {
    memcpy(dst + i * BPS, vals, 4);
  }
}

static void HE4(uint8_t* dst, const uint8_t* top) {    // horizontal
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  *(uint32_t*)(dst + 0 * BPS) = 0x01010101U * AVG3(X, I, J);
  *(uint32_t*)(dst + 1 * BPS) = 0x01010101U * AVG3(I, J, K);
  *(uint32_t*)(dst + 2 * BPS) = 0x01010101U * AVG3(J, K, L);
  *(uint32_t*)(dst + 3 * BPS) = 0x01010101U * AVG3(K, L, L);
}

static void DC4(uint8_t* dst, const uint8_t* top) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += top[i] + top[-5 + i];
  Fill(dst, dc >> 3, 4);
}

static void RD4(uint8_t* dst, const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  OUT(0, 3)                                     = AVG3(J, K, L);
  OUT(0, 2) = OUT(1, 3)                         = AVG3(I, J, K);
  OUT(0, 1) = OUT(1, 2) = OUT(2, 3)             = AVG3(X, I, J);
  OUT(0, 0) = OUT(1, 1) = OUT(2, 2) = OUT(3, 3) = AVG3(A, X, I);
  OUT(1, 0) = OUT(2, 1) = OUT(3, 2)             = AVG3(B, A, X);
  OUT(2, 0) = OUT(3, 1)                         = AVG3(C, B, A);
  OUT(3, 0)                                     = AVG3(D, C, B);
}

static void LD4(uint8_t* dst, const uint8_t* top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  OUT(0, 0)                                     = AVG3(A, B, C);
  OUT(1, 0) = OUT(0, 1)                         = AVG3(B, C, D);
  OUT(2, 0) = OUT(1, 1) = OUT(0, 2)             = AVG3(C, D, E);
  OUT(3, 0) = OUT(2, 1) = OUT(1, 2) = OUT(0, 3) = AVG3(D, E, F);
  OUT(3, 1) = OUT(2, 2) = OUT(1, 3)             = AVG3(E, F, G);
  OUT(3, 2) = OUT(2, 3)                         = AVG3(F, G, H);
  OUT(3, 3)                                     = AVG3(G, H, H);
}

static void VR4(uint8_t* dst, const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  OUT(0, 0) = OUT(1, 2) = AVG2(X, A);
  OUT(1, 0) = OUT(2, 2) = AVG2(A, B);
  OUT(2, 0) = OUT(3, 2) = AVG2(B, C);
  OUT(3, 0)             = AVG2(C, D);

  OUT(0, 3) =             AVG3(K, J, I);
  OUT(0, 2) =             AVG3(J, I, X);
  OUT(0, 1) = OUT(1, 3) = AVG3(I, X, A);
  OUT(1, 1) = OUT(2, 3) = AVG3(X, A, B);
  OUT(2, 1) = OUT(3, 3) = AVG3(A, B, C);
  OUT(3, 1) =             AVG3(B, C, D);
}

static void VL4(uint8_t* dst, const uint8_t* top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  OUT(0, 0) =             AVG2(A, B);
  OUT(1, 0) = OUT(0, 2) = AVG2(B, C);
  OUT(2, 0) = OUT(1, 2) = AVG2(C, D);
  OUT(3, 0) = OUT(2, 2) = AVG2(D, E);

  OUT(0, 1) =             AVG3(A, B, C);
  OUT(1, 1) = OUT(0, 3) = AVG3(B, C, D);
  OUT(2, 1) = OUT(1, 3) = AVG3(C, D, E);
  OUT(3, 1) = OUT(2, 3) = AVG3(D, E, F);
              OUT(3, 2) = AVG3(E, F, G);
              OUT(3, 3) = AVG3(F, G, H);
}

static void HU4(uint8_t* dst, const uint8_t* top) {
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  OUT(0, 0) =             AVG2(I, J);
  OUT(2, 0) = OUT(0, 1) = AVG2(J, K);
  OUT(2, 1) = OUT(0, 2) = AVG2(K, L);
  OUT(1, 0) =             AVG3(I, J, K);
  OUT(3, 0) = OUT(1, 1) = AVG3(J, K, L);
  OUT(3, 1) = OUT(1, 2) = AVG3(K, L, L);
  OUT(3, 2) = OUT(2, 2) =
  OUT(0, 3) = OUT(1, 3) = OUT(2, 3) = OUT(3, 3) = L;
}

static void HD4(uint8_t* dst, const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];

  OUT(0, 0) = OUT(2, 1) = AVG2(I, X);
  OUT(0, 1) = OUT(2, 2) = AVG2(J, I);
  OUT(0, 2) = OUT(2, 3) = AVG2(K, J);
  OUT(0, 3)             = AVG2(L, K);

  OUT(3, 0)             = AVG3(A, B, C);
  OUT(2, 0)             = AVG3(X, A, B);
  OUT(1, 0) = OUT(3, 1) = AVG3(I, X, A);
  OUT(1, 1) = OUT(3, 2) = AVG3(J, I, X);
  OUT(1, 2) = OUT(3, 3) = AVG3(K, J, I);
  OUT(1, 3)             = AVG3(L, K, J);
}

static void TM4(uint8_t* dst, const uint8_t* top) {
  int x, y;
  const uint8_t* const clip = clip1 + 255 - top[-1];
  for (y = 0; y < 4; ++y) {
    const uint8_t* const clip_table = clip + top[-2 - y];
    for (x = 0; x < 4; ++x) {
      dst[x] = clip_table[top[x]];
    }
    dst += BPS;
  }
}

#undef AVG3
#undef AVG2

// Left samples are top[-5 .. -2], top_left is top[-1], top are
// located at top[0..3], and top right is top[4..7]
static void Intra4Preds(uint8_t* dst, const uint8_t* top) {
  DC4(I4DC4 + dst, top);
  TM4(I4TM4 + dst, top);
  VE4(I4VE4 + dst, top);
  HE4(I4HE4 + dst, top);
  RD4(I4RD4 + dst, top);
  VR4(I4VR4 + dst, top);
  LD4(I4LD4 + dst, top);
  VL4(I4VL4 + dst, top);
  HD4(I4HD4 + dst, top);
  HU4(I4HU4 + dst, top);
}

// default C implementations
VP8Intra4Preds VP8EncPredLuma4 = Intra4Preds;
VP8IntraPreds VP8EncPredLuma16 = Intra16Preds;
VP8IntraPreds VP8EncPredChroma8 = IntraChromaPreds;

//-----------------------------------------------------------------------------
// Metric

static inline int GetSSE(const uint8_t* a, const uint8_t* b, int w, int h) {
  int count = 0;
  int y, x;
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      const int diff = (int)a[x] - b[x];
      count += diff * diff;
    }
    a += BPS;
    b += BPS;
  }
  return count;
}

static int SSE16x16(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 16);
}
static int SSE16x8(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 8);
}
static int SSE8x8(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 8, 8);
}
static int SSE4x4(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 4, 4);
}

// default C implementations
VP8Metric VP8SSE16x16 = SSE16x16;
VP8Metric VP8SSE8x8 = SSE8x8;
VP8Metric VP8SSE16x8 = SSE16x8;
VP8Metric VP8SSE4x4 = SSE4x4;

//-----------------------------------------------------------------------------
// Texture distortion
//
// We try to match the spectral content (weighted) between source and
// reconstructed samples.

// Hadamard transform
static void TTransform(const uint8_t* in, int16_t* out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i, in += BPS) {
    const int a0 = (in[0] + in[2]) << 2;
    const int a1 = (in[1] + in[3]) << 2;
    const int a2 = (in[1] - in[3]) << 2;
    const int a3 = (in[0] - in[2]) << 2;
    tmp[0 + i * 4] = a0 + a1 + (a0 != 0);
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[8 + i]);
    const int a1 = (tmp[4 + i] + tmp[12+ i]);
    const int a2 = (tmp[4 + i] - tmp[12+ i]);
    const int a3 = (tmp[0 + i] - tmp[8 + i]);
    const int b0 = a0 + a1;
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;
    out[ 0 + i] = (b0 + (b0 < 0) + 3) >> 3;
    out[ 4 + i] = (b1 + (b1 < 0) + 3) >> 3;
    out[ 8 + i] = (b2 + (b2 < 0) + 3) >> 3;
    out[12 + i] = (b3 + (b3 < 0) + 3) >> 3;
  }
}

static int Disto4x4(const uint8_t* const a, const uint8_t* const b,
                    const uint16_t* const w) {
  int16_t tmp1[16], tmp2[16];
  int k;
  int D;
  TTransform(a, tmp1);
  TTransform(b, tmp2);
  D = 0;
  for (k = 0; k < 16; ++k)
    D += w[k] * (abs(tmp2[k]) - abs(tmp1[k]));
  return (abs(D) + 8) >> 4;
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

VP8WMetric VP8TDisto4x4 = Disto4x4;
VP8WMetric VP8TDisto16x16 = Disto16x16;

//-----------------------------------------------------------------------------
// Block copy

static inline void Copy(const uint8_t* src, uint8_t* dst, int size) {
  int y;
  for (y = 0; y < size; ++y) {
    memcpy(dst, src, size);
    src += BPS;
    dst += BPS;
  }
}

static void Copy4x4(const uint8_t* src, uint8_t* dst) { Copy(src, dst, 4); }
static void Copy8x8(const uint8_t* src, uint8_t* dst) { Copy(src, dst, 8); }
static void Copy16x16(const uint8_t* src, uint8_t* dst) { Copy(src, dst, 16); }

// default C implementations
VP8BlockCopy VP8Copy4x4 = Copy4x4;
VP8BlockCopy VP8Copy8x8 = Copy8x8;
VP8BlockCopy VP8Copy16x16 = Copy16x16;

//-----------------------------------------------------------------------------

void VP8EncDspInit() {
  InitTables();
  // later we'll plug some SSE2 variant here
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
