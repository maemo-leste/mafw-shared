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

#include <string.h>
#include <dbus/dbus.h>
#include <libmafw/mafw.h>
#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"
#include "mafw-proxy-extension.h"
#include "mafw-proxy-source.h"
#include "mafw-proxy-renderer.h"

#define MAFW_DBUS_INTERFACE MAFW_EXTENSION_INTERFACE

struct _extension_attach_data {
	GObject *extension;
	MafwRegistry *registry;
};

static DBusConnection *conn;

static gchar *get_extension_path(MafwExtension *self)
{
	gchar *path = NULL;

	if (MAFW_IS_PROXY_RENDERER(self))
	{
		path = g_strconcat(
                        MAFW_RENDERER_OBJECT "/",
                        mafw_extension_get_uuid(MAFW_EXTENSION(self)),
                        NULL);
	}
	else if (MAFW_IS_PROXY_SOURCE(self))
	{
		path = g_strconcat(
                        MAFW_SOURCE_OBJECT "/",
                        mafw_extension_get_uuid(MAFW_EXTENSION(self)),
                        NULL);
	}

	return path;
}

static gchar *get_extension_service(MafwExtension *self, const gchar *plugin)
{
	gchar *service = NULL;

	if (MAFW_IS_PROXY_RENDERER(self))
	{
		service = g_strconcat(
                        MAFW_RENDERER_SERVICE ".", plugin, ".",
                        mafw_extension_get_uuid(MAFW_EXTENSION(self)), NULL);
	}
	else if (MAFW_IS_PROXY_SOURCE(self))
	{
		service = g_strconcat(
                        MAFW_SOURCE_SERVICE ".", plugin, ".",
                        mafw_extension_get_uuid(MAFW_EXTENSION(self)), NULL);
	}

	return service;
}
static void add_properties_to_extension(DBusMessage *msg, MafwExtension *self)
{
	gchar **names;
	GType *types;
	guint nlen, tlen, i;
	int dbus_type = sizeof(GType) == sizeof(guint) ?
				DBUS_TYPE_UINT32 :
				DBUS_TYPE_UINT64;

	mafw_dbus_parse(msg, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&names, &nlen,
			DBUS_TYPE_ARRAY, dbus_type,
			&types, &tlen);
	g_assert(nlen == tlen);
	for (i = 0; i < nlen; ++i)
		mafw_extension_add_property(self, names[i], types[i]);
	g_strfreev(names);
	g_object_set_data(G_OBJECT(self), "got_props", GINT_TO_POINTER(TRUE));
}

const GPtrArray *proxy_extension_list_properties(MafwExtension *self)
{
	DBusMessage *r;
	gboolean gotp;
	GError *error = NULL;

	g_assert(conn);
	gotp = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(self), "got_props"));
	if (gotp) goto done;

	r = mafw_dbus_call(conn,
			   mafw_dbus_method_full(
				proxy_extension_return_service(self),
				proxy_extension_return_path(self),
				MAFW_EXTENSION_INTERFACE,
				MAFW_EXTENSION_METHOD_LIST_PROPERTIES),
			   MAFW_EXTENSION_ERROR, &error);
	if (!r)
	{
		g_critical("Unable to get the property-list: %s",
                           error->message);
		g_error_free(error);
		return NULL;
	}

	add_properties_to_extension(r, self);
	dbus_message_unref(r);
done:
	return MAFW_EXTENSION_CLASS(
		g_type_class_peek_parent(
			MAFW_EXTENSION_GET_CLASS(self)))->list_extension_properties(self);
}

void proxy_extension_set_extension_property(MafwExtension *self,
                                            const gchar *name,
                                            const GValue *value)
{
	g_assert(conn);
	mafw_dbus_send(conn,
		       mafw_dbus_method_full(
					proxy_extension_return_service(self),
					proxy_extension_return_path(self),
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_SET_PROPERTY,
					MAFW_DBUS_STRING(name),
					MAFW_DBUS_GVALUE(value)));
}

typedef struct {
	MafwExtension *extension;
	MafwExtensionPropertyCallback cb;
	gpointer data;
} GetPropInfo;

