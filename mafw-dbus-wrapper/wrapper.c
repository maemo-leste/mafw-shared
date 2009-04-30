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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libmafw/mafw.h>
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "libmafw-shared/mafw-proxy-source.h"
#include "libmafw-shared/mafw-proxy-renderer.h"
#include "wrapper.h"

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN         "mafw-dbus-wrapper"
/* mafw-dbus related #defines. */
#define MAFW_DBUS_PATH      MAFW_OBJECT
#define MAFW_DBUS_INTERFACE MAFW_DISCOVERY_INTERFACE

/* Used by the single registry callback to determine what happened. */
enum {
	EXTENSION_ADDED    = (1 << 3),
	EXTENSION_REMOVED  = (1 << 4),
};

/* The list of exported extensions and plugins (ExportedComponent) */
static GList *Exports;
/* The connection to session bus */
static DBusConnection *Session_bus;

/* Connects @handler to the named signal of ecomp->component, and remembers it
 * for disconnecting later.  The handler's user_data argument will be
 * @ecomp. */
void connect_signal(ExportedComponent *ecomp, const gchar *signal,
		    gpointer handler)
{
	gulong id;

	id = g_signal_connect(ecomp->comp, signal, handler, ecomp);
	g_array_append_val(ecomp->sighandlers, id);
}

static void disconnect_sighandlers(ExportedComponent *ecomp)
{
	guint i;

	for (i = 0; i < ecomp->sighandlers->len; ++i)
		g_signal_handler_disconnect(ecomp->comp,
					    g_array_index(ecomp->sighandlers,
							  gulong, i));
	g_array_free(ecomp->sighandlers, TRUE);
	ecomp->sighandlers = NULL;
}

/**
 * wrapper_export:
 * @comp:    the component to export (source or renderer)
 *
 * Exports a component on D-Bus, if it is not already exported, which means:
 * 1. registering an appropriate service name for it
 *    -- in case of sources: MAFW_SOURCE_SERVICE "." plugin "." uuid
 *    -- in case of renderers: MAFW_RENDERER_SERVICE "." plugin "." uuid
 * 2. attaching the appropriate D-Bus message handlers:
 *    -- common one for all #MafwExtensions
 *    -- a specific one for either #MafwSource or #MafwRenderer
 * 3. registering all this information in the Exports list.
 */
static void wrapper_export(gpointer comp)
{
	DBusError err;
	ExportedComponent *ecomp;
	const gchar *plugin, *name, *uuid;
	gchar *service_name;
	gchar *object_path;
	GList *t;
	DBusObjectPathVTable path_vtable;

	/* Export a component only once. */
	for (t = Exports; t; t = t->next)
		if (((ExportedComponent *)t->data)->comp == comp)
			return;

	plugin = mafw_extension_get_plugin(comp);
	name = mafw_extension_get_name(comp);
	uuid = mafw_extension_get_uuid(comp);
	g_assert(plugin);
	g_assert(name);
	g_assert(uuid);

	if (MAFW_IS_SOURCE(comp)) {
		service_name = g_strconcat(MAFW_SOURCE_SERVICE ".",
					   plugin, ".", uuid, NULL);
		object_path = g_strconcat(MAFW_SOURCE_OBJECT "/",
					  uuid, NULL);
	} else if (MAFW_IS_RENDERER(comp)) {
		service_name = g_strconcat(MAFW_RENDERER_SERVICE ".",
					   plugin, ".", uuid, NULL);
		object_path = g_strconcat(MAFW_RENDERER_OBJECT "/",
					  uuid, NULL);
	} else {
		service_name = NULL;
		object_path = NULL;
		g_critical("wrapper_export(): Neither source nor renderer, "
			   "someone is putting strange things "
			   "in your registry.");
		goto err1;
	}

	dbus_error_init(&err);
	switch (dbus_bus_request_name(Session_bus, service_name,
				      DBUS_NAME_FLAG_DO_NOT_QUEUE, &err)) {
	case DBUS_REQUEST_NAME_REPLY_EXISTS:
		g_warning("dbus_bus_request_name(): service already exists: "
			  "%s", service_name);
		goto err1;
	default:
		if (dbus_error_is_set(&err)) {
			g_warning("dbus_bus_request_name() an error was set: "
				  "%s", err.message);
			dbus_error_free(&err);
			goto err1;
		}
	}
	mafw_dbus_send(Session_bus,
			mafw_dbus_signal_full(
					NULL,
					MAFW_REGISTRY_PATH,
					MAFW_REGISTRY_INTERFACE,
					MAFW_REGISTRY_SIGNAL_HELLO,
					MAFW_DBUS_STRING(service_name)));
	ecomp = g_new0(ExportedComponent, 1);
	ecomp->comp = comp;
	ecomp->connection = Session_bus;
	ecomp->name = g_strdup(name);
	ecomp->uuid = g_strdup(uuid);
	ecomp->service_name = service_name;
	ecomp->object_path = object_path;
	ecomp->sighandlers = g_array_sized_new(FALSE, FALSE, sizeof(gulong),
					       13);

	if (MAFW_IS_SOURCE(comp))
		ecomp->handler = handle_source_msg;
	else if (MAFW_IS_RENDERER(comp))
		ecomp->handler = handle_renderer_msg;
	g_assert(ecomp->handler);

	memset(&path_vtable, 0, sizeof(DBusObjectPathVTable));
	path_vtable.message_function =
		(DBusObjectPathMessageFunction)ecomp->handler;

	if (!dbus_connection_register_object_path(Session_bus,
                                                  ecomp->object_path,
                                                  &path_vtable,
                                                  ecomp))
		goto err2;

	Exports = g_list_prepend(Exports, ecomp);

	if (MAFW_IS_SOURCE(comp))
		connect_to_source_signals(ecomp);
	if (MAFW_IS_RENDERER(comp))
		connect_to_renderer_signals(ecomp);
	if (MAFW_IS_EXTENSION(comp))
		connect_to_extension_signals(ecomp);

	return;

err2:   g_free(ecomp->name);
	g_free(ecomp->uuid);
	g_free(ecomp);
err1:   g_free(service_name);
	g_free(object_path);
}

