// Copyright 2013 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Windows Imaging Component (WIC) decode.

#include "./wicdec.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

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

#include "webp/encode.h"

#define IFS(fn)                                                     \
  do {                                                              \
     if (SUCCEEDED(hr)) {                                           \
        hr = (fn);                                                  \
        if (FAILED(hr)) fprintf(stderr, #fn " failed %08x\n", hr);  \
     }                                                              \
  } while (0)

// modified version of DEFINE_GUID from guiddef.h.
#define WEBP_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
  const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#ifdef __cplusplus
#define MAKE_REFGUID(x) (x)
#else
#define MAKE_REFGUID(x) &(x)
#endif

typedef struct WICFormatImporter {
  const GUID* pixel_format;
  int bytes_per_pixel;
  int (*import)(WebPPicture* const, const uint8_t* const, int);
} WICFormatImporter;

static HRESULT OpenInputStream(const char* filename, IStream** ppStream) {
  HRESULT hr = S_OK;
  IFS(SHCreateStreamOnFileA(filename, STGM_READ, ppStream));
  if (FAILED(hr))
    fprintf(stderr, "Error opening input file %s (%08x)\n", filename, hr);
  return hr;
}

int ReadPictureWithWIC(const char* const filename,
                       WebPPicture* const pic, int keep_alpha) {
  // From Microsoft SDK 7.0a -- wincodec.h
  // Create local copies for compatibility when building against earlier
  // versions of the SDK.
  WEBP_DEFINE_GUID(GUID_WICPixelFormat24bppBGR_,
                   0x6fddc324, 0x4e03, 0x4bfe,
                   0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c);
  WEBP_DEFINE_GUID(GUID_WICPixelFormat24bppRGB_,
                   0x6fddc324, 0x4e03, 0x4bfe,
                   0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0d);
  WEBP_DEFINE_GUID(GUID_WICPixelFormat32bppBGRA_,
                   0x6fddc324, 0x4e03, 0x4bfe,
                   0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f);
  WEBP_DEFINE_GUID(GUID_WICPixelFormat32bppRGBA_,
                   0xf5c7ad2d, 0x6a8d, 0x43dd,
                   0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9);
  const WICFormatImporter alphaFormatImporters[] = {
    { &GUID_WICPixelFormat32bppBGRA_, 4, WebPPictureImportBGRA },
    { &GUID_WICPixelFormat32bppRGBA_, 4, WebPPictureImportRGBA },
    { NULL, 0, NULL },
  };
  const WICFormatImporter nonAlphaFormatImporters[] = {
    { &GUID_WICPixelFormat24bppBGR_, 3, WebPPictureImportBGR },
    { &GUID_WICPixelFormat24bppRGB_, 3, WebPPictureImportRGB },
    { NULL, 0, NULL },
  };
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
  const WICFormatImporter* importer = NULL;
  GUID srcContainerFormat = { 0 };
  const GUID* alphaContainers[] = {
    &GUID_ContainerFormatBmp,
    &GUID_ContainerFormatPng,
    &GUID_ContainerFormatTiff
  };
  int has_alpha = 0;
  int i, stride;

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

  if (keep_alpha) {
    for (i = 0;
         i < sizeof(alphaContainers) / sizeof(alphaContainers[0]);
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
  }

  // Prepare for pixel format conversion (if necessary).
  IFS(IWICImagingFactory_CreateFormatConverter(pFactory, &pConverter));

  for (importer = has_alpha ? alphaFormatImporters : nonAlphaFormatImporters;
       hr == S_OK && importer->import != NULL; ++importer) {
    BOOL canConvert;
    const HRESULT cchr = IWICFormatConverter_CanConvert(
        pConverter,
        MAKE_REFGUID(srcPixelFormat),
        MAKE_REFGUID(*importer->pixel_format),
        &canConvert);
    if (SUCCEEDED(cchr) && canConvert) break;
  }
  if (importer->import == NULL) hr = E_FAIL;

  IFS(IWICFormatConverter_Initialize(pConverter, (IWICBitmapSource*)pFrame,
          importer->pixel_format,
          WICBitmapDitherTypeNone,
          NULL, 0.0, WICBitmapPaletteTypeCustom));

  // Decode.
  IFS(IWICFormatConverter_GetSize(pConverter, &width, &height));
  stride = importer->bytes_per_pixel * width * sizeof(*rgb);
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
    ok = importer->import(pic, rgb, stride);
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
  return SUCCEEDED(hr);
}
#else  // !HAVE_WINCODEC_H
int ReadPictureWithWIC(const char* const filename,
                       struct WebPPicture* const pic, int keep_alpha) {
  (void)filename;
  (void)pic;
  (void)keep_alpha;
  fprintf(stderr, "Windows Imaging Component (WIC) support not compiled. "
                  "Visual Studio and mingw-w64 builds support WIC. Make sure "
                  "wincodec.h detection is working correctly if using autoconf "
                  "and HAVE_WINCODEC_H is defined before building.\n");
  return 0;
}
#endif  // HAVE_WINCODEC_H

// -----------------------------------------------------------------------------
