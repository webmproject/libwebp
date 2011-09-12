AM_CPPFLAGS = -I$(top_srcdir)/src

libwebpdsp_la_SOURCES = dsp.h cpu.c \
                        enc.c enc_sse2.c \
                        dec.c dec_sse2.c dec_neon.c \
                        upsampling.c upsampling_sse2.c \
                        yuv.h yuv.c
libwebpdsp_la_LDFLAGS = -version-info 0:0:0 -lm
libwebpdsp_la_CPPFLAGS = $(USE_EXPERIMENTAL_CODE)
libwebpdspinclude_HEADERS = ../webp/types.h
libwebpdspincludedir = $(includedir)/webp

noinst_HEADERS = dsp.h yuv.h
noinst_LTLIBRARIES = libwebpdsp.la
