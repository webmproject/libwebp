AC_INIT([libwebp], [0.3.0],
        [http://code.google.com/p/webp/issues],,
        [http://developers.google.com/speed/webp])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])

dnl === automake >= 1.12 requires this for 'unusual archivers' support.
dnl === it must occur before LT_INIT (AC_PROG_LIBTOOL).
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_PROG_LIBTOOL
AM_PROG_CC_C_O

dnl === Enable less verbose output when building.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ARG_WITH([pkgconfigdir], AS_HELP_STRING([--with-pkgconfigdir=DIR],
            [Path to the pkgconfig directory @<:@LIBDIR/pkgconfig@:>@]),
            [pkgconfigdir="$withval"], [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST([pkgconfigdir])

dnl === WITHLIB_OPTION([opt_pfx], [outvar_pfx])
dnl ===   Defines --with-<opt_pfx>{include,lib}dir options which set
dnl ===   the variables <outvar_pfx>_{INCLUDES,LIBS}.
AC_DEFUN([WITHLIB_OPTION],
  [AC_ARG_WITH([$1includedir],
               AS_HELP_STRING([--with-$1includedir=DIR],
                              [use $2 includes from DIR]),
               $2_INCLUDES="-I$withval")
   AC_ARG_WITH([$1libdir],
               AS_HELP_STRING([--with-$1libdir=DIR],
                              [use $2 libraries from DIR]),
               [$2_LIBS="-L$withval"])])

dnl === Check for pthread support
AC_ARG_ENABLE([threading],
              AS_HELP_STRING([--disable-threading],
                             [Disable detection of thread support]),,
              [enable_threading=yes])
if test "$enable_threading" = "yes"; then
  AC_MSG_NOTICE([checking for threading support...])
  AX_PTHREAD([AC_DEFINE([WEBP_USE_THREAD], [1],
                        [Undefine this to disable thread support.])
              LIBS="$PTHREAD_LIBS $LIBS"
              CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
              CC="$PTHREAD_CC"
             ],
             [enable_threading=no])
fi
AC_MSG_NOTICE([checking if threading is enabled... ${enable_threading-no}])

dnl === check for PNG support ===

PNG_INCLUDES=""
PNG_LIBS=""
AC_PATH_PROGS(LIBPNG_CONFIG,
              [libpng-config libpng15-config libpng14-config libpng12-config])
if test -n "$LIBPNG_CONFIG"; then
  PNG_INCLUDES=`$LIBPNG_CONFIG --cflags`
  PNG_PREFIX=`$LIBPNG_CONFIG --prefix`
  if test "${PNG_PREFIX}/lib" != "/usr/lib" ; then
    PNG_LIBS="-L${PNG_PREFIX}/lib"
  fi
  PNG_LIBS="$PNG_LIBS `$LIBPNG_CONFIG --libs`"
fi

WITHLIB_OPTION([png], [PNG])

SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LIBS=$LIBS
CPPFLAGS="$PNG_INCLUDES $CPPFLAGS"
LIBS="$PNG_LIBS $LIBS"

AC_CHECK_HEADER(png.h,
  AC_SEARCH_LIBS(png_get_libpng_ver, [png],
                 [test "$ac_cv_search_png_get_libpng_ver" = "none required" \
                    || PNG_LIBS="$PNG_LIBS $ac_cv_search_png_get_libpng_ver"
                  PNG_INCLUDES="$PNG_INCLUDES -DWEBP_HAVE_PNG"
                  AC_DEFINE(WEBP_HAVE_PNG, [1],
                            [Set to 1 if PNG library is installed])
                  png_support=yes
                 ],
                 [AC_MSG_WARN(Optional png library not found)
                  PNG_LIBS=""
                  PNG_INCLUDES=""
                 ],
                 [$MATH_LIBS]),
  [AC_MSG_WARN(png library not available - no png.h)
   PNG_LIBS=""
   PNG_INCLUDES=""
  ],
)
AC_SUBST(PNG_LIBS)
AC_SUBST(PNG_INCLUDES)

CPPFLAGS=$SAVED_CPPFLAGS
LIBS=$SAVED_LIBS

dnl === check for JPEG support ===

JPEG_INCLUDES=""
JPEG_LIBS=""
WITHLIB_OPTION([jpeg], [JPEG])

SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LIBS=$LIBS
CPPFLAGS="$JPEG_INCLUDES $CPPFLAGS"
LIBS="$JPEG_LIBS $LIBS"

AC_CHECK_HEADER(jpeglib.h,
  AC_CHECK_LIB(jpeg, jpeg_set_defaults,
               [JPEG_LIBS="$JPEG_LIBS -ljpeg"
                JPEG_INCLUDES="$JPEG_INCLUDES -DWEBP_HAVE_JPEG"
                AC_DEFINE(WEBP_HAVE_JPEG, [1],
                          [Set to 1 if JPEG library is installed])
                jpeg_support=yes
               ],
               AC_MSG_WARN(Optional jpeg library not found),
               [$MATH_LIBS]),
  AC_MSG_WARN(jpeg library not available - no jpeglib.h)
)
AC_SUBST(JPEG_LIBS)
AC_SUBST(JPEG_INCLUDES)

CPPFLAGS=$SAVED_CPPFLAGS
LIBS=$SAVED_LIBS

dnl === check for TIFF support ===

TIFF_INCLUDES=""
TIFF_LIBS=""
WITHLIB_OPTION([tiff], [TIFF])

SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LIBS=$LIBS
CPPFLAGS="$TIFF_INCLUDES $CPPFLAGS"
LIBS="$TIFF_LIBS $LIBS"

AC_CHECK_HEADER(tiffio.h,
  AC_CHECK_LIB(tiff, TIFFGetVersion,
               [TIFF_LIBS="$TIFF_LIBS -ltiff"
                TIFF_INCLUDES="$TIFF_INCLUDES -DWEBP_HAVE_TIFF"
                AC_DEFINE(WEBP_HAVE_TIFF, [1],
                          [Set to 1 if TIFF library is installed])
                tiff_support=yes
               ],
               AC_MSG_WARN(Optional tiff library not found),
               [$MATH_LIBS]),
  AC_MSG_WARN(tiff library not available - no tiffio.h)
)
AC_SUBST(TIFF_LIBS)
AC_SUBST(TIFF_INCLUDES)

CPPFLAGS=$SAVED_CPPFLAGS
LIBS=$SAVED_LIBS

dnl === check for GIF support ===

GIF_INCLUDES=""
GIF_LIBS=""
WITHLIB_OPTION([gif], [GIF])

SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LIBS=$LIBS
CPPFLAGS="$GIF_INCLUDES $CPPFLAGS"
LIBS="$GIF_LIBS $LIBS"

AC_CHECK_HEADER(gif_lib.h,
  AC_CHECK_LIB([gif], [DGifOpenFileHandle],
               [GIF_LIBS="$GIF_LIBS -lgif"
                gif_support=yes
               ],
               AC_MSG_WARN(Optional gif library not found),
               [$MATH_LIBS]),
  AC_MSG_WARN(gif library not available - no gif_lib.h)
)
AC_SUBST(GIF_LIBS)
AC_SUBST(GIF_INCLUDES)

CPPFLAGS=$SAVED_CPPFLAGS
LIBS=$SAVED_LIBS

if test "$gif_support" = "yes" -a \
        "$enable_libwebpmux" = "yes"; then
  build_gif2webp=yes
fi
AM_CONDITIONAL([BUILD_GIF2WEBP], [test "${build_gif2webp}" = "yes"])

dnl === check for WIC support ===

if test "$target_os" = "mingw32"; then
  AC_CHECK_HEADERS([wincodec.h shlwapi.h windows.h])
  if test "$ac_cv_header_wincodec_h" = "yes"; then
    AC_MSG_CHECKING(for Windows Imaging Component support)
    SAVED_LIBS=$LIBS
    LIBS="-lshlwapi -lole32 $LIBS"
    # match include structure from [cd]webp.c
    wic_headers="
      #define INITGUID
      #define CINTERFACE
      #define COBJMACROS
      #define _WIN32_IE 0x500

      #include <shlwapi.h>
      #include <windows.h>
      #include <wincodec.h>
      "
    # test for functions from each lib and the GUID is created properly
    wic_main="
      int main(void) {
        CLSID_WICImagingFactory;
        CoInitialize(NULL);
        SHCreateStreamOnFile(NULL, 0, NULL);
        return 0;
      }
      "
    AC_LANG_PUSH(C)
    AC_LINK_IFELSE(
      [AC_LANG_SOURCE([
         $wic_headers
         $wic_main])],
      [wic_support=yes],
      [wic_support=no]
    )
    AC_LANG_POP

    test "$wic_support" = "yes" || LIBS=$SAVED_LIBS
    AC_MSG_RESULT(${wic_support-no})
  fi
fi

dnl === If --enable-swap-16bit-csp is defined, add -DWEBP_SWAP_16BIT_CSP

USE_SWAP_16BIT_CSP=""
AC_MSG_CHECKING(if --enable-swap-16bit-csp option is specified)
AC_ARG_ENABLE([swap-16bit-csp],
              AS_HELP_STRING([--enable-swap-16bit-csp],
                             [Enable byte swap for 16 bit colorspaces]))
if test "$enable_swap_16bit_csp" = "yes"; then
  USE_SWAP_16BIT_CSP="-DWEBP_SWAP_16BIT_CSP"
fi
AC_MSG_RESULT(${enable_swap_16bit_csp-no})
AC_SUBST(USE_SWAP_16BIT_CSP)

dnl === If --enable-experimental is defined, add -DWEBP_EXPERIMENTAL_FEATURES

USE_EXPERIMENTAL_CODE=""
AC_MSG_CHECKING(if --enable-experimental option is specified)
AC_ARG_ENABLE([experimental], AS_HELP_STRING([--enable-experimental],
                                             [Activate experimental features]))
if test "$enable_experimental" = "yes"; then
  AC_DEFINE(WEBP_EXPERIMENTAL_FEATURES, [1], [Enable experimental code])
  USE_EXPERIMENTAL_CODE="-DWEBP_EXPERIMENTAL_FEATURES"
fi
AC_MSG_RESULT(${enable_experimental-no})
AC_SUBST(USE_EXPERIMENTAL_CODE)

dnl === Check whether libwebpmux should be built
AC_MSG_CHECKING(whether libwebpmux is to be built)
AC_ARG_ENABLE([libwebpmux],
              AS_HELP_STRING([--enable-libwebpmux],
                             [Build libwebpmux @<:@default=no@:>@]))
AC_MSG_RESULT(${enable_libwebpmux-no})
AM_CONDITIONAL([WANT_MUX], [test "$enable_libwebpmux" = "yes"])

dnl === Check whether libwebpdemux should be built
AC_MSG_CHECKING(whether libwebpdemux is to be built)
AC_ARG_ENABLE([libwebpdemux],
              AS_HELP_STRING([--enable-libwebpdemux],
                             [Build libwebpdemux @<:@default=no@:>@]))
AC_MSG_RESULT(${enable_libwebpdemux-no})
AM_CONDITIONAL([WANT_DEMUX], [test "$enable_libwebpdemux" = "yes"])

dnl === Check whether decoder library should be built.
AC_MSG_CHECKING(whether decoder library is to be built)
AC_ARG_ENABLE([libwebpdecoder],
              AS_HELP_STRING([--enable-libwebpdecoder],
                             [Build libwebpdecoder @<:@default=no@:>@]))
AC_MSG_RESULT(${enable_libwebpdecoder-no})
AM_CONDITIONAL([BUILD_LIBWEBPDECODER], [test "$enable_libwebpdecoder" = "yes"])

dnl =========================

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile \
                 examples/Makefile src/dec/Makefile \
                 src/enc/Makefile src/dsp/Makefile \
                 src/demux/Makefile src/mux/Makefile \
                 src/utils/Makefile \
                 src/libwebp.pc src/libwebpdecoder.pc \
                 src/demux/libwebpdemux.pc src/mux/libwebpmux.pc])


AC_OUTPUT

AC_MSG_NOTICE([
WebP Configuration Summary
--------------------------

Shared libraries: ${enable_shared}
Static libraries: ${enable_static}
Threaded decode: ${enable_threading-no}
libwebp: yes
libwebpdecoder: ${enable_libwebpdecoder-no}
libwebpdemux: ${enable_libwebpdemux-no}
libwebpmux: ${enable_libwebpmux-no}

Tools:
cwebp: yes
  Input format support
  ====================
  JPEG : ${jpeg_support-no}
  PNG  : ${png_support-no}
  TIFF : ${tiff_support-no}
  WIC  : ${wic_support-no}
dwebp: yes
  Output format support
  =====================
  PNG  : ${png_support-no}
  WIC  : ${wic_support-no}
gif2webp: ${build_gif2webp-no}
webpmux: ${enable_libwebpmux-no}
])
