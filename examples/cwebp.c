// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WEBP_HAVE_PNG
#include <png.h>
#endif

#ifdef WEBP_HAVE_JPEG
#include <setjmp.h>   // note: this must be included *after* png.h
#include <jpeglib.h>
#endif

#ifdef WEBP_HAVE_TIFF
#include <tiffio.h>
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
#endif  /* HAVE_WINCODEC_H */


#include "webp/encode.h"
#include "./stopwatch.h"
#ifndef WEBP_DLL
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

extern void* VP8GetCPUInfo;   // opaque forward declaration.

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
#endif  // WEBP_DLL

//------------------------------------------------------------------------------

static int verbose = 0;

static int ReadYUV(FILE* in_file, WebPPicture* const pic) {
  const int use_argb = pic->use_argb;
  const int uv_width = (pic->width + 1) / 2;
  const int uv_height = (pic->height + 1) / 2;
  int y;
  int ok = 0;

  pic->use_argb = 0;
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
  if (use_argb) ok = WebPPictureYUVAToARGB(pic);

 End:
  return ok;
}

#ifdef HAVE_WINCODEC_H

#define IFS(fn)                \
  do {                         \
     if (SUCCEEDED(hr))        \
     {                         \
        hr = (fn);             \
        if (FAILED(hr) && verbose)           \
          fprintf(stderr, #fn " failed %08x\n", hr);  \
     }                         \
  } while (0)

// modified version of DEFINE_GUID from guiddef.h.
#define WEBP_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
  const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#ifdef __cplusplus
#define MAKE_REFGUID(x) (x)
#else
#define MAKE_REFGUID(x) &(x)
#endif

static HRESULT OpenInputStream(const char* filename, IStream** ppStream) {
  HRESULT hr = S_OK;
  IFS(SHCreateStreamOnFileA(filename, STGM_READ, ppStream));
  if (FAILED(hr))
    fprintf(stderr, "Error opening input file %s (%08x)\n", filename, hr);
  return hr;
}

static HRESULT ReadPictureWithWIC(const char* filename,
                                  WebPPicture* const pic, int keep_alpha) {
  HRESULT hr = S_OK;
  IWICBitmapFrameDecode* pFrame = NULL;
  IWICFormatConverter* pConverter = NULL;
  IWICImagingFactory* pFactory = NULL;
  IWICBitmapDecoder* pDecoder = NULL;
  IStream* pStream = NULL;
  UINT frameCount = 0;
  UINT width = 0, height = 0;
  BYTE* rgb = NULL;
  WICPixelFormatGUID srcPixelFormat = { 0 };
  GUID srcContainerFormat = { 0 };
  const GUID* alphaContainers[] = {
    &GUID_ContainerFormatBmp,
    &GUID_ContainerFormatPng,
    &GUID_ContainerFormatTiff
  };
  int has_alpha = 0;
  int i, stride;
  // From Microsoft SDK 7.0a
  // Create local copies for compatibility when building against earlier
  // versions of the SDK.
  WEBP_DEFINE_GUID(GUID_WICPixelFormat24bppRGB_,
                   0x6fddc324, 0x4e03, 0x4bfe,
                   0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0d);
  WEBP_DEFINE_GUID(GUID_WICPixelFormat32bppRGBA_,
                   0xf5c7ad2d, 0x6a8d, 0x43dd,
                   0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9);
  WEBP_DEFINE_GUID(GUID_WICPixelFormat32bppBGRA_,
                   0x6fddc324, 0x4e03, 0x4bfe,
                   0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f);

  IFS(CoInitialize(NULL));
  IFS(CoCreateInstance(MAKE_REFGUID(CLSID_WICImagingFactory), NULL,
          CLSCTX_INPROC_SERVER, MAKE_REFGUID(IID_IWICImagingFactory),
          (LPVOID*)&pFactory));
  if (hr == REGDB_E_CLASSNOTREG) {
    fprintf(stderr,
            "Couldn't access Windows Imaging Component (are you running "
            "Windows XP SP3 or newer?). Most formats not available. "
            "Use -s for the available YUV input.\n");
  }
  // Prepare for image decoding.
  IFS(OpenInputStream(filename, &pStream));
  IFS(IWICImagingFactory_CreateDecoderFromStream(pFactory, pStream, NULL,
          WICDecodeMetadataCacheOnDemand, &pDecoder));
  IFS(IWICBitmapDecoder_GetFrameCount(pDecoder, &frameCount));
  if (SUCCEEDED(hr) && frameCount == 0) {
    fprintf(stderr, "No frame found in input file.\n");
    hr = E_FAIL;
  }
  IFS(IWICBitmapDecoder_GetFrame(pDecoder, 0, &pFrame));
  IFS(IWICBitmapFrameDecode_GetPixelFormat(pFrame, &srcPixelFormat));
  IFS(IWICBitmapDecoder_GetContainerFormat(pDecoder, &srcContainerFormat));

  has_alpha = keep_alpha;
  for (i = 0;
       has_alpha && i < sizeof(alphaContainers)/sizeof(alphaContainers[0]);
       ++i) {
    if (IsEqualGUID(MAKE_REFGUID(srcContainerFormat),
                    MAKE_REFGUID(*alphaContainers[i]))) {
      has_alpha =
          IsEqualGUID(MAKE_REFGUID(srcPixelFormat),
                      MAKE_REFGUID(GUID_WICPixelFormat32bppRGBA_)) ||
          IsEqualGUID(MAKE_REFGUID(srcPixelFormat),
                      MAKE_REFGUID(GUID_WICPixelFormat32bppBGRA_));
      break;
    }
  }

  // Prepare for pixel format conversion (if necessary).
  IFS(IWICImagingFactory_CreateFormatConverter(pFactory, &pConverter));
  IFS(IWICFormatConverter_Initialize(pConverter, (IWICBitmapSource*)pFrame,
          has_alpha ? MAKE_REFGUID(GUID_WICPixelFormat32bppRGBA_)
                    : MAKE_REFGUID(GUID_WICPixelFormat24bppRGB_),
          WICBitmapDitherTypeNone,
          NULL, 0.0, WICBitmapPaletteTypeCustom));

  // Decode.
  IFS(IWICFormatConverter_GetSize(pConverter, &width, &height));
  stride = (has_alpha ? 4 : 3) * width * sizeof(*rgb);
  if (SUCCEEDED(hr)) {
    rgb = (BYTE*)malloc(stride * height);
    if (rgb == NULL)
      hr = E_OUTOFMEMORY;
  }
  IFS(IWICFormatConverter_CopyPixels(pConverter, NULL, stride,
          stride * height, rgb));

  // WebP conversion.
  if (SUCCEEDED(hr)) {
    int ok;
    pic->width = width;
    pic->height = height;
    ok = has_alpha ? WebPPictureImportRGBA(pic, rgb, stride)
                   : WebPPictureImportRGB(pic, rgb, stride);
    if (!ok)
      hr = E_FAIL;
  }
  if (SUCCEEDED(hr)) {
    if (has_alpha && keep_alpha == 2) {
      WebPCleanupTransparentArea(pic);
    }
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

static int ReadPicture(const char* const filename, WebPPicture* const pic,
                       int keep_alpha) {
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
    ok = SUCCEEDED(ReadPictureWithWIC(filename, pic, keep_alpha));
  }
  if (!ok) {
    fprintf(stderr, "Error! Could not process file %s\n", filename);
  }
  return ok;
}

#else  // !HAVE_WINCODEC_H

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

  if (setjmp(jerr.setjmp_buffer)) {
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

  jpeg_finish_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);

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
  (void)in_file;
  (void)pic;
  fprintf(stderr, "JPEG support not compiled. Please install the libjpeg "
          "development package before building.\n");
  return 0;
}
#endif

#ifdef WEBP_HAVE_PNG
static void PNGAPI error_function(png_structp png, png_const_charp dummy) {
  (void)dummy;  // remove variable-unused warning
  longjmp(png_jmpbuf(png), 1);
}

static int ReadPNG(FILE* in_file, WebPPicture* const pic, int keep_alpha) {
  png_structp png;
  png_infop info;
  int color_type, bit_depth, interlaced;
  int has_alpha;
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
    free(rgb);
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
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    if (bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(png);
    }
    png_set_gray_to_rgb(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
    has_alpha = 1;
  } else {
    has_alpha = !!(color_type & PNG_COLOR_MASK_ALPHA);
  }

  if (!keep_alpha) {
    png_set_strip_alpha(png);
    has_alpha = 0;
  }

  num_passes = png_set_interlace_handling(png);
  png_read_update_info(png, info);
  stride = (has_alpha ? 4 : 3) * width * sizeof(*rgb);
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
  ok = has_alpha ? WebPPictureImportRGBA(pic, rgb, stride)
                 : WebPPictureImportRGB(pic, rgb, stride);
  free(rgb);

  if (ok && has_alpha && keep_alpha == 2) {
    WebPCleanupTransparentArea(pic);
  }

 End:
  return ok;
}
#else
static int ReadPNG(FILE* in_file, WebPPicture* const pic, int keep_alpha) {
  (void)in_file;
  (void)pic;
  (void)keep_alpha;
  fprintf(stderr, "PNG support not compiled. Please install the libpng "
          "development package before building.\n");
  return 0;
}
#endif

#ifdef WEBP_HAVE_TIFF
static int ReadTIFF(const char* const filename,
                    WebPPicture* const pic, int keep_alpha) {
  TIFF* const tif = TIFFOpen(filename, "r");
  uint32 width, height;
  uint32* raster;
  int ok = 0;
  int dircount = 1;

  if (tif == NULL) {
    fprintf(stderr, "Error! Cannot open TIFF file '%s'\n", filename);
    return 0;
  }

  while (TIFFReadDirectory(tif)) ++dircount;

  if (dircount > 1) {
    fprintf(stderr, "Warning: multi-directory TIFF files are not supported.\n"
                    "Only the first will be used, %d will be ignored.\n",
                    dircount - 1);
  }

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  raster = (uint32*)_TIFFmalloc(width * height * sizeof(*raster));
  if (raster != NULL) {
    if (TIFFReadRGBAImageOriented(tif, width, height, raster,
                                  ORIENTATION_TOPLEFT, 1)) {
      const int stride = width * sizeof(*raster);
      pic->width = width;
      pic->height = height;
      // TIFF data is ABGR
#ifdef __BIG_ENDIAN__
      TIFFSwabArrayOfLong(raster, width * height);
#endif
      ok = keep_alpha
         ? WebPPictureImportRGBA(pic, (const uint8_t*)raster, stride)
         : WebPPictureImportRGBX(pic, (const uint8_t*)raster, stride);
    }
    _TIFFfree(raster);
  } else {
    fprintf(stderr, "Error allocating TIFF RGBA memory!\n");
  }

  if (ok && keep_alpha == 2) {
    WebPCleanupTransparentArea(pic);
  }

  TIFFClose(tif);
  return ok;
}
#else
static int ReadTIFF(const char* const filename,
                    WebPPicture* const pic, int keep_alpha) {
  (void)filename;
  (void)pic;
  (void)keep_alpha;
  fprintf(stderr, "TIFF support not compiled. Please install the libtiff "
          "development package before building.\n");
  return 0;
}
#endif

typedef enum {
  PNG_ = 0,
  JPEG_,
  TIFF_,  // 'TIFF' clashes with libtiff
  UNSUPPORTED
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
    format = PNG_;
  } else if (magic >= 0xFFD8FF00U && magic <= 0xFFD8FFFFU) {
    format = JPEG_;
  } else if (magic == 0x49492A00 || magic == 0x4D4D002A) {
    format = TIFF_;
  }
  return format;
}

