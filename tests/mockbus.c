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

/*
 * MOCKBUS - Module for limited testing of D-Bus components without a
 * peer.
 *
 *
 * Works by overriding functions from libdbus and checking assertions
 * in them.  The entire message is checked against the expectations.
 * See the mafw_dbus_*() functions for easier message construction
 * (and don't forget to #define MAFW_DBUS_{PATH,INTERFACE}).
 *
 * Call mockbus_reset() before each test case, this clears previous
 * assumptions.  Also, mockbus_finish() should be called at the end of
 * each testcase, to check that all expectations were met.
 *
 * Most functions put the passed message into a queue and they will be
 * dequeued at appropriate times, e.g. you can expect more messages to
 * be sent or you can arrange more incoming messages.
 *
 * Sending messages:
 * 1. Call mockbus_expect(msg) with msg being the message you expect
 *    to be sent.  It may be called several times, that way it queues
 *    up the messages for checking more dbus_connection_send()s.
 * 2. Call user code.
 *
 * Getting replies:
 * 1. mockbus_reply(msg) will queue up the replies to the upcoming
 *    D-Bus method calls.
 * 2. Each dbus_connection_send_with_reply_and_block() will return
 *    the next reply in the queue.  So does dbus_connection_send()
 *    do, only asynchronously.
 *
 * Receiving messages:
 * 1. Use mockbus_incoming(msg) to queue up incoming messages.
 * 2. User code probably calls dbus_connection_add_filter() to set up
 *    a handler, also sets up the connection with the GLib mainloop
 *    and spins it
 * 3. The handler specified in step 2 will be called with the
 *    messages given in step 1.
 *
 * Connecting:
 * 1. Call mockbus_expect_conn(address) to set the address that is
 *    expected to be connected to.
 * 2. dbus_connection_open{_private}() asserts.
 *
 * Example (from the beginning of a test-case):
 *
 * // `tabula rasa'
 * mockbus_reset();
 * // we expect that it will connect to SOURCE_ADDR...
 * mockbus_expect_conn(SOURCE_ADDR);
 * // ...and also sends a method call ...
 * mockbus_expect(
 *	mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
 *			 MAFW_DBUS_STRING("testobject"),
 *			 MAFW_DBUS_BOOLEAN(FALSE),
 *			 MAFW_DBUS_STRING("!(rating=sucks)"),
 *			 MAFW_DBUS_STRING("-year"),
 *			 MAFW_DBUS_C_STRVZ("title", "artist"),
 *			 MAFW_DBUS_UINT32(0),
 *			 MAFW_DBUS_UINT32(11)));
 * // ... to which we reply with this.
 * mockbus_reply(MAFW_DBUS_UINT32(4444));
 * // we also expect an incoming signal
 * mockbus_incoming(
 * 	mafw_dbus_signal(MAFW_SOURCE_SIGNAL_BROWSE_RESULT,
 *                       MAFW_DBUS_UINT32(4444),
 *                       MAFW_DBUS_INT32(-1),
 *                       MAFW_DBUS_UINT32(0),
 *                       MAFW_DBUS_STRING("testobject"),
 *                       MAFW_DBUS_C_STRVZ("title", "Easy"),
 *                       MAFW_DBUS_UINT32(2)));
 *
 * XXX object paths are not supported and I don't even like the idea
 * very much...  YAGNI.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <check.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <inttypes.h>

#include <libmafw/mafw-metadata.h>
#include <libmafw/mafw-metadata-serializer.h>

#include "mockbus.h"
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"

#if 0
#include <assert.h>
#undef ck_assert
#define ck_assert(x,...) assert((x))
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mockbus"

/* These were only introduced in GLib 2.14 */
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 14
#define G_QUEUE_INIT { NULL, NULL, 0 }
static void g_queue_clear(GQueue *q)
{
	g_list_free(q->head);
	q->head = q->tail = NULL;
	q->length = 0;
}
#endif

#define MATCH_STR "type='signal',interface='org.freedesktop.DBus'," \
			"member='NameOwnerChanged',arg0='%s',arg2=''"

/* Private variables. */
static GHashTable *object_path_hash;
static GList *fallback_list;

