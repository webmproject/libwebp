AM_CPPFLAGS = -I$(top_srcdir)/src

dwebp_SOURCES = dwebp.c stopwatch.h
dwebp_CPPFLAGS = $(AM_CPPFLAGS) $(PNG_INCLUDES)
dwebp_LDADD = ../src/dec/libwebpdecode.la $(PNG_LIBS)

bin_PROGRAMS = dwebp
