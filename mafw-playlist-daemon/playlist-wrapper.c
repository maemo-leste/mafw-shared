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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libmafw/mafw-errors.h>

#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"
#include "libmafw-shared/mafw-proxy-playlist.h"

#include "mpd-internal.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-playlist-wrapper"

/* D-Bus utilities. */

static void send_item_moved(guint plid, guint from, guint to)
{
	DBusConnection* conn = NULL;
	DBusMessage *msg = NULL;
	gchar *path;

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	g_assert(conn != NULL);

	path = g_strdup_printf("%s/%u",MAFW_PLAYLIST_PATH, plid);

	/* Create the message. */
	msg = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	dbus_message_set_path(msg, path);
	dbus_message_set_interface(msg, MAFW_PLAYLIST_INTERFACE);
	dbus_message_set_member(msg, MAFW_PLAYLIST_ITEM_MOVED);
	dbus_message_append_args(msg,
				 DBUS_TYPE_UINT32, &from,
				 DBUS_TYPE_UINT32, &to,
				 DBUS_TYPE_INVALID);

	/* Send the message */
	mafw_dbus_send(conn, msg);
	dbus_connection_unref(conn);
	g_free(path);
}

static void send_contents_changed(guint plid, guint from,
				  guint nremove, guint nreplace)
{
	DBusConnection* conn = NULL;
	DBusMessage *msg = NULL;
	gchar *path;

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	g_assert(conn != NULL);

	path = g_strdup_printf("%s/%u",MAFW_PLAYLIST_PATH, plid);

	/* Create the message. */
	msg = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	dbus_message_set_path(msg, path);
	dbus_message_set_interface(msg, MAFW_PLAYLIST_INTERFACE);
	dbus_message_set_member(msg, MAFW_PLAYLIST_CONTENTS_CHANGED);
	dbus_message_append_args(msg,
				 DBUS_TYPE_UINT32, &plid,
				 DBUS_TYPE_UINT32, &from,
				 DBUS_TYPE_UINT32, &nremove,
				 DBUS_TYPE_UINT32, &nreplace,
				 DBUS_TYPE_INVALID);

	/* Send the message */
	mafw_dbus_send(conn, msg);
	dbus_connection_unref(conn);
	g_free(path);
}

static void send_property_changed(guint32 plid, const gchar *property)
{
	DBusConnection* conn = NULL;
	DBusMessage *msg = NULL;
	gchar *path;

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	g_assert(conn != NULL);

	path = g_strdup_printf("%s/%u",MAFW_PLAYLIST_PATH, plid);

	/* Create the message. */
	msg = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	dbus_message_set_path(msg, path);
	dbus_message_set_interface(msg, MAFW_PLAYLIST_INTERFACE);
	dbus_message_set_member(msg, MAFW_PLAYLIST_PROPERTY_CHANGED);
	dbus_message_append_args(msg,
				 DBUS_TYPE_STRING, &property,
				 DBUS_TYPE_INVALID);

	/* Send the message */
	mafw_dbus_send(conn, msg);
	dbus_connection_unref(conn);
	g_free(path);
}

#define MATCH_STR "type='signal',interface='org.freedesktop.DBus'," \
			"member='NameOwnerChanged',arg0='%s',arg2=''"
/** Hash table to store which client increased the use-count */
static GHashTable *_usecount_holders;

/**
 * Removes a dbus-match registered for a client, and removes it's list from
 * the _usecount_holders hash-table
 */
static void _unregister_requestor(DBusConnection *conn, const gchar *requestor)
{
	gchar *matchstr = NULL;
	
	if (!_usecount_holders)
		return;

	matchstr = g_strdup_printf(MATCH_STR, requestor);
	dbus_bus_remove_match(conn, matchstr, NULL);
	g_hash_table_remove(_usecount_holders, requestor);
	g_free(matchstr);
	return;
}

/**
 * Removes a Pls pointer from the client's list, and unregisters the client
 * if the list is empty after this
 */