typedef struct _ObjectPathData {
	DBusConnection *connection;
	DBusObjectPathVTable *vtable;
	void *user_data;
	gchar *path;
}ObjectPathData;

/* The expected address to be connected. */
static char const *Expected_conn_address = NULL;
/* Whether there was any open() attempt. */
static gboolean Connect_attempted = FALSE;

/* Expected messages to be sent. */
static GQueue Expected_messages = G_QUEUE_INIT;

/* The replies for dbus_send_with_reply_and_block() .*/
static GQueue Replies = G_QUEUE_INIT;
/* Incoming messages. */
static GQueue Incoming_messages = G_QUEUE_INIT;

/* The handler function, its data and free func,
 * as set by dbus_connection_add_filter. */
typedef struct {
	DBusHandleMessageFunction handler;
	void *data;
	DBusFreeFunction free_data;
} MockbusHandler;
/* list of filters added */
static GSList *Handlers = 0;

struct stored_notify_dat {
	DBusPendingCall *pending;
	DBusPendingCallNotifyFunction func;
	void *udata;
	DBusFreeFunction free_udata;
} stored_notify;

/* A fake connection.  All open functions return this, and then it's
 * asserted in all sending functions. */
#define Mockbus_conn ((DBusConnection *)0x6162647a)
/* Fake connection for session bus.  Allows more checks. */
#define Mockbus_bus ((DBusConnection *)0x656d2167)
/* Fake pending call */
#define Mockbus_pendingcall ((DBusPendingCall *)0xB16455)


/*
 * Returns a statically allocated compact information about @msg.  For
 * debug purposes.
 */
static G_GNUC_UNUSED char const *msginfo(DBusMessage *m)
{
	static char info[512];
	static const char mtype[][7] = {
		[DBUS_MESSAGE_TYPE_METHOD_CALL] =   "method",
		[DBUS_MESSAGE_TYPE_METHOD_RETURN] = "reply",
		[DBUS_MESSAGE_TYPE_ERROR] =         "error",
		[DBUS_MESSAGE_TYPE_SIGNAL] =        "signal",
		[DBUS_MESSAGE_TYPE_INVALID] =       "???",
	};
	sprintf(info, "[%s] %s %s.%s(%s)",
		mtype[dbus_message_get_type(m)],
		dbus_message_get_path(m),
		dbus_message_get_interface(m),
		dbus_message_get_member(m),
		dbus_message_get_signature(m));
	return info;
}

/*
 * Resets internal state of mockbus.
 */
void mockbus_reset(void)
{
	GSList *t;

	Expected_conn_address = 0;
	Connect_attempted = FALSE;

	g_queue_foreach(&Expected_messages, (GFunc)dbus_message_unref, NULL);
	g_queue_clear(&Expected_messages);
	g_queue_foreach(&Replies, (GFunc)dbus_message_unref, NULL);
	g_queue_clear(&Replies);
	g_queue_foreach(&Incoming_messages, (GFunc)dbus_message_unref, NULL);
	g_queue_clear(&Incoming_messages);

	for (t = Handlers; t; t = t->next) {
		MockbusHandler *h = (MockbusHandler *)t->data;

		if (h->data && h->free_data) h->free_data(h->data);
		g_free(h);
	}
	g_slist_free(Handlers);
	Handlers = NULL;

	stored_notify.pending = NULL;
	stored_notify.func = NULL;
	stored_notify.udata = NULL;
	stored_notify.free_udata = NULL;
}

/*
 * Signifies the end of a test-case, checks if all expectations were
 * met.
 */
void mockbus_finish(void)
{
	ck_assert_msg(Expected_conn_address == NULL || Connect_attempted,
		      "MOCKBUS: a dbus_open_connection() was expected");
	if (!g_queue_is_empty(&Expected_messages)) {
		DBusMessage *m;
		while ((m = g_queue_pop_head(&Expected_messages)))
		{
			puts(msginfo(m));
		}
		ck_abort_msg("MOCKBUS: expected more messages");
	}
	if (!g_queue_is_empty(&Replies)) {
		DBusMessage *m;
		while ((m = g_queue_pop_head(&Replies)))
		{
			puts(msginfo(m));
		}
		ck_abort_msg("MOCKBUS: not all replies were consumed");
	}
	if (!g_queue_is_empty(&Incoming_messages)) {
		DBusMessage *m;
		while ((m = g_queue_pop_head(&Incoming_messages)))
			puts(msginfo(m));
		ck_abort_msg("MOCKBUS: not all incoming messages were consumed");
	}
	ck_assert_msg(!stored_notify.pending,
		      "A reply was not set, but async method sent");
}

