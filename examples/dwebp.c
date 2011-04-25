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

#ifdef _WIN32
#define CINTERFACE
#define COBJMACROS
#define _WIN32_IE 0x500  // Workaround bug in shlwapi.h when compiling C++
                         // code with COBJMACROS.
#include <shlwapi.h>
#include <windows.h>
#include <wincodec.h>
#endif

#include "webp/decode.h"
#include "stopwatch.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//-----------------------------------------------------------------------------

static int verbose = 0;

#ifdef _WIN32

#define IFS(fn)                \
  do {                         \
     if (SUCCEEDED(hr))        \
     {                         \
        hr = (fn);             \
        if (FAILED(hr) && verbose)           \
          printf(#fn " failed %08x\n", hr);  \
     }                         \
  } while (0)

#ifdef __cplusplus
#define MAKE_REFGUID(x) (x)
#else
#define MAKE_REFGUID(x) &(x)
#endif

static HRESULT CreateOutputStream(const char* out_file_name, IStream** ppStream) {
  HRESULT hr = S_OK;
  IFS(SHCreateStreamOnFileA(out_file_name, STGM_WRITE | STGM_CREATE, ppStream));
  if (FAILED(hr))
    printf("Error opening output file %s (%08x)\n", out_file_name, hr);
  return hr;
}

static HRESULT WriteUsingWIC(const char* out_file_name, REFGUID container_guid,
                             unsigned char* rgb, int stride,
                             uint32_t width, uint32_t height) {
  HRESULT hr = S_OK;
  IWICImagingFactory* pFactory = NULL;
  IWICBitmapFrameEncode* pFrame = NULL;
  IWICBitmapEncoder* pEncoder = NULL;
  IStream* pStream = NULL;
  GUID pixel_format = GUID_WICPixelFormat24bppBGR;

  IFS(CoInitialize(NULL));
  IFS(CoCreateInstance(MAKE_REFGUID(CLSID_WICImagingFactory), NULL,
          CLSCTX_INPROC_SERVER, MAKE_REFGUID(IID_IWICImagingFactory),
          (LPVOID*)&pFactory));
  if (hr == REGDB_E_CLASSNOTREG) {
    printf("Couldn't access Windows Imaging Component (are you running \n");
    printf("Windows XP SP3 or newer?). PNG support not available.\n");
    printf("Use -ppm or -pgm for available PPM and PGM formats.\n");
  }
  IFS(CreateOutputStream(out_file_name, &pStream));
  IFS(IWICImagingFactory_CreateEncoder(pFactory, container_guid, NULL,
          &pEncoder));
  IFS(IWICBitmapEncoder_Initialize(pEncoder, pStream,
                                   WICBitmapEncoderNoCache));
  IFS(IWICBitmapEncoder_CreateNewFrame(pEncoder, &pFrame, NULL));
  IFS(IWICBitmapFrameEncode_Initialize(pFrame, NULL));
  IFS(IWICBitmapFrameEncode_SetSize(pFrame, width, height));
  IFS(IWICBitmapFrameEncode_SetPixelFormat(pFrame, &pixel_format));
  IFS(IWICBitmapFrameEncode_WritePixels(pFrame, height, stride,
          height * stride, rgb));
  IFS(IWICBitmapFrameEncode_Commit(pFrame));
  IFS(IWICBitmapEncoder_Commit(pEncoder));

  if (pFrame != NULL) IUnknown_Release(pFrame);
  if (pEncoder != NULL) IUnknown_Release(pEncoder);
  if (pFactory != NULL) IUnknown_Release(pFactory);
  if (pStream != NULL) IUnknown_Release(pStream);
  return hr;
}

static int WritePNG(const char* out_file_name, unsigned char* rgb, int stride,
                    uint32_t width, uint32_t height, int has_alpha) {
  assert(!has_alpha);   // TODO(mikolaj)
  return SUCCEEDED(WriteUsingWIC(out_file_name,
             MAKE_REFGUID(GUID_ContainerFormatPng), rgb, stride, width,
             height));
}

#elif defined(WEBP_HAVE_PNG)    // !WIN32
static void PNGAPI error_function(png_structp png, png_const_charp dummy) {
  (void)dummy;  // remove variable-unused warning
  longjmp(png_jmpbuf(png), 1);
}

static int WritePNG(FILE* out_file, unsigned char* rgb, int stride,
                    png_uint_32 width, png_uint_32 height, int has_alpha) {
  png_structp png;
  png_infop info;
  png_uint_32 y;

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
  png_set_IHDR(png, info, width, height, 8,
               has_alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
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
#else    // !WIN32 && !WEBP_HAVE_PNG

typedef uint32_t png_uint_32;

static int WritePNG(FILE* out_file, unsigned char* rgb, int stride,
                    png_uint_32 width, png_uint_32 height, int has_alpha) {
  printf("PNG support not compiled. Please install the libpng development "
         "package before building.\n");
  printf("You can run with -ppm flag to decode in PPM format.\n");
  return 0;
}
#endif

static int WritePPM(FILE* fout, const unsigned char* rgb,
                    uint32_t width, uint32_t height) {
  fprintf(fout, "P6\n%d %d\n255\n", width, height);
  return (fwrite(rgb, width * height, 3, fout) == 3);
}

static int WriteAlphaPlane(FILE* fout, const unsigned char* rgba,
                           uint32_t width, uint32_t height) {
  uint32_t y;
  fprintf(fout, "P5\n%d %d\n255\n", width, height);
  for (y = 0; y < height; ++y) {
    const unsigned char* line = rgba + y * (width * 4);
    uint32_t x;
    for (x = 0; x < width; ++x) {
      if (fputc(line[4 * x + 3], fout) == EOF) {
        return 0;
      }
    }
  }
  return 1;
}

static int WritePGM(FILE* fout,
                    unsigned char* y_plane, unsigned char *u, unsigned char* v,
                    int y_stride, int uv_stride,
                    uint32_t width, uint32_t height) {
  // Save a grayscale PGM file using the IMC4 layout
  // (http://www.fourcc.org/yuv.php#IMC4). This is a very
  // convenient format for viewing the samples, esp. for
  // odd dimensions.
  int ok = 1;
  unsigned int y;
  const unsigned int uv_width = (width + 1) / 2;
  const unsigned int uv_height = (height + 1) / 2;
  const unsigned int out_stride = (width + 1) & ~1;
  fprintf(fout, "P5\n%d %d\n255\n", out_stride, height + uv_height);
  for (y = 0; ok && y < height; ++y) {
    ok &= (fwrite(y_plane + y * y_stride, width, 1, fout) == 1);
    if (width & 1) fputc(0, fout);    // padding byte
  }
  for (y = 0; ok && y < uv_height; ++y) {
    ok &= (fwrite(u + y * uv_stride, uv_width, 1, fout) == 1);
    ok &= (fwrite(v + y * uv_stride, uv_width, 1, fout) == 1);
  }
  return ok;
}

typedef enum {
  PNG = 0,
  PPM,
  PGM,
  ALPHA_PLANE_ONLY  // this is for experimenting only
} OutputFileFormat;

static void Help(void) {
  printf("Usage: dwebp "
         "[in_file] [-h] [-v] [-ppm] [-pgm] [-version] [-o out_file]\n\n"
         "Decodes the WebP image file to PNG format [Default]\n"
         "Use following options to convert into alternate image formats:\n"
         " -ppm:  save the raw RGB samples as color PPM\n"
         " -pgm:  save the raw YUV samples as a grayscale PGM\n"
         "        file with IMC4 layout.\n"
         " -version: print version number and exit.\n"
         "Use -v for verbose (e.g. print encoding/decoding times)\n"
        );
}

int main(int argc, const char *argv[]) {
  const char *in_file = NULL;
  const char *out_file = NULL;

  int width, height, stride, uv_stride;
  int has_alpha = 0;
  uint8_t* out = NULL, *u = NULL, *v = NULL;
  OutputFileFormat format = PNG;
  Stopwatch stop_watch;
  int c;
  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      Help();
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-alpha")) {
      format = ALPHA_PLANE_ONLY;
    } else if (!strcmp(argv[c], "-ppm")) {
      format = PPM;
    } else if (!strcmp(argv[c], "-version")) {
      const int version = WebPGetDecoderVersion();
      printf("%d.%d.%d\n",
        (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
    } else if (!strcmp(argv[c], "-pgm")) {
      format = PGM;
    } else if (!strcmp(argv[c], "-v")) {
      verbose = 1;
    } else if (argv[c][0] == '-') {
      printf("Unknown option '%s'\n", argv[c]);
      Help();
      return -1;
    } else {
      in_file = argv[c];
    }
  }

  if (in_file == NULL) {
    printf("missing input file!!\n");
    Help();
    return -1;
  }

  {
    uint32_t data_size = 0;
    void* data = NULL;
    int ok;
    FILE* const in = fopen(in_file, "rb");
    if (!in) {
      fprintf(stderr, "cannot open input file '%s'\n", in_file);
      return 1;
    }
    fseek(in, 0, SEEK_END);
    data_size = ftell(in);
    fseek(in, 0, SEEK_SET);
    data = malloc(data_size);
    ok = (fread(data, data_size, 1, in) == 1);
    fclose(in);
    if (!ok) {
      free(data);
      return -1;
    }

    if (verbose)
      StopwatchReadAndReset(&stop_watch);
    switch (format) {
      case PNG:
#ifdef _WIN32
        out = WebPDecodeBGR((const uint8_t*)data, data_size, &width, &height);
        stride = 3 * width;
        has_alpha = 0;
#else
        out = WebPDecodeRGBA((const uint8_t*)data, data_size, &width, &height);
        stride = 4 * width;
        has_alpha = 1;
#endif
        break;
      case PPM:
        out = WebPDecodeRGB((const uint8_t*)data, data_size, &width, &height);
        break;
      case PGM:
        out = WebPDecodeYUV((const uint8_t*)data, data_size, &width, &height,
                            &u, &v, &stride, &uv_stride);
        break;
      case ALPHA_PLANE_ONLY:
        out = WebPDecodeRGBA((const uint8_t*)data, data_size, &width, &height);
        break;
      default:
        free(data);
        return -1;
    }

    if (verbose) {
      const double time = StopwatchReadAndReset(&stop_watch);
      printf("Time to decode picture: %.3fs\n", time);
    }

    free(data);
  }

  if (!out) {
    fprintf(stderr, "Decoding of %s failed.\n", in_file);
    return -1;
  }

  if (out_file) {
    FILE* fout = NULL;
    int needs_open_file = 0;

    printf("Decoded %s. Dimensions: %d x %d. Now saving...\n", in_file, width, height);
    StopwatchReadAndReset(&stop_watch);
#ifdef _WIN32
    if (format != PNG) {
      needs_open_file = 1;
    }
#else
    needs_open_file = 1;
#endif
    if (needs_open_file) fout = fopen(out_file, "wb");
    if (!needs_open_file || fout) {
      int ok = 1;
      if (format == PNG) {
#ifdef _WIN32
        ok &= WritePNG(out_file, out, stride, width, height, has_alpha);
#else
        ok &= WritePNG(fout, out, stride, width, height, has_alpha);
#endif
      } else if (format == PPM) {
        ok &= WritePPM(fout, out, width, height);
      } else if (format == PGM) {
        ok &= WritePGM(fout, out, u, v, stride, uv_stride, width, height);
      } else if (format == ALPHA_PLANE_ONLY) {
        ok &= WriteAlphaPlane(fout, out, width, height);
      }
      if (fout)
        fclose(fout);
      if (ok) {
        printf("Saved file %s\n", out_file);
        if (verbose) {
          const double time = StopwatchReadAndReset(&stop_watch);
          printf("Time to write output: %.3fs\n", time);
        }
      } else {
        fprintf(stderr, "Error writing file %s !!\n", out_file);
      }
    } else {
      fprintf(stderr, "Error opening output file %s\n", out_file);
    }
  } else {
    printf("File %s can be decoded (dimensions: %d x %d).\n",
           in_file, width, height);
    printf("Nothing written; use -o flag to save the result as e.g. PNG.\n");
  }
  free(out);

  return 0;
}

//-----------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
