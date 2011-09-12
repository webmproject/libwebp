// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// CPU detection
//
// Author: Christian Duvivier (cduvivier@google.com)

#include <stddef.h>  // for NULL

#include "./dsp.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// SSE2 detection.
//

#if defined(__pic__) && defined(__i386__)
static inline void GetCPUInfo(int cpu_info[4], int info_type) {
  __asm__ volatile (
    "mov %%ebx, %%edi\n"
    "cpuid\n"
    "xchg %%edi, %%ebx\n"
    : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
    : "a"(info_type));
}
#elif defined(__i386__) || defined(__x86_64__)
static inline void GetCPUInfo(int cpu_info[4], int info_type) {
  __asm__ volatile (
    "cpuid\n"
    : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
    : "a"(info_type));
}
#elif defined(_MSC_VER)  // Visual C++
#define GetCPUInfo __cpuid
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(_MSC_VER)
static int x86CPUInfo(CPUFeature feature) {
  int cpu_info[4];
  GetCPUInfo(cpu_info, 1);
  if (feature == kSSE2) {
    return 0 != (cpu_info[3] & 0x04000000);
  }
  if (feature == kSSE3) {
    return 0 != (cpu_info[2] & 0x00000001);
  }
  return 0;
}
VP8CPUInfo VP8GetCPUInfo = x86CPUInfo;
#elif defined(__ARM_NEON__)
// define a dummy function to enable turning off NEON at runtime by setting
// VP8DecGetCPUInfo = NULL
static int armCPUInfo(CPUFeature feature) {
  return 1;
}
VP8CPUInfo VP8GetCPUInfo = armCPUInfo;
#else
VP8CPUInfo VP8GetCPUInfo = NULL;
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