/*
 * @address must be static!
 */
void mockbus_expect_conn(const char *address)
{
	Expected_conn_address = address;
}

/*
 * Expect @msg to be sent.  Multiple calls queue up messages.
 */
void mockbus_expect(DBusMessage *msg)
{
	g_queue_push_tail(&Expected_messages, msg);
}

/*
 * Inserts @msg into the incoming queue, dispatched to filters.
 */
void mockbus_incoming(DBusMessage *msg)
{
	g_queue_push_tail(&Incoming_messages, msg);
}

/*
 * Delivers the first mockbus_incoming() message to the D-BUS handlers
 * as though it was coming on $conn.  If $conn is NULL it defaults to
 * the session bus connection.  The return value is unspecified.
 */
gboolean mockbus_deliver(DBusConnection *conn)
{
	DBusMessage *m;

	m = g_queue_pop_head(&Incoming_messages);

	if (m) {
		GSList *t;
		const gchar *object_path = dbus_message_get_path(m);
		ObjectPathData *path_data;
		DBusHandlerResult hres = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		GList *cur_path = fallback_list;

		if (!conn)
			conn = Mockbus_bus;
		for (t = Handlers; t; t = t->next) {
			MockbusHandler *h = (MockbusHandler *)t->data;

			if ((hres = h->handler(conn, m, h->data)) ==
			    DBUS_HANDLER_RESULT_HANDLED)
				break;
		}
		if (hres == DBUS_HANDLER_RESULT_NOT_YET_HANDLED &&
					object_path_hash)
		{
			path_data = g_hash_table_lookup(object_path_hash,
							object_path);
			if (path_data)
			{
				hres = path_data->vtable->message_function(
                                        conn, m, path_data->user_data);
			}
		}
		if (hres == DBUS_HANDLER_RESULT_NOT_YET_HANDLED)
		{
			while(cur_path)
			{
				path_data = fallback_list->data;
				if (g_str_has_prefix(object_path,
                                                     path_data->path))
				{
					path_data->vtable->message_function(
                                                conn, m, path_data->user_data);
				}
				cur_path = cur_path->next;
			}
		}
		dbus_message_unref(m);
	}
	return TRUE;
}

void mockbus_send_stored_reply(void)
{
	/* process a reply, if a delayed reply was expected */
	if (stored_notify.pending)
	{

		stored_notify.func(stored_notify.pending, stored_notify.udata);
		if (stored_notify.free_udata)
                        stored_notify.free_udata(stored_notify.udata);
		stored_notify.pending = NULL;
		stored_notify.func = NULL;
		stored_notify.udata = NULL;
		stored_notify.free_udata = NULL;
	}
}

/*
 * Creates a reply message.
 */
void mockbus_reply_msg(DBusMessage *msg)
{
	g_queue_push_tail(&Replies, msg);
}


/* Adds a MAFW-DBUS error message (understood by mafw_dbus_error_to_gerror())
 * to the $Replies queue. */
void mockbus_error(GQuark domain, guint code, const gchar *message)
{
	DBusMessage *msg;
	gchar *msg_with_code;

	msg_with_code = g_strdup_printf("%s:%u:%s", g_quark_to_string(domain),
                                        code, message);

	msg = dbus_message_new(DBUS_MESSAGE_TYPE_ERROR);
	dbus_message_set_error_name(msg, "com.nokia.mafw");
	dbus_message_append_args(msg,
				 DBUS_TYPE_STRING, &msg_with_code,
				 DBUS_TYPE_INVALID);
	g_free(msg_with_code);
	g_queue_push_tail(&Replies, msg);
}

/*
 * Returns a mafw metadata hash table, created out of the arguments as
 * string key-value pairs, terminated by a NULL.  Don't forget to
 * release the returned hash table.
 */