static void _remove_usecount_holder(DBusConnection *connection,
					const gchar *requestor, Pls *pls)
{
	GList *pllist = NULL;

	if (!_usecount_holders)
		return;

	pllist = g_hash_table_lookup(_usecount_holders, requestor);
	pllist = g_list_remove(pllist, pls);
	
	if (!pllist)
	{
		_unregister_requestor(connection, requestor);
		return;
	}
	
	g_hash_table_replace(_usecount_holders, g_strdup(requestor), pllist);
}

/**
 * Extracts the client from the message, and removes the Pls pointer from it's
 * list
 */
static void _remove_usecount_holder_by_msg(DBusConnection *connection,
						DBusMessage *msg, Pls *pls)
{
	const gchar *requestor = dbus_message_get_sender(msg);

	_remove_usecount_holder(connection, requestor, pls);
}

/**
 * Handles the NameOwnerChanged signals, and unregisters a client if needed.
 * This will decrease the use-count, if a registered client disappears.
 */
static DBusHandlerResult
handle_usecount_holder_msgs(DBusConnection *conn,
                                     DBusMessage *msg,
                                     gpointer data)
{
	gchar *name, *oldname, *newname;

	if (!_usecount_holders)
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
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (*newname) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (*oldname) {
		GList *pllist = g_hash_table_lookup(_usecount_holders, oldname);
		
		if (pllist)
		{
			while(pllist)
			{
				Pls *pls = pllist->data;

				pls->use_count--;
				pls_set_use_count(pls, pls->use_count);
				pllist = g_list_next(pllist);
			}
			_unregister_requestor(conn, oldname);
		}
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/**
 * Add a dbus-filter
 */
void init_pl_wrapper(DBusConnection *connection)
{	
	if (!dbus_connection_add_filter(connection,
                                        handle_usecount_holder_msgs,
                                        NULL, NULL))
		g_assert_not_reached();
}

/**
 * If a client has increased a playlist's use-count, this will store the client's
 * request in the _usecount_holders hash-table.
 */
static void _store_usecount_holder(DBusConnection *connection, DBusMessage *msg,
					Pls *pls)
{
	GList *pllist = NULL;
	gchar *requestor = g_strdup(dbus_message_get_sender(msg));

	if (!_usecount_holders)
	{
		_usecount_holders = g_hash_table_new_full(g_str_hash,
							g_str_equal,
							(GDestroyNotify)g_free,
							NULL);
	}
	else
		pllist = g_hash_table_lookup(_usecount_holders, requestor);

	if (!pllist)
	{/* Adding match */
		gchar *match_str = g_strdup_printf(MATCH_STR, requestor);
		DBusError err;

		dbus_error_init(&err);
		
		dbus_bus_add_match(connection, match_str, &err);
		if (dbus_error_is_set(&err))
		{
                	g_critical("Unable to add match: %s", match_str);
			dbus_error_free(&err);
		}
		g_free(match_str);
	}
	
	pllist = g_list_prepend(pllist, pls);
	
	g_hash_table_replace(_usecount_holders, requestor, pllist);
}

DBusHandlerResult handle_playlist_request(DBusConnection *conn,
                                          DBusMessage *msg,
                                          const gchar *path)
{
	const gchar *member;
	guint plid;
	Pls *pls;

	/* Object path should look like: "/com/nokia/mafw/playlist/<ID>" */
	plid = atoi(path + sizeof(MAFW_PLAYLIST_PATH));
	if (plid == MAFW_PROXY_PLAYLIST_INVALID_ID) {
		g_warning("Not a valid playlist id");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	pls = g_tree_lookup(Playlists, GUINT_TO_POINTER(plid));
	if (!pls) {
		mafw_dbus_send(
			conn, mafw_dbus_error(
				msg, MAFW_PLAYLIST_ERROR,
				MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND,
				"No such playlist"));
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	member = dbus_message_get_member(msg);
	if (!strcmp(member, MAFW_PLAYLIST_METHOD_SET_NAME)) {
		gchar *name, *oldname;

		mafw_dbus_parse(msg, DBUS_TYPE_STRING, &name);

		if (g_tree_lookup(Playlists_by_name, name))
		{
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		
		oldname = g_strdup(pls->name);
		if (pls_set_name(pls, name)) {
			/* Name change invalidates $Playlist_by_name, thus we
			 * must update it. */
			g_assert(g_tree_remove(Playlists_by_name, oldname));
			g_tree_insert(Playlists_by_name,
				      g_strdup(pls->name), pls);
			send_property_changed(plid, "name");
		}
		g_free(oldname);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_NAME)) {
		gchar *name;
		name = g_strdup(pls->name);
		mafw_dbus_send(conn, name
			? mafw_dbus_reply(msg, MAFW_DBUS_STRING(name))
		       	: mafw_dbus_error(msg, MAFW_PLAYLIST_ERROR,
				    MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND,
				    "or whatever"));
		g_free(name);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_SET_REPEAT)) {
		gboolean repeat;
		mafw_dbus_parse(msg, DBUS_TYPE_BOOLEAN, &repeat);
		pls_set_repeat(pls, repeat);
		send_property_changed(plid, "repeat");
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_REPEAT)) {
		gboolean repeat;
		repeat = pls->repeat;
		mafw_dbus_send(conn,
                               mafw_dbus_reply(msg,MAFW_DBUS_BOOLEAN(repeat)));
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_SHUFFLE)) {
		pls_shuffle(pls);
		send_property_changed(plid, "is-shuffled");
		mafw_dbus_ack_or_error(conn, msg, NULL);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_IS_SHUFFLED)) {
		gboolean shuffled;
		shuffled = pls_is_shuffled(pls);
		mafw_dbus_send(conn,
                               mafw_dbus_reply(msg,
                                               MAFW_DBUS_BOOLEAN(shuffled)));
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_UNSHUFFLE)) {
		pls_unshuffle(pls);
		send_property_changed(plid, "is-shuffled");
		mafw_dbus_ack_or_error(conn, msg, NULL);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_INCREMENT_USE_COUNT)) {
		pls->use_count++;
		pls_set_use_count(pls, pls->use_count);
		_store_usecount_holder(conn, msg, pls);
		mafw_dbus_ack_or_error(conn, msg, NULL);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_DECREMENT_USE_COUNT)) {
		pls->use_count--;
		pls_set_use_count(pls, pls->use_count);
		_remove_usecount_holder_by_msg(conn, msg, pls);
		mafw_dbus_ack_or_error(conn, msg, NULL);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_INSERT_ITEM)) {
		guint index;
		gchar **objectids;
		guint len;
		GError *err = NULL;

		mafw_dbus_parse(msg,
				DBUS_TYPE_UINT32, &index,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                        	&objectids, &len);
		if (!pls_inserts(pls, index, (const gchar **)objectids, len))
			err = g_error_new(MAFW_PLAYLIST_ERROR,
					  MAFW_PLAYLIST_ERROR_INVALID_INDEX,
					  "Wrong index");
		mafw_dbus_ack_or_error(conn, msg, err);
		send_contents_changed(plid, index, 0, len);
		g_strfreev(objectids);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_APPEND_ITEM)) {
		GError *err = NULL;
		gchar **objectids;
		guint len;

		mafw_dbus_parse(msg, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                        	&objectids, &len);
		if (!pls_appends(pls, (const gchar **)objectids, len))
			err = g_error_new(MAFW_PLAYLIST_ERROR,
					  MAFW_PLAYLIST_ERROR_INVALID_INDEX,
					  "and what now");
		mafw_dbus_ack_or_error(conn, msg, err);
		send_contents_changed(plid, pls->len-len, 0, len);
		g_strfreev(objectids);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_REMOVE_ITEM)) {
		guint index;
		GError *error = NULL;

		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &index);
		if (!pls_remove(pls, index)) {
			error = g_error_new(MAFW_PLAYLIST_ERROR,
					    MAFW_PLAYLIST_ERROR_INVALID_INDEX,
					    "Wrong index");
			mafw_dbus_send(conn, mafw_dbus_gerror(msg, error));
			g_error_free(error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg, MAFW_DBUS_BOOLEAN(TRUE)));
		send_contents_changed(plid, index, 1, 0);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_ITEM)) {
		gchar *oid;
		guint index;

		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &index);
		oid = pls_get_item(pls, index);
		if (oid) {
			mafw_dbus_send(conn,
					mafw_dbus_reply(
						msg,
						MAFW_DBUS_STRING(oid)));
			g_free(oid);
		} else {
			mafw_dbus_send(conn,
					mafw_dbus_reply(
						msg,
						MAFW_DBUS_STRING("")));
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}  else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_ITEMS)) {
		gchar **oids = NULL;
		guint start_index, end_index;
		GError *error = NULL;

		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &start_index,
					DBUS_TYPE_UINT32, &end_index);
		oids = pls_get_items(pls, start_index, end_index);
		if (oids)
		{
			mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg,
					MAFW_DBUS_STRVZ(oids)));
			g_free(oids);
		}
		else
		{
			error = g_error_new(MAFW_PLAYLIST_ERROR,
					    MAFW_PLAYLIST_ERROR_INVALID_INDEX,
					    "Wrong index");
			mafw_dbus_send(conn, mafw_dbus_gerror(msg, error));
			g_error_free(error);
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_STARTING_INDEX)) {
		gchar *oid = NULL;
		guint index;

		pls_get_starting(pls, &index, &oid);
		if (!oid) {
			oid = g_strdup("");
		}
		mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg,
					MAFW_DBUS_UINT32(index),
					MAFW_DBUS_STRING(oid)));
		g_free(oid);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_LAST_INDEX)) {
		gchar *oid = NULL;
		guint index;

		pls_get_last(pls, &index, &oid);
		if (!oid) {
			oid = g_strdup("");
		}
		mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg,
					MAFW_DBUS_UINT32(index),
					MAFW_DBUS_STRING(oid)));
		g_free(oid);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_NEXT)) {
		gchar *oid = NULL;
		guint index;

		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &index);
		pls_get_next(pls, &index, &oid);
		if (!oid) {
			oid = g_strdup("");
		}
		mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg,
					MAFW_DBUS_UINT32(index),
					MAFW_DBUS_STRING(oid)));
		g_free(oid);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_PREV)) {
		gchar *oid = NULL;
		guint index;

		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &index);
		pls_get_prev(pls, &index, &oid);
		if (!oid) {
			oid = g_strdup("");
		}
		mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg,
					MAFW_DBUS_UINT32(index),
					MAFW_DBUS_STRING(oid)));
		g_free(oid);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_MOVE)) {
		guint from, to;

		mafw_dbus_parse(msg,
				 DBUS_TYPE_UINT32, &from,
				 DBUS_TYPE_UINT32, &to);
		if (!pls_move(pls, from, to)) {
			mafw_dbus_send(conn,
					mafw_dbus_reply(
						msg,
						MAFW_DBUS_BOOLEAN(FALSE)));
		} else {
			mafw_dbus_send(conn,
					mafw_dbus_reply(
						msg,
						MAFW_DBUS_BOOLEAN(TRUE)));
			send_item_moved(plid, from, to);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_GET_SIZE)) {
		mafw_dbus_send(conn,
				mafw_dbus_reply(
					msg,
					MAFW_DBUS_UINT32(pls->len)));
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_CLEAR)) {
		guint oldlen;

		oldlen = pls->len;
		pls_clear(pls);
		mafw_dbus_ack_or_error(conn, msg, NULL);
		send_contents_changed(plid, 0, oldlen, 0);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
