// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// ARM NEON version of dsp functions and loop filtering.
//
// Author: somnath@google.com (Somnath Banerjee)

#if defined(__GNUC__) && defined(__ARM_NEON__)

#include "../dec/vp8i.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define QRegs "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",                  \
              "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"

#define FLIP_SIGN_BIT2(a, b, s)                                                \
  "veor     " #a "," #a "," #s "               \n"                             \
  "veor     " #b "," #b "," #s "               \n"                             \

#define FLIP_SIGN_BIT4(a, b, c, d, s)                                          \
  FLIP_SIGN_BIT2(a, b, s)                                                      \
  FLIP_SIGN_BIT2(c, d, s)                                                      \

#define NEEDS_FILTER(p1, p0, q0, q1, thresh, mask)                             \
  "vabd.u8    q15," #p0 "," #q0 "         \n"  /* abs(p0 - q0) */              \
  "vabd.u8    q14," #p1 "," #q1 "         \n"  /* abs(p1 - q1) */              \
  "vqadd.u8   q15, q15, q15               \n"  /* abs(p0 - q0) * 2 */          \
  "vshr.u8    q14, q14, #1                \n"  /* abs(p1 - q1) / 2 */          \
  "vqadd.u8   q15, q15, q14     \n"  /* abs(p0 - q0) * 2 + abs(p1 - q1) / 2 */ \
  "vdup.8     q14, " #thresh "            \n"                                  \
  "vcge.u8   " #mask ", q14, q15          \n"  /* mask <= thresh */

#define GET_BASE_DELTA(p1, p0, q0, q1, o)                                      \
  "vqsub.s8   q15," #q0 "," #p0 "         \n"  /* (q0 - p0) */                 \
  "vqsub.s8  " #o "," #p1 "," #q1 "       \n"  /* (p1 - q1) */                 \
  "vqadd.s8  " #o "," #o ", q15           \n"  /* (p1 - q1) + 1 * (p0 - q0) */ \
  "vqadd.s8  " #o "," #o ", q15           \n"  /* (p1 - q1) + 2 * (p0 - q0) */ \
  "vqadd.s8  " #o "," #o ", q15           \n"  /* (p1 - q1) + 3 * (p0 - q0) */

#define DO_SIMPLE_FILTER(p0, q0, fl)                                           \
  "vmov.i8    q15, #0x03                  \n"                                  \
  "vqadd.s8   q15, q15, " #fl "           \n"  /* filter1 = filter + 3 */      \
  "vshr.s8    q15, q15, #3                \n"  /* filter1 >> 3 */              \
  "vqadd.s8  " #p0 "," #p0 ", q15         \n"  /* p0 += filter1 */             \
                                                                               \
  "vmov.i8    q15, #0x04                  \n"                                  \
  "vqadd.s8   q15, q15, " #fl "           \n"  /* filter1 = filter + 4 */      \
  "vshr.s8    q15, q15, #3                \n"  /* filter2 >> 3 */              \
  "vqsub.s8  " #q0 "," #q0 ", q15         \n"  /* q0 -= filter2 */

// Applies filter on 2 pixels (p0 and q0)
#define DO_FILTER2(p1, p0, q0, q1, thresh)                                     \
  NEEDS_FILTER(p1, p0, q0, q1, thresh, q9)     /* filter mask in q9 */         \
  "vmov.i8    q10, #0x80                  \n"  /* sign bit */                  \
  FLIP_SIGN_BIT4(p1, p0, q0, q1, q10)          /* convert to signed value */   \
  GET_BASE_DELTA(p1, p0, q0, q1, q11)          /* get filter level  */         \
  "vand       q9, q9, q11                 \n"  /* apply filter mask */         \
  DO_SIMPLE_FILTER(p0, q0, q9)                 /* apply filter */              \
  FLIP_SIGN_BIT2(p0, q0, q10)

