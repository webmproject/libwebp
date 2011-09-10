AC_INIT([libwebp], [0.1.3],
        [http://code.google.com/p/webp/issues],,
        [http://code.google.com/speed/webp])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

AC_ARG_WITH([pkgconfigdir], AS_HELP_STRING([--with-pkgconfigdir=PATH],
	[Path to the pkgconfig directory [[LIBDIR/pkgconfig]]]),
	[pkgconfigdir="$withval"], [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST([pkgconfigdir])

dnl === Check libz is present

if test "$enable_experimental" = "yes"; then
  AC_CHECK_HEADER(zlib.h,
    AC_CHECK_LIB(z, gzsetparams,,AC_MSG_ERROR(zlib library not found)),
    AC_MSG_ERROR(zlib not available - no zlib.h)
  )
fi

dnl === check for PNG support ===

PNG_INCLUDES=""
PNG_LIBS=""
AC_PATH_PROG(LIBPNG_CONFIG, libpng-config)
if test -n "$LIBPNG_CONFIG"; then
  PNG_INCLUDES=`$LIBPNG_CONFIG --cflags`
  PNG_PREFIX=`$LIBPNG_CONFIG --prefix`
  if test "${PNG_PREFIX}/lib" != "/usr/lib" ; then
    PNG_LIBS="-L${PNG_PREFIX}/lib"
  fi
fi

AC_ARG_WITH(pngincludedir,
            [--with-pngincludedir=DIR use PNG includes from DIR],
            PNG_INCLUDES="-I$withval")
AC_ARG_WITH(pnglibdir,
            [--with-pnglibdir=DIR    use PNG libraries from DIR],
            [PNG_LIBS="-L$withval"])

SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LIBS=$LIBS
CPPFLAGS="$PNG_INCLUDES $CPPFLAGS"
LIBS="$PNG_LIBS $LIBS"

AC_CHECK_HEADER(png.h,
  AC_CHECK_LIB(png, main,
               [PNG_LIBS="$PNG_LIBS -lpng"
                PNG_INCLUDES="$PNG_INCLUDES -DWEBP_HAVE_PNG"
                AC_DEFINE(WEBP_HAVE_PNG, [1], [Set to 1 if PNG library is installed])
               ],
               AC_MSG_WARN(Optional png library not found),
               [$MATH_LIBS]),
  AC_MSG_WARN(png library not available - no png.h)
)
AC_SUBST(PNG_LIBS)
AC_SUBST(PNG_INCLUDES)

CPPFLAGS=$SAVED_CPPFLAGS
LIBS=$SAVED_LIBS

dnl === check for JPEG support ===

JPEG_INCLUDES=""
JPEG_LIBS=""
AC_ARG_WITH(jpegincludedir,
            [--with-jpegincludedir=DIR use JPEG includes from DIR],
            JPEG_INCLUDES="-I$withval")
AC_ARG_WITH(jpeglibdir,
            [--with-jpeglibdir=DIR    use JPEG libraries from DIR],
            [JPEG_LIBS="-L$withval"])

SAVED_CPPFLAGS=$CPPFLAGS
SAVED_LIBS=$LIBS
CPPFLAGS="$JPEG_INCLUDES $CPPFLAGS"
LIBS="$JPEG_LIBS $LIBS"

AC_CHECK_HEADER(jpeglib.h,
  AC_CHECK_LIB(jpeg, jpeg_set_defaults,
               [JPEG_LIBS="$JPEG_LIBS -ljpeg"
                JPEG_INCLUDES="$JPEG_INCLUDES -DWEBP_HAVE_JPEG"
                AC_DEFINE(WEBP_HAVE_JPEG, [1], [Set to 1 if JPEG library is installed])
               ],
               AC_MSG_WARN(Optional jpeg library not found),
               [$MATH_LIBS]),
  AC_MSG_WARN(jpeg library not available - no jpeglib.h)
)
AC_SUBST(JPEG_LIBS)
AC_SUBST(JPEG_INCLUDES)

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

dnl === If --enable-experimental is defined, add the flag WEBP_EXPERIMENTAL_FEATURES

USE_EXPERIMENTAL_CODE=""
AC_MSG_CHECKING(if --enable-experimental option is specified)
AC_ARG_ENABLE(experimental, [  --enable-experimental         Activate experimental features])
if test "$enable_experimental" = "yes"; then
        AC_DEFINE(EXPERIMENTAL,,[Enable experimental code])
        USE_EXPERIMENTAL_CODE="-DWEBP_EXPERIMENTAL_FEATURES"
fi
AC_MSG_RESULT(${enable_experimental-no})
AC_SUBST(USE_EXPERIMENTAL_CODE)

dnl =========================

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile \
                 examples/Makefile src/dec/Makefile \
                 src/enc/Makefile src/dsp/Makefile \
                 src/utils/Makefile \
                 src/libwebp.pc])


AC_OUTPUT
