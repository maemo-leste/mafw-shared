/*
 * This file is a part of MAFW
 *
 * Copyright (C) 2007, 2008, 2009 Nokia Corporation, all rights reserved.
 *
 * Contact: Visa Smolander <visa.smolander@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**
 * SECTION:mafwdbusdiscover
 * @short_description: Discovery of MAFW components over D-Bus
 *
 * These functions are required if you run MAFW plugins out-of-process (using
 * mafw-dbus-wrapper).  The mafw_shared_init() function installs handlers
 * for watching the session bus for appearing components and populates the
 * registry with their proxies, which expose the usual #MafwSource and
 * #MafwRenderer interfaces.  To stop tracking out-of-process components use
 * mafw_shared_deinit(), but note that existing proxies will not be removed.
 */

/* Include files */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libmafw/mafw.h>
#include <libmafw-shared/mafw-shared.h>
#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"
#include "mafw-proxy-renderer.h"
#include "mafw-proxy-source.h"

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-shared"

#define MATCH_STR "type='signal',interface='org.freedesktop.DBus'," \
			"member='NameOwnerChanged',arg0='%s',arg2=''"
/* Private variables */
static DBusConnection *connection = NULL;

/* Program code */

/**
 * Extracts the extension kind, plugin name and uuid from the service name.
 * Empty parts (plugin or uuid) are treated as missing (i.e. %NULL is
 * returned).  Returns %TRUE if @svc was a valid service name.
 */
static gboolean split_servicename(const gchar *svc, GType *kind,
				  gchar **plugin, gchar **uuid)
{
	gchar **parts;
	gboolean valid;

	if (g_str_has_prefix(svc, MAFW_SOURCE_SERVICE)) {
		if (kind)
			*kind = MAFW_TYPE_PROXY_SOURCE;
		parts = g_strsplit(&svc[sizeof(MAFW_SOURCE_SERVICE)], ".", 2);
	} else if (g_str_has_prefix(svc, MAFW_RENDERER_SERVICE)) {
		if (kind)
			*kind = MAFW_TYPE_PROXY_RENDERER;
		parts = g_strsplit(&svc[sizeof(MAFW_RENDERER_SERVICE)], ".", 2);
	} else
		return FALSE;
	if (!parts || !parts[0] || !parts[1])
	{
		g_strfreev(parts);
		return FALSE;
	}
	if (plugin)
		*plugin = parts[0][0] ? g_strdup(parts[0]) : NULL;
	if (uuid)
		*uuid = parts[1][0] ? g_strdup(parts[1]) : NULL;
	valid = parts[0][0] && parts[1][0];
	g_strfreev(parts);
	return valid;
}

/**
 * Interprets @svc, and if it represents a MAFW component, tries to create
 * either a renderer or source proxy, and adds it to @registry.
 */
static void create_proxy(MafwRegistry *registry,
			 const gchar *svc)
{
	GType pxtype;
	gchar *plugin, *uuid;


	/* Extensions are exported using the name:
	 *   com.nokia.mafw.{renderer,source}.<plugin>.<uuid> */
	plugin = uuid = NULL;
	if (!split_servicename(svc, &pxtype, &plugin, &uuid))
		/* It is possible that $svc is not a mafw-thing after all,
		 * because we just get whatever NameOwnerChanged. */
		goto out;

	if (!mafw_registry_get_extension_by_uuid(MAFW_REGISTRY(registry),
						  uuid))
	{
		DBusError err;
		gchar *matchstr;
		dbus_error_init(&err);
		if (pxtype == MAFW_TYPE_PROXY_SOURCE)
			mafw_proxy_source_new(uuid,
					      plugin,
					      registry);
		else if (pxtype == MAFW_TYPE_PROXY_RENDERER)
			mafw_proxy_renderer_new(uuid,
						plugin,
						registry);

		/* Do not add the created SiSo-s to the registry.... It will be
		   added automatically, soon after it collected all the needed
		   informations about the wrapped object */
		g_debug("proxy added for '%s'", svc);
		matchstr = g_strdup_printf(MATCH_STR, svc);
		dbus_bus_add_match(connection, matchstr, &err);

		if (dbus_error_is_set(&err))
		{
                	g_critical("Unable to add match: %s", matchstr);
			dbus_error_free(&err);
		}
		g_free(matchstr);
	}
out:	if (plugin)
		g_free(plugin);
	if (uuid)
		g_free(uuid);
}

