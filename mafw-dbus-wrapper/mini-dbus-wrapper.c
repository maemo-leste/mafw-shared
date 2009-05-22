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
 * mini-dbus-wrapper.c -- minimalistic program to host outp extensions
 *
 * Usage: mini-dbus-wrapper [<file name>|<directory>|<plugin-name>]...
 * -- file name: path to the plugin.so to load
 * -- directory: try to load all files from this directory
 * -- plugin-name: anything else MafwRegistry::load_plugin() understands
 *
 * If the path to the .so file name is known (cases 1&2) the program will
 * relaunch itself if any of the loaded things change.
 */

/* Include files */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/inotify.h>

#include <glib.h>
#include <glib-object.h>

#include "wrapper.h"

#include <libmafw/mafw-log.h>
#include <libmafw/mafw-errors.h>
#include <libmafw/mafw-registry.h>
#include <libmafw-shared/mafw-shared.h>

/* Get inotify; they forgot(?) to define the syscalls in 2.3. */
#if !__GLIBC_PREREQ(2, 4)
# include <sys/syscall.h>
# define inotify_init() \
	syscall(__NR_inotify_init)
# define inotify_add_watch(fd, fname, mask) \
	syscall(__NR_inotify_add_watch, fd, fname, mask)
#endif

/* Program code */
static gboolean load(MafwRegistry *regi, const gchar *plugin)
{
	GError *err;

	err = NULL;
	if (!mafw_registry_load_plugin(MAFW_REGISTRY(regi), plugin, &err)) {
		if (err->code == MAFW_ERROR_PLUGIN_NAME_CONFLICT)
			g_warning("%s: %s", plugin, err->message);
		else
			g_error(  "%s: %s", plugin, err->message);
		return FALSE;
	} else
		return TRUE;
}

static void watch(int ifd, const gchar *fname)
{
	if (ifd < 0)
		/* We may not have inotify support at all. */
		return;
	if (inotify_add_watch(ifd, fname, IN_MODIFY) < 0)
		g_error("inotify_add_watch(%s): %m", fname);
}

static gboolean renaissance(GIOChannel *chnl, GIOCondition cond,
			    const char *const *argv)
{
	g_warning("See you soon");
	execv(argv[0], (char **)argv);
	g_error("exec(%s): %m", argv[0]);
	return FALSE;
}

/* The main function */
int main(int arch, const char *argv[])
{
	int ifd;
	unsigned i;
	MafwRegistry *regi;

	/* Don't log debug messages. */
	mafw_log_init(":warning");

	/* Init wrapping */
	g_type_init();
	regi = MAFW_REGISTRY(mafw_registry_get_instance());
	mafw_shared_init(regi, NULL);
	wrapper_init();

	/* Load the plugins specified on the command line. */
	if ((ifd = inotify_init()) < 0)
		g_warning("inotify_init: %m");
	for (i = 1; argv[i]; i++) {
		DIR *hdir;
		const struct dirent *dent;

		if (strlen(argv[i]) > PATH_MAX)
			continue;
		hdir = opendir(argv[i]);
		if (hdir != NULL) {
			while ((dent = readdir(hdir)) != NULL) {
				gchar *path;

				/* Try to ignore non-shared object files. */
				if (!g_str_has_suffix(dent->d_name, ".so")
				    && !strstr(dent->d_name, ".so."))
					continue;
				path = g_strjoin("/", argv[i], dent->d_name,
						 NULL);
				if (load(regi, path))
					watch(ifd, path);
				g_free(path);
			} /* while */
			closedir(hdir);
			watch(ifd, argv[i]);
		} else if (errno == ENOTDIR) {
			if (load(regi, argv[i]))
				watch(ifd, argv[i]);
		} else if (errno == ENOENT)
			load(regi, argv[i]);
		else
			g_error("%s: %m", argv[i]);
	} /* for */

	/* Watch $ifd. */
	if (ifd >= 0)
		g_io_add_watch(g_io_channel_unix_new(ifd), G_IO_IN,
			       (GIOFunc)renaissance, argv);

	/* The main loop should not return. */
	g_main_loop_run(g_main_loop_new(NULL, FALSE));
	return 1;
}

/* End of mini-dbus-wrapper.c */
