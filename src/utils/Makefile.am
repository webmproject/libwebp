AM_CPPFLAGS = -I$(top_srcdir)/src

libwebputils_la_SOURCES = bit_reader.h bit_reader.c \
                          bit_writer.h bit_writer.c \
                          thread.h thread.c
libwebputils_la_LDFLAGS = -version-info 0:0:0
libwebputils_la_CPPFLAGS = $(USE_EXPERIMENTAL_CODE)
libwebputilsinclude_HEADERS = ../webp/types.h
libwebputilsincludedir = $(includedir)/webp

noinst_HEADERS = bit_reader.h bit_writer.h thread.h

noinst_LTLIBRARIES = libwebputils.la