GHashTable *mockbus_mkmeta(gchar const *key, ...)
{
	va_list args;
	GHashTable *md;
	const gchar *val;

	md = mafw_metadata_new();

	va_start(args, key);
	while (key != NULL) {
		val = va_arg(args, gchar const *);
		mafw_metadata_add_str(md, (gchar *)key, (gchar *)val);
		key = va_arg(args, gchar const *);
	}
	va_end(args);

	return md;
}

static gboolean cmpmsgs(DBusMessageIter *ia, DBusMessageIter *ib)
{
	while (1) {
		int atype, btype;
		dbus_bool_t ra, rb;

		atype = dbus_message_iter_get_arg_type(ia);
		btype = dbus_message_iter_get_arg_type(ib);
		if (atype != btype) return FALSE;
		if (atype == DBUS_TYPE_INVALID) break;

		if (atype == DBUS_TYPE_ARRAY ||
		    atype == DBUS_TYPE_VARIANT ||
		    atype == DBUS_TYPE_STRUCT ||
		    atype == DBUS_TYPE_DICT_ENTRY)
		{
			DBusMessageIter ira, irb;

			dbus_message_iter_recurse(ia, &ira);
			dbus_message_iter_recurse(ib, &irb);
			if (!cmpmsgs(&ira, &irb))
				return FALSE;
		} else {
			union {
				dbus_uint64_t uint64;
				char *charp;
			} aval, bval;

			aval.uint64 = bval.uint64 = 0;
			dbus_message_iter_get_basic(ia, &aval);
			dbus_message_iter_get_basic(ib, &bval);
			switch (atype) {
			case DBUS_TYPE_BYTE:
			case DBUS_TYPE_BOOLEAN:
			case DBUS_TYPE_INT16:
			case DBUS_TYPE_UINT16:
			case DBUS_TYPE_INT32:
			case DBUS_TYPE_UINT32:
			case DBUS_TYPE_INT64:
			case DBUS_TYPE_UINT64:
			case DBUS_TYPE_DOUBLE:
				if (aval.uint64 != bval.uint64) {
					g_debug("%" PRIu64 " != %" PRIu64,
						aval.uint64, bval.uint64);
					return FALSE;
				}
				break;
			case DBUS_TYPE_STRING:
			case DBUS_TYPE_OBJECT_PATH:
			case DBUS_TYPE_SIGNATURE:
				if (strcmp(aval.charp, bval.charp)) {
					g_debug("'%s' != '%s'", aval.charp,
                                                bval.charp);
					return FALSE;
				}
				break;
			default:
				g_debug("unknown arg type: %u", atype);
				return FALSE;
				break;
			}
		}
		ra = dbus_message_iter_next(ia);
		rb = dbus_message_iter_next(ib);
		ck_assert(ra == rb);
	}
	return TRUE;
}

/*
 * Compares contents of @a and @b.
 */
static gboolean compare_msgs(DBusMessage *a, DBusMessage *b)
{
	DBusMessageIter ia, ib;

	dbus_message_iter_init(a, &ia);
	dbus_message_iter_init(b, &ib);
	return cmpmsgs(&ia, &ib);
}

/*
 * Checks @m against the head of Expected_messages.
 */
static void ckmsg(DBusMessage *m)
{
	DBusMessage *emsg;

 	emsg = g_queue_pop_head(&Expected_messages);

	ck_assert_msg(emsg != NULL, "MOCKBUS: this message was unexpected: %s",
		      dbus_message_get_member(m));
	ck_assert_msg(
		dbus_message_get_type(m) == dbus_message_get_type(emsg),
		"MOCKBUS: expected different message type");
	ck_assert_msg(
		dbus_message_has_path(m, dbus_message_get_path(emsg)),
		"MOCKBUS: expected different message path: %s vs %s",
					dbus_message_get_path(emsg),
					dbus_message_get_path(m));
	ck_assert_msg(
		dbus_message_has_interface(m, dbus_message_get_interface(emsg)),
		"MOCKBUS: expected different interface: %s vs %s",
					dbus_message_get_interface(emsg),
					dbus_message_get_interface(m));
	ck_assert_msg(
		dbus_message_has_member(m, dbus_message_get_member(emsg)),
		"MOCKBUS: expected different member");
	ck_assert_msg(
		dbus_message_has_signature(m, dbus_message_get_signature(emsg)),
		"MOCKBUS: expected different signature");
	ck_assert_msg(compare_msgs(emsg, m),
		    "MOCKBUS: message contents are not according to "
		    "expectations\n%s\nvs\n%s", msginfo(emsg), msginfo(m));
	dbus_message_unref(emsg);
}