// Load/Store vertical edge
#define LOAD8x4(c1, c2, c3, c4, b1, b2, stride)                                \
  "vld4.8   {" #c1"[0], " #c2"[0], " #c3"[0], " #c4"[0]}," #b1 "," #stride"\n" \
  "vld4.8   {" #c1"[1], " #c2"[1], " #c3"[1], " #c4"[1]}," #b2 "," #stride"\n" \
  "vld4.8   {" #c1"[2], " #c2"[2], " #c3"[2], " #c4"[2]}," #b1 "," #stride"\n" \
  "vld4.8   {" #c1"[3], " #c2"[3], " #c3"[3], " #c4"[3]}," #b2 "," #stride"\n" \
  "vld4.8   {" #c1"[4], " #c2"[4], " #c3"[4], " #c4"[4]}," #b1 "," #stride"\n" \
  "vld4.8   {" #c1"[5], " #c2"[5], " #c3"[5], " #c4"[5]}," #b2 "," #stride"\n" \
  "vld4.8   {" #c1"[6], " #c2"[6], " #c3"[6], " #c4"[6]}," #b1 "," #stride"\n" \
  "vld4.8   {" #c1"[7], " #c2"[7], " #c3"[7], " #c4"[7]}," #b2 "," #stride"\n"

#define STORE8x2(c1, c2, p,stride)                                             \
  "vst2.8   {" #c1"[0], " #c2"[0]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[1], " #c2"[1]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[2], " #c2"[2]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[3], " #c2"[3]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[4], " #c2"[4]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[5], " #c2"[5]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[6], " #c2"[6]}," #p "," #stride " \n"                      \
  "vst2.8   {" #c1"[7], " #c2"[7]}," #p "," #stride " \n"

//-----------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

static void SimpleVFilter16NEON(uint8_t* p, int stride, int thresh) {
  __asm__ volatile (
    "sub        %[p], %[p], %[stride], lsl #1  \n"  // p -= 2 * stride

    "vld1.u8    {q1}, [%[p]], %[stride]        \n"  // p1
    "vld1.u8    {q2}, [%[p]], %[stride]        \n"  // p0
    "vld1.u8    {q3}, [%[p]], %[stride]        \n"  // q0
    "vld1.u8    {q4}, [%[p]]                   \n"  // q1

    DO_FILTER2(q1, q2, q3, q4, %[thresh])

    "sub        %[p], %[p], %[stride], lsl #1  \n"  // p -= 2 * stride

    "vst1.u8    {q2}, [%[p]], %[stride]        \n"  // store op0
    "vst1.u8    {q3}, [%[p]]                   \n"  // store oq0
    : [p] "+r"(p)
    : [stride] "r"(stride), [thresh] "r"(thresh)
    : "memory", QRegs
  );
}

static void SimpleHFilter16NEON(uint8_t* p, int stride, int thresh) {
  __asm__ volatile (
    "sub        r4, %[p], #2                   \n"  // base1 = p - 2
    "lsl        r6, %[stride], #1              \n"  // r6 = 2 * stride
    "add        r5, r4, %[stride]              \n"  // base2 = base1 + stride

    LOAD8x4(d2, d3, d4, d5, [r4], [r5], r6)
    LOAD8x4(d6, d7, d8, d9, [r4], [r5], r6)
    "vswp       d3, d6                         \n"  // p1:q1 p0:q3
    "vswp       d5, d8                         \n"  // q0:q2 q1:q4
    "vswp       q2, q3                         \n"  // p1:q1 p0:q2 q0:q3 q1:q4

    DO_FILTER2(q1, q2, q3, q4, %[thresh])

    "sub        %[p], %[p], #1                 \n"  // p - 1

    "vswp        d5, d6                        \n"
    STORE8x2(d4, d5, [%[p]], %[stride])
    STORE8x2(d6, d7, [%[p]], %[stride])

    : [p] "+r"(p)
    : [stride] "r"(stride), [thresh] "r"(thresh)
    : "memory", "r4", "r5", "r6", QRegs
  );
}

static void SimpleVFilter16iNEON(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16NEON(p, stride, thresh);
  }
}

static void SimpleHFilter16iNEON(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16NEON(p, stride, thresh);
  }
}

extern void VP8DspInitNEON(void);

void VP8DspInitNEON(void) {
  VP8SimpleVFilter16 = SimpleVFilter16NEON;
  VP8SimpleHFilter16 = SimpleHFilter16NEON;
  VP8SimpleVFilter16i = SimpleVFilter16iNEON;
  VP8SimpleHFilter16i = SimpleHFilter16iNEON;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif   // __GNUC__ && __ARM_NEON__
