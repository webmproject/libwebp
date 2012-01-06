// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Simple WebP file viewer.
//
// Compiling on linux:
//   sudo apt-get install libglut3-dev mesa-common-dev
//   gcc -o vwebp vwebp.c -O3 -lwebp -lglut -lGL
// Compiling on Mac + XCode:
//   gcc -o vwebp vwebp.c -lwebp -framework GLUT -framework OpenGL
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#ifdef FREEGLUT
#include <GL/freeglut.h>
#endif
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

// Unfortunate global variables
static const WebPDecBuffer* kPic = NULL;
static const char* file_name = NULL;
static int print_info = 0;

//------------------------------------------------------------------------------
// Callbacks

static void HandleKey(unsigned char key, int pos_x, int pos_y) {
  (void)pos_x;
  (void)pos_y;
  if (key == 'q' || key == 'Q' || key == 27 /* Esc */) {
#ifdef FREEGLUT
    glutLeaveMainLoop();
#else
    WebPFreeDecBuffer((WebPDecBuffer*)kPic);
    kPic = NULL;
    exit(0);
#endif
  } else if (key == 'i') {
    print_info = 1 - print_info;
    glutPostRedisplay();
  }
}

static void HandleReshape(int width, int height) {
  // TODO(skal): proper handling of resize, esp. for large pictures.
  // + key control of the zoom.
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void PrintString(const char* const text) {
  void* const font = GLUT_BITMAP_9_BY_15;
  int i;
  for (i = 0; text[i]; ++i) {
    glutBitmapCharacter(font, text[i]);
  }
}

static void HandleDisplay(void) {
  if (kPic == NULL) return;
  glClear(GL_COLOR_BUFFER_BIT);
  glPushMatrix();
  glPixelZoom(1, -1);
  glRasterPos2f(-1, 1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, kPic->u.RGBA.stride / 4);
  glDrawPixels(kPic->width, kPic->height, GL_RGBA, GL_UNSIGNED_BYTE,
               (GLvoid*)kPic->u.RGBA.rgba);
  if (print_info) {
    char tmp[32];

    glColor4f(0.0, 0.0, 0.0, 0.0);
    glRasterPos2f(-0.95f, 0.90f);
    PrintString(file_name);

    snprintf(tmp, sizeof(tmp), "Dimension:%d x %d", kPic->width, kPic->height);
    glColor4f(0.0, 0.0, 0.0, 0.0);
    glRasterPos2f(-0.95f, 0.80f);
    PrintString(tmp);
  }
  glFlush();
}

static void Show(const WebPDecBuffer* const pic) {
  glutInitDisplayMode(GLUT_RGBA);
  glutInitWindowSize(pic->width, pic->height);
  glutCreateWindow("WebP viewer");
  glutReshapeFunc(HandleReshape);
  glutDisplayFunc(HandleDisplay);
  glutIdleFunc(NULL);
  glutKeyboardFunc(HandleKey);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  HandleReshape(pic->width, pic->height);
  glutMainLoop();
}

//------------------------------------------------------------------------------
// File decoding

static const char* const kStatusMessages[] = {
  "OK", "OUT_OF_MEMORY", "INVALID_PARAM", "BITSTREAM_ERROR",
  "UNSUPPORTED_FEATURE", "SUSPENDED", "USER_ABORT", "NOT_ENOUGH_DATA"
};

static int Decode(const char* const in_file, WebPDecoderConfig* const config) {
  WebPDecBuffer* const output_buffer = &config->output;
  WebPBitstreamFeatures* const bitstream = &config->input;
  VP8StatusCode status = VP8_STATUS_OK;
  int ok;
  size_t data_size = 0;
  void* data = NULL;
  FILE* const in = fopen(in_file, "rb");

  if (!in) {
    fprintf(stderr, "cannot open input file '%s'\n", in_file);
    return 0;
  }
  fseek(in, 0, SEEK_END);
  data_size = ftell(in);
  fseek(in, 0, SEEK_SET);
  data = malloc(data_size);
  if (data == NULL) return 0;
  ok = (fread(data, data_size, 1, in) == 1);
  fclose(in);
  if (!ok) {
    fprintf(stderr, "Could not read %zu bytes of data from file %s\n",
            data_size, in_file);
    free(data);
    return 0;
  }

  status = WebPGetFeatures((const uint8_t*)data, data_size, bitstream);
  if (status != VP8_STATUS_OK) {
    goto end;
  }

  output_buffer->colorspace = MODE_RGBA;
  status = WebPDecode((const uint8_t*)data, data_size, config);

 end:
  free(data);
  ok = (status == VP8_STATUS_OK);
  if (!ok) {
    fprintf(stderr, "Decoding of %s failed.\n", in_file);
    fprintf(stderr, "Status: %d (%s)\n", status, kStatusMessages[status]);
  }
  return ok;
}

//------------------------------------------------------------------------------
// Main

static void Help(void) {
  printf("Usage: vwebp in_file [options]\n\n"
         "Decodes the WebP image file and visualize it using OpenGL\n"
         "Options are:\n"
         "  -version  .... print version number and exit.\n"
         "  -nofancy ..... don't use the fancy YUV420 upscaler.\n"
         "  -nofilter .... disable in-loop filtering.\n"
         "  -mt .......... use multi-threading\n"
         "  -crop <x> <y> <w> <h> ... crop output with the given rectangle\n"
         "  -scale <w> <h> .......... scale the output (*after* any cropping)\n"
         "  -h     ....... this help message.\n"
        );
}

int main(int argc, char *argv[]) {
  WebPDecoderConfig config;
  int c;

  if (!WebPInitDecoderConfig(&config)) {
    fprintf(stderr, "Library version mismatch!\n");
    return -1;
  }

  for (c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      Help();
      return 0;
    } else if (!strcmp(argv[c], "-nofancy")) {
      config.options.no_fancy_upsampling = 1;
    } else if (!strcmp(argv[c], "-nofilter")) {
      config.options.bypass_filtering = 1;
    } else if (!strcmp(argv[c], "-version")) {
      const int version = WebPGetDecoderVersion();
      printf("%d.%d.%d\n",
        (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
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
    } else if (argv[c][0] == '-') {
      printf("Unknown option '%s'\n", argv[c]);
      Help();
      return -1;
    } else {
      file_name = argv[c];
    }
  }

  if (file_name == NULL) {
    printf("missing input file!!\n");
    Help();
    return -1;
  }
  if (!Decode(file_name, &config)) {
    return -1;
  }

  kPic = &config.output;
  printf("Displaying [%s]: %d x %d. Press Esc to exit, 'i' for info.\n",
         file_name, kPic->width, kPic->height);

  glutInit(&argc, argv);
  Show(kPic);

  // Should only be reached when using FREEGLUT:
  WebPFreeDecBuffer(&config.output);
  return 0;
}

//------------------------------------------------------------------------------