static int ReadPicture(const char* const filename, WebPPicture* const pic,
                       int keep_alpha) {
  int ok = 0;
  FILE* in_file = fopen(filename, "rb");
  if (in_file == NULL) {
    fprintf(stderr, "Error! Cannot open input file '%s'\n", filename);
    return ok;
  }

  if (pic->width == 0 || pic->height == 0) {
    // If no size specified, try to decode it as PNG/JPEG (as appropriate).
    const InputFileFormat format = GetImageType(in_file);
    if (format == PNG_) {
      ok = ReadPNG(in_file, pic, keep_alpha);
    } else if (format == JPEG_) {
      ok = ReadJPEG(in_file, pic);
    } else if (format == TIFF_) {
      ok = ReadTIFF(filename, pic, keep_alpha);
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

#endif  // !HAVE_WINCODEC_H

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
  fprintf(stderr, "| %7d  (%.1f%%)\n", total, 100.f * total / total_size);
}

static void PrintPercents(const int counts[4], int total) {
  int s;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "|      %2d%%", 100 * counts[s] / total);
  }
  fprintf(stderr, "| %7d\n", total);
}

static void PrintValues(const int values[4]) {
  int s;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "| %7d ", values[s]);
  }
  fprintf(stderr, "|\n");
}

static void PrintFullLosslessInfo(const WebPAuxStats* const stats,
                                  const char* const description) {
  fprintf(stderr, "Lossless-%s compressed size: %d bytes\n",
          description, stats->lossless_size);
  if (stats->lossless_features) {
    fprintf(stderr, "  * Lossless features used:");
    if (stats->lossless_features & 1) fprintf(stderr, " PREDICTION");
    if (stats->lossless_features & 2) fprintf(stderr, " CROSS-COLOR-TRANSFORM");
    if (stats->lossless_features & 4) fprintf(stderr, " SUBTRACT-GREEN");
    if (stats->lossless_features & 8) fprintf(stderr, " PALETTE");
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "  * Precision Bits: histogram=%d transform=%d cache=%d\n",
          stats->histogram_bits, stats->transform_bits, stats->cache_bits);
  if (stats->palette_size > 0) {
    fprintf(stderr, "  * Palette size:   %d\n", stats->palette_size);
  }
}

