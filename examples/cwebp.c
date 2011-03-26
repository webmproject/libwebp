// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  simple command line calling the WebPEncode function.
//  Encodes a raw .YUV into WebP bitstream
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WEBP_HAVE_PNG
#include <png.h>
#endif

#ifdef WEBP_HAVE_JPEG
#include <setjmp.h>   // note: this must be included *after* png.h
#include <jpeglib.h>
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


#include "webp/encode.h"
#include "stopwatch.h"

//-----------------------------------------------------------------------------

static int verbose = 0;

static int ReadYUV(FILE* in_file, WebPPicture* const pic) {
  const int uv_width = (pic->width + 1) / 2;
  const int uv_height = (pic->height + 1) / 2;
  int y;
  int ok = 0;

  if (!WebPPictureAlloc(pic)) return ok;

  for (y = 0; y < pic->height; ++y) {
    if (fread(pic->y + y * pic->y_stride, pic->width, 1, in_file) != 1) {
      goto End;
    }
  }
  for (y = 0; y < uv_height; ++y) {
    if (fread(pic->u + y * pic->uv_stride, uv_width, 1, in_file) != 1)
      goto End;
  }
  for (y = 0; y < uv_height; ++y) {
    if (fread(pic->v + y * pic->uv_stride, uv_width, 1, in_file) != 1)
      goto End;
  }
  ok = 1;

 End:
  return ok;
}

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

static HRESULT OpenInputStream(const char* filename, IStream** ppStream) {
  HRESULT hr = S_OK;
  IFS(SHCreateStreamOnFileA(filename, STGM_READ, ppStream));
  if (FAILED(hr))
    printf("Error opening input file %s (%08x)\n", filename, hr);
  return hr;
}

static HRESULT ReadPictureWithWIC(const char* filename,
                                  WebPPicture* const pic) {
  HRESULT hr = S_OK;
  IWICBitmapFrameDecode* pFrame = NULL;
  IWICFormatConverter* pConverter = NULL;
  IWICImagingFactory* pFactory = NULL;
  IWICBitmapDecoder* pDecoder = NULL;
  IStream* pStream = NULL;
  UINT frameCount = 0;
  UINT width, height = 0;
  BYTE* rgb = NULL;

  IFS(CoInitialize(NULL));
  IFS(CoCreateInstance(MAKE_REFGUID(CLSID_WICImagingFactory), NULL,
          CLSCTX_INPROC_SERVER, MAKE_REFGUID(IID_IWICImagingFactory),
          (LPVOID*)&pFactory));
  if (hr == REGDB_E_CLASSNOTREG) {
    printf("Couldn't access Windows Imaging Component (are you running \n");
    printf("Windows XP SP3 or newer?). Most formats not available.\n");
    printf("Use -s for the available YUV input.\n");
  }
  // Prepare for image decoding.
  IFS(OpenInputStream(filename, &pStream));
  IFS(IWICImagingFactory_CreateDecoderFromStream(pFactory, pStream, NULL,
          WICDecodeMetadataCacheOnDemand, &pDecoder));
  IFS(IWICBitmapDecoder_GetFrameCount(pDecoder, &frameCount));
  if (SUCCEEDED(hr) && frameCount == 0) {
    printf("No frame found in input file.\n");
    hr = E_FAIL;
  }
  IFS(IWICBitmapDecoder_GetFrame(pDecoder, 0, &pFrame));

  // Prepare for pixel format conversion (if necessary).
  IFS(IWICImagingFactory_CreateFormatConverter(pFactory, &pConverter));
  IFS(IWICFormatConverter_Initialize(pConverter, (IWICBitmapSource*)pFrame,
          MAKE_REFGUID(GUID_WICPixelFormat24bppRGB), WICBitmapDitherTypeNone,
          NULL, 0.0, WICBitmapPaletteTypeCustom));

  // Decode.
  IFS(IWICFormatConverter_GetSize(pConverter, &width, &height));
  if (SUCCEEDED(hr)) {
    rgb = (BYTE*)malloc(3 * width * height);
    if (rgb == NULL)
      hr = E_OUTOFMEMORY;
  }
  IFS(IWICFormatConverter_CopyPixels(pConverter, NULL, 3 * width,
          3 * width * height, rgb));

  // WebP conversion.
  if (SUCCEEDED(hr)) {
    pic->width = width;
    pic->height = height;
    if (!WebPPictureImportRGB(pic, rgb, 3 * width))
      hr = E_FAIL;
  }

  // Cleanup.
  if (pConverter != NULL) IUnknown_Release(pConverter);
  if (pFrame != NULL) IUnknown_Release(pFrame);
  if (pDecoder != NULL) IUnknown_Release(pDecoder);
  if (pFactory != NULL) IUnknown_Release(pFactory);
  if (pStream != NULL) IUnknown_Release(pStream);
  free(rgb);
  return hr;
}

