ACLOCAL_AMFLAGS = -I m4

dist_doc_DATA = CHANGES COPYING README

EXTRA_DIST = xclip.man m4

TESTS = xctest

bin_PROGRAMS = xcbclip

xcbclip_SOURCES = \
	xcdef.h \
	xclib.c \
	xclib.h \
	xclip.c \
	xcprint.c \
	xcprint.h
xcbclip_CFLAGS = $(VISIBILITY_FLAG)
xcbclip_LDADD = -lX11 -lXmu
