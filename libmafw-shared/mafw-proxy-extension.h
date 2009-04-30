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

#ifndef __EXTENSION_PROXY_H__
#define __EXTENSION_PROXY_H__

#include <glib-object.h>
#include <dbus/dbus.h>
#include <libmafw/mafw-extension.h>

#define MAFW_EXTENSION_MATCH			\
	"type='signal',"				\
	"interface='%s', " \
	"path='%s'"

#define PATH_NAME g_quark_from_static_string("extension_path")
#define SERVICE_NAME g_quark_from_static_string("extension_service")

#define proxy_extension_return_path(extension) \
        g_object_get_qdata(G_OBJECT(extension), PATH_NAME)
#define proxy_extension_return_service(extension) \
        g_object_get_qdata(G_OBJECT(extension), SERVICE_NAME)

extern const GPtrArray *proxy_extension_list_properties(MafwExtension *self);
extern void proxy_extension_set_extension_property(MafwExtension *self,
                                                   const gchar *name,
					 const GValue *value);
extern void proxy_extension_get_extension_property(MafwExtension *self,
                                                   const gchar *name,
					 MafwExtensionPropertyCallback cb,
					 gpointer udata);
extern void proxy_extension_attach(GObject *extension, DBusConnection *conn,
				const gchar *plugin, MafwRegistry *registry);

DBusHandlerResult proxy_extension_dispatch(DBusConnection *conn,
					     DBusMessage *msg,
					     gpointer extension);
#endif
