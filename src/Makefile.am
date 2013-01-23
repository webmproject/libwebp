SUBDIRS = dec enc dsp utils

if WANT_MUX
  SUBDIRS += mux
endif
if WANT_DEMUX
  SUBDIRS += demux
endif

AM_CPPFLAGS = -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebp.la

if BUILD_LIBWEBPDECODER
  lib_LTLIBRARIES += libwebpdecoder.la
endif

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

# Use '-no-undefined' to declare that libwebp does not depend on any libraries
# other than the ones listed on the command line, i.e., after linking, it will
# not have unresolved symbols. Some platforms (Windows among them) require all
# symbols in shared libraries to be resolved at library creation.
libwebp_la_LDFLAGS = -no-undefined -version-info 4:1:0
libwebpincludedir = $(includedir)/webp

if BUILD_LIBWEBPDECODER
  libwebpdecoder_la_SOURCES =
  libwebpdecoderinclude_HEADERS =
  libwebpdecoderinclude_HEADERS += webp/decode.h
  libwebpdecoderinclude_HEADERS += webp/types.h

  libwebpdecoder_la_LIBADD =
  libwebpdecoder_la_LIBADD += dec/libwebpdecode.la
  libwebpdecoder_la_LIBADD += dsp/libwebpdspdecode.la
  libwebpdecoder_la_LIBADD += utils/libwebputilsdecode.la

  libwebpdecoder_la_LDFLAGS = -no-undefined -version-info 4:1:0
  libwebpdecoderincludedir = $(includedir)/webp
endif

pkgconfig_DATA = libwebp.pc
${pkgconfig_DATA}: ${top_builddir}/config.status
