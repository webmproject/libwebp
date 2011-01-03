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
         "[options] [in_file] [-h] [-raw] [-o ppm_file]\n\n"
         " -raw:  save the raw YUV samples as a grayscale PGM\n"
         "        file with IMC4 layout.\n"
        );
}

int main(int argc, char *argv[]) {
  const char *in_file = NULL;
  const char *out_file = NULL;
  int raw_output = 0;

  int width, height, stride, uv_stride;
  uint8_t* out = NULL, *u = NULL, *v = NULL;

  int c;
  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h")) {
      help(argv[0]);
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-raw")) {
      raw_output = 1;
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

    if (!raw_output) {
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
      int ok = 1;
      if (!raw_output) {
        fprintf(fout, "P6\n%d %d\n255\n", width, height);
        ok &= (fwrite(out, width * height, 3, fout) == 3);
      } else {
        // Save a grayscale PGM file using the IMC4 layout
        // (http://www.fourcc.org/yuv.php#IMC4). This is a very
        // convenient format for viewing the samples, esp. for
        // odd dimensions.
        int y;
        const int uv_width = (width + 1) / 2;
        const int uv_height = (height + 1) / 2;
        const int out_stride = (width + 1) & ~1;
        fprintf(fout, "P5\n%d %d\n255\n", out_stride, height + uv_height);
        for (y = 0; ok && y < height; ++y) {
          ok &= (fwrite(out + y * stride, width, 1, fout) == 1);
          if (width & 1) fputc(0, fout);    // padding byte
        }
        for (y = 0; ok && y < uv_height; ++y) {
          ok &= (fwrite(u + y * uv_stride, uv_width, 1, fout) == 1);
          ok &= (fwrite(v + y * uv_stride, uv_width, 1, fout) == 1);
        }
      }
      fclose(fout);
      if (ok) {
        printf("Saved file %s\n", out_file);
      } else {
        printf("Error writing file %s !!\n", out_file);
      }
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
