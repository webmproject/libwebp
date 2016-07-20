AM_CPPFLAGS += -I$(top_builddir)/src -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebpextras.la

noinst_HEADERS =
noinst_HEADERS += ../src/webp/types.h

libwebpextras_la_SOURCES =
libwebpextras_la_SOURCES += extras.c extras.h quality_estimate.c

libwebpextras_la_CPPFLAGS = $(AM_CPPFLAGS)
libwebpextras_la_LIBADD = ../src/libwebp.la

noinst_PROGRAMS = webp_quality

webp_quality_SOURCES  = webp_quality.c
webp_quality_CPPFLAGS  = $(AM_CPPFLAGS) $(USE_EXPERIMENTAL_CODE)
webp_quality_LDADD  = ../imageio/libexample_util.la ../imageio/libimagedec.la
webp_quality_LDADD += ./libwebpextras.la
webp_quality_LDADD += ../src/libwebp.la $(JPEG_LIBS) $(PNG_LIBS) $(TIFF_LIBS)
