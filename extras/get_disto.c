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

static void Help(void) {
  fprintf(stderr,
          "Usage: get_disto [-ssim][-psnr][-alpha] compressed.webp orig.webp\n"
          "  -ssim ..... print SSIM distortion\n"
          "  -psnr ..... print PSNR distortion (default)\n"
          "  -alpha .... preserve alpha plane\n"
          "  -h ........ this message\n"
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
  const char* name1 = NULL;
  const char* name2 = NULL;

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
    } else if (!strcmp(argv[c], "-h")) {
      help = 1;
      ret = 0;
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
  ret = 0;

 End:
  WebPPictureFree(&pic1);
  WebPPictureFree(&pic2);
  return ret;
}
