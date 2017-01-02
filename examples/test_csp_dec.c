/*
 gcc -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a8 \
  test_csp_dec.c -O3 -lm -lpthread -Isrc/webp/ -Isrc/ -I./examples \
  examples/example_util.o src/libwebpdecoder.a -o test_csp_dec
*/
/* reference:
[/home/skal/bryce_big.webp 3668620 bytes]:
   0 | 0.802 | 0x1a058d69
   1 | 0.837 | 0xa0f51e36
   2 | 0.698 | 0xe45e4267
   3 | 0.867 | 0x35b1fae2
   4 | 1.052 | 0x3f1cf239
   5 | 0.992 | 0xc8c280c6
   6 | 1.200 | 0xa9fc61de
   7 | 0.943 | 0xa0f51e36
   8 | 0.737 | 0x35b1fae2
   9 | 1.038 | 0x3f1cf239
   10 | 0.992 | 0xc8c280c6
   11 | 0.762 | 0x5c0857d7
   12 | 0.847 | 0x5c0857d7
*/

#include <stdlib.h>
#include <stdio.h>
#include "../src/webp/decode.h"
#include "../imageio/imageio_util.h"
#include "./stopwatch.h"
#include "../src/dsp/dsp.h"   // for VP8GetCPUInfo

static const WEBP_CSP_MODE kColorspaces[13] = {
  MODE_RGB, MODE_RGBA, MODE_BGR, MODE_BGRA,
  MODE_ARGB, MODE_RGBA_4444, MODE_RGB_565,
  MODE_rgbA, MODE_bgrA, MODE_Argb, MODE_rgbA_4444,
  MODE_YUV, MODE_YUVA
};
static const int kModeBpp[] = { 3, 4, 3, 4, 4, 2, 2, 4, 4, 4, 2, 1, 1 };

static void UpdateCrc(const uint8_t* v, int w, int h, int stride,
                      uint32_t* crc) {
  // simplistic crc
  int y, i;
  for (y = 0; y < h; ++y) {
    for (i = 0; i < w; ++i) {
     *crc = (*crc ^ v[i]) + ((*crc >> 3) * (v[i] + 17));
    }
    v += stride;
  }
}

int main(int argc, const char* argv[]) {
  int c, n;
  int csp_min = 0, csp_max = 12;
  uint32_t crc = 0;
  double elapsed = 0, start;
  int no_fancy = 0;
  Stopwatch watch;
  StopwatchReset(&watch);
  for (n = 1; n < argc; ++n) {
    if (argv[n][0] == '-') {
      if (!strcmp(argv[n], "-noasm")) VP8GetCPUInfo = NULL;
      else if (!strcmp(argv[n], "-rgb")) csp_min = csp_max = 0;
      else if (!strcmp(argv[n], "-rgba")) csp_min = csp_max = 1;
      else if (!strcmp(argv[n], "-bgr")) csp_min = csp_max = 2;
      else if (!strcmp(argv[n], "-bgra")) csp_min = csp_max = 3;
      else if (!strcmp(argv[n], "-argb")) csp_min = csp_max = 4;
      else if (!strcmp(argv[n], "-4444")) csp_min = csp_max = 5;
      else if (!strcmp(argv[n], "-565")) csp_min = csp_max = 6;
      else if (!strcmp(argv[n], "-yuv")) csp_min = csp_max = 10;
      else if (!strcmp(argv[n], "-nofancy")) no_fancy = 1;
      else {
        fprintf(stderr, "Unknown option '%s'\n", argv[n]);
        return -1;
      }
      continue;
    }
    const uint8_t* data = NULL;
    size_t size = 0;
    int warmup = 2;
    if (!ImgIoUtilReadFile(argv[n], &data, &size)) {
      fprintf(stderr, "Couldn't read file %s!\n", argv[n]);
      return -1;
    }
    printf("[%s %d bytes]: \n", argv[n], (int)size);
    for (c = csp_min; c <= csp_max; warmup ? --warmup : ++c) {
      int w, h, ok;
      WebPDecoderConfig config;
      WebPInitDecoderConfig(&config);
      WebPDecBuffer* const buf = &config.output;
      buf->colorspace = kColorspaces[c];
      config.options.no_fancy_upsampling = no_fancy;
      start = StopwatchReadAndReset(&watch);
      ok = WebPDecode(data, size, &config);
      if (ok != VP8_STATUS_OK) {
        fprintf(stderr, "Error decoding file! status=%d\n", ok);
        break;
      }
      w = buf->width;
      h = buf->height;
      elapsed = StopwatchReadAndReset(&watch) - start;
      if (buf->colorspace < MODE_YUV) {
        UpdateCrc((const uint8_t*)buf->u.RGBA.rgba, w * kModeBpp[buf->colorspace], h,
                  buf->u.RGBA.stride, &crc);
      } else {
        const int uv_w = (w + 1) / 2;
        const int uv_h = (h + 1) / 2;
        UpdateCrc(buf->u.YUVA.y, w, h, buf->u.YUVA.y_stride, &crc);
        UpdateCrc(buf->u.YUVA.u, uv_w, uv_h, buf->u.YUVA.u_stride, &crc);
        UpdateCrc(buf->u.YUVA.v, uv_w, uv_h, buf->u.YUVA.v_stride, &crc);
      }
      WebPFreeDecBuffer(buf);
      if (!warmup) printf("   %d | %.3f | 0x%.8x\n", c, elapsed, crc);
    }
    free((void*)data);
  }
  printf("[%d files]\n", argc - 1);
}
