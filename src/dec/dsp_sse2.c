// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// SSE2 version of dsp functions and loop filtering.
//
// Author: somnath@google.com (Somnath Banerjee)

#if defined(__SSE2__) || defined(_MSC_VER)

#include <emmintrin.h>
#include "vp8i.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static inline void SignedShift3(__m128i *a) {
  __m128i t1 = *a;
  // Shift the lower byte of 16 bit by 3 while preserving the sign bit
  t1 = _mm_slli_epi16(t1, 8);
  t1 = _mm_srai_epi16(t1, 3);
  t1 = _mm_srli_epi16(t1, 8);

  // Shift the upper byte of 16 bit by 3 while preserving the sign bit
  *a = _mm_srai_epi16(*a, 11);
  *a = _mm_slli_epi16(*a, 8);

  *a = _mm_or_si128(t1, *a);       // put the two together
}

// 4 columns in, 2 columns out
static void DoFilter2SSE2(__m128i p1, __m128i p0, __m128i q0, __m128i q1,
                          int thresh, __m128i* op, __m128i* oq) {
  __m128i t1, t2, t3;
  __m128i mask = _mm_setzero_si128();

  const __m128i one = _mm_set1_epi8(1);
  const __m128i four = _mm_set1_epi8(4);
  const __m128i lsb_mask = _mm_set1_epi8(0xFE);
  const __m128i sign_bit = _mm_set1_epi8(0x80);

  // Calculate mask
  t3 = _mm_subs_epu8(q1, p1);        // (q1 - p1)
  t1 = _mm_subs_epu8(p1, q1);        // (p1 - q1)
  t1 = _mm_or_si128(t1, t3);         // abs(p1 - q1)
  t1 = _mm_and_si128(t1, lsb_mask);  // set lsb of each byte to zero
  t1 = _mm_srli_epi16(t1, 1);        // abs(p1 - q1) / 2

  t3 = _mm_subs_epu8(p0, q0);        // (p0 - q0)
  t2 = _mm_subs_epu8(q0, p0);        // (q0 - p0)
  t2 = _mm_or_si128(t2, t3);         // abs(p0 - q0)
  t2 = _mm_adds_epu8(t2, t2);        // abs(p0 - q0) * 2
  t2 = _mm_adds_epu8(t2, t1);        // abs(p0 - q0) * 2 + abs(p1 - q1) / 2

  t3 = _mm_set1_epi8(thresh);
  t2 = _mm_subs_epu8(t2, t3);  // abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > thresh
  mask = _mm_cmpeq_epi8(t2, mask);

  // Start work on filters
  p1 = _mm_xor_si128(p1, sign_bit);  // convert to signed values
  q1 = _mm_xor_si128(q1, sign_bit);
  p0 = _mm_xor_si128(p0, sign_bit);
  q0 = _mm_xor_si128(q0, sign_bit);

  p1 = _mm_subs_epi8(p1, q1);        // p1 - q1
  t1 = _mm_subs_epi8(q0, p0);        // q0 - p0
  p1 = _mm_adds_epi8(p1, t1);        // p1 - q1 + 1 * (q0 - p0)
  p1 = _mm_adds_epi8(p1, t1);        // p1 - q1 + 2 * (q0 - p0)
  p1 = _mm_adds_epi8(p1, t1);        // p1 - q1 + 3 * (q0 - p0)
  p1 = _mm_and_si128(mask, p1);      // mask filter values we don't care about

  // Do +4 side
  p1 = _mm_adds_epi8(p1, four);      // 3 * (q0 - p0) + (p1 - q1) + 4
  t1 = p1;
  SignedShift3(&t1);                 // t1 >> 3
  q0 = _mm_subs_epi8(q0, t1);        // q0 -= a
  *oq = _mm_xor_si128(q0, sign_bit); // unoffset

  // Now do +3 side
  p1 = _mm_subs_epi8(p1, one);       // +3 instead of +4
  SignedShift3(&p1);                 // p1 >> 3
  p0 = _mm_adds_epi8(p0, p1);        // p0 += b
  *op = _mm_xor_si128(p0, sign_bit); // unoffset
}

// Reads 8 rows across a vertical edge.
//
// TODO(somnath): Investigate _mm_shuffle* also see if it can be broken into
// two Load4x4() to avoid code duplication.
static void Load8x4(const uint8_t* b, int stride, __m128i* p, __m128i* q) {
  __m128i t1, t2;

  // Load 0th, 1st, 4th and 5th rows
  __m128i r0 =  _mm_cvtsi32_si128(*((int*)&b[0 * stride]));  // 03 02 01 00
  __m128i r1 =  _mm_cvtsi32_si128(*((int*)&b[1 * stride]));  // 13 12 11 10
  __m128i r4 =  _mm_cvtsi32_si128(*((int*)&b[4 * stride]));  // 43 42 41 40
  __m128i r5 =  _mm_cvtsi32_si128(*((int*)&b[5 * stride]));  // 53 52 51 50

  r0 = _mm_unpacklo_epi32(r0, r4);               // 43 42 41 40 03 02 01 00
  r1 = _mm_unpacklo_epi32(r1, r5);               // 53 52 51 50 13 12 11 10

  // t1 = 53 43 52 42 51 41 50 40 13 03 12 02 11 01 10 00
  t1 = _mm_unpacklo_epi8(r0, r1);

  // Load 2nd, 3rd, 6th and 7th rows
  r0 =  _mm_cvtsi32_si128(*((int*)&b[2 * stride]));          // 23 22 21 22
  r1 =  _mm_cvtsi32_si128(*((int*)&b[3 * stride]));          // 33 32 31 30
  r4 =  _mm_cvtsi32_si128(*((int*)&b[6 * stride]));          // 63 62 61 60
  r5 =  _mm_cvtsi32_si128(*((int*)&b[7 * stride]));          // 73 72 71 70

  r0 = _mm_unpacklo_epi32(r0, r4);               // 63 62 61 60 23 22 21 20
  r1 = _mm_unpacklo_epi32(r1, r5);               // 73 72 71 70 33 32 31 30

  // t2 = 73 63 72 62 71 61 70 60 33 23 32 22 31 21 30 20
  t2 = _mm_unpacklo_epi8(r0, r1);

  // t1 = 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00
  // t2 = 73 63 53 43 72 62 52 42 71 61 51 41 70 60 50 40
  r0 = t1;
  t1 = _mm_unpacklo_epi16(t1, t2);
  t2 = _mm_unpackhi_epi16(r0, t2);

  // *p = 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
  // *q = 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
  *p = _mm_unpacklo_epi32(t1, t2);
  *q = _mm_unpackhi_epi32(t1, t2);
}

