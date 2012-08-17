SUBDIRS = dec enc dsp utils
if WANT_MUX
  SUBDIRS += mux
endif

AM_CPPFLAGS = -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebp.la

libwebp_la_SOURCES =
libwebpinclude_HEADERS =
libwebpinclude_HEADERS += webp/decode.h
libwebpinclude_HEADERS += webp/encode.h
libwebpinclude_HEADERS += webp/types.h
noinst_HEADERS =
noinst_HEADERS += webp/format_constants.h

libwebp_la_LIBADD =
libwebp_la_LIBADD += dec/libwebpdecode.la
libwebp_la_LIBADD += dsp/libwebpdsp.la
libwebp_la_LIBADD += enc/libwebpencode.la
libwebp_la_LIBADD += utils/libwebputils.la

libwebp_la_LDFLAGS = -version-info 4:0:0
libwebpincludedir = $(includedir)/webp

pkgconfig_DATA = libwebp.pc
${pkgconfig_DATA}: ${top_builddir}/config.status
