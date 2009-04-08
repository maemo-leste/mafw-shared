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

#ifndef MOCKBUS_H
#define MOCKBUS_H

#include <dbus/dbus.h>

/* Mockbus API */

extern void mockbus_reset(void);
extern void mockbus_expect_conn(const char *address);
extern void mockbus_expect(DBusMessage *msg);
extern void mockbus_reply_msg(DBusMessage *msg);
extern GHashTable *mockbus_mkmeta(gchar const *key, ...);
extern void mockbus_incoming(DBusMessage *msg);
extern gboolean mockbus_deliver(DBusConnection *conn);
extern void mockbus_finish(void);
extern void mockbus_error(GQuark domain, guint code, const gchar *message);
extern void mockbus_send_stored_reply(void);

/*
 * Similar to mafw_dbus_reply().
 */
#define mockbus_reply(...)						\
	mockbus_reply_msg(						\
		mafw_dbus_reply((void*)0x1, ##__VA_ARGS__, DBUS_TYPE_INVALID))
#define FAKE_NAME "TESTName"

void mock_disappearing_extension(const gchar *service, gboolean proxy_side);
void mock_appearing_extension(const gchar *service, gboolean proxy_side);
void mock_services(const gchar *const *active);
void mock_empty_props(const gchar *service, const gchar *object);
#endif