static int ReadPicture(const char* const filename, WebPPicture* const pic) {
  int ok;
  if (pic->width != 0 && pic->height != 0) {
    // If image size is specified, infer it as YUV format.
    FILE* in_file = fopen(filename, "rb");
    if (in_file == NULL) {
      fprintf(stderr, "Error! Cannot open input file '%s'\n", filename);
      return 0;
    }
    ok = ReadYUV(in_file, pic);
    fclose(in_file);
  } else {
    // If no size specified, try to decode it using WIC.
    ok = SUCCEEDED(ReadPictureWithWIC(filename, pic));
  }
  if (!ok) {
    fprintf(stderr, "Error! Could not process file %s\n", filename);
  }
  return ok;
}

#else  // !_WIN32

#ifdef WEBP_HAVE_JPEG
struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void my_error_exit(j_common_ptr dinfo) {
  struct my_error_mgr* myerr = (struct my_error_mgr*) dinfo->err;
  (*dinfo->err->output_message) (dinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

static int ReadJPEG(FILE* in_file, WebPPicture* const pic) {
  int ok = 0;
  int stride, width, height;
  uint8_t* rgb = NULL;
  uint8_t* row_ptr = NULL;
  struct jpeg_decompress_struct dinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;

  dinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;

  if (setjmp (jerr.setjmp_buffer)) {
 Error:
    jpeg_destroy_decompress(&dinfo);
    goto End;
  }

  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, in_file);
  jpeg_read_header(&dinfo, TRUE);

  dinfo.out_color_space = JCS_RGB;
  dinfo.dct_method = JDCT_IFAST;
  dinfo.do_fancy_upsampling = TRUE;

  jpeg_start_decompress(&dinfo);

  if (dinfo.output_components != 3) {
    goto Error;
  }

  width = dinfo.output_width;
  height = dinfo.output_height;
  stride = dinfo.output_width * dinfo.output_components * sizeof(*rgb);

  rgb = (uint8_t*)malloc(stride * height);
  if (rgb == NULL) {
    goto End;
  }
  row_ptr = rgb;

  buffer = (*dinfo.mem->alloc_sarray) ((j_common_ptr) &dinfo,
                                       JPOOL_IMAGE, stride, 1);
  if (buffer == NULL) {
    goto End;
  }

  while (dinfo.output_scanline < dinfo.output_height) {
    if (jpeg_read_scanlines(&dinfo, buffer, 1) != 1) {
      goto End;
    }
    memcpy(row_ptr, buffer[0], stride);
    row_ptr += stride;
  }

  jpeg_finish_decompress (&dinfo);
  jpeg_destroy_decompress (&dinfo);

  // WebP conversion.
  pic->width = width;
  pic->height = height;
  ok = WebPPictureImportRGB(pic, rgb, stride);

 End:
  if (rgb) {
    free(rgb);
  }
  return ok;
}

#else
static int ReadJPEG(FILE* in_file, WebPPicture* const pic) {
  printf("JPEG support not compiled. Please install the libjpeg development "
         "package before building.\n");
  return 0;
}
#endif

#ifdef WEBP_HAVE_PNG
static void PNGAPI error_function(png_structp png, png_const_charp dummy) {
  (void)dummy;  // remove variable-unused warning
  longjmp(png_jmpbuf(png), 1);
}

static int ReadPNG(FILE* in_file, WebPPicture* const pic) {
  png_structp png;
  png_infop info;
  int color_type, bit_depth, interlaced;
  int num_passes;
  int p;
  int ok = 0;
  png_uint_32 width, height, y;
  int stride;
  uint8_t* rgb = NULL;

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (png == NULL) {
    goto End;
  }

  png_set_error_fn(png, 0, error_function, NULL);
  if (setjmp(png_jmpbuf(png))) {
 Error:
    png_destroy_read_struct(&png, NULL, NULL);
    if (rgb) free(rgb);
    goto End;
  }

  info = png_create_info_struct(png);
  if (info == NULL) goto Error;

  png_init_io(png, in_file);
  png_read_info(png, info);
  if (!png_get_IHDR(png, info,
                    &width, &height, &bit_depth, &color_type, &interlaced,
                    NULL, NULL)) goto Error;

  png_set_strip_16(png);
  png_set_packing(png);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
  if (color_type == PNG_COLOR_TYPE_GRAY) {
    if (bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(png);
    }
    png_set_gray_to_rgb(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }

  // TODO(skal): Strip Alpha for now (till Alpha is supported).
  png_set_strip_alpha(png);
  num_passes = png_set_interlace_handling(png);
  png_read_update_info(png, info);
  stride = 3 * width * sizeof(*rgb);
  rgb = (uint8_t*)malloc(stride * height);
  if (rgb == NULL) goto Error;
  for (p = 0; p < num_passes; ++p) {
    for (y = 0; y < height; ++y) {
      png_bytep row = rgb + y * stride;
      png_read_rows(png, &row, NULL, 1);
    }
  }
  png_read_end(png, info);
  png_destroy_read_struct(&png, &info, NULL);

  pic->width = width;
  pic->height = height;
  ok = WebPPictureImportRGB(pic, rgb, stride);
  free(rgb);

 End:
  return ok;
}
#else
static int ReadPNG(FILE* in_file, WebPPicture* const pic) {
  printf("PNG support not compiled. Please install the libpng development "
         "package before building.\n");
  return 0;
}
#endif

typedef enum {
  PNG = 0,
  JPEG,
  UNSUPPORTED,
} InputFileFormat;

static InputFileFormat GetImageType(FILE* in_file) {
  InputFileFormat format = UNSUPPORTED;
  unsigned int magic;
  unsigned char buf[4];

  if ((fread(&buf[0], 4, 1, in_file) != 1) ||
      (fseek(in_file, 0, SEEK_SET) != 0)) {
    return format;
  }

  magic = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  if (magic == 0x89504E47U) {
    format = PNG;
  } else if (magic >= 0xFFD8FF00U && magic <= 0xFFD8FFFFU) {
    format = JPEG;
  }
  return format;
}

static int ReadPicture(const char* const filename, WebPPicture* const pic) {
  int ok = 0;
  FILE* in_file = fopen(filename, "rb");
  if (in_file == NULL) {
    fprintf(stderr, "Error! Cannot open input file '%s'\n", filename);
    return ok;
  }

  if (pic->width == 0 || pic->height == 0) {
    // If no size specified, try to decode it as PNG/JPEG (as appropriate).
    const InputFileFormat format = GetImageType(in_file);
    if (format == PNG) {
      ok = ReadPNG(in_file, pic);
    } else if (format == JPEG) {
      ok = ReadJPEG(in_file, pic);
    }
  } else {
    // If image size is specified, infer it as YUV format.
    ok = ReadYUV(in_file, pic);
  }
  if (!ok) {
    fprintf(stderr, "Error! Could not process file %s\n", filename);
  }

  fclose(in_file);
  return ok;
}

#endif  // !_WIN32

static void AllocExtraInfo(WebPPicture* const pic) {
  const int mb_w = (pic->width + 15) / 16;
  const int mb_h = (pic->height + 15) / 16;
  pic->extra_info = (uint8_t*)malloc(mb_w * mb_h * sizeof(*pic->extra_info));
}

static void PrintByteCount(const int bytes[4], int total_size,
                           int* const totals) {
  int s;
  int total = 0;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "| %7d ", bytes[s]);
    total += bytes[s];
    if (totals) totals[s] += bytes[s];
  }
  fprintf(stderr,"| %7d  (%.1f%%)\n", total, 100.f * total / total_size);
}