static void got_extension_property(DBusPendingCall *pending, void *udata)
{
	DBusMessage *msg;
	gchar *prop = NULL;
	GValue *val = NULL;
	GetPropInfo *info = (GetPropInfo *)udata;
	GError *err;

	msg = dbus_pending_call_steal_reply(pending);
	if (!(err = mafw_dbus_is_error(msg, MAFW_EXTENSION_ERROR))) {
		val = g_new0(GValue, 1);
		mafw_dbus_parse(msg, DBUS_TYPE_STRING, &prop,
				MAFW_DBUS_TYPE_GVALUE, val);
	}
	info->cb(info->extension, prop, val, info->data, err);
	if (err) g_error_free(err);
	dbus_message_unref(msg);
	dbus_pending_call_unref(pending);
}

void proxy_extension_get_extension_property(MafwExtension *self,
                                            const gchar *name,
                                            MafwExtensionPropertyCallback cb,
                                            gpointer udata)
{
	DBusPendingCall *pending;
	GetPropInfo *info;

	g_assert(conn);
	mafw_dbus_send_async(
                conn, &pending,
                mafw_dbus_method_full(
                        proxy_extension_return_service(self),
                        proxy_extension_return_path(self),
                        MAFW_EXTENSION_INTERFACE,
                        MAFW_EXTENSION_METHOD_GET_PROPERTY,
                        MAFW_DBUS_STRING(name)));
	info = g_new(GetPropInfo, 1);
	/* XXX should we ref @self? */
	info->extension = self;
	info->cb = cb;
	info->data = udata;
	dbus_pending_call_set_notify(pending, got_extension_property,
				     info, g_free);
}

/* GObject::notify handler for proxies.  Sends a D-Bus message when
 * the extension's name changes. */
static void extension_name_set(GObject *o, GParamSpec *pspec,
                               DBusConnection *conn)
{
	gchar *name;

	g_object_get(o, "name", &name, NULL);
	mafw_dbus_send(conn,
		       mafw_dbus_method_full(
					proxy_extension_return_service(o),
					proxy_extension_return_path(o),
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_SET_NAME,
					MAFW_DBUS_STRING(name)));
	g_free(name);
}

