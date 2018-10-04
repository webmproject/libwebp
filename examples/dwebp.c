// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Command-line tool for decoding a WebP image.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

#include "../examples/example_util.h"
#include "../imageio/image_enc.h"
#include "../imageio/webpdec.h"
#include "./stopwatch.h"

#include "./unicode.h"

static int verbose = 0;
static int quiet = 0;
#ifndef WEBP_DLL
#ifdef __cplusplus
extern "C" {
#endif

extern void* VP8GetCPUInfo;   // opaque forward declaration.

#ifdef __cplusplus
}    // extern "C"
#endif
#endif  // WEBP_DLL


static int SaveOutput(const WebPDecBuffer* const buffer,
                      WebPOutputFileFormat format, const GCHAR* const out_file) {
  const int use_stdout = (out_file != NULL) && !STRCMP(out_file, "-");
  int ok = 1;
  Stopwatch stop_watch;

  if (verbose) {
    StopwatchReset(&stop_watch);
  }
  ok = WebPSaveImage(buffer, format, (const char*)out_file);

  if (ok) {
    if (!quiet) {
      if (use_stdout) {
        fprintf(stderr, "Saved to stdout\n");
      } else {
        FPRINTF(stderr, "Saved file %s\n", out_file);
      }
    }
    if (verbose) {
      const double write_time = StopwatchReadAndReset(&stop_watch);
      fprintf(stderr, "Time to write output: %.3fs\n", write_time);
    }
  } else {
    if (use_stdout) {
      fprintf(stderr, "Error writing to stdout !!\n");
    } else {
      FPRINTF(stderr, "Error writing file %s !!\n", out_file);
    }
  }
  return ok;
}

static void Help(void) {
  printf("Usage: dwebp in_file [options] [-o out_file]\n\n"
         "Decodes the WebP image file to PNG format [Default]\n"
         "Use following options to convert into alternate image formats:\n"
         "  -pam ......... save the raw RGBA samples as a color PAM\n"
         "  -ppm ......... save the raw RGB samples as a color PPM\n"
         "  -bmp ......... save as uncompressed BMP format\n"
         "  -tiff ........ save as uncompressed TIFF format\n"
         "  -pgm ......... save the raw YUV samples as a grayscale PGM\n"
         "                 file with IMC4 layout\n"
         "  -yuv ......... save the raw YUV samples in flat layout\n"
         "\n"
         " Other options are:\n"
         "  -version ..... print version number and exit\n"
         "  -nofancy ..... don't use the fancy YUV420 upscaler\n"
         "  -nofilter .... disable in-loop filtering\n"
         "  -nodither .... disable dithering\n"
         "  -dither <d> .. dithering strength (in 0..100)\n"
         "  -alpha_dither  use alpha-plane dithering if needed\n"
         "  -mt .......... use multi-threading\n"
         "  -crop <x> <y> <w> <h> ... crop output with the given rectangle\n"
         "  -resize <w> <h> ......... scale the output (*after* any cropping)\n"
         "  -flip ........ flip the output vertically\n"
         "  -alpha ....... only save the alpha plane\n"
         "  -incremental . use incremental decoding (useful for tests)\n"
         "  -h ........... this help message\n"
         "  -v ........... verbose (e.g. print encoding/decoding times)\n"
         "  -quiet ....... quiet mode, don't print anything\n"
#ifndef WEBP_DLL
         "  -noasm ....... disable all assembly optimizations\n"
#endif
        );
}

static const char* const kFormatType[] = {
  "unspecified", "lossy", "lossless"
};

