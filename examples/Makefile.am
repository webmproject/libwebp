AM_CPPFLAGS = -I$(top_srcdir)/src

bin_PROGRAMS = dwebp cwebp

dwebp_SOURCES = dwebp.c stopwatch.h
dwebp_CPPFLAGS = $(AM_CPPFLAGS) $(PNG_INCLUDES) $(JPEG_INCLUDES) $(USE_EXPERIMENTAL_CODE)
dwebp_LDADD = ../src/libwebp.la $(PNG_LIBS) $(JPEG_LIBS)

cwebp_SOURCES = cwebp.c stopwatch.h
cwebp_CPPFLAGS = $(AM_CPPFLAGS) $(PNG_INCLUDES) $(JPEG_INCLUDES) $(USE_EXPERIMENTAL_CODE)
cwebp_LDADD = ../src/libwebp.la $(PNG_LIBS) $(JPEG_LIBS)
