ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src imageio man
EXTRA_DIST = COPYING autogen.sh

if BUILD_SHARPYUV
  SUBDIRS += sharpyuv
endif

if BUILD_EXTRAS
  SUBDIRS += extras
endif

SUBDIRS += examples