/* Listens to NameOwnerChanged messages and creates / removes proxies
 * accordingly. */
static DBusHandlerResult handle_message(DBusConnection *conn,
					DBusMessage *msg,
					gpointer registry)
{
	if (dbus_message_is_signal(msg, MAFW_REGISTRY_INTERFACE,
				   MAFW_REGISTRY_SIGNAL_HELLO))
	{
		gchar *name;
		mafw_dbus_parse(msg,
				 DBUS_TYPE_STRING, &name);
		create_proxy(registry, name);
	} else if (dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS,
				   "NameOwnerChanged"))
	{
		gchar *name, *oldname, *newname;

		name = oldname = newname = NULL;
		mafw_dbus_parse(msg,
				 DBUS_TYPE_STRING, &name,
				 DBUS_TYPE_STRING, &oldname,
				 DBUS_TYPE_STRING, &newname);

		/* If both old- and newname are valid, then the underlying
		 * unique names have changed.  Since we access extensions
		 * through their service name, we don't care. */
		if (*newname && *oldname)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (*newname) {
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		} else if (*oldname) {
			gchar *uuid;
			gpointer extension;
			gchar *matchstr;

			uuid = NULL;
			if (!split_servicename(name, NULL, NULL, &uuid))
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			extension = mafw_registry_get_extension_by_uuid(
				MAFW_REGISTRY(registry),
				uuid);
			g_free(uuid);
			if (extension)
			{
				mafw_registry_remove_extension(
					MAFW_REGISTRY(registry),
					extension);
			}
			matchstr = g_strdup_printf(MATCH_STR, name);
			dbus_bus_remove_match(connection, matchstr, NULL);
			g_free(matchstr);
		}
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/**
 * Creates proxies for existing components.
 */
static void create_proxy_extensions(MafwRegistry *registry)
{
	DBusMessage *ret;
	gchar **found_extensions, **ext;

	ret = mafw_dbus_call(connection,
			      mafw_dbus_method_full(
				      DBUS_SERVICE_DBUS,
				      DBUS_PATH_DBUS,
				      DBUS_INTERFACE_DBUS,
				      "ListNames"),
			      0, NULL);

	mafw_dbus_parse(ret, MAFW_DBUS_TYPE_STRVZ, &found_extensions);
	dbus_message_unref(ret);
	for (ext = found_extensions; *ext; ext++)
		create_proxy(registry, *ext);
	g_strfreev(found_extensions);
}

/* Public API */

/**
 * mafw_shared_init:
 * @reg:   registry instance used to store proxy sources and renderers.  In 99
 *         percent of the cases it will be mafw_registry_get_instance().
 * @error: location of a possible #GError
 *
 * Tracks renderers and sources exported in session bus and adds/removes them
 * from the provided registry when they show up/get removed from session bus.
 *
 * Returns: %TRUE if all went OK, %FALSE otherwise.
 */
gboolean mafw_shared_init(MafwRegistry *reg, GError **error)
{
	DBusError err;

	if (connection)
		return TRUE;

        dbus_error_init(&err);
        /* Get a reference to the session bus and add a filter. */
        connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err))
		goto error;

	/* A match is required in order to receive session bus signals. */
	dbus_bus_add_match(connection, "type='signal',"
			   "interface='" MAFW_REGISTRY_INTERFACE "'", &err);
        if (dbus_error_is_set(&err))
                goto error_unref_conn;

        if (!dbus_connection_add_filter(connection,
                                        handle_message,
                                        reg, NULL))
                goto error_unref_conn;

        dbus_connection_setup_with_g_main(connection, NULL);
        create_proxy_extensions(reg);
        return TRUE;

error_unref_conn:
        dbus_connection_unref(connection);
        connection = NULL;
error:
	g_set_error(error, MAFW_ERROR, /*TODO: fix*/0,
		    "Discovery initialization failed: %s",
		    dbus_error_is_set(&err) ? err.message : "error");
        dbus_error_free(&err);

        return FALSE;
}

/**
 * mafw_shared_deinit:
 *
 * Stops tracking of sources and renderers exported on the session bus.
 * Existing proxies will not be removed from the registry.
 */
void mafw_shared_deinit(void)
{
	if (!connection)
		return;

	dbus_connection_remove_filter(connection, handle_message, NULL);
        dbus_connection_unref(connection);
        connection = NULL;
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
