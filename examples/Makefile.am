AM_CPPFLAGS = -I$(top_srcdir)/src

bin_PROGRAMS = dwebp cwebp
if WANT_MUX
  bin_PROGRAMS += webpmux
endif
noinst_LTLIBRARIES = libexampleutil.la

libexampleutil_la_SOURCES = example_util.c
libexampleutilinclude_HEADERS = example_util.h
libexampleutilincludedir =

dwebp_SOURCES = dwebp.c stopwatch.h
dwebp_CPPFLAGS  = $(AM_CPPFLAGS) $(USE_EXPERIMENTAL_CODE)
dwebp_CPPFLAGS += $(JPEG_INCLUDES) $(PNG_INCLUDES)
dwebp_LDADD = libexampleutil.la ../src/libwebp.la $(PNG_LIBS) $(JPEG_LIBS)

cwebp_SOURCES = cwebp.c stopwatch.h
cwebp_CPPFLAGS  = $(AM_CPPFLAGS) $(USE_EXPERIMENTAL_CODE)
cwebp_CPPFLAGS += $(JPEG_INCLUDES) $(PNG_INCLUDES) $(TIFF_INCLUDES)
cwebp_LDADD = ../src/libwebp.la $(JPEG_LIBS) $(PNG_LIBS) $(TIFF_LIBS)

webpmux_SOURCES = webpmux.c
webpmux_CPPFLAGS = $(AM_CPPFLAGS) $(USE_EXPERIMENTAL_CODE)
webpmux_LDADD = libexampleutil.la ../src/mux/libwebpmux.la ../src/libwebp.la
