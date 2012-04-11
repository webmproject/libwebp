AM_CPPFLAGS = -I$(top_srcdir)/src

libwebputils_la_SOURCES = alpha.h alpha.c \
                          bit_reader.h bit_reader.c \
                          bit_writer.h bit_writer.c \
                          color_cache.h color_cache.c \
                          filters.h filters.c \
                          huffman.h huffman.c         \
                          quant_levels.c rescaler.c \
                          tcoder.h tcoderi.h tcoder.c \
                          thread.h thread.c
libwebputilsinclude_HEADERS = ../webp/types.h
libwebputilsincludedir = $(includedir)/webp

noinst_LTLIBRARIES = libwebputils.la