static DBusHandlerResult handle_extension_prop_changed(DBusConnection *conn,
						  DBusMessage *msg,
						  MafwExtension *self)
{
	gchar *prop;
	GValue val = { 0 };

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &prop,
			MAFW_DBUS_TYPE_GVALUE, &val);
	mafw_extension_emit_property_changed(self, prop, &val);
	g_value_unset(&val);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult handle_extension_name_changed(DBusConnection *conn,
						  DBusMessage *msg,
						  MafwExtension *extension)
{
	gchar *name;

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &name);
	g_signal_handlers_block_by_func (extension,
                                         extension_name_set,
                                         conn);
	mafw_extension_set_name(extension, name);
	g_signal_handlers_unblock_by_func (extension,
                                         extension_name_set,
                                         conn);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult handle_extension_error(DBusConnection *conn,
					   DBusMessage *msg,
					   MafwExtension *extension)
{
	gchar *domain, *message;
	gint code;

	mafw_dbus_parse(msg, DBUS_TYPE_STRING, &domain,
			DBUS_TYPE_INT32, &code,
			DBUS_TYPE_STRING, &message);
	g_signal_emit_by_name(extension, "error",
			      g_quark_from_string(domain),
			      code, message);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*
 * Filters and handles messages common to extensions.
 */
DBusHandlerResult proxy_extension_dispatch(DBusConnection *conn,
					     DBusMessage *msg,
					     gpointer extension)
{
	if (mafw_dbus_is_signal(msg, MAFW_EXTENSION_SIGNAL_PROPERTY_CHANGED))
		return handle_extension_prop_changed(conn, msg,
                                                     MAFW_EXTENSION(extension));
	else if (mafw_dbus_is_signal(msg, MAFW_EXTENSION_SIGNAL_NAME_CHANGED))
		return handle_extension_name_changed(conn, msg,
                                                     MAFW_EXTENSION(extension));
	else if (mafw_dbus_is_signal(msg, MAFW_EXTENSION_SIGNAL_ERROR)) {
		return handle_extension_error(conn, msg,
                                              MAFW_EXTENSION(extension));
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void proxy_extension_detach(gpointer data, GObject *extension)
{
	g_free(proxy_extension_return_path(extension));
	g_free(proxy_extension_return_service(extension));
}

static void got_prop_lists(DBusPendingCall *pendelum,
                           struct _extension_attach_data *att_data)
{
	DBusMessage *reply;

	reply = dbus_pending_call_steal_reply(pendelum);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
	{
		add_properties_to_extension(
                        reply,
                        MAFW_EXTENSION(att_data->extension));
	}
	else
	{
		DBusError dbuserr;

		dbus_error_init(&dbuserr);

		if (dbus_set_error_from_message(&dbuserr, reply))
		{
			g_critical(
                                "Received error message for list_properties: %s",
                                dbuserr.message);
		}
		else
		{
			g_critical(
                                "Unable to get the properties of the extension");
		}
		dbus_error_free(&dbuserr);
	}
	dbus_message_unref(reply);
	if (!mafw_registry_get_extension_by_uuid(
                    MAFW_REGISTRY(att_data->registry),
                    mafw_extension_get_uuid(
                            MAFW_EXTENSION(att_data->extension))))
		mafw_registry_add_extension(
                        att_data->registry,
                        MAFW_EXTENSION(att_data->extension));
	else
		g_object_unref(att_data->extension);
	g_free(att_data);
	dbus_pending_call_unref(pendelum);
}

static void got_name(DBusPendingCall *pendelum,
                     struct _extension_attach_data *att_data)
{
	DBusMessage *reply;
	DBusPendingCall *pending_list_prop;
	gchar *name = NULL;

	reply = dbus_pending_call_steal_reply(pendelum);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
	{
		mafw_dbus_parse(reply, DBUS_TYPE_STRING, &name);
		g_object_set(att_data->extension, "name", name, NULL);
	}
	else
	{
		DBusError dbuserr;

		dbus_error_init(&dbuserr);

		if (dbus_set_error_from_message(&dbuserr, reply))
		{
			g_critical("Received error message for get_name: %s",
					dbuserr.message);
		}
		else
		{
			g_critical("Unable to get the name of the extension");
		}
		dbus_error_free(&dbuserr);
	}
	dbus_message_unref(reply);
	g_signal_connect(att_data->extension, "notify::name",
                         G_CALLBACK(extension_name_set),
			 conn);
	/* XXX we do an early list_properties because mafw_extension_*
	 * functions check registered properties and fail since the
	 * proxy doesn't have any properties on its own until
	 * list_properties has been called. */
	mafw_dbus_send_async(
                conn,
                &pending_list_prop,
                mafw_dbus_method_full(
                        proxy_extension_return_service(att_data->extension),
                        proxy_extension_return_path(att_data->extension),
                        MAFW_EXTENSION_INTERFACE,
                        MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	dbus_pending_call_set_notify(pending_list_prop,
				     (gpointer)got_prop_lists,
				     (gpointer)att_data, NULL);
	dbus_pending_call_unref(pendelum);
}

void proxy_extension_attach(GObject *extension, DBusConnection *connection,
			const gchar *plugin, MafwRegistry *registry)
{
	gchar *path, *service, *match_str;
	DBusPendingCall *pending_name;
	struct _extension_attach_data *att_data =
                g_new0(struct _extension_attach_data, 1);

	att_data->extension = extension;
	att_data->registry = registry;

	path = get_extension_path(MAFW_EXTENSION(extension));
	service = get_extension_service(MAFW_EXTENSION(extension), plugin);

	g_object_set_qdata(G_OBJECT(extension), PATH_NAME, path);
	g_object_set_qdata(G_OBJECT(extension), SERVICE_NAME, service);

	conn = connection;

	match_str = g_strdup_printf(MAFW_EXTENSION_MATCH,
                                    MAFW_EXTENSION_INTERFACE,
					path);
	dbus_bus_add_match(connection, match_str, NULL);
	g_free(match_str);

	mafw_dbus_send_async(conn,
		       &pending_name,
		       mafw_dbus_method_full(
					service,
					path,
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_GET_NAME));
	dbus_pending_call_set_notify(pending_name,
				     (gpointer)got_name,
				     (gpointer)att_data, NULL);

	g_object_weak_ref(extension, proxy_extension_detach, NULL);
}
