AM_CPPFLAGS = -I$(top_srcdir)/src

libwebputils_la_SOURCES = alpha.h alpha.c \
                          bit_reader.h bit_reader.c \
                          bit_writer.h bit_writer.c \
                          filters.h filters.c \
                          quant_levels.c \
                          tcoder.h tcoderi.h tcoder.c \
                          thread.h thread.c
libwebputils_la_LDFLAGS = -version-info 0:0:0
libwebputilsinclude_HEADERS = ../webp/types.h
libwebputilsincludedir = $(includedir)/webp

noinst_LTLIBRARIES = libwebputils.la
