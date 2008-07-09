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
	xcb-contrib.c \
	xcb-contrib.h \
	xcprint.c \
	xcprint.h
xcbclip_CFLAGS = $(VISIBILITY_FLAG) $(XCB_CFLAGS)
xcbclip_LDADD = $(XCB_LIBS)
