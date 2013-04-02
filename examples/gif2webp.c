// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  simple tool to convert animated GIFs to WebP
//
// Getting the prerequisites:
// Debian-like linux:
//   sudo apt-get install libgif-dev
// MacPorts
//   sudo port install giflib
//
// Compiling:
//   gcc -o gif2webp gif2webp.c -O3 -lwebpmux -lwebp -lgif -lpthread -lm
//
// Authors: Skal (pascal.massimino@gmail.com)
//          Urvang (urvang@google.com)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gif_lib.h>
#include "webp/encode.h"
#include "webp/mux.h"
#include "./example_util.h"

#define GIF_TRANSPARENT_MASK 0x01
#define GIF_DISPOSE_MASK     0x07
#define GIF_DISPOSE_SHIFT    2
#define TRANSPARENT_COLOR    0x00ffffff
#define WHITE_COLOR          0xffffffff

//------------------------------------------------------------------------------

static int transparent_index = -1;  // No transparency by default.

static void ClearPicture(WebPPicture* const picture, uint32_t color) {
  int x, y;
  for (y = 0; y < picture->height; ++y) {
    uint32_t* const dst = picture->argb + y * picture->argb_stride;
    for (x = 0; x < picture->width; ++x) dst[x] = color;
  }
}

static void Remap(const uint8_t* const src, const GifFileType* const gif,
                  uint32_t* dst, int len) {
  int i;
  const GifColorType* colors;
  const ColorMapObject* const cmap =
      gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap;
  if (cmap == NULL) return;
  colors = cmap->Colors;

  for (i = 0; i < len; ++i) {
    const GifColorType c = colors[src[i]];
    dst[i] = (src[i] == transparent_index) ? TRANSPARENT_COLOR
           : c.Blue | (c.Green << 8) | (c.Red << 16) | (0xff << 24);
  }
}

static int ReadSubImage(GifFileType* gif, WebPPicture* pic, WebPPicture* view) {
  const GifImageDesc image_desc = gif->Image;
  const int offset_x = image_desc.Left;
  const int offset_y = image_desc.Top;
  const int sub_w = image_desc.Width;
  const int sub_h = image_desc.Height;
  uint32_t* dst = NULL;
  uint8_t* tmp = NULL;
  int ok = 0;

  // Use a view for the sub-picture:
  if (!WebPPictureView(pic, offset_x, offset_y, sub_w, sub_h, view)) {
    fprintf(stderr, "Sub-image %dx%d at position %d,%d is invalid!\n",
            sub_w, sub_h, offset_x, offset_y);
    goto End;
  }
  dst = view->argb;

  tmp = (uint8_t*)malloc(sub_w * sizeof(*tmp));
  if (tmp == NULL) goto End;

  if (image_desc.Interlace) {  // Interlaced image.
    // We need 4 passes, with the following offsets and jumps.
    const int interlace_offsets[] = { 0, 4, 2, 1 };
    const int interlace_jumps[]   = { 8, 8, 4, 2 };
    int pass;
    for (pass = 0; pass < 4; ++pass) {
      int y;
      for (y = interlace_offsets[pass]; y < sub_h; y += interlace_jumps[pass]) {
        if (DGifGetLine(gif, tmp, sub_w) == GIF_ERROR) goto End;
        Remap(tmp, gif, dst + y * view->argb_stride, sub_w);
      }
    }
  } else {  // Non-interlaced image.
    int y;
    for (y = 0; y < sub_h; ++y) {
      if (DGifGetLine(gif, tmp, sub_w) == GIF_ERROR) goto End;
      Remap(tmp, gif, dst + y * view->argb_stride, sub_w);
    }
  }
  // re-align the view with even offset (and adjust dimensions if needed).
  WebPPictureView(pic, offset_x & ~1, offset_y & ~1,
                  sub_w + (offset_x & 1), sub_h + (offset_y & 1), view);
  ok = 1;

 End:
  free(tmp);
  return ok;
}

static int GetBackgroundColor(const ColorMapObject* const color_map,
                              GifWord bgcolor_idx, uint32_t* const bgcolor) {
  if (transparent_index != -1 && bgcolor_idx == transparent_index) {
    *bgcolor = TRANSPARENT_COLOR;  // Special case.
    return 1;
  } else if (color_map == NULL || color_map->Colors == NULL
             || bgcolor_idx >= color_map->ColorCount) {
    return 0;  // Invalid color map or index.
  } else {
    const GifColorType color = color_map->Colors[bgcolor_idx];
    *bgcolor = (0xff        << 24)
             | (color.Red   << 16)
             | (color.Green <<  8)
             | (color.Blue  <<  0);
    return 1;
  }
}