/* When called before instantiating a registry, causes the registry to
 * `see' the given @active services. */
void mock_services(const gchar *const *active)
{
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
					     DBUS_PATH_DBUS,
					     DBUS_INTERFACE_DBUS,
					     "ListNames"));
	mockbus_reply(MAFW_DBUS_STRVZ(active));
}

void mock_appearing_extension(const gchar *service, gboolean proxy_side)
{
	DBusMessage *msg;
	if (!proxy_side)
	{
		mockbus_expect(mafw_dbus_method_full(
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS,
		"RequestName",
		MAFW_DBUS_STRING(service),
		MAFW_DBUS_UINT32(4)
		));
		mockbus_reply(MAFW_DBUS_UINT32(4));
	}
	msg = mafw_dbus_signal_full(NULL,
					MAFW_REGISTRY_PATH,
					MAFW_REGISTRY_INTERFACE,
					MAFW_REGISTRY_SIGNAL_HELLO,
					MAFW_DBUS_STRING(service));
	if (proxy_side)
		mockbus_incoming(msg);
	else
		mockbus_expect(msg);
}

void mock_disappearing_extension(const gchar *service, gboolean proxy_side)
{
	if (proxy_side)
	{
		gchar *matchstr = g_strdup_printf(MATCH_STR, service);
		mockbus_incoming(
			mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
					      DBUS_INTERFACE_DBUS,
					      "NameOwnerChanged",
					      MAFW_DBUS_STRING(service),
					      MAFW_DBUS_STRING(service),
					      MAFW_DBUS_STRING("")));
		mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
		g_free(matchstr);
	}
	else
	{
		mockbus_expect(mafw_dbus_method_full(
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS,
		"ReleaseName",
		MAFW_DBUS_STRING(service)
		));
	}
}

/* Mocks the messages happening at the construction of a extension:
 * - querying its very very friendly name (and returning FAKE_NAME)
 * - returning none for the list of runtime properties
 *
 * Pass the desired service name and object path as argument.
 */
void mock_empty_props(const gchar *service, const gchar *object)
{
	mockbus_expect(mafw_dbus_method_full(service, object,
					     MAFW_EXTENSION_INTERFACE,
					     MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING(FAKE_NAME));
	mockbus_expect(mafw_dbus_method_full(service, object,
					     MAFW_EXTENSION_INTERFACE,
					     MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	if (sizeof(GType) == sizeof(guint32))
	{
		mockbus_reply(MAFW_DBUS_STRVZ(NULL),
			      MAFW_DBUS_C_ARRAY(UINT32, GType));
	}
	else
	{
		mockbus_reply(MAFW_DBUS_STRVZ(NULL),
			      MAFW_DBUS_C_ARRAY(UINT64, GType));
	}
}

/* libdbus overrides */

DBusConnection *dbus_bus_get(DBusBusType type,
			     DBusError *error)
{
	return Mockbus_bus;
}

const char *dbus_bus_get_unique_name(DBusConnection *conn)
{
	if (conn == Mockbus_bus)
		return G_STRINGIFY(Mockbus_bus);
	else if (conn == Mockbus_conn)
		return G_STRINGIFY(Mockbus_conn);
	else
		g_assert_not_reached();
}

void dbus_bus_add_match(DBusConnection *connection,
			const char *rule,
			DBusError *error)
{
	ck_assert_msg(connection == Mockbus_bus, "MOCKBUS: invalid connection");
}

DBusConnection* dbus_connection_open(const char *address,
				     DBusError *error)
{
	Connect_attempted = TRUE;
	if (Expected_conn_address && strcmp(address, Expected_conn_address))
		return NULL;
	else
		return Mockbus_conn;
}


DBusConnection* dbus_connection_open_private(const char *address,
					     DBusError *error)
{
	Connect_attempted = TRUE;
	if (Expected_conn_address && strcmp(address, Expected_conn_address))
		return NULL;
	else
		return Mockbus_conn;
}


DBusConnection *dbus_connection_ref(DBusConnection *connection)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
	return connection;
}

void dbus_connection_unref(DBusConnection *connection)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
}

void dbus_connection_flush(DBusConnection *connection)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
}

