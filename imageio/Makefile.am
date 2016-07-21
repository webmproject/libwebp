AM_CPPFLAGS += -I$(top_builddir)/src -I$(top_srcdir)/src
noinst_LTLIBRARIES = libexample_util.la libimageio_util.la libimagedec.la

noinst_HEADERS =
noinst_HEADERS += ../src/webp/decode.h
noinst_HEADERS += ../src/webp/types.h
noinst_HEADERS += ../examples/stopwatch.h

libexample_util_la_SOURCES = example_util.c example_util.h

libimageio_util_la_SOURCES = imageio_util.c imageio_util.h

libimagedec_la_SOURCES  = image_dec.c image_dec.h
libimagedec_la_SOURCES += jpegdec.c jpegdec.h
libimagedec_la_SOURCES += metadata.c metadata.h
libimagedec_la_SOURCES += pngdec.c pngdec.h
libimagedec_la_SOURCES += tiffdec.c tiffdec.h
libimagedec_la_SOURCES += webpdec.c webpdec.h
libimagedec_la_SOURCES += wicdec.c wicdec.h
libimagedec_la_CPPFLAGS = $(JPEG_INCLUDES) $(PNG_INCLUDES) $(TIFF_INCLUDES)
libimagedec_la_CPPFLAGS += $(AM_CPPFLAGS) $(USE_EXPERIMENTAL_CODE)

libexample_util_la_LIBADD = ../src/libwebp.la
