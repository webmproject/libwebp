// Copyright 2010 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Command-line tool for decoding a WebP image
//
//  Compile with:     gcc -o dwebp dwebp.c -lwebpdecode
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WEBP_HAVE_PNG
#include <png.h>
#endif

#ifdef HAVE_WINCODEC_H
#ifdef __MINGW32__
#define INITGUID  // Without this GUIDs are declared extern and fail to link
#endif
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

static int verbose = 0;
#ifndef WEBP_DLL
extern void* VP8GetCPUInfo;   // opaque forward declaration.
#endif

//------------------------------------------------------------------------------

// Output types
typedef enum {
  PNG = 0,
  PPM,
  PGM,
  ALPHA_PLANE_ONLY  // this is for experimenting only
} OutputFileFormat;

#ifdef HAVE_WINCODEC_H

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

static HRESULT CreateOutputStream(const char* out_file_name,
                                  IStream** ppStream) {
  HRESULT hr = S_OK;
  IFS(SHCreateStreamOnFileA(out_file_name, STGM_WRITE | STGM_CREATE, ppStream));
  if (FAILED(hr))
    printf("Error opening output file %s (%08x)\n", out_file_name, hr);
  return hr;
}

static HRESULT WriteUsingWIC(const char* out_file_name, REFGUID container_guid,
                             unsigned char* rgb, int stride,
                             uint32_t width, uint32_t height, int has_alpha) {
  HRESULT hr = S_OK;
  IWICImagingFactory* pFactory = NULL;
  IWICBitmapFrameEncode* pFrame = NULL;
  IWICBitmapEncoder* pEncoder = NULL;
  IStream* pStream = NULL;
  WICPixelFormatGUID pixel_format = has_alpha ? GUID_WICPixelFormat32bppBGRA
                                              : GUID_WICPixelFormat24bppBGR;

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

static int WritePNG(const char* out_file_name,
                    const WebPDecBuffer* const buffer) {
  const uint32_t width = buffer->width;
  const uint32_t height = buffer->height;
  unsigned char* const rgb = buffer->u.RGBA.rgba;
  const int stride = buffer->u.RGBA.stride;
  const int has_alpha = (buffer->colorspace == MODE_BGRA);

  return SUCCEEDED(WriteUsingWIC(out_file_name,
             MAKE_REFGUID(GUID_ContainerFormatPng), rgb, stride, width,
             height, has_alpha));
}

#elif defined(WEBP_HAVE_PNG)    // !HAVE_WINCODEC_H
static void PNGAPI error_function(png_structp png, png_const_charp dummy) {
  (void)dummy;  // remove variable-unused warning
  longjmp(png_jmpbuf(png), 1);
}

static int WritePNG(FILE* out_file, const WebPDecBuffer* const buffer) {
  const uint32_t width = buffer->width;
  const uint32_t height = buffer->height;
  unsigned char* const rgb = buffer->u.RGBA.rgba;
  const int stride = buffer->u.RGBA.stride;
  const int has_alpha = (buffer->colorspace == MODE_RGBA);
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
#else    // !HAVE_WINCODEC_H && !WEBP_HAVE_PNG

typedef uint32_t png_uint_32;

static int WritePNG(FILE* out_file, const WebPDecBuffer* const buffer) {
  (void)out_file;
  (void)buffer;
  printf("PNG support not compiled. Please install the libpng development "
         "package before building.\n");
  printf("You can run with -ppm flag to decode in PPM format.\n");
  return 0;
}
#endif

static int WritePPM(FILE* fout, const WebPDecBuffer* const buffer) {
  const uint32_t width = buffer->width;
  const uint32_t height = buffer->height;
  const unsigned char* const rgb = buffer->u.RGBA.rgba;
  const int stride = buffer->u.RGBA.stride;
  uint32_t y;
  fprintf(fout, "P6\n%d %d\n255\n", width, height);
  for (y = 0; y < height; ++y) {
    if (fwrite(rgb + y * stride, width, 3, fout) != 3) {
      return 0;
    }
  }
  return 1;
}

static int WriteAlphaPlane(FILE* fout, const WebPDecBuffer* const buffer) {
  const uint32_t width = buffer->width;
  const uint32_t height = buffer->height;
  const unsigned char* const a = buffer->u.YUVA.a;
  const int a_stride = buffer->u.YUVA.a_stride;
  uint32_t y;
  assert(a != NULL);
  fprintf(fout, "P5\n%d %d\n255\n", width, height);
  for (y = 0; y < height; ++y) {
    if (fwrite(a + y * a_stride, width, 1, fout) != 1) {
      return 0;
    }
  }
  return 1;
}

static int WritePGM(FILE* fout, const WebPDecBuffer* const buffer) {
  const int width = buffer->width;
  const int height = buffer->height;
  const WebPYUVABuffer* const yuv = &buffer->u.YUVA;
  // Save a grayscale PGM file using the IMC4 layout
  // (http://www.fourcc.org/yuv.php#IMC4). This is a very
  // convenient format for viewing the samples, esp. for
  // odd dimensions.
  int ok = 1;
  int y;
  const int uv_width = (width + 1) / 2;
  const int uv_height = (height + 1) / 2;
  const int out_stride = (width + 1) & ~1;
  const int a_height = yuv->a ? height : 0;
  fprintf(fout, "P5\n%d %d\n255\n", out_stride, height + uv_height + a_height);
  for (y = 0; ok && y < height; ++y) {
    ok &= (fwrite(yuv->y + y * yuv->y_stride, width, 1, fout) == 1);
    if (width & 1) fputc(0, fout);    // padding byte
  }
  for (y = 0; ok && y < uv_height; ++y) {
    ok &= (fwrite(yuv->u + y * yuv->u_stride, uv_width, 1, fout) == 1);
    ok &= (fwrite(yuv->v + y * yuv->v_stride, uv_width, 1, fout) == 1);
  }
  for (y = 0; ok && y < a_height; ++y) {
    ok &= (fwrite(yuv->a + y * yuv->a_stride, width, 1, fout) == 1);
    if (width & 1) fputc(0, fout);    // padding byte
  }
  return ok;
}

static void SaveOutput(const WebPDecBuffer* const buffer,
                       OutputFileFormat format, const char* const out_file) {
  FILE* fout = NULL;
  int needs_open_file = 1;
  int ok = 1;
  Stopwatch stop_watch;

  if (verbose)
    StopwatchReadAndReset(&stop_watch);

#ifdef HAVE_WINCODEC_H
  needs_open_file = (format != PNG);
#endif
  if (needs_open_file) {
    fout = fopen(out_file, "wb");
    if (!fout) {
      fprintf(stderr, "Error opening output file %s\n", out_file);
      return;
    }
  }

  if (format == PNG) {
#ifdef HAVE_WINCODEC_H
    ok &= WritePNG(out_file, buffer);
#else
    ok &= WritePNG(fout, buffer);
#endif
  } else if (format == PPM) {
    ok &= WritePPM(fout, buffer);
  } else if (format == PGM) {
    ok &= WritePGM(fout, buffer);
  } else if (format == ALPHA_PLANE_ONLY) {
    ok &= WriteAlphaPlane(fout, buffer);
  }
  if (fout) {
    fclose(fout);
  }
  if (ok) {
    printf("Saved file %s\n", out_file);
    if (verbose) {
      const double time = StopwatchReadAndReset(&stop_watch);
      printf("Time to write output: %.3fs\n", time);
    }
  } else {
    fprintf(stderr, "Error writing file %s !!\n", out_file);
  }
}

static void Help(void) {
  printf("Usage: dwebp in_file [options] [-o out_file]\n\n"
         "Decodes the WebP image file to PNG format [Default]\n"
         "Use following options to convert into alternate image formats:\n"
         "  -ppm ......... save the raw RGB samples as color PPM\n"
         "  -pgm ......... save the raw YUV samples as a grayscale PGM\n"
         "                 file with IMC4 layout.\n"
         " Other options are:\n"
         "  -version  .... print version number and exit.\n"
         "  -nofancy ..... don't use the fancy YUV420 upscaler.\n"
         "  -nofilter .... disable in-loop filtering.\n"
         "  -mt .......... use multi-threading\n"
         "  -crop <x> <y> <w> <h> ... crop output with the given rectangle\n"
         "  -scale <w> <h> .......... scale the output (*after* any cropping)\n"
#ifdef WEBP_EXPERIMENTAL_FEATURES
         "  -alpha ....... only save the alpha plane.\n"
#endif
         "  -h     ....... this help message.\n"
         "  -v     ....... verbose (e.g. print encoding/decoding times)\n"
#ifndef WEBP_DLL
         "  -noasm ....... disable all assembly optimizations.\n"
#endif
        );
}

static const char* const kStatusMessages[] = {
  "OK", "OUT_OF_MEMORY", "INVALID_PARAM", "BITSTREAM_ERROR",
  "UNSUPPORTED_FEATURE", "SUSPENDED", "USER_ABORT", "NOT_ENOUGH_DATA"
};

int main(int argc, const char *argv[]) {
  const char *in_file = NULL;
  const char *out_file = NULL;

  WebPDecoderConfig config;
  WebPDecBuffer* const output_buffer = &config.output;
  WebPBitstreamFeatures* const bitstream = &config.input;
  OutputFileFormat format = PNG;
  int c;

  if (!WebPInitDecoderConfig(&config)) {
    fprintf(stderr, "Library version mismatch!\n");
    return -1;
  }

  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      Help();
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-alpha")) {
      format = ALPHA_PLANE_ONLY;
    } else if (!strcmp(argv[c], "-nofancy")) {
      config.options.no_fancy_upsampling = 1;
    } else if (!strcmp(argv[c], "-nofilter")) {
      config.options.bypass_filtering = 1;
    } else if (!strcmp(argv[c], "-ppm")) {
      format = PPM;
    } else if (!strcmp(argv[c], "-version")) {
      const int version = WebPGetDecoderVersion();
      printf("%d.%d.%d\n",
        (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
    } else if (!strcmp(argv[c], "-pgm")) {
      format = PGM;
    } else if (!strcmp(argv[c], "-mt")) {
      config.options.use_threads = 1;
    } else if (!strcmp(argv[c], "-crop") && c < argc - 4) {
      config.options.use_cropping = 1;
      config.options.crop_left   = strtol(argv[++c], NULL, 0);
      config.options.crop_top    = strtol(argv[++c], NULL, 0);
      config.options.crop_width  = strtol(argv[++c], NULL, 0);
      config.options.crop_height = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-scale") && c < argc - 2) {
      config.options.use_scaling = 1;
      config.options.scaled_width  = strtol(argv[++c], NULL, 0);
      config.options.scaled_height = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-v")) {
      verbose = 1;
#ifndef WEBP_DLL
    } else if (!strcmp(argv[c], "-noasm")) {
      VP8GetCPUInfo = NULL;
#endif
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
    Stopwatch stop_watch;
    VP8StatusCode status = VP8_STATUS_OK;
    int ok;
    uint32_t data_size = 0;
    void* data = NULL;
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
      fprintf(stderr, "Could not read %d bytes of data from file %s\n",
              data_size, in_file);
      free(data);
      return -1;
    }

    if (verbose)
      StopwatchReadAndReset(&stop_watch);

    status = WebPGetFeatures((const uint8_t*)data, data_size, bitstream);
    if (status != VP8_STATUS_OK) {
      goto end;
    }

    switch (format) {
      case PNG:
#ifdef HAVE_WINCODEC_H
        output_buffer->colorspace = bitstream->has_alpha ? MODE_BGRA : MODE_BGR;
#else
        output_buffer->colorspace = bitstream->has_alpha ? MODE_RGBA : MODE_RGB;
#endif
        break;
      case PPM:
        output_buffer->colorspace = MODE_RGB;  // drops alpha for PPM
        break;
      case PGM:
        output_buffer->colorspace = bitstream->has_alpha ? MODE_YUVA : MODE_YUV;
        break;
      case ALPHA_PLANE_ONLY:
        output_buffer->colorspace = MODE_YUVA;
        break;
      default:
        free(data);
        return -1;
    }
    status = WebPDecode((const uint8_t*)data, data_size, &config);

    if (verbose) {
      const double time = StopwatchReadAndReset(&stop_watch);
      printf("Time to decode picture: %.3fs\n", time);
    }
 end:
    free(data);
    ok = (status == VP8_STATUS_OK);
    if (!ok) {
      fprintf(stderr, "Decoding of %s failed.\n", in_file);
      fprintf(stderr, "Status: %d (%s)\n", status, kStatusMessages[status]);
      return -1;
    }
  }

  if (out_file) {
    printf("Decoded %s. Dimensions: %d x %d%s. Now saving...\n", in_file,
           output_buffer->width, output_buffer->height,
           bitstream->has_alpha ? " (with alpha)" : "");
    SaveOutput(output_buffer, format, out_file);
  } else {
    printf("File %s can be decoded (dimensions: %d x %d)%s.\n",
           in_file, output_buffer->width, output_buffer->height,
           bitstream->has_alpha ? " (with alpha)" : "");
    printf("Nothing written; use -o flag to save the result as e.g. PNG.\n");
  }
  WebPFreeDecBuffer(output_buffer);

  return 0;
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
