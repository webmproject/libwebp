AC_INIT([libwebp], [0.2.0],
        [http://code.google.com/p/webp/issues],,
        [http://developers.google.com/speed/webp])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

dnl === Enable less verbose output when building.
dnl === If an older aclocal exits with an error comment these lines out.
m4_define_default([AM_SILENT_RULES], [])
AM_SILENT_RULES

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
               ],
               AC_MSG_WARN(Optional tiff library not found),
               [$MATH_LIBS]),
  AC_MSG_WARN(tiff library not available - no tiffio.h)
)
AC_SUBST(TIFF_LIBS)
AC_SUBST(TIFF_INCLUDES)

CPPFLAGS=$SAVED_CPPFLAGS
LIBS=$SAVED_LIBS

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

dnl === If --enable-experimental is defined, add -DWEBP_EXPERIMENTAL_FEATURES

USE_EXPERIMENTAL_CODE=""
AC_MSG_CHECKING(if --enable-experimental option is specified)
AC_ARG_ENABLE([experimental], AS_HELP_STRING([--enable-experimental],
                                             [Activate experimental features]))
if test "$enable_experimental" = "yes"; then
        AC_DEFINE(EXPERIMENTAL,,[Enable experimental code])
        USE_EXPERIMENTAL_CODE="-DWEBP_EXPERIMENTAL_FEATURES"
fi
AC_MSG_RESULT(${enable_experimental-no})
AC_SUBST(USE_EXPERIMENTAL_CODE)

dnl === Check whether libwebpmux should be built
AC_MSG_CHECKING(whether libwebpmux is to be built)
AC_ARG_ENABLE([experimental-libwebpmux],
              AS_HELP_STRING([--enable-experimental-libwebpmux],
                             [Build libwebpmux @<:@default=no@:>@]))
AC_MSG_RESULT(${enable_experimental_libwebpmux-no})
AM_CONDITIONAL([WANT_MUX], [test "$enable_experimental_libwebpmux" = "yes"])

dnl =========================

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile \
                 examples/Makefile src/dec/Makefile \
                 src/enc/Makefile src/dsp/Makefile \
                 src/utils/Makefile \
                 src/mux/Makefile \
                 src/libwebp.pc])


AC_OUTPUT
