AC_INIT([webpdecode], [0.1])
AM_INIT_AUTOMAKE
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

AC_ARG_WITH([pkgconfigdir], AS_HELP_STRING([--with-pkgconfigdir=PATH],
	[Path to the pkgconfig directory [[LIBDIR/pkgconfig]]]),
	[pkgconfigdir="$withval"], [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST([pkgconfigdir])

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

dnl === check for JPEG support ===

JPEG_INCLUDES=""
JPEG_LIBS=""
AC_ARG_WITH(jpegincludedir,
            [--with-jpegincludedir=DIR use JPEG includes from DIR],
            JPEG_INCLUDES="-I$withval")
AC_ARG_WITH(jpeglibdir,
            [--with-jpeglibdir=DIR    use JPEG libraries from DIR],
            [JPEG_LIBS="-L$withval"])

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

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile examples/Makefile src/dec/Makefile src/enc/Makefile src/libwebp.pc])


AC_OUTPUT
