AM_CPPFLAGS = -I$(top_srcdir)/src

libwebpdecode_la_SOURCES = bits.h vp8i.h yuv.h bits.c dsp.c dsp_sse2.c frame.c \
                          quant.c tree.c vp8.c webp.c yuv.c idec.c alpha.c \
                          layer.c io.c io_sse2.c buffer.c
libwebpdecode_la_LDFLAGS = -version-info 0:0:0
libwebpdecode_la_CPPFLAGS = $(USE_EXPERIMENTAL_CODE)
libwebpdecodeinclude_HEADERS = ../webp/decode.h ../webp/decode_vp8.h ../webp/types.h
libwebpdecodeincludedir = $(includedir)/webp

noinst_HEADERS = bits.h vp8i.h webpi.h yuv.h

noinst_LTLIBRARIES = libwebpdecode.la
# uncomment the following line (and comment the above) if you want
# to install libwebpdecode library.
#lib_LTLIBRARIES = libwebpdecode.la
