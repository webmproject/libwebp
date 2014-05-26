AM_CPPFLAGS = -I$(top_srcdir)/src
noinst_LTLIBRARIES = libwebpdsp.la libwebpdsp_avx2.la

if BUILD_LIBWEBPDECODER
  noinst_LTLIBRARIES += libwebpdspdecode.la
endif

common_HEADERS = ../webp/types.h
commondir = $(includedir)/webp

COMMON_SOURCES =
COMMON_SOURCES += alpha_processing.c
COMMON_SOURCES += cpu.c
COMMON_SOURCES += dec.c
COMMON_SOURCES += dec_clip_tables.c
COMMON_SOURCES += dec_mips32.c
COMMON_SOURCES += dec_neon.c
COMMON_SOURCES += dec_sse2.c
COMMON_SOURCES += dsp.h
COMMON_SOURCES += lossless.c
COMMON_SOURCES += lossless.h
COMMON_SOURCES += lossless_mips32.c
COMMON_SOURCES += lossless_neon.c
COMMON_SOURCES += lossless_sse2.c
COMMON_SOURCES += neon.h
COMMON_SOURCES += upsampling.c
COMMON_SOURCES += upsampling_mips32.c
COMMON_SOURCES += upsampling_neon.c
COMMON_SOURCES += upsampling_sse2.c
COMMON_SOURCES += yuv.c
COMMON_SOURCES += yuv.h
COMMON_SOURCES += yuv_tables_sse2.h

ENC_SOURCES =
ENC_SOURCES += enc.c
ENC_SOURCES += enc_mips32.c
ENC_SOURCES += enc_neon.c
ENC_SOURCES += enc_sse2.c

libwebpdsp_avx2_la_SOURCES =
libwebpdsp_avx2_la_SOURCES += enc_avx2.c
libwebpdsp_avx2_la_CPPFLAGS = $(libwebpdsp_la_CPPFLAGS)
libwebpdsp_avx2_la_CFLAGS = $(AM_CFLAGS) $(AVX2_FLAGS)

libwebpdsp_la_SOURCES = $(COMMON_SOURCES) $(ENC_SOURCES)

noinst_HEADERS =
noinst_HEADERS += ../dec/decode_vp8.h
noinst_HEADERS += ../webp/decode.h

libwebpdsp_la_CPPFLAGS = $(USE_EXPERIMENTAL_CODE) $(USE_SWAP_16BIT_CSP)
libwebpdsp_la_LDFLAGS = -lm
libwebpdsp_la_LIBADD = libwebpdsp_avx2.la

if BUILD_LIBWEBPDECODER
  libwebpdspdecode_la_SOURCES = $(COMMON_SOURCES)

  libwebpdspdecode_la_CPPFLAGS = $(libwebpdsp_la_CPPFLAGS)
  libwebpdspdecode_la_LDFLAGS = $(libwebpdsp_la_LDFLAGS)
  libwebpdspdecode_la_LIBADD = libwebpdsp_avx2.la
endif
