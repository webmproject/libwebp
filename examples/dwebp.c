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

#ifdef WEBP_HAVE_PNG
#include <png.h>
#endif

#include "webp/decode.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------

#ifdef WEBP_HAVE_PNG
static void PNGAPI error_function(png_structp png, png_const_charp dummy) {
  longjmp(png_jmpbuf(png), 1);
}

static int WritePNG(FILE* out_file, unsigned char* rgb, int stride,
                    png_uint_32 width, png_uint_32 height) {
  png_structp png;
  png_infop info;
  int y;

  png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                NULL, error_function, NULL);
  if (png == NULL) {
    return 0;
  }
  info = png_create_info_struct(png);
  if (info == NULL) {
    png_destroy_write_struct(&png, NULL);
    return 0;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    return 0;
  }
  png_init_io(png, out_file);
  png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (y = 0; y < height; ++y) {
    png_bytep row = rgb + y * stride;
    png_write_rows(png, &row, 1);
  }
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  return 1;
}
#else

typedef uint32_t png_uint_32;

static int WritePNG(FILE* out_file, unsigned char* rgb, int stride,
                    png_uint_32 width, png_uint_32 height) {
  printf("PNG support not compiled. Please use ./configure --enable-png\n");
  printf("You can run with -ppm flag to decode in PPM format.\n");
  return 0;
}
#endif

typedef enum {
  PNG = 0,
  PPM,
  PGM,
} OutputFileFormat;

static void help(const char *s) {
  printf("Usage: dwebp "
         "[in_file] [-h] [-ppm] [-pgm] [-o out_file]\n\n"
         "Decodes the WebP image file to PNG format [Default]\n"
         "Use following options to convert into alternate image formats:\n"
         " -ppm:  save the raw RGB samples as color PPM\n"
         " -pgm:  save the raw YUV samples as a grayscale PGM\n"
         "        file with IMC4 layout.\n"
        );
}

int main(int argc, const char *argv[]) {
  const char *in_file = NULL;
  const char *out_file = NULL;

  int width, height, stride, uv_stride;
  uint8_t* out = NULL, *u = NULL, *v = NULL;
  OutputFileFormat format = PNG;
  int c;
  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      help(argv[0]);
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-ppm")) {
      format = PPM;
    } else if (!strcmp(argv[c], "-pgm")) {
      format = PGM;
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

    switch (format) {
      case PNG:
      case PPM:
        out = WebPDecodeRGB((const uint8_t*)data, data_size, &width, &height);
        break;
      case PGM:
        out = WebPDecodeYUV((const uint8_t*)data, data_size, &width, &height,
                            &u, &v, &stride, &uv_stride);
        break;
      default:
        free(data);
        return -1;
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
      if (format == PNG) {
        ok &= WritePNG(fout, out, 3 * width, width, height);
      } else if (format == PPM) {
        fprintf(fout, "P6\n%d %d\n255\n", width, height);
        ok &= (fwrite(out, width * height, 3, fout) == 3);
      } else if (format == PGM) {
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