static void PrintPercents(const int counts[4], int total) {
  int s;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "|      %2d%%", 100 * counts[s] / total);
  }
  fprintf(stderr,"| %7d\n", total);
}

static void PrintValues(const int values[4]) {
  int s;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "| %7d ", values[s]);
  }
  fprintf(stderr,"|\n");
}

static void PrintExtraInfo(const WebPPicture* const pic, int short_output) {
  const WebPAuxStats* const stats = pic->stats;
  if (short_output) {
    fprintf(stderr, "%7d %2.2f\n", stats->coded_size, stats->PSNR[3]);
  } else{
    const int num_i4 = stats->block_count[0];
    const int num_i16 = stats->block_count[1];
    const int num_skip = stats->block_count[2];
    const int total = num_i4 + num_i16;
    fprintf(stderr,
            "%7d bytes Y-U-V-All-PSNR %2.2f %2.2f %2.2f   %2.2f dB\n",
            stats->coded_size,
            stats->PSNR[0], stats->PSNR[1], stats->PSNR[2], stats->PSNR[3]);
    if (total > 0) {
      int totals[4] = { 0, 0, 0, 0 };
      fprintf(stderr, "block count:  intra4: %d\n"
                      "              intra16: %d  (-> %.2f%%)\n",
              num_i4, num_i16, 100.f * num_i16 / total);
      fprintf(stderr, "              skipped block: %d (%.2f%%)\n",
              num_skip, 100.f * num_skip / total);
      fprintf(stderr, "bytes used:  header:         %6d  (%.1f%%)\n"
                      "             mode-partition: %6d  (%.1f%%)\n",
              stats->header_bytes[0],
              100.f * stats->header_bytes[0] / stats->coded_size,
              stats->header_bytes[1],
              100.f * stats->header_bytes[1] / stats->coded_size);
      fprintf(stderr, " Residuals bytes  "
                      "|segment 1|segment 2|segment 3"
                      "|segment 4|  total\n");
      fprintf(stderr, "  intra4-coeffs:  ");
      PrintByteCount(stats->residual_bytes[0], stats->coded_size, totals);
      fprintf(stderr, " intra16-coeffs:  ");
      PrintByteCount(stats->residual_bytes[1], stats->coded_size, totals);
      fprintf(stderr, "  chroma coeffs:  ");
      PrintByteCount(stats->residual_bytes[2], stats->coded_size, totals);
      fprintf(stderr, "    macroblocks:  ");
      PrintPercents(stats->segment_size, total);
      fprintf(stderr, "      quantizer:  ");
      PrintValues(stats->segment_quant);
      fprintf(stderr, "   filter level:  ");
      PrintValues(stats->segment_level);
      fprintf(stderr, "------------------+---------");
      fprintf(stderr, "+---------+---------+---------+-----------------\n");
      fprintf(stderr, " segments total:  ");
      PrintByteCount(totals, stats->coded_size, NULL);
    }
  }
  if (pic->extra_info) {
    const int mb_w = (pic->width + 15) / 16;
    const int mb_h = (pic->height + 15) / 16;
    const int type = pic->extra_info_type;
    int x, y;
    for (y = 0; y < mb_h; ++y) {
      for (x = 0; x < mb_w; ++x) {
        const int c = pic->extra_info[x + y * mb_w];
        if (type == 1) {   // intra4/intra16
          printf("%c", "+."[c]);
        } else if (type == 2) {    // segments
          printf("%c", ".-*X"[c]);
        } else if (type == 3) {    // quantizers
          printf("%.2d ", c);
        } else if (type == 6 || type == 7) {
          printf("%3d ", c);
        } else {
          printf("0x%.2x ", c);
        }
      }
      printf("\n");
    }
  }
}

