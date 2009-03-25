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

#ifndef __WRAPPER_H__
#define __WRAPPER_H__

#include <dbus/dbus.h>
#include <glib.h>

/**
 * ExportedComponent:
 * @comp:        the component that is exported
 * @handler:     the D-Bus message handler function for this component
 * @name:        name of the component
 * @uuid:        uuid of the component
 * @sighandlers: GSignal handlers connected to @comp, disconnected at unexport
 *
 * Represents an exported component.
 */
typedef struct {
	DBusConnection            *connection;
	gpointer                   comp;
	DBusHandleMessageFunction  handler;
	gchar                     *name;
	gchar                     *uuid;
	gchar                     *service_name;
	gchar                     *object_path;
	GArray                    *sighandlers;
} ExportedComponent;

/* wrapper.c */
extern void wrapper_init(void);
extern void connect_signal(ExportedComponent *ecomp, const gchar *signal,
			   gpointer handler);

/* extension-wrapper.c */
extern void connect_to_extension_signals(gpointer ecomp);
extern DBusHandlerResult handle_extension_msg(DBusConnection *conn,
				       DBusMessage *msg,
				       void *data);

/* {source,renderer}-wrapper.c */
extern void connect_to_source_signals(gpointer ecomp);
extern DBusHandlerResult handle_source_msg(DBusConnection *conn,
					   DBusMessage *msg,
					   void *data);

extern void connect_to_renderer_signals(gpointer ecomp);
extern DBusHandlerResult handle_renderer_msg(DBusConnection *conn,
					     DBusMessage *msg,
					     void *data);

#endif
