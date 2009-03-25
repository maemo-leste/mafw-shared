/*
 * This file is a part of MAFW
 *
 * Copyright (C) 2007, 2008, 2009 Nokia Corporation, all rights reserved.
 *
 * Contact: Visa Smolander <visa.smolander@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

/*
 * MPD keeps playlists used by MAFW in core (see aplaylist.c).
 *
 * A main goal is to minimize user data loss.
 *
 * Persistence is reached by saving each playlist into a file:
 * -- after playlist editing operations have settled, i.e. none happened in the
 *    last N seconds.
 * -- on exit, all playlists are saved unconditionally.
 * Saving a playlist is atomic, by saving first to a temporary file, then
 * renaming it.
 *
 * On startup, saved playlist are loaded.  If a .tmp file exists, we assume that
 * the rename on saving failed, and do it now.
 *
 * SIGINT and SIGTERM cause termination of the main loop and then falling
 * throught the normal exit procedure.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include <glib/gstdio.h>
#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libmafw/mafw-log.h>

#include "common/dbus-interface.h"
#include "mpd-internal.h"

/* Standard definitions */
#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN		"mafw-playlist-daemon"

/* Program code */
/* We got a signal, exit as soon as we can. */
static void sigh(int unused)
{
	/* It may be inappropriate to exit() now if we're under fakeroot,
	 * because it may be doing something right at the moment.  Let's hope
	 * that this call is safe enough to call from the signal handler. */
	g_idle_add((void *)g_main_loop_quit, Loop);
}

/* The main function */
int main(int argc, char *argv[])
{
	int optchar;
	DBusError dbe;
	DBusConnection *dbus;
	gboolean opt_daemonize, opt_kill, opt_stayalive;

	/* Parse the command line. */
	opt_daemonize = opt_kill = FALSE;
	opt_stayalive = TRUE;
	while ((optchar = getopt(argc, argv, "dfk")) != EOF)
		switch (optchar) {
		case 'd':
			/* Go to the background. */
			opt_daemonize = TRUE;
			opt_stayalive = TRUE;
			break;
		case 'f':
			/* Stay in the foreground. */
			opt_daemonize = FALSE;
			opt_stayalive = TRUE;
			break;
		case 'k':
			/* Kill the currently running daemon. */
			opt_kill      = TRUE;
			opt_stayalive = FALSE;
			break;
		default:
			printf("usage: %s [-dkf]\n", argv[0]);
			exit(1);
		}

	/* Don't log debug messages. */
	mafw_log_init(opt_daemonize ? ":warning" : ":info");

	/* Hook on D-BUS. */
	dbus_error_init(&dbe);
	dbus = dbus_bus_get(DBUS_BUS_SESSION, &dbe);
	if (dbus_error_is_set(&dbe))
		g_error("dbus_bus_get: %s", dbe.name);

	init_playlist_wrapper(dbus, opt_stayalive, opt_kill);

	if (opt_daemonize && daemon(1, 0) < 0)
		g_error("daemon(): %m");

	/* Stop the loop on SIGTERM and SIGINT. */
	signal(SIGTERM, sigh);
	signal(SIGINT, sigh);

	dbus_connection_setup_with_g_main(dbus, NULL);
	dbus_connection_unref(dbus);
	
	g_main_loop_run(Loop = g_main_loop_new(NULL, FALSE));
	save_all_playlists();
	return 0;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
