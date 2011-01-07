AM_CPPFLAGS = -I$(top_buildir)/src
lib_LTLIBRARIES = libwebpdecode.la

libwebpdecode_la_SOURCES = webp/decode.h webp/vp8.h \
  dec/bits.h dec/vp8i.h dec/yuv.h dec/bits.c dec/dsp.c dec/frame.c \
  dec/quant.c dec/tree.c dec/vp8.c dec/webp.c dec/yuv.c
libwebpdecode_la_LDFLAGS = -version-info 0:0:0
libwebpdecodeinclude_HEADERS = webp/decode.h webp/decode_vp8.h webp/types.h
libwebpdecodeincludedir = $(includedir)/webp

noinst_HEADERS = dec/bits.h dec/vp8i.h dec/yuv.h
