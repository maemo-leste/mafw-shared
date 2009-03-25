#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

TESTS 				= test-util \
				  test-dbus \
				  test-pld \
				  test-aplaylist \
				  test-playlist-manager-msg \
				  test-playlist-manager \
				  test-proxy-playlist \
				  test-proxy-playlist-msg \
				  test-proxy-extension \
				  test-proxy-source \
				  test-proxy-renderer \
				  test-renderer-wrapper \
				  test-dbus-discover \
				  test-source-wrapper \
				  test-plmanager-import
#				  test-together

check_PROGRAMS			= $(TESTS)
noinst_PROGRAMS			= $(TESTS)

AM_CFLAGS			= $(_CFLAGS)
AM_CPPFLAGS 			= $(GOBJECT_CFLAGS) \
				  $(DBUS_CFLAGS) \
				  $(MAFW_CFLAGS) \
				  $(CHECKMORE_CFLAGS) \
				  -I$(top_srcdir) \
				  -I$(top_srcdir)/common
AM_LDFLAGS			= $(_LDFLAGS)
# Beware to add libcommon.la at the end, because you'll
# get undefined references when linking.
LDADD 				= $(CHECKMORE_LIBS) \
				  $(GOBJECT_LIBS) \
				  $(DBUS_LIBS) \
				  $(MAFW_LIBS) \
				  $(top_builddir)/common/libcommon.la

test_util_SOURCES		= test-util.c
test_plmanager_import_CFLAGS	= $(CFLAGS) $(TOTEMPL_CFLAGS)
test_plmanager_import_SOURCES	= test-plmngr-import.c \
				  mockbus.c mockbus.h 
test_plmanager_import_LDADD	= $(top_builddir)/mafw-playlist-daemon/libmafw-playlist-daemon.a \
				  $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD) $(TOTEMPL_LIBS)
test_dbus_SOURCES		= test-dbus.c
test_pld_SOURCES		= test-pld.c
test_pld_LDADD			= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)
test_aplaylist_SOURCES		= test-aplaylist.c
test_aplaylist_LDADD		= $(top_builddir)/mafw-playlist-daemon/aplaylist.o \
				  $(MAFW_LIBS) $(CHECKMORE_LIBS)

test_proxy_playlist_msg_SOURCES	= mockbus.c mockbus.h test-proxy-playlist-msg.c
test_proxy_playlist_msg_LDADD	= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_proxy_playlist_SOURCES 	= test-proxy-playlist.c
test_proxy_playlist_LDADD 	= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)
test_playlist_manager_msg_SOURCES = mockbus.c mockbus.h \
				    test-playlist-manager-msg.c
test_playlist_manager_msg_LDADD	= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_playlist_manager_SOURCES	= test-playlist-manager.c
test_playlist_manager_LDADD 	= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_proxy_extension_SOURCES 	= mockbus.c mockbus.h test-proxy-extension.c
test_proxy_extension_LDADD 		= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_proxy_source_SOURCES 	= mockbus.c mockbus.h test-proxy-source.c
test_proxy_source_LDADD 	= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_proxy_renderer_SOURCES		= mockbus.c mockbus.h test-proxy-renderer.c
test_proxy_renderer_LDADD 		= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_renderer_wrapper_SOURCES	= mockbus.c mockbus.h mockrenderer.c mockrenderer.h \
				  errorrenderer.c errorrenderer.h test-renderer-wrapper.c
test_renderer_wrapper_LDADD 	= $(top_builddir)/mafw-dbus-wrapper/libmafw-dbus-wrapper.a \
				  $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_dbus_discover_SOURCES	= mockbus.c mockbus.h test-dbus-discover.c
test_dbus_discover_LDADD	= $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

test_source_wrapper_SOURCES 	= mockbus.c mockbus.h \
				  mocksource.c mocksource.h \
				  errorsource.c errorsource.h \
				  test-source-wrapper.c
test_source_wrapper_LDADD 	= $(top_builddir)/mafw-dbus-wrapper/libmafw-dbus-wrapper.a \
				  $(top_builddir)/libmafw-shared/libmafw-shared.la \
				  $(LDADD)

#test_together_SOURCES = test-together.c
#test_together_LDADD = \
#	$(top_builddir)/mafw-dbus-wrapper/libmafw-dbus-wrapper.a \
#	$(top_builddir)/libmafw-shared/libmafw-shared.la \
#	$(LDADD)

CLEANFILES 			= $(BUILT_SOURCES) $(TESTS) *.db *.gcda \
				  *.gcno vglog.*
DISTCLEANFILES			= $(BUILT_SOURCES) $(TESTS) tale.mp p1.mp
MAINTAINERCLEANFILES		= Makefile.in $(BUILT_SOURCES) $(TESTS)

clean-local:
	rm -fr testpld testproxyplaylist testplaylistmanager

# Run valgrind on tests.
VG_OPTS				:= --leak-check=full --show-reachable=yes --suppressions=test.suppressions
vg: $(PROGRAMS)
	for p in $^; do \
		G_DEBUG='always-malloc' \
		libtool --mode=execute valgrind $(VG_OPTS) $$p 2>vglog.$$p; \
	done;
