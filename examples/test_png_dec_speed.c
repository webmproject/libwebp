// Simple proggy to test the pure decoding speed PNG/WEBP
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../imageio/image_dec.h"
#include "../imageio/imageio_util.h"
#include "./stopwatch.h"
#include "webp/encode.h"

//------------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
  int N = 30;
  for (int c = 1; c < argc; ++c) {
    int k;
    const char* file = argv[c];
    const uint8_t* data = NULL;
    size_t data_size = 0;
    Stopwatch stop_watch;
    double read_time;
    WebPPicture picture;
    if (file[0] == '-') {
      N = atoi(file + 1);
      if (N < 1) N = 1;
      continue;
    }

    ImgIoUtilReadFile(file, &data, &data_size);

    WebPImageReader reader = WebPGuessImageReader(data, data_size);

    StopwatchReset(&stop_watch);
    for (k = 0; k < N; ++k) {
      WebPPictureInit(&picture);
      picture.use_argb = 1;
      reader(data, data_size, &picture, 1, NULL);
      WebPPictureFree(&picture);
    }
    read_time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "[%s] read: %.3fs\n", file, read_time);
    free((void*)data);
  }
}

//------------------------------------------------------------------------------
