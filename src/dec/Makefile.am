AM_CPPFLAGS = -I$(top_srcdir)/src

libwebpdecode_la_SOURCES = vp8i.h webpi.h \
                           frame.c quant.c tree.c vp8.c webp.c \
                           idec.c alpha.c layer.c io.c buffer.c
libwebpdecode_la_LDFLAGS = -version-info 2:0:0
libwebpdecode_la_CPPFLAGS = $(USE_EXPERIMENTAL_CODE)
libwebpdecodeinclude_HEADERS = ../webp/decode.h ../webp/decode_vp8.h ../webp/types.h
libwebpdecodeincludedir = $(includedir)/webp

noinst_HEADERS = vp8i.h webpi.h

noinst_LTLIBRARIES = libwebpdecode.la
