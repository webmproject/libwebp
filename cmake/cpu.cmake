## Check for SIMD extensions.
set(WEBP_SIMD_FLAGS "SSE2;SSE41;AVX2")
set(WEBP_SIMD_FILE_EXTENSIONS "_sse2.c;_sse41.c;_avx2.c")
if(MSVC)
  # MSVC does not have a SSE4 flag but AVX2 support implies
  # SSE4 support.
  set(SIMD_ENABLE_FLAGS "/arch:SSE2;/arch:AVX2;/arch:AVX2")
  set(SIMD_DISABLE_FLAGS)
else()
  set(SIMD_ENABLE_FLAGS "-msse2;-msse4.1;-mavx2")
  set(SIMD_DISABLE_FLAGS "-mno-sse2;-mno-sse4.1;-mno-avx2")
endif()

set(WEBP_SIMD_FILES_TO_NOT_INCLUDE)
set(WEBP_SIMD_FILES_TO_INCLUDE)
set(WEBP_SIMD_FLAGS_TO_INCLUDE)

list(LENGTH WEBP_SIMD_FLAGS WEBP_SIMD_FLAGS_LENGTH)
math(EXPR WEBP_SIMD_FLAGS_RANGE "${WEBP_SIMD_FLAGS_LENGTH} - 1")

foreach(I_SIMD RANGE ${WEBP_SIMD_FLAGS_RANGE})
  list(GET WEBP_SIMD_FLAGS ${I_SIMD} WEBP_SIMD_FLAG)
  list(GET SIMD_ENABLE_FLAGS ${I_SIMD} SIMD_COMPILE_FLAG)
  set(CMAKE_REQUIRED_FLAGS ${SIMD_COMPILE_FLAG})
  check_c_source_compiles("
      #include \"${CMAKE_CURRENT_LIST_DIR}/../src/dsp/dsp.h\"
      int main(void) {
        #if !defined(WEBP_USE_${WEBP_SIMD_FLAG})
        this is not valid code
        #endif
        return 0;
      }
    "
    WEBP_HAVE_${WEBP_SIMD_FLAG}
  )

  # Check which files we should include or not.
  list(GET WEBP_SIMD_FILE_EXTENSIONS ${I_SIMD} WEBP_SIMD_FILE_EXTENSION)
  file(GLOB SIMD_FILES "${CMAKE_CURRENT_LIST_DIR}/../"
    "src/dsp/*${WEBP_SIMD_FILE_EXTENSION}"
  )
  if(WEBP_HAVE_${WEBP_SIMD_FLAG})
    # Memorize the file and flags.
    foreach(FILE ${SIMD_FILES})
      list(APPEND WEBP_SIMD_FILES_TO_INCLUDE ${FILE})
      list(APPEND WEBP_SIMD_FLAGS_TO_INCLUDE ${SIMD_COMPILE_FLAG})
    endforeach()
  else()
    # Remove the file from the list.
    foreach(FILE ${SIMD_FILES})
      list(APPEND WEBP_SIMD_FILES_NOT_TO_INCLUDE ${FILE})
    endforeach()
    # Explicitly disable SIMD.
    if(SIMD_DISABLE_FLAGS)
      list(GET SIMD_DISABLE_FLAGS ${I_SIMD} SIMD_COMPILE_FLAG)
      include(CheckCCompilerFlag)
      check_c_compiler_flag(${SIMD_COMPILE_FLAG} HAS_COMPILE_FLAG)
      if(HAS_COMPILE_FLAG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SIMD_COMPILE_FLAG}")
      endif()
    endif()
  endif()
endforeach()
