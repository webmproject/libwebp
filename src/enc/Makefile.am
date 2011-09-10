AM_CPPFLAGS = -I$(top_srcdir)/src

libwebpencode_la_SOURCES = analysis.c config.c cost.c cost.h filter.c \
                           frame.c iterator.c picture.c quant.c  \
                           syntax.c tree.c vp8enci.h webpenc.c alpha.c \
                           layer.c
libwebpencode_la_LDFLAGS = -version-info 2:0:0 -lm
libwebpencode_la_CPPFLAGS = $(USE_EXPERIMENTAL_CODE)
libwebpencodeinclude_HEADERS = ../webp/encode.h ../webp/types.h
libwebpencodeincludedir = $(includedir)/webp

noinst_HEADERS = cost.h vp8enci.h
noinst_LTLIBRARIES = libwebpencode.la