/**
 * wrapper_unexport:
 * @comp: the extension to unexport.
 *
 * De-registers the given component from D-Bus, frees data structures for
 * which it's responsible.
 */
static void wrapper_unexport(gpointer comp)
{
	DBusError err;
	GList *node;
	ExportedComponent *ecomp;

	ecomp = NULL;
	for (node = Exports; node; node = node->next) {
		ecomp = (ExportedComponent *)node->data;
		if (ecomp->comp == comp)
			break;
	}
	if (!ecomp)
		return;

	dbus_connection_unregister_object_path(Session_bus, ecomp->object_path);

	dbus_error_init(&err);
	switch (dbus_bus_release_name(Session_bus, ecomp->service_name, &err)){
	case DBUS_RELEASE_NAME_REPLY_NOT_OWNER:
	case DBUS_RELEASE_NAME_REPLY_NON_EXISTENT:
		g_warning("dbus_bus_release_name() failed, either "
			  "'%s' is non-existent, or we are not the owners",
			  ecomp->service_name);
		break;
	default:
		if (dbus_error_is_set(&err)) {
			g_warning("dbus_bus_release_name() an error was set: "
				  "%s", err.message);
			dbus_error_free(&err);
		}
	}
	disconnect_sighandlers(ecomp);
	g_free(ecomp->service_name);
	g_free(ecomp->object_path);
	g_free(ecomp->name);
	g_free(ecomp->uuid);
	g_free(ecomp);
	Exports = g_list_delete_link(Exports, node);
}

/* Common handler for all {source,renderer}-{added,removed} signals. */
static void registry_action(MafwRegistry *reg, gpointer ext, guint action)
{
	/* Since the same registry serves as home for proxies, we need to
	 * filter their additions/removals. */
	if (MAFW_IS_PROXY_SOURCE(ext) || MAFW_IS_PROXY_RENDERER(ext))
		return;
	if (action & EXTENSION_ADDED) {
		g_object_ref(ext);
		wrapper_export(ext);
	} else if (action & EXTENSION_REMOVED) {
		wrapper_unexport(ext);
		g_object_unref(ext);
	}
}

/**
 * wrapper_init:
 *
 * Acquires connection to the session bus and initializes source and renderer
 * wrappers.
 */
void wrapper_init(void)
{
	DBusError err;

	dbus_error_init(&err);
	Session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		g_error("wrapper_init(): %s",
			dbus_error_is_set(&err) ? err.message : "error");
		dbus_error_free(&err);
		g_assert_not_reached();
	}
	dbus_connection_setup_with_g_main(Session_bus, NULL);

	g_signal_connect(mafw_registry_get_instance(), "source-added",
			 G_CALLBACK(registry_action),
			 GUINT_TO_POINTER(EXTENSION_ADDED));
	g_signal_connect(mafw_registry_get_instance(), "source-removed",
			 G_CALLBACK(registry_action),
			 GUINT_TO_POINTER(EXTENSION_REMOVED));
	g_signal_connect(mafw_registry_get_instance(), "renderer-added",
			 G_CALLBACK(registry_action),
			 GUINT_TO_POINTER(EXTENSION_ADDED));
	g_signal_connect(mafw_registry_get_instance(), "renderer-removed",
			 G_CALLBACK(registry_action),
			 GUINT_TO_POINTER(EXTENSION_REMOVED));
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