//-----------------------------------------------------------------------------

static int MyWriter(const uint8_t* data, size_t data_size,
                    const WebPPicture* const pic) {
  FILE* const out = (FILE*)pic->custom_ptr;
  return data_size ? (fwrite(data, data_size, 1, out) == 1) : 1;
}

// Dumps a picture as a PGM file using the IMC4 layout.
static int DumpPicture(const WebPPicture* const picture, const char* PGM_name) {
  int y;
  const int uv_width = (picture->width + 1) / 2;
  const int uv_height = (picture->height + 1) / 2;
  const int stride = (picture->width + 1) & ~1;
  const int height = picture->height + uv_height;
  FILE* const f = fopen(PGM_name, "wb");
  if (!f) return 0;
  fprintf(f, "P5\n%d %d\n255\n", stride, height);
  for (y = 0; y < picture->height; ++y) {
    if (fwrite(picture->y + y * picture->y_stride, picture->width, 1, f) != 1)
      return 0;
    if (picture->width & 1) fputc(0, f);  // pad
  }
  for (y = 0; y < uv_height; ++y) {
    if (fwrite(picture->u + y * picture->uv_stride, uv_width, 1, f) != 1)
      return 0;
    if (fwrite(picture->v + y * picture->uv_stride, uv_width, 1, f) != 1)
      return 0;
  }
  fclose(f);
  return 1;
}

