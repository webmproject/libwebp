// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// TIFF decode.

#include "./tiffdec.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef WEBP_HAVE_TIFF
#include <tiffio.h>

#include "webp/encode.h"

int ReadTIFF(const char* const filename,
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
#else  // !WEBP_HAVE_TIFF
int ReadTIFF(const char* const filename,
             struct WebPPicture* const pic, int keep_alpha) {
  (void)filename;
  (void)pic;
  (void)keep_alpha;
  fprintf(stderr, "TIFF support not compiled. Please install the libtiff "
          "development package before building.\n");
  return 0;
}
#endif  // WEBP_HAVE_TIFF

// -----------------------------------------------------------------------------
