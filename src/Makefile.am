SUBDIRS = dec enc dsp utils

AM_CPPFLAGS = -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebp.la

libwebp_la_SOURCES =
libwebp_la_LIBADD = dec/libwebpdecode.la \
                    enc/libwebpencode.la \
                    utils/libwebputils.la \
                    dsp/libwebpdsp.la
libwebp_la_LDFLAGS = -version-info 2:0:0
libwebpinclude_HEADERS = webp/types.h webp/decode.h webp/decode_vp8.h \
                         webp/encode.h
libwebpincludedir = $(includedir)/webp

pkgconfig_DATA = libwebp.pc

${pkgconfig_DATA}: ${top_builddir}/config.status
