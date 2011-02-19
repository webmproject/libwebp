SUBDIRS = dec enc

AM_CPPFLAGS = -I$(top_builddir)/src
lib_LTLIBRARIES = libwebp.la

libwebp_la_SOURCES =
libwebp_la_LIBADD = dec/libwebpdecode.la \
                    enc/libwebpencode.la
libwebp_la_LDFLAGS = -version-info 0:0:0
libwebpinclude_HEADERS = webp/types.h webp/decode.h webp/decode_vp8.h \
                         webp/encode.h
libwebpincludedir = $(includedir)/webp
