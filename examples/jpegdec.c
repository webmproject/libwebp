// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// JPEG decode.

#include "./jpegdec.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef WEBP_HAVE_JPEG
#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void my_error_exit(j_common_ptr dinfo) {
  struct my_error_mgr* myerr = (struct my_error_mgr*)dinfo->err;
  (*dinfo->err->output_message)(dinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

int ReadJPEG(FILE* in_file, WebPPicture* const pic) {
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

  buffer = (*dinfo.mem->alloc_sarray)((j_common_ptr)&dinfo,
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
  free(rgb);
  return ok;
}
#else  // !WEBP_HAVE_JPEG
int ReadJPEG(FILE* in_file, struct WebPPicture* const pic) {
  (void)in_file;
  (void)pic;
  fprintf(stderr, "JPEG support not compiled. Please install the libjpeg "
          "development package before building.\n");
  return 0;
}
#endif  // WEBP_HAVE_JPEG

// -----------------------------------------------------------------------------
