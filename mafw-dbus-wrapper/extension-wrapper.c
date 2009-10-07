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

#include <string.h>
#include <dbus/dbus.h>
#include <libmafw/mafw.h>
#include <libmafw/mafw-metadata-serializer.h>
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "wrapper.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-extension-wrapper"

#define MAFW_DBUS_PATH MAFW_OBJECT
#define MAFW_DBUS_INTERFACE MAFW_EXTENSION_INTERFACE

static GHashTable *source_activators;
static DBusConnection *conn;

#define SOURCE_REFDATA_NAME "mafw-refcount"

static void _increase_mafwcount(gpointer object)
{
	gint count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(object),
					SOURCE_REFDATA_NAME));

	count++;
	g_object_set_data(G_OBJECT(object), SOURCE_REFDATA_NAME,
				GINT_TO_POINTER(count));
}

static void _decrease_mafwcount(gpointer object)
{
	gint count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(object),
					SOURCE_REFDATA_NAME));

	if (count)
	{
		count--;
		g_object_set_data(G_OBJECT(object), SOURCE_REFDATA_NAME,
				GINT_TO_POINTER(count));
	
		if (count == 0)
		{
			mafw_extension_set_property_boolean(MAFW_EXTENSION(object),
						MAFW_PROPERTY_EXTENSION_ACTIVATE,
						FALSE);
			
		}
		
	}
	
}

static void _unregister_client(gpointer object, const gchar *name);

#define MATCH_STR "type='signal',interface='org.freedesktop.DBus'," \
			"member='NameOwnerChanged',arg0='%s',arg2=''"