static void DisplayGifError(const GifFileType* const gif, int gif_error) {
  // GIFLIB_MAJOR is only defined in libgif >= 4.2.0.
  // libgif 4.2.0 has retired PrintGifError() and added GifErrorString().
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && \
        ((GIFLIB_MAJOR == 4 && GIFLIB_MINOR >= 2) || GIFLIB_MAJOR > 4)
#if GIFLIB_MAJOR >= 5
    // Static string actually, hence the const char* cast.
    const char* error_str = (const char*)GifErrorString(
        (gif == NULL) ? gif_error : gif->Error);
#else
    const char* error_str = (const char*)GifErrorString();
    (void)gif;
#endif
    if (error_str == NULL) error_str = "Unknown error";
    fprintf(stderr, "GIFLib Error %d: %s\n", gif_error, error_str);
#else
    (void)gif;
    fprintf(stderr, "GIFLib Error %d: ", gif_error);
    PrintGifError();
    fprintf(stderr, "\n");
#endif
}

static const char* const kErrorMessages[] = {
  "WEBP_MUX_NOT_FOUND", "WEBP_MUX_INVALID_ARGUMENT", "WEBP_MUX_BAD_DATA",
  "WEBP_MUX_MEMORY_ERROR", "WEBP_MUX_NOT_ENOUGH_DATA"
};

static const char* ErrorString(WebPMuxError err) {
  assert(err <= WEBP_MUX_NOT_FOUND && err >= WEBP_MUX_NOT_ENOUGH_DATA);
  return kErrorMessages[-err];
}

//------------------------------------------------------------------------------

static void Help(void) {
  printf("Usage:\n");
  printf(" gif2webp [options] gif_file -o webp_file\n");
  printf("options:\n");
  printf("  -h / -help  ............ this help\n");
  printf("  -lossy ................. Encode image using lossy compression.\n");
  printf("  -q <float> ............. quality factor (0:small..100:big)\n");
  printf("  -m <int> ............... compression method (0=fast, 6=slowest)\n");
  printf("  -f <int> ............... filter strength (0=off..100)\n");
  printf("\n");
  printf("  -version ............... print version number and exit.\n");
  printf("  -v ..................... verbose.\n");
  printf("  -quiet ................. don't print anything.\n");
  printf("\n");
}

//------------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
  int verbose = 0;
  int gif_error = GIF_ERROR;
  WebPMuxError err = WEBP_MUX_OK;
  int ok = 0;
  const char *in_file = NULL, *out_file = NULL;
  FILE* out = NULL;
  GifFileType* gif = NULL;
  WebPPicture picture;
  WebPPicture view;
  WebPMemoryWriter memory;
  WebPMuxFrameInfo frame;
  WebPMuxAnimParams anim = { WHITE_COLOR, 0 };

  int is_first_frame = 1;
  int done;
  int c;
  int quiet = 0;
  WebPConfig config;
  WebPMux* mux = NULL;
  WebPData webp_data = { NULL, 0 };

  memset(&frame, 0, sizeof(frame));
  frame.id = WEBP_CHUNK_ANMF;
  frame.dispose_method = WEBP_MUX_DISPOSE_BACKGROUND;

  if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) {
    fprintf(stderr, "Error! Version mismatch!\n");
    return -1;
  }
  config.lossless = 1;  // Use lossless compression by default.

  if (argc == 1) {
    Help();
    return 0;
  }

  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      Help();
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-lossy")) {
      config.lossless = 0;
    } else if (!strcmp(argv[c], "-q") && c < argc - 1) {
      config.quality = (float)strtod(argv[++c], NULL);
    } else if (!strcmp(argv[c], "-m") && c < argc - 1) {
      config.method = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-f") && c < argc - 1) {
      config.filter_strength = strtol(argv[++c], NULL, 0);
    } else if (!strcmp(argv[c], "-version")) {
      const int enc_version = WebPGetEncoderVersion();
      const int mux_version = WebPGetMuxVersion();
      printf("WebP Encoder version: %d.%d.%d\nWebP Mux version: %d.%d.%d\n",
             (enc_version >> 16) & 0xff, (enc_version >> 8) & 0xff,
             enc_version & 0xff, (mux_version >> 16) & 0xff,
             (mux_version >> 8) & 0xff, mux_version & 0xff);
      return 0;
    } else if (!strcmp(argv[c], "-quiet")) {
      quiet = 1;
    } else if (!strcmp(argv[c], "-v")) {
      verbose = 1;
    } else if (argv[c][0] == '-') {
      fprintf(stderr, "Error! Unknown option '%s'\n", argv[c]);
      Help();
      return -1;
    } else {
      in_file = argv[c];
    }
  }
  if (!WebPValidateConfig(&config)) {
    fprintf(stderr, "Error! Invalid configuration.\n");
    goto End;
  }

  if (in_file == NULL) {
    fprintf(stderr, "No input file specified!\n");
    Help();
    goto End;
  }

  // Start the decoder object