void dbus_connection_close(DBusConnection *connection)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
}

dbus_bool_t dbus_connection_send(DBusConnection *connection,
				 DBusMessage *message,
				 dbus_uint32_t *client_serial)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
	ckmsg(message);
	return TRUE;
}

/* TODO mock all pending call funcs too */
dbus_bool_t dbus_pending_call_get_completed(DBusPendingCall *pending)
{
	ck_assert_msg(pending == Mockbus_pendingcall,
		    "MOCKBUS: who gave you that pending call?");
	return TRUE;
}

DBusMessage *dbus_pending_call_steal_reply(DBusPendingCall *pending)
{
	DBusMessage *m;

	ck_assert_msg(pending == Mockbus_pendingcall,
		    "MOCKBUS: who gave you that pending call?");
	m = g_queue_pop_head(&Replies);
	return m;
}

dbus_bool_t dbus_pending_call_set_notify(DBusPendingCall *pending,
					 DBusPendingCallNotifyFunction func,
					 void *user_data,
					 DBusFreeFunction free_user_data)
{
	ck_assert_msg(pending == Mockbus_pendingcall,
		    "MOCKBUS: who gave you that pending call?");
	if (g_queue_is_empty(&Replies))
	{/* Emulating a slow reply...reply will be added later */
		stored_notify.func = func;
		stored_notify.udata = user_data;
		stored_notify.free_udata = free_user_data;
		stored_notify.pending = pending;
	}
	else
	{
		func(pending, user_data);
		if (free_user_data) free_user_data(user_data);
	}
	return TRUE;
}

void dbus_pending_call_unref(DBusPendingCall *pending)
{
	ck_assert_msg(pending == Mockbus_pendingcall,
		    "MOCKBUS: who gave you that pending call?");
}

dbus_bool_t dbus_connection_send_with_reply(DBusConnection *connection,
					    DBusMessage *message,
					    DBusPendingCall **pending_return,
					    int timeout_milliseconds)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
	ckmsg(message);
	*pending_return = Mockbus_pendingcall;
	return TRUE;
}

DBusMessage
*dbus_connection_send_with_reply_and_block(DBusConnection *connection,
                                           DBusMessage *message,
                                           int timeout_milliseconds,
                                           DBusError *error)
{
	DBusMessage *reply = NULL;

	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
	ckmsg(message);

	/*
	 * Set $error if $reply is an error message.  There must be $Replies
	 * because dbus_connection_send_with_reply_and_block() _expects_ one.
	 * It's no sense generating a synthetic (say timeout) error if we don't
	 * have replies because the caller can just as easily predicate any
	 * error it wishes.
	 */
	reply = g_queue_pop_head(&Replies);
	ck_assert(reply != NULL);
	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		if (error != NULL)
			dbus_set_error_from_message(error, reply);
		dbus_message_unref(reply);
		return NULL;
	} else
		return reply;
}

dbus_bool_t dbus_connection_add_filter(DBusConnection *connection,
				       DBusHandleMessageFunction function,
				       void *user_data,
				       DBusFreeFunction free_data_function)
{
	MockbusHandler *h;

	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
	h = g_new0(MockbusHandler, 1);
	h->handler = function;
	h->data = user_data;
	h->free_data = free_data_function;
	Handlers = g_slist_append(Handlers, h);
	return TRUE;
}

static void _free_obpath_data(ObjectPathData *reg_data)
{
	if (reg_data->path)
		g_free(reg_data->path);
	if (reg_data->vtable->unregister_function && reg_data->user_data)
		reg_data->vtable->unregister_function(reg_data->connection,
				reg_data->user_data);
	g_free(reg_data->vtable);
	g_free(reg_data);
}

