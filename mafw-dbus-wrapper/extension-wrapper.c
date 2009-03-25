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

static DBusHandlerResult handle_set_property(DBusConnection *conn,
					     DBusMessage *msg,
					     ExportedComponent *ecomp)
{
	gchar *prop;
	GValue val = { 0 };

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &prop,
			MAFW_DBUS_TYPE_GVALUE, &val);
	mafw_extension_set_property(MAFW_EXTENSION(ecomp->comp), prop, &val);
	g_value_unset(&val);
	return DBUS_HANDLER_RESULT_HANDLED;
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
	mafw_dbus_send(conn,
		       mafw_dbus_reply(msg,
				       MAFW_DBUS_STRING(mafw_extension_get_name(
						       MAFW_EXTENSION(ecomp->comp)))));
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult handle_extension_msg(DBusConnection *conn,
				       DBusMessage *msg, void *comp)
{
	if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_SET_PROPERTY))
		return handle_set_property(conn, msg, (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_GET_PROPERTY))
		return handle_get_property(conn, msg, (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_LIST_PROPERTIES))
		return handle_list_properties(conn, msg, (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_SET_NAME))
		return handle_set_name(conn, msg, (ExportedComponent *)comp);
	else if (mafw_dbus_is_method(msg, MAFW_EXTENSION_METHOD_GET_NAME))
		return handle_get_name(conn, msg, (ExportedComponent *)comp);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void name_changed(MafwExtension *extension, GParamSpec *pspec, ExportedComponent *ecomp)
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
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