static void PrintExtraInfoLossless(const WebPPicture* const pic,
                                   int short_output,
                                   const char* const file_name) {
  const WebPAuxStats* const stats = pic->stats;
  if (short_output) {
    fprintf(stderr, "%7d %2.2f\n", stats->coded_size, stats->PSNR[3]);
  } else {
    fprintf(stderr, "File:      %s\n", file_name);
    fprintf(stderr, "Dimension: %d x %d\n", pic->width, pic->height);
    fprintf(stderr, "Output:    %d bytes\n", stats->coded_size);
    PrintFullLosslessInfo(stats, "ARGB");
  }
}

static void PrintExtraInfoLossy(const WebPPicture* const pic, int short_output,
                                const char* const file_name) {
  const WebPAuxStats* const stats = pic->stats;
  if (short_output) {
    fprintf(stderr, "%7d %2.2f\n", stats->coded_size, stats->PSNR[3]);
  } else {
    const int num_i4 = stats->block_count[0];
    const int num_i16 = stats->block_count[1];
    const int num_skip = stats->block_count[2];
    const int total = num_i4 + num_i16;
    fprintf(stderr, "File:      %s\n", file_name);
    fprintf(stderr, "Dimension: %d x %d%s\n",
            pic->width, pic->height,
            stats->alpha_data_size ? " (with alpha)" : "");
    fprintf(stderr, "Output:    "
            "%d bytes Y-U-V-All-PSNR %2.2f %2.2f %2.2f   %2.2f dB\n",
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
      if (stats->alpha_data_size > 0) {
        fprintf(stderr, "             transparency:   %6d (%.1f dB)\n",
                stats->alpha_data_size, stats->PSNR[4]);
      }
      if (stats->layer_data_size) {
        fprintf(stderr, "             enhancement:    %6d\n",
                stats->layer_data_size);
      }
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
    if (stats->lossless_size > 0) {
      PrintFullLosslessInfo(stats, "alpha");
    }
  }
  if (pic->extra_info != NULL) {
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

//------------------------------------------------------------------------------

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
  const int alpha_height =
      WebPPictureHasTransparency(picture) ? picture->height : 0;
  const int height = picture->height + uv_height + alpha_height;
  FILE* const f = fopen(PGM_name, "wb");
  if (f == NULL) return 0;
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
  for (y = 0; y < alpha_height; ++y) {
    if (fwrite(picture->a + y * picture->a_stride, picture->width, 1, f) != 1)
      return 0;
    if (picture->width & 1) fputc(0, f);  // pad
  }
  fclose(f);
  return 1;
}

//------------------------------------------------------------------------------

static int ProgressReport(int percent, const WebPPicture* const picture) {
  printf("[%s]: %3d %%      \r",
         (char*)picture->user_data, percent);
  fflush(stdout);
  return 1;  // all ok
}

//------------------------------------------------------------------------------

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
         "it is assumed to be a PNG, JPEG or TIFF file.\n");
#ifdef HAVE_WINCODEC_H
  printf("Windows builds can take as input any of the files handled by WIC\n");
#endif
  printf("options:\n");
  printf("  -h / -help  ............ short help\n");
  printf("  -H / -longhelp  ........ long help\n");
  printf("  -q <float> ............. quality factor (0:small..100:big)\n");
  printf("  -alpha_q <int> ......... Transparency-compression quality "
         "(0..100).\n");
  printf("  -preset <string> ....... Preset setting, one of:\n");
  printf("                            default, photo, picture,\n");
  printf("                            drawing, icon, text\n");
  printf("     -preset must come first, as it overwrites other parameters.");
  printf("\n");
  printf("  -m <int> ............... compression method (0=fast, 6=slowest)\n");
  printf("  -segments <int> ........ number of segments to use (1..4)\n");
  printf("  -size <int> ............ Target size (in bytes)\n");
  printf("  -psnr <float> .......... Target PSNR (in dB. typically: 42)\n");
  printf("\n");
  printf("  -s <int> <int> ......... Input size (width x height) for YUV\n");
  printf("  -sns <int> ............. Spatial Noise Shaping (0:off, 100:max)\n");
  printf("  -f <int> ............... filter strength (0=off..100)\n");
  printf("  -sharpness <int> ....... "
         "filter sharpness (0:most .. 7:least sharp)\n");
  printf("  -strong ................ use strong filter instead of simple.\n");
  printf("  -partition_limit <int> . limit quality to fit the 512k limit on\n");
  printf("                           "
         "the first partition (0=no degradation ... 100=full)\n");
  printf("  -pass <int> ............ analysis pass number (1..10)\n");
  printf("  -crop <x> <y> <w> <h> .. crop picture with the given rectangle\n");
  printf("  -resize <w> <h> ........ resize picture (after any cropping)\n");
#ifdef WEBP_EXPERIMENTAL_FEATURES
  printf("  -444 / -422 / -gray ..... Change colorspace\n");
#endif
  printf("  -map <int> ............. print map of extra info.\n");
  printf("  -print_ssim ............ prints averaged SSIM distortion.\n");
  printf("  -print_psnr ............ prints averaged PSNR distortion.\n");
  printf("  -d <file.pgm> .......... dump the compressed output (PGM file).\n");
  printf("  -alpha_method <int> .... Transparency-compression method (0..1)\n");
  printf("  -alpha_filter <string> . predictive filtering for alpha plane.\n");
  printf("                           One of: none, fast (default) or best.\n");
  printf("  -alpha_cleanup ......... Clean RGB values in transparent area.\n");
  printf("  -noalpha ............... discard any transparency information.\n");
  printf("  -lossless .............. Encode image losslessly.\n");
  printf("  -hint <string> ......... Specify image characteristics hint.\n");
  printf("                           One of: photo, picture or graph\n");

  printf("\n");
  printf("  -short ................. condense printed message\n");
  printf("  -quiet ................. don't print anything.\n");
  printf("  -version ............... print version number and exit.\n");
#ifndef WEBP_DLL
  printf("  -noasm ................. disable all assembly optimizations.\n");
#endif
  printf("  -v ..................... verbose, e.g. print encoding/decoding "
         "times\n");
  printf("  -progress .............. report encoding progress\n");
  printf("\n");
  printf("Experimental Options:\n");
  printf("  -af .................... auto-adjust filter strength.\n");
  printf("  -pre <int> ............. pre-processing filter\n");
  printf("\n");
}

