#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

#We deactivate subprocesses
MAKEFLAGS = -j1

# The name of the module, e.g. 'glib'.
DOC_MODULE = mafw-shared

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR = ../libmafw-shared

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS =

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
SCAN_OPTIONS =

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS = --sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS =

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = --extra-dir=$(MAFW_PREFIX)/share/gtk-doc/html \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB = $(top_srcdir)/libmafw-shared/*.h
CFILE_GLOB = $(top_srcdir)/libmafw-shared/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES = mafw-marshal.h mafw-proxy-extension.h mafw-proxy-source.h \
	mafw-proxy-renderer.h

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files = version.xml describing-mafw.sgml initializing-mafw.sgml \
	renderers.sgml playlists.sgml sources.sgml built_in_plugins.sgml \
	creating_plugins.sgml examples.sgml preface.sgml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files = $(content_files)

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
INCLUDES = \
	$(GOBJECT_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(MAFW_CFLAGS) \
	-I$(top_srcdir)/libmafw-shared \
	-I$(top_srcdir) \
	-I. \
	-DLOCALSTATEDIR="\"$(localstatedir)\""
GTKDOC_LIBS = \
	$(top_builddir)/libmafw-shared/libmafw-shared.la \
	$(GOBJECT_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/doc/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += version.xml.in

MAINTAINERCLEANFILES = \
	Makefile.in \
	tmpl/mafw* \
	mafw-shared-undocumented.txt \
	mafw-shared-decl.txt \
	mafw-shared-overrides.txt \
	mafw-shared-decl-list.txt

UPLOAD_REPLACINGS = -e "s/\$(GLIB_PREFIX)\/share\/gtk-doc\/html\/gobject/http:\/\/library.gnome.org\/devel\/gobject\/stable/g" \
	-e "s/\$(GLIB_PREFIX)\/share\/gtk-doc\/html\/glib/http:\/\/library.gnome.org\/devel\/glib\/stable/g"
#Still need to now mafw official site to replace it here.

html-upload-build.stamp: html.stamp
	rm -fr html-upload
	mkdir html-upload
	-ln -f html/* html-upload/
	rm -fr html-upload/*.html
	for f in html/*.html; do \
		sed ${UPLOAD_REPLACINGS} $$f > html-upload/`basename $$f`; \
	done;
	touch html-upload-build.stamp

html-upload.stamp: html-upload-build.stamp
	@true

CLEANFILES += html-upload-build $(srcdir)/html-upload.stamp
