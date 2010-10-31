// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  simple command-line example calling libwebpdecode to
//  decode a WebP image into a PPM image.
//
//  Compile with:     gcc -o dwebp dwebp.c -lwebpdecode
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------

static void help(const char *s) {
  printf("Usage: dwebp "
         "[options] [in_file] [-h] [-yuv] [-o ppm_file]\n");
}

int main(int argc, char *argv[]) {
  const char *in_file = NULL;
  const char *out_file = NULL;
  int yuv_out = 0;

  int width, height, stride, uv_stride;
  uint8_t* out = NULL, *u = NULL, *v = NULL;

  int c;
  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h")) {
      help(argv[0]);
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-yuv")) {
      yuv_out = 1;
    } else if (argv[c][0] == '-') {
      printf("Unknown option '%s'\n", argv[c]);
      help(argv[0]);
      return -1;
    } else {
      in_file = argv[c];
    }
  }

  if (in_file == NULL) {
    printf("missing input file!!\n");
    help(argv[0]);
    return -1;
  }

  {
    uint32_t data_size = 0;
    void* data = NULL;
    FILE* const in = fopen(in_file, "rb");
    if (!in) {
      printf("cannot open input file '%s'\n", in_file);
      return 1;
    }
    fseek(in, 0, SEEK_END);
    data_size = ftell(in);
    fseek(in, 0, SEEK_SET);
    data = malloc(data_size);
    const int ok = (fread(data, data_size, 1, in) == 1);
    fclose(in);
    if (!ok) {
      free(data);
      return -1;
    }

    if (!yuv_out) {
      out = WebPDecodeRGB(data, data_size, &width, &height);
    } else {
      out = WebPDecodeYUV(data, data_size, &width, &height,
                          &u, &v, &stride, &uv_stride);
    }
    free(data);
  }

  if (!out) {
    printf("Decoding of %s failed.\n", in_file);
    return -1;
  }

  if (out_file) {
    FILE* const fout = fopen(out_file, "wb");
    if (fout) {
      if (!yuv_out) {
        fprintf(fout, "P6\n%d %d\n255\n", width, height);
        fwrite(out, width * height, 3, fout);
      } else {
        int y;
        fprintf(fout, "P5\n%d %d\n255\n", width, height * 3 / 2);
        for (y = 0; y < height; ++y) {
          fwrite(out + y * stride, width, 1, fout);
        }
        for (y = 0; y < (height + 1) / 2; ++y) {
          fwrite(u + y * uv_stride, (width + 1) / 2, 1, fout);
        }
        for (y = 0; y < (height + 1) / 2; ++y) {
          fwrite(v + y * uv_stride, (width + 1) / 2, 1, fout);
        }
      }
      fclose(fout);
      printf("Saved file %s\n", out_file);
    } else {
      printf("Error opening output file %s\n", out_file);
    }
  }
  printf("Decoded %s. Dimensions: %d x %d.\n", in_file, width, height);
  free(out);

  return 0;
}

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
