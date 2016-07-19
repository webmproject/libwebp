AM_CPPFLAGS += -I$(top_builddir)/src -I$(top_srcdir)/src
lib_LTLIBRARIES = libwebpextras.la

libwebpextras_la_SOURCES =
libwebpextras_la_SOURCES += extras.c

noinst_HEADERS =
noinst_HEADERS += extras.h
noinst_HEADERS += ../src/webp/types.h

libwebpextras_la_CPPFLAGS = $(AM_CPPFLAGS)
libwebpextras_la_LIBADD = ../src/libwebp.la