static inline void Store4x4(__m128i* x, uint8_t* dst, int stride) {
  int i;
  for (i = 0; i < 4; ++i, dst += stride) {
    *((int32_t*)dst) = _mm_cvtsi128_si32(*x);
    *x = _mm_srli_si128(*x, 4);
  }
}

//-----------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

static void SimpleVFilter16SSE2(uint8_t* p, int stride, int thresh) {
  __m128i op, oq;

  // Load
  const __m128i p1 = _mm_loadu_si128((__m128i*)&p[-2 * stride]);
  const __m128i p0 = _mm_loadu_si128((__m128i*)&p[-stride]);
  const __m128i q0 = _mm_loadu_si128((__m128i*)&p[0]);
  const __m128i q1 = _mm_loadu_si128((__m128i*)&p[stride]);

  DoFilter2SSE2(p1, p0, q0, q1, thresh, &op, &oq);

  // Store
  _mm_store_si128((__m128i*)&p[-stride], op);
  _mm_store_si128((__m128i*)p, oq);
}

static void SimpleHFilter16SSE2(uint8_t* p, int stride, int thresh) {
  __m128i t1, t2;
  __m128i p1, p0, q0, q1;
  // Assume the pixels around the edge (|) are numbered as follows
  //                00 01 | 02 03
  //                10 11 | 12 13
  //                 ...  |  ...
  //                e0 e1 | e2 e3
  //                f0 f1 | f2 f3
  p -= 2;  // beginning of the first segment

  // Load
  // p1 = 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
  // q0 = 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
  // p0 = f1 e1 d1 c1 b1 a1 91 81 f0 e0 d0 c0 b0 a0 90 80
  // q1 = f3 e3 d3 c3 b3 a3 93 83 f2 e2 d2 c2 b2 a2 92 82
  Load8x4(p, stride, &p1, &q0);
  Load8x4(p + 8 * stride, stride, &p0, &q1);

  t1 = p1;
  t2 = q0;
  // p1 = f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00
  // p0 = f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
  // q0 = f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
  // q1 = f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03
  p1 = _mm_unpacklo_epi64(p1, p0);
  p0 = _mm_unpackhi_epi64(t1, p0);
  q0 = _mm_unpacklo_epi64(q0, q1);
  q1 = _mm_unpackhi_epi64(t2, q1);

  // Filter
  DoFilter2SSE2(p1, p0, q0, q1, thresh, &t1, &t2);

  // Transpose back to write out
  // p0 = 71 70 61 60 51 50 41 40 31 30 21 20 11 10 01 00
  // p1 = f1 f0 e1 e0 d1 d0 c1 c0 b1 b0 a1 a0 91 90 81 80
  // q0 = 73 72 63 62 53 52 43 42 33 32 23 22 13 12 03 02
  // q1 = f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82
  p0 = _mm_unpacklo_epi8(p1, t1);
  p1 = _mm_unpackhi_epi8(p1, t1);
  q0 = _mm_unpacklo_epi8(t2, q1);
  q1 = _mm_unpackhi_epi8(t2, q1);

  t1 = p0;
  t2 = p1;
  // p0 = 33 32 31 30 23 22 21 20 13 12 11 10 03 02 01 00
  // q0 = 73 72 71 70 63 62 61 60 53 52 51 50 43 42 41 40
  // p1 = b3 b2 b1 b0 a3 a2 a1 a0 93 92 91 90 83 82 81 80
  // q1 = f3 f2 f1 f0 e3 e2 e1 e0 d3 d2 d1 d0 c3 c2 c1 c0
  p0 = _mm_unpacklo_epi16(p0, q0);
  q0 = _mm_unpackhi_epi16(t1, q0);
  p1 = _mm_unpacklo_epi16(p1, q1);
  q1 = _mm_unpackhi_epi16(t2, q1);

  // Store
  Store4x4(&p0, p, stride);
  p += 4 * stride;
  Store4x4(&q0, p, stride);
  p += 4 * stride;
  Store4x4(&p1, p, stride);
  p += 4 * stride;
  Store4x4(&q1, p, stride);
}

static void SimpleVFilter16iSSE2(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16SSE2(p, stride, thresh);
  }
}

static void SimpleHFilter16iSSE2(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16SSE2(p, stride, thresh);
  }
}

extern void VP8DspInitSSE2(void);

void VP8DspInitSSE2(void) {
  VP8SimpleVFilter16 = SimpleVFilter16SSE2;
  VP8SimpleHFilter16 = SimpleHFilter16SSE2;
  VP8SimpleVFilter16i = SimpleVFilter16iSSE2;
  VP8SimpleHFilter16i = SimpleHFilter16iSSE2;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif   //__SSE2__ || _MSC_VER