#if defined(GIFLIB_MAJOR) && (GIFLIB_MAJOR >= 5)
  // There was an API change in version 5.0.0.
  gif = DGifOpenFileName(in_file, &gif_error);
#else
  gif = DGifOpenFileName(in_file);
#endif
  if (gif == NULL) goto End;

  // Allocate picture buffer
  picture.width = gif->SWidth;
  picture.height = gif->SHeight;
  picture.use_argb = 1;
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &memory;
  if (!WebPPictureAlloc(&picture)) goto End;

  mux = WebPMuxNew();
  if (mux == NULL) {
    fprintf(stderr, "ERROR: could not create a mux object.\n");
    goto End;
  }

  // Loop over GIF images
  done = 0;
  do {
    GifRecordType type;
    if (DGifGetRecordType(gif, &type) == GIF_ERROR) goto End;

    switch (type) {
      case IMAGE_DESC_RECORD_TYPE: {
        if (frame.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND) {
          ClearPicture(&picture, anim.bgcolor);
        }

        if (!DGifGetImageDesc(gif)) goto End;
        if (!ReadSubImage(gif, &picture, &view)) goto End;

        WebPMemoryWriterInit(&memory);
        if (!config.lossless) {
          // We need to call BGRA variant because of the way we do Remap().
          // TODO(later): This works for little-endian only due to uint32_t to
          // uint8_t conversion. Make it work for big-endian too.
          WebPPictureImportBGRA(&view, (uint8_t*)view.argb,
                                view.argb_stride * sizeof(*view.argb));
          view.use_argb = 0;
        } else {
          view.use_argb = 1;
        }
        if (!WebPEncode(&config, &view)) {
          fprintf(stderr, "Error! Cannot encode picture as WebP\n");
          fprintf(stderr, "Error code: %d\n", view.error_code);
          goto End;
        }

        // Now we have all the info about the frame, as a Graphic Control
        // Extension Block always appears before the Image Descriptor Block.
        // So add the frame to mux.
        frame.x_offset = gif->Image.Left & ~1;
        frame.y_offset = gif->Image.Top & ~1;
        frame.bitstream.bytes = memory.mem;
        frame.bitstream.size = memory.size;
        err = WebPMuxPushFrame(mux, &frame, 1);
        if (err != WEBP_MUX_OK) {
          fprintf(stderr, "ERROR (%s): Could not add animation frame.\n",
                  ErrorString(err));
          goto End;
        }
        if (verbose) {
          printf("Added frame %dx%d (offset:%d,%d duration:%d) ",
                 view.width, view.height, frame.x_offset, frame.y_offset,
                 frame.duration);
          printf("dispose:%d transparent index:%d\n",
                 frame.dispose_method, transparent_index);
        }
        WebPDataClear(&frame.bitstream);
        break;
      }
      case EXTENSION_RECORD_TYPE: {
        int extension;
        GifByteType *data = NULL;
        if (DGifGetExtension(gif, &extension, &data) == GIF_ERROR) {
          goto End;
        }
        switch (extension) {
          case COMMENT_EXT_FUNC_CODE: {
            break;  // Do nothing for now.
          }
          case GRAPHICS_EXT_FUNC_CODE: {
            const int flags = data[1];
            const int dispose = (flags >> GIF_DISPOSE_SHIFT) & GIF_DISPOSE_MASK;
            const int delay = data[2] | (data[3] << 8);  // In 10 ms units.
            if (data[0] != 4) goto End;
            frame.duration = delay * 10;  // Duration is in 1 ms units for WebP.
            if (dispose == 3) {
              fprintf(stderr, "WARNING: GIF_DISPOSE_RESTORE not supported.");
              // failsafe. TODO(urvang): emulate the correct behaviour by
              // recoding the whole frame.
              frame.dispose_method = WEBP_MUX_DISPOSE_BACKGROUND;
            } else {
              frame.dispose_method =
                  (dispose == 2) ? WEBP_MUX_DISPOSE_BACKGROUND
                                 : WEBP_MUX_DISPOSE_NONE;
            }
            transparent_index = (flags & GIF_TRANSPARENT_MASK) ? data[4] : -1;
            if (is_first_frame) {
              if (!GetBackgroundColor(gif->SColorMap, gif->SBackGroundColor,
                                      &anim.bgcolor)) {
                fprintf(stderr, "GIF decode warning: invalid background color "
                        "index. Assuming white background.\n");
              }
              ClearPicture(&picture, anim.bgcolor);
              is_first_frame = 0;
            }
            break;
          }
          case PLAINTEXT_EXT_FUNC_CODE: {
            break;
          }
          case APPLICATION_EXT_FUNC_CODE: {
            if (data[0] != 11) break;    // Chunk is too short
            if (!memcmp(data + 1, "NETSCAPE2.0", 11)) {
              // Recognize and parse Netscape2.0 NAB extension for loop count.
              if (DGifGetExtensionNext(gif, &data) == GIF_ERROR) goto End;
              if (data == NULL) goto End;  // Loop count sub-block missing.
              if (data[0] != 3 && data[1] != 1) break;   // wrong size/marker
              anim.loop_count = data[2] | (data[3] << 8);
              if (verbose) printf("Loop count: %d\n", anim.loop_count);
            } else if (!memcmp(data + 1, "XMP dataXMP", 11)) {
              // Read XMP metadata.
              WebPData xmp;
              if (DGifGetExtensionNext(gif, &data) == GIF_ERROR) goto End;
              if (data == NULL) goto End;
              xmp.bytes = (uint8_t*)data;
              xmp.size = data[0] + 1;
              WebPMuxSetChunk(mux, "XMP ", &xmp, 1);
              if (verbose) printf("XMP size: %d\n", (int)xmp.size);
            } else if (!memcmp(data + 1, "ICCRGBG1012", 11)) {
              // Read ICC profile.
              WebPData icc;
              if (DGifGetExtensionNext(gif, &data) == GIF_ERROR) goto End;
              if (data == NULL) goto End;
              icc.bytes = (uint8_t*)data;
              icc.size = data[0] + 1;
              WebPMuxSetChunk(mux, "ICCP", &icc, 1);
              if (verbose) printf("ICC size: %d\n", (int)icc.size);
            }
            break;
          }
          default: {
            break;  // skip
          }
        }
        do {
          if (DGifGetExtensionNext(gif, &data) == GIF_ERROR) goto End;
        } while (data != NULL);
        break;
      }
      case TERMINATE_RECORD_TYPE: {
        done = 1;
        break;
      }
      default: {
        if (verbose) {
          fprintf(stderr, "Skipping over unknown record type %d\n", type);
        }
        break;
      }
    }
  } while (!done);

  // Finish muxing
  err = WebPMuxSetAnimationParams(mux, &anim);
  if (err != WEBP_MUX_OK) {
    fprintf(stderr, "ERROR (%s): Could not set animation parameters.\n",
            ErrorString(err));
    goto End;
  }

  err = WebPMuxAssemble(mux, &webp_data);
  if (err != WEBP_MUX_OK) {
    fprintf(stderr, "ERROR (%s) assembling the WebP file.\n", ErrorString(err));
    goto End;
  }
  if (out_file != NULL) {
    if (!ExUtilWriteFile(out_file, webp_data.bytes, webp_data.size)) {
      fprintf(stderr, "Error writing output file: %s\n", out_file);
      goto End;
    }
    if (!quiet) {
      printf("Saved output file: %s\n", out_file);
    }
  } else {
    if (!quiet) {
      printf("Nothing written; use -o flag to save the result.\n");
    }
  }

  // All OK.
  ok = 1;
  gif_error = GIF_OK;

 End:
  WebPDataClear(&webp_data);
  WebPMuxDelete(mux);
  WebPPictureFree(&picture);
  if (out != NULL && out_file != NULL) fclose(out);

  if (gif_error != GIF_OK) {
    DisplayGifError(gif, gif_error);
  }
  if (gif != NULL) {
    DGifCloseFile(gif);
  }

  return !ok;
}

//------------------------------------------------------------------------------