static DBusHandlerResult
_handle_client_exits(DBusConnection *connection,
                                     DBusMessage *msg,
                                     gpointer data)
{
	gchar *name, *oldname, *newname;

	if (!source_activators || g_hash_table_size(source_activators) == 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (!dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS,
				   "NameOwnerChanged"))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	

	name = oldname = newname = NULL;
	mafw_dbus_parse(msg,
			 DBUS_TYPE_STRING, &name,
			 DBUS_TYPE_STRING, &oldname,
			 DBUS_TYPE_STRING, &newname);
	if (*newname && *oldname)
	{
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (*newname) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (*oldname) {
		GList *object_list = g_hash_table_lookup(source_activators,
								name);
		while(object_list)
		{
			gpointer object = object_list->data;
			_unregister_client(object, name);
			object_list = g_hash_table_lookup(source_activators,
								name);
		}
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/**
 * Registers a watch for a client, to get the crash/exit signals
 */
static void _register_watch(const gchar *name)
{
	gchar *match_str = g_strdup_printf(MATCH_STR, name);
	DBusError err;

	dbus_error_init(&err);

	dbus_bus_add_match(conn, match_str, &err);
	if (dbus_error_is_set(&err))
	{
               	g_critical("Unable to add match: %s", match_str);
		dbus_error_free(&err);
	}
	g_free(match_str);
}

/**
 * Deregisters a watch on dbus, for the given name
 */
static void _deregister_watch(const gchar *name)
{
	gchar *matchstr = NULL;
	matchstr = g_strdup_printf(MATCH_STR, name);
	dbus_bus_remove_match(conn, matchstr, NULL);
	g_free(matchstr);
}

/**
 * Registers a client. Registers a watch if needed, and stores the request for
 * the given object if needed.
 */
static gboolean _register_client(gpointer object, const gchar *name)
{
	GList *object_list;

	if (!source_activators)
	{
		source_activators = g_hash_table_new_full(g_str_hash,
						g_str_equal,
						g_free,
						NULL);
	}
	object_list = g_hash_table_lookup(source_activators, name);
	
	if (!object_list)
	{
		_register_watch(name);
	}
	else
	{
		if (g_list_find(object_list, object))
		{// this UI already requested activity
			return FALSE;
		}
	}
	
	object_list = g_list_prepend(object_list, object);
	g_hash_table_insert(source_activators, g_strdup(name), object_list);

	return TRUE;
}

/**
 * Removes an object from the given list. If needed, deregisters the watch for
 * the client. It removes or updates only the new list for the client
 */
static void _remove_object_from_list(GList *object_list,
					gpointer object, const gchar *name)
{
	object_list = g_list_remove(object_list, object);
	if (!object_list)
	{// There isn't any source, what should be active because of this UI
		_deregister_watch(name);
		g_hash_table_remove(source_activators, name);
	}
	else
	{
		g_hash_table_insert(source_activators, g_strdup(name),
					object_list);
	}
}

/**
 * Unregisters a client for the object.
 */ 
static void _unregister_client(gpointer object, const gchar *name)
{
	GList *object_list;

	if (!source_activators || g_hash_table_size(source_activators) == 0)
	{
		return;
	}
	object_list = g_hash_table_lookup(source_activators, name);

	if (!object_list)
	{
		return;
	}
	else
	{
		if (!g_list_find(object_list, object))
		{// this UI never requested activity
			return;
		}
		
		_remove_object_from_list(object_list, object, name);
	}
	_decrease_mafwcount(object);
}

static DBusHandlerResult handle_set_property(DBusConnection *conn,
					     DBusMessage *msg,
					     ExportedComponent *ecomp)
{
	gchar *prop;
	GValue val = { 0 };

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &prop,
			MAFW_DBUS_TYPE_GVALUE, &val);
	
	if (G_VALUE_TYPE(&val) == G_TYPE_BOOLEAN &&
				!strcmp(prop, MAFW_PROPERTY_EXTENSION_ACTIVATE))
	{/* Activate handling */
		if (g_value_get_boolean(&val))
		{/* activating */
			if (mafw_extension_set_property(MAFW_EXTENSION(ecomp->comp), prop, &val))
			{
				const gchar *client_id = dbus_message_get_sender(msg);

				if (_register_client(ecomp->comp, client_id))
				{
					_increase_mafwcount(ecomp->comp);
				}
				
			}
		}
		else
		{/* deactivating */
			const gchar *client_id = dbus_message_get_sender(msg);
			_unregister_client(ecomp->comp, client_id);
		}
	}
	else
	{
		mafw_extension_set_property(MAFW_EXTENSION(ecomp->comp), prop, &val);
	}

	g_value_unset(&val);
	return DBUS_HANDLER_RESULT_HANDLED;
}

struct _oblist_finder_data {
	gpointer extension;
	GPtrArray *ui_ids;
};

/**
 * Collects the UIs, which has requested activity from the given extension
 */
static void _find_ext(const gchar *ui_id, GList *ext_list,
			struct _oblist_finder_data *obl_finder_data)
{
	if (g_list_find(ext_list, obl_finder_data->extension))
	{
		g_ptr_array_add(obl_finder_data->ui_ids, (gpointer)ui_id);
		
	}
}

/**
 * Removes the given extension, reffed by the given UI
 */
static void _ext_remover(const gchar *ui_id, gpointer comp)
{
	GList *ext_list = g_hash_table_lookup(source_activators, ui_id);
	_remove_object_from_list(ext_list, comp, ui_id);
}

/**
 * Called if the extension has removed from the registry
 */
void extension_deregister(gpointer comp)
{
	struct _oblist_finder_data obl_finder_data = {0};
	
	obl_finder_data.extension = comp;
	obl_finder_data.ui_ids = g_ptr_array_new();

	if (source_activators)
	{
		g_hash_table_foreach(source_activators, (GHFunc)_find_ext,
						&obl_finder_data);
		g_ptr_array_foreach(obl_finder_data.ui_ids, (GFunc)_ext_remover,
					comp);
	}
	g_ptr_array_free(obl_finder_data.ui_ids, TRUE);
}

static void get_property_reply(MafwExtension *self, const gchar *prop,
			       GValue *val, MafwDBusOpCompletedInfo *oci,
			       const GError *err)
{
	if (err)
		mafw_dbus_oci_error(oci, g_error_copy(err));
	else {
		mafw_dbus_send(oci->con,
			       mafw_dbus_reply(oci->msg,
					       MAFW_DBUS_STRING(prop),
					       MAFW_DBUS_GVALUE(val)));
		g_value_unset(val);
		g_free(val);
		mafw_dbus_oci_free(oci);
	}
}

static DBusHandlerResult handle_get_property(DBusConnection *conn,
					     DBusMessage *msg,
					     ExportedComponent *ecomp)
{
	gchar *prop;
	MafwDBusOpCompletedInfo *oci;

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &prop);
	oci = mafw_dbus_oci_new(conn, msg);
	mafw_extension_get_property(MAFW_EXTENSION(ecomp->comp), prop,
			       (gpointer)get_property_reply, oci);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_list_properties(DBusConnection *conn,
						DBusMessage *msg,
						ExportedComponent *ecomp)
{
	const GPtrArray *props;
	gchar **names;
	GType *types;
	guint i;

	/* TODO memoize the results? */
	props = mafw_extension_list_properties(MAFW_EXTENSION(ecomp->comp));
	names = g_new0(gchar *, props->len + 1);
	types = g_new(GType, props->len);
	for (i = 0; i < props->len; ++i) {
		names[i] = ((MafwExtensionProperty *)(props->pdata[i]))->name;
		types[i] = ((MafwExtensionProperty *)(props->pdata[i]))->type;
	}
	mafw_dbus_send(conn,
		       mafw_dbus_reply(msg,
				       MAFW_DBUS_STRVZ(names),
				       DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
				       types, props->len));
	g_free(names);
	g_free(types);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_set_name(DBusConnection *conn,
					 DBusMessage *msg,
					 ExportedComponent *ecomp)
{
	gchar *name;

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &name);
	mafw_extension_set_name(MAFW_EXTENSION(ecomp->comp), name);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_get_name(DBusConnection *conn,
					 DBusMessage *msg,
					 ExportedComponent *ecomp)
{
	mafw_dbus_send(
                conn,
                mafw_dbus_reply(
                        msg,
                        MAFW_DBUS_STRING(
                                mafw_extension_get_name(
                                        MAFW_EXTENSION(ecomp->comp)))));
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult handle_extension_msg(DBusConnection *conn,
				       DBusMessage *msg, void *comp)
{
	if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_SET_PROPERTY))
		return handle_set_property(conn, msg,
                                           (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_GET_PROPERTY))
		return handle_get_property(conn, msg,
                                           (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg,
                                     MAFW_EXTENSION_METHOD_LIST_PROPERTIES))
		return handle_list_properties(conn, msg,
                                              (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_SET_NAME))
		return handle_set_name(conn, msg, (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_GET_NAME))
		return handle_get_name(conn, msg, (ExportedComponent *)comp);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void name_changed(MafwExtension *extension, GParamSpec *pspec,
                         ExportedComponent *ecomp)
{
	gchar *name;

	g_object_get(extension, "name", &name, NULL);
	mafw_dbus_send(ecomp->connection,
			mafw_dbus_signal_full(NULL,
				ecomp->object_path,
				MAFW_EXTENSION_INTERFACE,
				MAFW_EXTENSION_SIGNAL_NAME_CHANGED,
				MAFW_DBUS_STRING(name)));
	g_free(name);
}

static void error(MafwExtension *extension,
	   GQuark domain, gint code, const gchar *message,
	   ExportedComponent *ecomp)
{
	const gchar *domain_str;

	domain_str = g_quark_to_string(domain);
	mafw_dbus_send(ecomp->connection,
			mafw_dbus_signal_full(NULL,
				ecomp->object_path,
				MAFW_EXTENSION_INTERFACE,
				MAFW_EXTENSION_SIGNAL_ERROR,
				MAFW_DBUS_STRING(domain_str),
				MAFW_DBUS_INT32(code),
				MAFW_DBUS_STRING(message)));
}

static void prop_changed(MafwExtension *extension,
			 const gchar *prop, const GValue *val,
			 ExportedComponent *ecomp)
{
	mafw_dbus_send(ecomp->connection,
			mafw_dbus_signal_full(NULL,
				ecomp->object_path,
				MAFW_EXTENSION_INTERFACE,
				MAFW_EXTENSION_SIGNAL_PROPERTY_CHANGED,
				MAFW_DBUS_STRING(prop),
				MAFW_DBUS_GVALUE(val)));
}

void connect_to_extension_signals(gpointer ecomp)
{
	connect_signal(ecomp, "notify::name", name_changed);
	connect_signal(ecomp, "error", error);
	connect_signal(ecomp, "property-changed", prop_changed);
}

void extension_init(DBusConnection *connection)
{
	conn = connection;
	if (!dbus_connection_add_filter(connection,
                                        _handle_client_exits,
                                        NULL, NULL))
		g_assert_not_reached();
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
