// Copyright 2013 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Simple SDL-based WebP file viewer / emscripten helper.
//
// Compiling on linux:
//   sudo apt-get install libsdl-dev
//   gcc -o vwebp-sdl vwebp-sdl.c ../examples/example_util.c \
//     -O3 -lwebp -lSDL -lm
// Compiling on Mac + XCode:
//   sudo port install libsdl
//   gcc -o vwebp-sdl vwebp-sdl.c ../examples/example_util.c \
//     -O3 -lwebp `pkg-config --libs sdl` -lm
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include "webp/decode.h"

#include "../imageio/imageio_util.h"

#define ENABLE_TIMING

#ifdef ENABLE_TIMING
#include "../examples/stopwatch.h"

struct Timing {
  Stopwatch stop_watch;
  double init;
  double read;
  double get_features;
  double decode;
  double blit;
};

static void TimingSetMark(struct Timing* const timing, double* record) {
  const double mark = StopwatchReadAndReset(&timing->stop_watch);
  if (record != NULL) *record = mark;
}

static void DumpTiming(const struct Timing* const timing) {
  double total = 0.;
  printf("Timing:\n");
  printf("  init: %fs\n", timing->init);
  total += timing->init;
  printf("  read file: %fs\n", timing->read);
  total += timing->read;
  printf("  get bitstream info: %fs\n", timing->get_features);
  total += timing->get_features;
  printf("  decode: %fs\n", timing->decode);
  total += timing->decode;
  printf("  blit: %fs\n", timing->blit);
  total += timing->blit;
  printf("  total: %fs\n", total);
}
#else
#define TimingSetMark(s, r)
#define DumpTiming(t)
#endif  // ENABLE_TIMING

#ifndef EMSCRIPTEN
static void ProcessEvents(void) {
  int done = 0;
  SDL_Event event;
  while (!done && SDL_WaitEvent(&event)) {
    switch (event.type) {
      case SDL_KEYUP:
        switch (event.key.keysym.sym) {
          case SDLK_q: done = 1; break;
          default: break;
        }
        break;
      default: break;
    }
  }
}
#endif

extern int WebpToSDL(const char *file);
int WebpToSDL(const char *file) {
  int ok = 0;
  struct Timing timing;
  const uint8_t* webp = NULL;
  size_t webp_size = 0;
  VP8StatusCode status;
  WebPDecoderConfig config;
  WebPBitstreamFeatures* const input = &config.input;
  WebPDecBuffer* const output = &config.output;
  SDL_Surface* screen = NULL;
  SDL_Surface* surface = NULL;

#ifdef ENABLE_TIMING
  memset(&timing, 0, sizeof(timing));
#else
  (void)timing;
#endif

  if (!WebPInitDecoderConfig(&config)) {
    fprintf(stderr, "Library version mismatch!\n");
    return -1;
  }

  TimingSetMark(&timing, NULL);
  SDL_Init(SDL_INIT_VIDEO);
  TimingSetMark(&timing, &timing.init);

  if (!ImgIoUtilReadFile(file, &webp, &webp_size)) {
    printf("Error opening file: %s\n", file);
    goto Error;
  }
  TimingSetMark(&timing, &timing.read);

  status = WebPGetFeatures(webp, webp_size, &config.input);
  if (status != VP8_STATUS_OK) goto Error;
  TimingSetMark(&timing, &timing.get_features);

  screen = SDL_SetVideoMode(input->width, input->height, 32, SDL_SWSURFACE);
  if (screen == NULL) {
    fprintf(stderr, "Unable to set video mode (32bpp %dx%d)!\n",
            input->width, input->height);
    goto Error;
  }

  surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                 input->width, input->height, 32,
                                 0x000000ff,   // R mask
                                 0x0000ff00,   // G mask
                                 0x00ff0000,   // B mask
                                 0xff000000);  // A mask
  if (surface == NULL) {
    fprintf(stderr, "Unable to create %dx%d RGBA surface!\n",
            input->width, input->height);
    goto Error;
  }
  if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  output->colorspace = MODE_BGRA;
#else
  output->colorspace = MODE_RGBA;
#endif
  output->width  = surface->w;
  output->height = surface->h;
  output->u.RGBA.rgba   = surface->pixels;
  output->u.RGBA.stride = surface->pitch;
  output->u.RGBA.size   = surface->pitch * surface->h;
  output->is_external_memory = 1;

  TimingSetMark(&timing, NULL);
  status = WebPDecode(webp, webp_size, &config);
  if (status != VP8_STATUS_OK) {
    fprintf(stderr, "Error decoding image (%d)\n", status);
    goto Error;
  }
  TimingSetMark(&timing, &timing.decode);

  if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
  SDL_FillRect(screen, &screen->clip_rect, 0xffffffff);
  SDL_BlitSurface(surface, NULL, screen, NULL);
  SDL_Flip(screen);
  TimingSetMark(&timing, &timing.blit);

  DumpTiming(&timing);
#ifndef EMSCRIPTEN
  ProcessEvents();
#endif
  ok = 1;

 Error:
  SDL_FreeSurface(surface);
#ifdef EMSCRIPTEN
  SDL_FreeSurface(screen);  // normally freed by SDL_Quit().
#endif
  SDL_Quit();
  free((void*)webp);
  return !ok;

}
int main(int argc, char* argv[]) {
  const char* file = (argc > 1) ? argv[1] : "../examples/test.webp";
  return WebpToSDL(file);
}

//------------------------------------------------------------------------------
