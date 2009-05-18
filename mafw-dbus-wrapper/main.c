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
 * mafw-dbus-wrapper -- export MAFW plugins on the session bus
 *
 * SYNOPSIS
 *
 * mafw-dbus-wrapper <PLUGIN>
 *
 * DESCRIPTION
 *
 * Loads the given PLUGIN and exports its functionality on the session bus.
 * PLUGIN may be anything that mafw_registry_load_plugin() accepts, ie. a
 * plugin name or an absolute path.  The plugin search directory (in case of
 * plugin names) can be overridden via the $MAFW_PLUGIN_DIR environment
 * variable.  Don't forget that logging can be controlled via $MAFW_LOG.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <glib.h>
#include <libmafw/mafw.h>
#include "wrapper.h"

int main(int argc, char *argv[])
{
	GError *err = NULL;
	MafwRegistry *registry;
	gint i;

	if (argc < 2) {
		g_print("use: mafw-dbus-wrapper <PLUGIN>\n");
		return 1;
	}
	g_type_init();
	mafw_log_init(NULL);
	registry = mafw_registry_get_instance();
	wrapper_init();
	for (i = 1; argv[i]; i++) {
		mafw_registry_load_plugin(registry, argv[i], &err);
		if (err) {
			g_critical("Error loading plugin: %s: %s",
				   argv[i], err->message);
			return 2;
		}
	}
	g_main_loop_run(g_main_loop_new(NULL, FALSE));
	return 0;
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