//-----------------------------------------------------------------------------

static void HelpShort(void) {
  printf("Usage:\n\n");
  printf("   cwebp [options] -q quality input.png -o output.webp\n\n");
  printf("where quality is between 0 (poor) to 100 (very good).\n");
  printf("Typical value is around 80.\n\n");
  printf("Try -longhelp for an exhaustive list of advanced options.\n");
}

static void HelpLong(void) {
  printf("Usage:\n");
  printf(" cwebp [-preset <...>] [options] in_file [-o out_file]\n\n");
  printf("If input size (-s) for an image is not specified, "
         "it is assumed to be a PNG or JPEG file.\n");
#ifdef _WIN32
  printf("Windows builds can take as input any of the files handled by WIC\n");
#endif
  printf("options:\n");
  printf("  -h / -help  ............ short help\n");
  printf("  -H / -longhelp  ........ long help\n");
  printf("  -q <float> ............. quality factor (0:small..100:big)\n");
  printf("  -preset <string> ....... Preset setting, one of:\n");
  printf("                            default, photo, picture,\n");
  printf("                            drawing, icon, text\n");
  printf("     -preset must come first, as it overwrites other parameters.");
  printf("\n");
  printf("  -m <int> ............... compression method (0=fast, 6=slowest)\n");
  printf("  -segments <int> ........ number of segments to use (1..4)\n");
  printf("\n");
  printf("  -s <int> <int> ......... Input size (width x height) for YUV\n");
  printf("  -sns <int> ............. Spatial Noise Shaping (0:off, 100:max)\n");
  printf("  -f <int> ............... filter strength (0=off..100)\n");
  printf("  -sharpness <int> ....... "
         "filter sharpness (0:most .. 7:least sharp)\n");
  printf("  -strong ................ use strong filter instead of simple.\n");
  printf("  -pass <int> ............ analysis pass number (1..10)\n");
  printf("  -crop <x> <y> <w> <h> .. crop picture with the given rectangle\n");
  printf("  -map <int> ............. print map of extra info.\n");
  printf("  -d <file.pgm> .......... dump the compressed output (PGM file).\n");
  printf("\n");
  printf("  -short ................. condense printed message\n");
  printf("  -quiet ................. don't print anything.\n");
  printf("  -version ............... print version number and exit.\n");
  printf("  -v ..................... verbose, e.g. print encoding/decoding "
         "times\n");
  printf("\n");
  printf("Experimental Options:\n");
  printf("  -size <int> ............ Target size (in bytes)\n");
  printf("  -psnr <float> .......... Target PSNR (in dB. typically: 42)\n");
  printf("  -af .................... auto-adjust filter strength.\n");
  printf("  -pre <int> ............. pre-processing filter\n");
  printf("\n");
}

