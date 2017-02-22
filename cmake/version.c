// Executable used by CMake to return the version of the library as
// MAJOR.MINOR.PATCH
#include "webp/decode.h"

#include "stdio.h"

int main(int argc, char** argv) {
  const int version = WebPGetDecoderVersion();
  printf("%d.%d.%d", version >> 16, (version >> 8) & 0xff, version & 0xff);
  return 0;
}
