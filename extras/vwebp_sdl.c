// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Simple SDL-based WebP file viewer.
// Does not support animation, just static images.
//
// Press 'q' to exit.
//
// Author: James Zern (jzern@google.com)

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

#if defined(WEBP_HAVE_SDL)

#include "webp_to_sdl.h"

#include <SDL/SDL.h>
#include "webp/decode.h"
#include "../imageio/imageio_util.h"

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

int main(int argc, char* argv[]) {
  int c;
  for (c = 1; c < argc; ++c) {
    const char* file = NULL;
    const uint8_t* webp = NULL;
    size_t webp_size = 0;
    int ok = 0;
    if (!strcmp(argv[c], "-h")) {
      printf("Usage: %s [-h] image.webp [more_files.webp...]\n", argv[1]);
      return 0;
    } else {
      file = argv[c];
    }
    if (!ImgIoUtilReadFile(file, &webp, &webp_size)) {
      fprintf(stderr, "Error opening file: %s\n", file);
      return 1;
    }
    ok = WebpToSDL((const char*)webp, webp_size);
    free((void*)webp);
    if (!ok) {
      fprintf(stderr, "Error decoding file %s\n", file);
      return 1;
    }
    ProcessEvents();
  }
  SDL_Quit();
  return 0;
}

#else  // !WEBP_HAVE_SDL

int main(int argc, const char *argv[]) {
  fprintf(stderr, "SDL support not enabled in %s.\n", argv[0]);
  (void)argc;
  return 0;
}

#endif