//-----------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
  const char *in_file = NULL, *out_file = NULL, *dump_file = NULL;
  FILE *out = NULL;
  int c;
  int short_output = 0;
  int quiet = 0;
  int crop = 0, crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
  WebPPicture picture;
  WebPConfig config;
  WebPAuxStats stats;
  Stopwatch stop_watch;

  if (!WebPPictureInit(&picture) || !WebPConfigInit(&config)) {
    fprintf(stderr, "Error! Version mismatch!\n");
    goto Error;
  }

  if (argc == 1) {
    HelpShort();
    return 0;
  }

  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      HelpShort();
      return 0;
    } else if (!strcmp(argv[c], "-H") || !strcmp(argv[c], "-longhelp")) {
      HelpLong();
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-d") && c < argc - 1) {
      dump_file = argv[++c];
      config.show_compressed = 1;
    } else if (!strcmp(argv[c], "-short")) {
      short_output++;
    } else if (!strcmp(argv[c], "-s") && c < argc - 2) {
      picture.width = strtol(argv[++c], NULL, 0);
      picture.height = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-m") && c < argc - 1) {
      config.method = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-q") && c < argc - 1) {
      config.quality = strtod(argv[++c], NULL);
    } else if (!strcmp(argv[c], "-size") && c < argc - 1) {
      config.target_size = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-psnr") && c < argc - 1) {
      config.target_PSNR = strtod(argv[++c], NULL);
    } else if (!strcmp(argv[c], "-sns") && c < argc - 1) {
      config.sns_strength = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-f") && c < argc - 1) {
      config.filter_strength = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-af")) {
      config.autofilter = 1;
    } else if (!strcmp(argv[c], "-strong") && c < argc - 1) {
      config.filter_type = 1;
    } else if (!strcmp(argv[c], "-sharpness") && c < argc - 1) {
      config.filter_sharpness = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-pass") && c < argc - 1) {
      config.pass = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-pre") && c < argc - 1) {
      config.preprocessing = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-segments") && c < argc - 1) {
      config.segments = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-map") && c < argc - 1) {
      picture.extra_info_type = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-crop") && c < argc - 4) {
      crop = 1;
      crop_x = strtol(argv[++c], NULL, 0);
      crop_y = strtol(argv[++c], NULL, 0);
      crop_w = strtol(argv[++c], NULL, 0);
      crop_h = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-version")) {
      const int version = WebPGetEncoderVersion();
      printf("%d.%d.%d\n",
        (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
    } else if (!strcmp(argv[c], "-quiet")) {
      quiet = 1;
    } else if (!strcmp(argv[c], "-preset") && c < argc - 1) {
      WebPPreset preset;
      ++c;
      if (!strcmp(argv[c], "default")) {
        preset = WEBP_PRESET_DEFAULT;
      } else if (!strcmp(argv[c], "photo")) {
        preset = WEBP_PRESET_PHOTO;
      } else if (!strcmp(argv[c], "picture")) {
        preset = WEBP_PRESET_PICTURE;
      } else if (!strcmp(argv[c], "drawing")) {
        preset = WEBP_PRESET_DRAWING;
      } else if (!strcmp(argv[c], "icon")) {
        preset = WEBP_PRESET_ICON;
      } else if (!strcmp(argv[c], "text")) {
        preset = WEBP_PRESET_TEXT;
      } else {
        fprintf(stderr, "Error! Unrecognized preset: %s\n", argv[c]);
        goto Error;
      }
      if (!WebPConfigPreset(&config, preset, config.quality)) {
        fprintf(stderr, "Error! Could initialize configuration with preset.\n");
        goto Error;
      }
    } else if (!strcmp(argv[c], "-v")) {
      verbose = 1;
    } else if (argv[c][0] == '-') {
      fprintf(stderr, "Error! Unknown option '%s'\n", argv[c]);
      HelpLong();
      return -1;
    } else {
      in_file = argv[c];
    }
  }

  if (!WebPValidateConfig(&config)) {
    fprintf(stderr, "Error! Invalid configuration.\n");
    goto Error;
  }

  // Read the input
  if (verbose)
    StopwatchReadAndReset(&stop_watch);
  if (!ReadPicture(in_file, &picture)) {
    fprintf(stderr, "Error! Cannot read input picture\n");
    goto Error;
  }
  if (verbose) {
    const double time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "Time to read input: %.3fs\n", time);
  }

  // Open the output
  if (out_file) {
    out = fopen(out_file, "wb");
    if (!out) {
      fprintf(stderr, "Error! Cannot open output file '%s'\n", out_file);
      goto Error;
    } else {
      if (!short_output && !quiet) {
        fprintf(stderr, "Saving file '%s'\n", out_file);
      }
    }
    picture.writer = MyWriter;
    picture.custom_ptr = (void*)out;
  } else {
    out = NULL;
    if (!quiet && !short_output) {
      fprintf(stderr, "No output file specified (no -o flag). Encoding will\n");
      fprintf(stderr, "be performed, but its results discarded.\n\n");
    }
  }
  picture.stats = &stats;

  // Compress
  if (verbose)
    StopwatchReadAndReset(&stop_watch);
  if (crop != 0 && !WebPPictureCrop(&picture, crop_x, crop_y, crop_w, crop_h)) {
    fprintf(stderr, "Error! Cannot crop picture\n");
    goto Error;
  }
  if (picture.extra_info_type > 0) AllocExtraInfo(&picture);
  if (!WebPEncode(&config, &picture)) {
    fprintf(stderr, "Error! Cannot encode picture as WebP\n");
    goto Error;
  }
  if (verbose) {
    const double time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "Time to encode picture: %.3fs\n", time);
  }

  // Write info
  if (dump_file) DumpPicture(&picture, dump_file);
  if (!quiet) PrintExtraInfo(&picture, short_output);

 Error:
  free(picture.extra_info);
  WebPPictureFree(&picture);
  if (out != NULL) {
    fclose(out);
  }

  return 0;
}

//-----------------------------------------------------------------------------
