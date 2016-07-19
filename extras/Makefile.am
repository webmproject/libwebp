AM_CPPFLAGS += -I$(top_builddir)/src -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebpextras.la

libwebpextras_la_SOURCES =
libwebpextras_la_SOURCES += extras.c

libwebpextrasinclude_HEADERS =
libwebpextrasinclude_HEADERS += ../src/webp/extras.h
libwebpextrasinclude_HEADERS += ../src/webp/types.h

libwebpextras_la_CPPFLAGS = $(AM_CPPFLAGS)
libwebpextras_la_LIBADD = ../src/libwebp.la
libwebpextras_la_LDFLAGS = -no-undefined -version-info 0:0:0
libwebpextrasincludedir = $(includedir)/webp
pkgconfig_DATA = libwebpextras.pc
