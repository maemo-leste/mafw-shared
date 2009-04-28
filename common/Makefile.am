#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

noinst_LTLIBRARIES = \
	libcommon.la

AM_CFLAGS                       = $(_CFLAGS)

AM_CPPFLAGS = \
	$(GOBJECT_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(MAFW_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\""

AM_LDFLAGS = $(_LDFLAGS)
libcommon_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(DBUS_LIBS)

libcommon_la_SOURCES = \
	mafw-util.h mafw-util.c \
	mafw-dbus.h mafw-dbus.c \
	dbus-interface.h

CLEANFILES = *.gcno *.gcda
MAINTAINERCLEANFILES = \
	Makefile.in