static uint8_t* AllocateExternalBuffer(WebPDecoderConfig* config,
                                       WebPOutputFileFormat format,
                                       int use_external_memory) {
  uint8_t* external_buffer = NULL;
  WebPDecBuffer* const output_buffer = &config->output;
  int w = config->input.width;
  int h = config->input.height;
  if (config->options.use_scaling) {
    w = config->options.scaled_width;
    h = config->options.scaled_height;
  } else if (config->options.use_cropping) {
    w = config->options.crop_width;
    h = config->options.crop_height;
  }
  if (format >= RGB && format <= rgbA_4444) {
    const int bpp = (format == RGB || format == BGR) ? 3
                  : (format == RGBA_4444 || format == rgbA_4444 ||
                     format == RGB_565) ? 2
                  : 4;
    uint32_t stride = bpp * w + 7;   // <- just for exercising
    external_buffer = (uint8_t*)malloc(stride * h);
    if (external_buffer == NULL) return NULL;
    output_buffer->u.RGBA.stride = stride;
    output_buffer->u.RGBA.size = stride * h;
    output_buffer->u.RGBA.rgba = external_buffer;
  } else {    // YUV and YUVA
    const int has_alpha = WebPIsAlphaMode(output_buffer->colorspace);
    uint8_t* tmp;
    uint32_t stride = w + 3;
    uint32_t uv_stride = (w + 1) / 2 + 13;
    uint32_t total_size = stride * h * (has_alpha ? 2 : 1)
                        + 2 * uv_stride * (h + 1) / 2;
    assert(format >= YUV && format <= YUVA);
    external_buffer = (uint8_t*)malloc(total_size);
    if (external_buffer == NULL) return NULL;
    tmp = external_buffer;
    output_buffer->u.YUVA.y = tmp;
    output_buffer->u.YUVA.y_stride = stride;
    output_buffer->u.YUVA.y_size = stride * h;
    tmp += output_buffer->u.YUVA.y_size;
    if (has_alpha) {
      output_buffer->u.YUVA.a = tmp;
      output_buffer->u.YUVA.a_stride = stride;
      output_buffer->u.YUVA.a_size = stride * h;
      tmp += output_buffer->u.YUVA.a_size;
    } else {
      output_buffer->u.YUVA.a = NULL;
      output_buffer->u.YUVA.a_stride = 0;
    }
    output_buffer->u.YUVA.u = tmp;
    output_buffer->u.YUVA.u_stride = uv_stride;
    output_buffer->u.YUVA.u_size = uv_stride * (h + 1) / 2;
    tmp += output_buffer->u.YUVA.u_size;

    output_buffer->u.YUVA.v = tmp;
    output_buffer->u.YUVA.v_stride = uv_stride;
    output_buffer->u.YUVA.v_size = uv_stride * (h + 1) / 2;
    tmp += output_buffer->u.YUVA.v_size;
    assert(tmp <= external_buffer + total_size);
  }
  output_buffer->is_external_memory = use_external_memory;
  return external_buffer;
}