dbus_bool_t
dbus_connection_register_object_path(DBusConnection *connection,
                                     const char *path,
                                     const DBusObjectPathVTable *vtable,
                                     void *user_data)
{
	ObjectPathData *reg_data;

	if (!object_path_hash)
	{
		object_path_hash =
                        g_hash_table_new_full(
                                g_str_hash,
                                g_str_equal,
                                g_free, (GDestroyNotify)_free_obpath_data);
	}

	reg_data = g_new0(ObjectPathData, 1);
	reg_data->vtable = g_new0(DBusObjectPathVTable, 1);
	reg_data->vtable->unregister_function = vtable->unregister_function;
	reg_data->vtable->message_function = vtable->message_function;
	reg_data->user_data = user_data;
	reg_data->connection = connection;
	g_hash_table_insert(object_path_hash, g_strdup(path), reg_data);
	return TRUE;
}

dbus_bool_t
dbus_connection_register_fallback(DBusConnection *connection,
                                  const char *path,
                                  const DBusObjectPathVTable *vtable,
                                  void *user_data)
{
	ObjectPathData *reg_data;

	reg_data = g_new0(ObjectPathData, 1);
	reg_data->path = g_strdup(path);
	reg_data->vtable = g_new0(DBusObjectPathVTable, 1);
	reg_data->vtable->unregister_function = vtable->unregister_function;
	reg_data->vtable->message_function = vtable->message_function;
	reg_data->user_data = user_data;
	reg_data->connection = connection;
	fallback_list = g_list_prepend(fallback_list, reg_data);
	return TRUE;
}

dbus_bool_t dbus_connection_unregister_object_path(DBusConnection *connection,
						const char *path)
{
	ObjectPathData *reg_data = g_hash_table_lookup(object_path_hash, path);
	GList *cur_path = fallback_list;
	if (reg_data)
	{
		g_hash_table_remove(object_path_hash, path);
	}

	while(cur_path)
	{
		reg_data = fallback_list->data;
		if (strcmp(reg_data->path, path) == 0)
		{
			_free_obpath_data(reg_data);
			fallback_list = g_list_remove(fallback_list, reg_data);
			cur_path = fallback_list;
			continue;
		}
		cur_path = cur_path->next;
	}
	return TRUE;
}

static gint get_handler(MockbusHandler *a, DBusHandleMessageFunction function)
{
	if (a->handler == function)
	{
		return 0;
	}

	return -1;
}

void dbus_connection_remove_filter(DBusConnection *connection,
				       DBusHandleMessageFunction function,
				       void *user_data)
{
	GSList *removed_element;

	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");

	removed_element = g_slist_find_custom(Handlers, function,
						(GCompareFunc)get_handler);

	if (removed_element)
	{
		g_free(removed_element -> data);
		Handlers = g_slist_delete_link(Handlers, removed_element);
	}
}


void dbus_connection_setup_with_g_main(DBusConnection *connection,
				       GMainContext *context)
{
	ck_assert_msg(connection == Mockbus_conn || connection == Mockbus_bus,
		    "MOCKBUS: invalid connection");
	g_timeout_add(200, (GSourceFunc)mockbus_deliver, connection);
}

void *dbus_connection_get_data(DBusConnection *connection, dbus_int32_t id)
{
	return NULL;
}

void dbus_server_setup_with_g_main(DBusServer *server,
				   GMainContext *context)
{
	/* NOP */
}

dbus_bool_t dbus_message_is_signal (DBusMessage *message, const char *interface,
			const char *signal_name)
{
	return (strcmp(signal_name, dbus_message_get_member(message)) == 0);
}

void dbus_server_set_new_connection_function(DBusServer *server,
					     DBusNewConnectionFunction func,
					     void *data,
					     DBusFreeFunction free_data)
{
	/* let's call immediately */
	if (func) func(server, Mockbus_conn, data);
	if (data && free_data) free_data(data);
}

/* mockbus_reply() uses this without a valid call, let's please it. */
DBusMessage *dbus_message_new_method_return(DBusMessage *ignore)
{
	return dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
}

dbus_uint32_t dbus_message_get_serial(DBusMessage *message)
{
	/* dbus_message_new_error() fails if serial is zero,
	 * so let's satisfy it.  NOTE that there may be some other
	 * requirements wrt serials... */
	return GPOINTER_TO_UINT(message);
}

const char *msg_sender_id = ":1.103";

const char *dbus_message_get_sender(DBusMessage *message)
{
	return msg_sender_id;
}
