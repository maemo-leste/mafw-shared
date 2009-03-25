#
# Makefile.am for MAFW shared library.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

ACLOCAL_AMFLAGS			= -I m4
SUBDIRS				= common libmafw-shared mafw-dbus-wrapper \
				  mafw-playlist-daemon
if ENABLE_GTK_DOC
SUBDIRS				+= doc
endif
if ENABLE_TESTS
SUBDIRS				+= tests
endif

pkgconfigdir			= $(libdir)/pkgconfig
pkgconfig_DATA			= mafw-shared.pc
noinst_DATA			= mafw-shared-uninstalled.pc
EXTRA_DIST 			= mafw-shared.pc.in \
				  mafw-shared-uninstalled.pc.in

backupconfdir			= @BACKUP_DIR@/applications/
backupconf_DATA			= mafw-db.conf mafw-playlists.conf
backuppredir			= @BACKUP_DIR@/pre-backup.d/
backuppre_SCRIPTS		= mafw-db.prebackup.sh mafw-playlists.prebackup.sh
backuppostdir			= @BACKUP_DIR@/restore.d/always/
backuppost_SCRIPTS		= mafw-db.restore.sh mafw-playlists.restore.sh

cuddir				= /etc/osso-cud-scripts/
cud_SCRIPTS			= mafw-db-cud.sh mafw-playlists-cud.sh

DISTCHECK_CONFIGURE_FLAGS	= --enable-gtk-doc

MAINTAINERCLEANFILES 		= aclocal.m4 compile config.guess config.sub \
				  configure depcomp install-sh ltmain.sh \
				  Makefile.in missing config.h.in

maintainer-clean-local:
	rm -rf build-aux doc/html-upload

if ENABLE_COVERAGE
LCOV_DATA_DIR  = lcov-data
LCOV_DATA_FILE = lcov.info

distclean-local:
	-rm -rf $(LCOV_DATA_DIR)

lcov-zero-counters:
	$(LCOV) -z -d .

lcov:
	-mkdir -p $(LCOV_DATA_DIR)
	$(LCOV) -c -d . -o $(LCOV_DATA_DIR)/$(LCOV_DATA_FILE)
	genhtml -s $(LCOV_DATA_DIR)/$(LCOV_DATA_FILE) -o $(LCOV_DATA_DIR)
	@echo
	@echo "Please, have a look on ./$(LCOV_DATA_DIR)/index.html for coverage statistics"
	@echo
endif
