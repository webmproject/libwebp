// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Simple tool to load two webp/png/jpg/tiff files and compute PSNR/SSIM.
// This is mostly a wrapper around WebPPictureDistortion().
//
/*
 gcc -o get_disto get_disto.c -O3 -I../ -L../examples -L../imageio \
    -lexample_util -limagedec -lwebp -L/opt/local/lib \
    -lpng -lz -ljpeg -ltiff -lm -lpthread
*/
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"
#include "../imageio/image_dec.h"
#include "../imageio/imageio_util.h"

static size_t ReadPicture(const char* const filename, WebPPicture* const pic,
                          int keep_alpha) {
  const uint8_t* data = NULL;
  size_t data_size = 0;
  WebPImageReader reader = NULL;
  int ok = ImgIoUtilReadFile(filename, &data, &data_size);
  if (!ok) goto Error;

  pic->use_argb = 1;  // force ARGB

  reader = WebPGuessImageReader(data, data_size);
  ok = (reader != NULL) && reader(data, data_size, pic, keep_alpha, NULL);

 Error:
  if (!ok) {
    fprintf(stderr, "Error! Could not process file %s\n", filename);
  }
  free((void*)data);
  return ok ? data_size : 0;
}

// returns the max absolute difference
static int DiffScaleChannel(uint8_t* src1, int stride1,
                            const uint8_t* src2, int stride2,
                            int x_stride, int w, int h, int do_scaling) {
  int x, y;
  uint32_t max = 1;
  for (y = 0; y < h; ++y) {
    uint8_t* const ptr1 = src1 + y * stride1;
    const uint8_t* const ptr2 = src2 + y * stride2;
    for (x = 0; x < w * x_stride; x += x_stride) {
      const uint32_t diff = abs(ptr1[x] - ptr2[x]);
      if (diff > max) max = diff;
      ptr1[x] = diff;
    }
  }

  if (do_scaling) {
    const uint32_t factor = (255u << 16) / max;
    for (y = 0; y < h; ++y) {
      uint8_t* const ptr1 = src1 + y * stride1;
      for (x = 0; x < w * x_stride; x += x_stride) {
        const uint32_t diff = (ptr1[x] * factor) >> 16;
        ptr1[x] = diff;
      }
    }
  }
  return max;
}

static void Help(void) {
  fprintf(stderr,
          "Usage: get_disto [-ssim][-psnr][-alpha] compressed.webp orig.webp\n"
          "  -ssim ..... print SSIM distortion\n"
          "  -psnr ..... print PSNR distortion (default)\n"
          "  -alpha .... preserve alpha plane\n"
          "  -h ........ this message\n"
          "  -o <file> . save the diff map as a WebP lossless file\n"
          "  -scale .... scale the difference map to fit [0..255] range\n"
          " Also handles PNG, JPG and TIFF files, in addition to WebP.\n");
}

int main(int argc, const char *argv[]) {
  WebPPicture pic1, pic2;
  int ret = 1;
  float disto[5];
  size_t size1 = 0, size2 = 0;
  int type = 0;
  int c;
  int help = 0;
  int keep_alpha = 0;
  int scale = 0;
  const char* name1 = NULL;
  const char* name2 = NULL;
  const char* output = NULL;

  if (!WebPPictureInit(&pic1) || !WebPPictureInit(&pic2)) {
    fprintf(stderr, "Can't init pictures\n");
    return 1;
  }

  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-ssim")) {
      type = 1;
    } else if (!strcmp(argv[c], "-psnr")) {
      type = 0;
    } else if (!strcmp(argv[c], "-alpha")) {
      keep_alpha = 1;
    } else if (!strcmp(argv[c], "-scale")) {
      scale = 1;
    } else if (!strcmp(argv[c], "-h")) {
      help = 1;
      ret = 0;
    } else if (!strcmp(argv[c], "-o")) {
      if (++c == argc) {
        fprintf(stderr, "missing file name after %s option.\n", argv[c - 1]);
        goto End;
      }
      output = argv[c];
    } else if (name1 == NULL) {
      name1 = argv[c];
    } else {
      name2 = argv[c];
    }
  }
  if (help || name1 == NULL || name2 == NULL) {
    if (!help) {
      fprintf(stderr, "Error: missing arguments.\n");
    }
    Help();
    goto End;
  }
  if ((size1 = ReadPicture(name1, &pic1, 1)) == 0) {
    goto End;
  }
  if ((size2 = ReadPicture(name2, &pic2, 1)) == 0) {
    goto End;
  }
  if (!keep_alpha) {
    WebPBlendAlpha(&pic1, 0x00000000);
    WebPBlendAlpha(&pic2, 0x00000000);
  }

  if (!WebPPictureDistortion(&pic1, &pic2, type, disto)) {
    fprintf(stderr, "Error while computing the distortion.\n");
    goto End;
  }
  printf("%u %.2f    %.2f %.2f %.2f %.2f\n",
         (unsigned int)size1, disto[4],
         disto[0], disto[1], disto[2], disto[3]);

  if (output != NULL) {
    uint8_t* data = NULL;
    size_t data_size = 0;
    if (pic1.use_argb != pic1.use_argb) {
      fprintf(stderr, "Pictures are not in the same argb format. "
                      "Can't save the difference map.\n");
      goto End;
    }
    if (pic1.use_argb) {
      int n;
      fprintf(stderr, "max absolute differences per channel: ");
      for (n = 0; n < 3; ++n) {    // skip the alpha channel
        const int range = DiffScaleChannel((uint8_t*)pic1.argb + n,
                                           pic1.argb_stride * 4,
                                           (const uint8_t*)pic2.argb + n,
                                           pic2.argb_stride * 4,
                                           4, pic1.width, pic1.height, scale);
        fprintf(stderr, "[%d]", range);
      }
      fprintf(stderr, "\n");
    } else {
      fprintf(stderr, "Can only compute the difference map in ARGB format.\n");
      goto End;
    }
    data_size = WebPEncodeLosslessBGRA((const uint8_t*)pic1.argb,
                                       pic1.width, pic1.height,
                                       pic1.argb_stride * 4,
                                       &data);
    if (data_size == 0) {
      fprintf(stderr, "Error during lossless encoding.\n");
      goto End;
    }
    ret = ImgIoUtilWriteFile(output, data, data_size) ? 0 : 1;
    WebPFree(data);
    if (ret) goto End;
  }
  ret = 0;

 End:
  WebPPictureFree(&pic1);
  WebPPictureFree(&pic2);
  return ret;
}
