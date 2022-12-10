#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

bin_PROGRAMS 			= mafw-playlist-daemon

# To check the wrapper part of the daemon, we create a static library from that
noinst_LIBRARIES 		= libmafw-playlist-daemon.a

AM_LDFLAGS			= $(_LDFLAGS)
AM_CFLAGS			= $(_CFLAGS)
AM_CPPFLAGS 			= $(GOBJECT_CFLAGS) \
				  $(DBUS_CFLAGS) \
				  $(MAFW_CFLAGS) \
				  $(TOTEMPL_CFLAGS) \
				  -I$(top_srcdir)/common \
				  -I$(top_srcdir)

mafw_playlist_daemon_LDADD 	= libmafw-playlist-daemon.a \
				  $(GOBJECT_LIBS) \
				  $(DBUS_LIBS) \
				  $(MAFW_LIBS) \
				  $(TOTEMPL_LIBS) \
				  $(top_builddir)/common/libcommon.la \
				  $(top_builddir)/libmafw-shared/libmafw-shared.la


mafw_playlist_daemon_SOURCES 	= mafw-playlist-daemon.c

libmafw_playlist_daemon_a_SOURCES = playlist-manager-wrapper.c \
				  playlist-wrapper.c \
				  aplaylist.c \
				  mpd-internal.h

dbusserv_DATA			= com.nokia.mafw.playlist.service

CLEANFILES			= mafw.db *.gcda *.gcno
MAINTAINERCLEANFILES		= Makefile.in