//------------------------------------------------------------------------------
// Error messages

static const char* const kErrorMessages[] = {
  "OK",
  "OUT_OF_MEMORY: Out of memory allocating objects",
  "BITSTREAM_OUT_OF_MEMORY: Out of memory re-allocating byte buffer",
  "NULL_PARAMETER: NULL parameter passed to function",
  "INVALID_CONFIGURATION: configuration is invalid",
  "BAD_DIMENSION: Bad picture dimension. Maximum width and height "
  "allowed is 16383 pixels.",
  "PARTITION0_OVERFLOW: Partition #0 is too big to fit 512k.\n"
  "To reduce the size of this partition, try using less segments "
  "with the -segments option, and eventually reduce the number of "
  "header bits using -partition_limit. More details are available "
  "in the manual (`man cwebp`)",
  "PARTITION_OVERFLOW: Partition is too big to fit 16M",
  "BAD_WRITE: Picture writer returned an I/O error",
  "FILE_TOO_BIG: File would be too big to fit in 4G",
  "USER_ABORT: encoding abort requested by user"
};

//------------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
  int return_value = -1;
  const char *in_file = NULL, *out_file = NULL, *dump_file = NULL;
  FILE *out = NULL;
  int c;
  int short_output = 0;
  int quiet = 0;
  int keep_alpha = 1;
  int crop = 0, crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
  int resize_w = 0, resize_h = 0;
  int show_progress = 0;
  WebPPicture picture;
  int print_distortion = 0;        // 1=PSNR, 2=SSIM
  WebPPicture original_picture;    // when PSNR or SSIM is requested
  WebPConfig config;
  WebPAuxStats stats;
  Stopwatch stop_watch;

  if (!WebPPictureInit(&picture) ||
      !WebPPictureInit(&original_picture) ||
      !WebPConfigInit(&config)) {
    fprintf(stderr, "Error! Version mismatch!\n");
    return -1;
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
    } else if (!strcmp(argv[c], "-print_ssim")) {
      config.show_compressed = 1;
      print_distortion = 2;
    } else if (!strcmp(argv[c], "-print_psnr")) {
      config.show_compressed = 1;
      print_distortion = 1;
    } else if (!strcmp(argv[c], "-short")) {
      short_output++;
    } else if (!strcmp(argv[c], "-s") && c < argc - 2) {
      picture.width = strtol(argv[++c], NULL, 0);
      picture.height = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-m") && c < argc - 1) {
      config.method = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-q") && c < argc - 1) {
      config.quality = (float)strtod(argv[++c], NULL);
    } else if (!strcmp(argv[c], "-alpha_q") && c < argc - 1) {
      config.alpha_quality = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-alpha_method") && c < argc - 1) {
      config.alpha_compression = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-alpha_cleanup")) {
      keep_alpha = keep_alpha ? 2 : 0;
    } else if (!strcmp(argv[c], "-alpha_filter") && c < argc - 1) {
      ++c;
      if (!strcmp(argv[c], "none")) {
        config.alpha_filtering = 0;
      } else if (!strcmp(argv[c], "fast")) {
        config.alpha_filtering = 1;
      } else if (!strcmp(argv[c], "best")) {
        config.alpha_filtering = 2;
      } else {
        fprintf(stderr, "Error! Unrecognized alpha filter: %s\n", argv[c]);
        goto Error;
      }
    } else if (!strcmp(argv[c], "-noalpha")) {
      keep_alpha = 0;
    } else if (!strcmp(argv[c], "-lossless")) {
      config.lossless = 1;
      picture.use_argb = 1;
    } else if (!strcmp(argv[c], "-hint") && c < argc - 1) {
      ++c;
      if (!strcmp(argv[c], "photo")) {
        config.image_hint = WEBP_HINT_PHOTO;
      } else if (!strcmp(argv[c], "picture")) {
        config.image_hint = WEBP_HINT_PICTURE;
      } else if (!strcmp(argv[c], "graph")) {
        config.image_hint = WEBP_HINT_GRAPH;
      } else {
        fprintf(stderr, "Error! Unrecognized image hint: %s\n", argv[c]);
        goto Error;
      }
    } else if (!strcmp(argv[c], "-size") && c < argc - 1) {
      config.target_size = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-psnr") && c < argc - 1) {
      config.target_PSNR = (float)strtod(argv[++c], NULL);
    } else if (!strcmp(argv[c], "-sns") && c < argc - 1) {
      config.sns_strength = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-f") && c < argc - 1) {
      config.filter_strength = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-af")) {
      config.autofilter = 1;
    } else if (!strcmp(argv[c], "-strong")) {
      config.filter_type = 1;
    } else if (!strcmp(argv[c], "-sharpness") && c < argc - 1) {
      config.filter_sharpness = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-pass") && c < argc - 1) {
      config.pass = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-pre") && c < argc - 1) {
      config.preprocessing = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-segments") && c < argc - 1) {
      config.segments = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-partition_limit") && c < argc - 1) {
      config.partition_limit = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-map") && c < argc - 1) {
      picture.extra_info_type = strtol(argv[++c], NULL, 0);
#ifdef WEBP_EXPERIMENTAL_FEATURES
    } else if (!strcmp(argv[c], "-444")) {
      picture.colorspace = WEBP_YUV444;
    } else if (!strcmp(argv[c], "-422")) {
      picture.colorspace = WEBP_YUV422;
    } else if (!strcmp(argv[c], "-gray")) {
      picture.colorspace = WEBP_YUV400;
#endif
    } else if (!strcmp(argv[c], "-crop") && c < argc - 4) {
      crop = 1;
      crop_x = strtol(argv[++c], NULL, 0);
      crop_y = strtol(argv[++c], NULL, 0);
      crop_w = strtol(argv[++c], NULL, 0);
      crop_h = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-resize") && c < argc - 2) {
      resize_w = strtol(argv[++c], NULL, 0);
      resize_h = strtol(argv[++c], NULL, 0);
#ifndef WEBP_DLL
    } else if (!strcmp(argv[c], "-noasm")) {
      VP8GetCPUInfo = NULL;
#endif
    } else if (!strcmp(argv[c], "-version")) {
      const int version = WebPGetEncoderVersion();
      printf("%d.%d.%d\n",
        (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
    } else if (!strcmp(argv[c], "-progress")) {
      show_progress = 1;
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
  if (in_file == NULL) {
    fprintf(stderr, "No input file specified!\n");
    HelpShort();
    goto Error;
  }

  // Check for unsupported command line options for lossless mode and log
  // warning for such options.
  if (!quiet && config.lossless == 1) {
    if (config.target_size > 0 || config.target_PSNR > 0) {
      fprintf(stderr, "Encoding for specified size or PSNR is not supported"
                      " for lossless encoding. Ignoring such option(s)!\n");
    }
    if (config.partition_limit > 0) {
      fprintf(stderr, "Partition limit option is not required for lossless"
                      " encoding. Ignoring this option!\n");
    }
  }

  if (!WebPValidateConfig(&config)) {
    fprintf(stderr, "Error! Invalid configuration.\n");
    goto Error;
  }

  // Read the input
  if (verbose) {
    StopwatchReadAndReset(&stop_watch);
  }
  if (!ReadPicture(in_file, &picture, keep_alpha)) {
    fprintf(stderr, "Error! Cannot read input picture file '%s'\n", in_file);
    goto Error;
  }
  picture.progress_hook = (show_progress && !quiet) ? ProgressReport : NULL;

  if (verbose) {
    const double time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "Time to read input: %.3fs\n", time);
  }

  // Open the output
  if (out_file) {
    out = fopen(out_file, "wb");
    if (out == NULL) {
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
  if (!quiet) {
    picture.stats = &stats;
    picture.user_data = (void*)in_file;
  }

  // Compress
  if (verbose) {
    StopwatchReadAndReset(&stop_watch);
  }
  if (crop != 0) {
    // We use self-cropping using a view.
    if (!WebPPictureView(&picture, crop_x, crop_y, crop_w, crop_h, &picture)) {
      fprintf(stderr, "Error! Cannot crop picture\n");
      goto Error;
    }
  }
  if ((resize_w | resize_h) > 0) {
    if (!WebPPictureRescale(&picture, resize_w, resize_h)) {
      fprintf(stderr, "Error! Cannot resize picture\n");
      goto Error;
    }
  }
  if (picture.extra_info_type > 0) {
    AllocExtraInfo(&picture);
  }
  if (print_distortion > 0) {  // Save original picture for later comparison
    WebPPictureCopy(&picture, &original_picture);
  }
  if (!WebPEncode(&config, &picture)) {
    fprintf(stderr, "Error! Cannot encode picture as WebP\n");
    fprintf(stderr, "Error code: %d (%s)\n",
            picture.error_code, kErrorMessages[picture.error_code]);
    goto Error;
  }
  if (verbose) {
    const double time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "Time to encode picture: %.3fs\n", time);
  }

  // Write info
  if (dump_file) {
    if (picture.use_argb) {
      fprintf(stderr, "Warning: can't dump file (-d option) in lossless mode.");
    } else if (!DumpPicture(&picture, dump_file)) {
      fprintf(stderr, "Warning, couldn't dump picture %s\n", dump_file);
    }
  }

  if (!quiet) {
    if (config.lossless) {
      PrintExtraInfoLossless(&picture, short_output, in_file);
    } else {
      PrintExtraInfoLossy(&picture, short_output, in_file);
    }
  }
  if (!quiet && !short_output && print_distortion > 0) {  // print distortion
    float values[5];
    WebPPictureDistortion(&picture, &original_picture,
                          (print_distortion == 1) ? 0 : 1, values);
    fprintf(stderr, "%s: Y:%.2f U:%.2f V:%.2f A:%.2f  Total:%.2f\n",
            (print_distortion == 1) ? "PSNR" : "SSIM",
            values[0], values[1], values[2], values[3], values[4]);
  }
  return_value = 0;

 Error:
  free(picture.extra_info);
  WebPPictureFree(&picture);
  WebPPictureFree(&original_picture);
  if (out != NULL) {
    fclose(out);
  }

  return return_value;
}

//------------------------------------------------------------------------------
