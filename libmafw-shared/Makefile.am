#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

lib_LTLIBRARIES 		= libmafw-shared.la

AM_LDFLAGS 			= -version-info 0:0:0 $(_LDFLAGS)
AM_CFLAGS			= $(_CFLAGS)
AM_CPPFLAGS 			= $(GOBJECT_CFLAGS) \
				  $(DBUS_CFLAGS) \
				  $(MAFW_CFLAGS) \
				  -I$(top_srcdir)/common \
				  -I$(top_srcdir) -I. \
				  -DLOCALSTATEDIR="\"$(localstatedir)\""\
				  -DGLIB_DISABLE_DEPRECATION_WARNINGS
libmafw_shared_la_LIBADD 	= $(GOBJECT_LIBS) $(DBUS_LIBS) $(MAFW_LIBS) \
				  $(top_builddir)/common/libcommon.la
BUILT_SOURCES 			= mafw-marshal.c \
				  mafw-marshal.h
libmafw_shared_la_SOURCES 	= $(BUILT_SOURCES) \
				  mafw-proxy-extension.h \
				  mafw-proxy-extension.c \
				  mafw-proxy-renderer.h \
				  mafw-proxy-renderer.c \
				  mafw-proxy-source.h \
				  mafw-proxy-source.c \
				  mafw-playlist-manager.c \
				  mafw-proxy-playlist.c \
				  mafw-shared.c

# The generated C source doesn't #include the header which contains
# the function prototypes required by -Wmissing-declarations.
mafw-marshal.c: mafw-marshal.list
	( \
		echo '#include "mafw-marshal.h"'; \
		$(GLIB_GENMARSHAL) --prefix=mafw_marshal --body $^ \
	) > $@

mafw-marshal.h: mafw-marshal.list
	$(GLIB_GENMARSHAL) --prefix=mafw_marshal --header \
		$^ > $@


# maybe use some $mafwextdir instead of /usr/lib
libmafwincdir			= $(includedir)/mafw-1.0/libmafw-shared
libmafwinc_HEADERS 		= mafw-playlist-manager.h \
				  mafw-proxy-playlist.h \
				  mafw-shared.h

EXTRA_DIST			= mafw-marshal.list
CLEANFILES			= $(BUILT_SOURCES)
DISTCLEANFILES			= $(BUILT_SOURCES)
MAINTAINERCLEANFILES		= Makefile.in $(BUILT_SOURCES)
