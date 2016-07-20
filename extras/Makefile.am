AM_CPPFLAGS += -I$(top_builddir)/src -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebpextras.la

noinst_HEADERS =
noinst_HEADERS += ../src/webp/types.h

libwebpextras_la_SOURCES =
libwebpextras_la_SOURCES += extras.c extras.h

libwebpextras_la_CPPFLAGS = $(AM_CPPFLAGS)
libwebpextras_la_LIBADD = ../src/libwebp.la

noinst_PROGRAMS = get_disto

get_disto_SOURCES = get_disto.c
get_disto_CPPFLAGS  = $(AM_CPPFLAGS)
get_disto_LDADD = ../imageio/libexample_util.la ../imageio/libimagedec.la
get_disto_LDADD += ../src/libwebp.la
get_disto_LDADD += $(PNG_LIBS) $(JPEG_LIBS) $(TIFF_LIBS)
