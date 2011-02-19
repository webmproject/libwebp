AM_CPPFLAGS = -I$(top_builddir)/src

libwebpencode_la_SOURCES = analysis.c bit_writer.c bit_writer.h \
                          config.c cost.c cost.h dsp.c filter.c \
                          frame.c iterator.c picture.c quant.c  \
                          syntax.c tree.c vp8enci.h webpenc.c
libwebpencode_la_LDFLAGS = -version-info 0:0:0 -lm
libwebpencodeinclude_HEADERS = ../webp/encode.h ../webp/types.h
libwebpencodeincludedir = $(includedir)/webp

noinst_HEADERS = cost.h bit_writer.h vp8enci.h
noinst_LTLIBRARIES = libwebpencode.la
# uncomment the following line (and comment the above) if you want
# to install libwebpencode library.
#lib_LTLIBRARIES = libwebpencode.la
