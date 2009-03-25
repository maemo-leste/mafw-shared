#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

bin_PROGRAMS 			= mafw-dbus-wrapper mini-dbus-wrapper
# To check the wrapper part of the daemon, we create a static library from that
noinst_LIBRARIES 		= libmafw-dbus-wrapper.a

AM_LDFLAGS			= $(_LDFLAGS)
AM_CFLAGS			= $(_CFLAGS)
AM_CPPFLAGS 			= $(GOBJECT_CFLAGS) \
				  $(DBUS_CFLAGS) \
				  $(MAFW_CFLAGS) \
				  -I$(top_srcdir) \
				  -I$(top_srcdir)/common
mafw_dbus_wrapper_LDADD	= $(GOBJECT_LIBS) \
				  $(DBUS_LIBS) \
				  $(MAFW_LIBS) \
				  $(top_builddir)/common/libcommon.la \
				  $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  libmafw-dbus-wrapper.a
mini_dbus_wrapper_LDADD		= $(mafw_dbus_wrapper_LDADD)

libmafw_dbus_wrapper_a_SOURCES = wrapper.h \
				  wrapper.c \
				  source-wrapper.c \
				  renderer-wrapper.c \
				  extension-wrapper.c
mafw_dbus_wrapper_SOURCES 	= main.c
mini_dbus_wrapper_SOURCES	= mini-dbus-wrapper.c


CLEANFILES			= $(BUILT_SOURCES) mafw.db *.gcno *.gcda
DISTCLEANFILES			= $(BUILT_SOURCES)
MAINTAINERCLEANFILES		= Makefile.in $(BUILT_SOURCES)
