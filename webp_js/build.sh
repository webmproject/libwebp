#!/bin/sh

echo "EMSCRIPTEN variable is set to: $EMSCRIPTEN"
cmakefile=$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake

cmake -DWEBP_BUILD_WEBP_JS=ON -DWEBP_ENABLE_SIMD=OFF  \
      -DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=1 \
      -DCMAKE_TOOLCHAIN_FILE=${cmakefile} ../

make -j
