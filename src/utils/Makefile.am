AM_CPPFLAGS = -I$(top_srcdir)/src
noinst_LTLIBRARIES = libwebputils.la

if BUILD_LIBWEBPDECODER
  noinst_LTLIBRARIES += libwebputilsdecode.la
endif

COMMON_SOURCES  =
COMMON_SOURCES += bit_reader.c
COMMON_SOURCES += bit_reader.h
COMMON_SOURCES += color_cache.c
COMMON_SOURCES += color_cache.h
COMMON_SOURCES += filters.c
COMMON_SOURCES += filters.h
COMMON_SOURCES += huffman.c
COMMON_SOURCES += huffman.h
COMMON_SOURCES += quant_levels.c
COMMON_SOURCES += quant_levels.h
COMMON_SOURCES += rescaler.c
COMMON_SOURCES += rescaler.h
COMMON_SOURCES += thread.c
COMMON_SOURCES += thread.h
COMMON_SOURCES += utils.c
COMMON_SOURCES += utils.h

ENC_SOURCES  =
ENC_SOURCES += bit_writer.c
ENC_SOURCES += bit_writer.h
ENC_SOURCES += huffman_encode.c
ENC_SOURCES += huffman_encode.h


libwebputils_la_SOURCES = $(COMMON_SOURCES) $(ENC_SOURCES)

libwebputilsinclude_HEADERS = ../webp/types.h
libwebputilsincludedir = $(includedir)/webp

if BUILD_LIBWEBPDECODER
  libwebputilsdecode_la_SOURCES = $(COMMON_SOURCES)

  libwebputilsdecodeinclude_HEADERS = $(libwebputilsinclude_HEADERS)
  libwebputilsdecodeincludedir = $(libwebputilsincludedir)
endif
