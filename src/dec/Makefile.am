AM_CPPFLAGS = -I$(top_builddir)/src
lib_LTLIBRARIES = libwebpdecode.la

libwebpdecode_la_SOURCES = bits.h vp8i.h yuv.h bits.c dsp.c frame.c \
                          quant.c tree.c vp8.c webp.c yuv.c
libwebpdecode_la_LDFLAGS = -version-info 0:0:0
libwebpdecodeinclude_HEADERS = ../webp/decode.h ../webp/decode_vp8.h ../webp/types.h
libwebpdecodeincludedir = $(includedir)/webp

noinst_HEADERS = bits.h vp8i.h yuv.h