int MAIN(int argc, const GCHAR* argv[]) {
  int ok = 0;
  const GCHAR *in_file = NULL, *out_file = NULL;

  WebPDecoderConfig config;
  WebPDecBuffer* const output_buffer = &config.output;
  WebPBitstreamFeatures* const bitstream = &config.input;
  WebPOutputFileFormat format = PNG;
  uint8_t* external_buffer = NULL;
  int use_external_memory = 0;
  const uint8_t* data = NULL;

  int incremental = 0;
  int c;

  if (!WebPInitDecoderConfig(&config)) {
    fprintf(stderr, "Library version mismatch!\n");
    return -1;
  }

  for (c = 1; c < argc; ++c) {
    int parse_error = 0;
    if (!STRCMP(argv[c], "-h") || !STRCMP(argv[c], "-help")) {
      Help();
      return 0;
    } else if (!STRCMP(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!STRCMP(argv[c], "-alpha")) {
      format = ALPHA_PLANE_ONLY;
    } else if (!STRCMP(argv[c], "-nofancy")) {
      config.options.no_fancy_upsampling = 1;
    } else if (!STRCMP(argv[c], "-nofilter")) {
      config.options.bypass_filtering = 1;
    } else if (!STRCMP(argv[c], "-pam")) {
      format = PAM;
    } else if (!STRCMP(argv[c], "-ppm")) {
      format = PPM;
    } else if (!STRCMP(argv[c], "-bmp")) {
      format = BMP;
    } else if (!STRCMP(argv[c], "-tiff")) {
      format = TIFF;
    } else if (!STRCMP(argv[c], "-quiet")) {
      quiet = 1;
    } else if (!STRCMP(argv[c], "-version")) {
      const int version = WebPGetDecoderVersion();
      printf("%d.%d.%d\n",
             (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
    } else if (!STRCMP(argv[c], "-pgm")) {
      format = PGM;
    } else if (!STRCMP(argv[c], "-yuv")) {
      format = RAW_YUV;
    } else if (!STRCMP(argv[c], "-pixel_format") && c < argc - 1) {
      const GCHAR* const fmt = argv[++c];
      if      (!STRCMP(fmt, "RGB"))  format = RGB;
      else if (!STRCMP(fmt, "RGBA")) format = RGBA;
      else if (!STRCMP(fmt, "BGR"))  format = BGR;
      else if (!STRCMP(fmt, "BGRA")) format = BGRA;
      else if (!STRCMP(fmt, "ARGB")) format = ARGB;
      else if (!STRCMP(fmt, "RGBA_4444")) format = RGBA_4444;
      else if (!STRCMP(fmt, "RGB_565")) format = RGB_565;
      else if (!STRCMP(fmt, "rgbA")) format = rgbA;
      else if (!STRCMP(fmt, "bgrA")) format = bgrA;
      else if (!STRCMP(fmt, "Argb")) format = Argb;
      else if (!STRCMP(fmt, "rgbA_4444")) format = rgbA_4444;
      else if (!STRCMP(fmt, "YUV"))  format = YUV;
      else if (!STRCMP(fmt, "YUVA")) format = YUVA;
      else {
        FPRINTF(stderr, "Can't parse pixel_format %s\n", fmt);
        parse_error = 1;
      }
    } else if (!STRCMP(argv[c], "-external_memory") && c < argc - 1) {
      use_external_memory = EXUTILGETINT(argv[++c], 0, &parse_error);
      parse_error |= (use_external_memory > 2 || use_external_memory < 0);
      if (parse_error) {
        FPRINTF(stderr, "Can't parse 'external_memory' value %s\n", argv[c]);
      }
    } else if (!STRCMP(argv[c], "-mt")) {
      config.options.use_threads = 1;
    } else if (!STRCMP(argv[c], "-alpha_dither")) {
      config.options.alpha_dithering_strength = 100;
    } else if (!STRCMP(argv[c], "-nodither")) {
      config.options.dithering_strength = 0;
    } else if (!STRCMP(argv[c], "-dither") && c < argc - 1) {
      config.options.dithering_strength =
          EXUTILGETINT(argv[++c], 0, &parse_error);
    } else if (!STRCMP(argv[c], "-crop") && c < argc - 4) {
      config.options.use_cropping = 1;
      config.options.crop_left   = EXUTILGETINT(argv[++c], 0, &parse_error);
      config.options.crop_top    = EXUTILGETINT(argv[++c], 0, &parse_error);
      config.options.crop_width  = EXUTILGETINT(argv[++c], 0, &parse_error);
      config.options.crop_height = EXUTILGETINT(argv[++c], 0, &parse_error);
    } else if ((!STRCMP(argv[c], "-scale") || !STRCMP(argv[c], "-resize")) &&
               c < argc - 2) {  // '-scale' is left for compatibility
      config.options.use_scaling = 1;
      config.options.scaled_width  = EXUTILGETINT(argv[++c], 0, &parse_error);
      config.options.scaled_height = EXUTILGETINT(argv[++c], 0, &parse_error);
    } else if (!STRCMP(argv[c], "-flip")) {
      config.options.flip = 1;
    } else if (!STRCMP(argv[c], "-v")) {
      verbose = 1;
#ifndef WEBP_DLL
    } else if (!STRCMP(argv[c], "-noasm")) {
      VP8GetCPUInfo = NULL;
#endif
    } else if (!STRCMP(argv[c], "-incremental")) {
      incremental = 1;
    } else if (!STRCMP(argv[c], "--")) {
      if (c < argc - 1) in_file = argv[++c];
      break;
    } else if (argv[c][0] == TO_GCHAR('-')) {
      FPRINTF(stderr, "Unknown option '%s'\n", argv[c]);
      Help();
      return -1;
    } else {
      in_file = argv[c];
    }

    if (parse_error) {
      Help();
      return -1;
    }
  }

  if (in_file == NULL) {
    fprintf(stderr, "missing input file!!\n");
    Help();
    return -1;
  }

  if (quiet) verbose = 0;

  {
    VP8StatusCode status = VP8_STATUS_OK;
    size_t data_size = 0;
    if (!LoadWebP((const char*)in_file, &data, &data_size, bitstream)) {
      return -1;
    }

    switch (format) {
      case PNG:
#ifdef HAVE_WINCODEC_H
        output_buffer->colorspace = bitstream->has_alpha ? MODE_BGRA : MODE_BGR;
#else
        output_buffer->colorspace = bitstream->has_alpha ? MODE_RGBA : MODE_RGB;
#endif
        break;
      case PAM:
        output_buffer->colorspace = MODE_RGBA;
        break;
      case PPM:
        output_buffer->colorspace = MODE_RGB;  // drops alpha for PPM
        break;
      case BMP:
        output_buffer->colorspace = bitstream->has_alpha ? MODE_BGRA : MODE_BGR;
        break;
      case TIFF:
        output_buffer->colorspace = bitstream->has_alpha ? MODE_RGBA : MODE_RGB;
        break;
      case PGM:
      case RAW_YUV:
        output_buffer->colorspace = bitstream->has_alpha ? MODE_YUVA : MODE_YUV;
        break;
      case ALPHA_PLANE_ONLY:
        output_buffer->colorspace = MODE_YUVA;
        break;
      // forced modes:
      case RGB: output_buffer->colorspace = MODE_RGB; break;
      case RGBA: output_buffer->colorspace = MODE_RGBA; break;
      case BGR: output_buffer->colorspace = MODE_BGR; break;
      case BGRA: output_buffer->colorspace = MODE_BGRA; break;
      case ARGB: output_buffer->colorspace = MODE_ARGB; break;
      case RGBA_4444: output_buffer->colorspace = MODE_RGBA_4444; break;
      case RGB_565: output_buffer->colorspace = MODE_RGB_565; break;
      case rgbA: output_buffer->colorspace = MODE_rgbA; break;
      case bgrA: output_buffer->colorspace = MODE_bgrA; break;
      case Argb: output_buffer->colorspace = MODE_Argb; break;
      case rgbA_4444: output_buffer->colorspace = MODE_rgbA_4444; break;
      case YUV: output_buffer->colorspace = MODE_YUV; break;
      case YUVA: output_buffer->colorspace = MODE_YUVA; break;
      default: goto Exit;
    }

    if (use_external_memory > 0 && format >= RGB) {
      external_buffer = AllocateExternalBuffer(&config, format,
                                               use_external_memory);
      if (external_buffer == NULL) goto Exit;
    }

    {
      Stopwatch stop_watch;
      if (verbose) StopwatchReset(&stop_watch);

      if (incremental) {
        status = DecodeWebPIncremental(data, data_size, &config);
      } else {
        status = DecodeWebP(data, data_size, &config);
      }
      if (verbose) {
        const double decode_time = StopwatchReadAndReset(&stop_watch);
        fprintf(stderr, "Time to decode picture: %.3fs\n", decode_time);
      }
    }

    ok = (status == VP8_STATUS_OK);
    if (!ok) {
      PrintWebPError((const char*)in_file, status);
      goto Exit;
    }
  }

  if (out_file != NULL) {
    if (!quiet) {
      FPRINTF(stderr, "Decoded %s.", in_file);
      fprintf(stderr, "Dimensions: %d x %d %s. Format: %s. Now saving...\n",
              output_buffer->width, output_buffer->height,
              bitstream->has_alpha ? " (with alpha)" : "",
              kFormatType[bitstream->format]);
    }
    ok = SaveOutput(output_buffer, format, out_file);
  } else {
    if (!quiet) {
      FPRINTF(stderr, "File %s can be decoded ", in_file);
      fprintf(stderr, "(dimensions: %d x %d %s. Format: %s).\n",
              output_buffer->width, output_buffer->height,
              bitstream->has_alpha ? " (with alpha)" : "",
              kFormatType[bitstream->format]);
      fprintf(stderr, "Nothing written; "
                      "use -o flag to save the result as e.g. PNG.\n");
    }
  }
 Exit:
  WebPFreeDecBuffer(output_buffer);
  free((void*)external_buffer);
  free((void*)data);
  return ok ? 0 : -1;
}

//------------------------------------------------------------------------------
