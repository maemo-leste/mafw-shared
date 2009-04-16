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
#include <dbus/dbus.h>
#include <libmafw/mafw.h>
#include <libmafw/mafw-registry.h>
#include <libmafw/mafw-metadata-serializer.h>

#include "common/dbus-interface.h"
#include "common/mafw-util.h"
#include "common/mafw-dbus.h"
#include "wrapper.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-source-wrapper"

/* These have to be set for mafw_dbus_*() */
#define MAFW_DBUS_PATH MAFW_OBJECT
#define MAFW_DBUS_INTERFACE MAFW_SOURCE_INTERFACE

#define INITIAL_BROWSE_TIMEOUT 100 /* The initial time value in ms, until the
				   wrapper collects the first browse-resuls */
#define INITIAL_MAX_RESULTS 25	/* The number of the maximal amount of 
				   browse-results, what the first browse-results
				   message can contain */
#define TIMEOUT_INCREMENT 500	/* Increments the timeout value (in ms), if the
				   message contains the currently allowed
				   browse-results */
#define MAX_TIMEOUT 1000	/* Maximum time value (in ms), until the wrapper
				   collects the browse results */
#define MAX_BROWSE_RESULT 500	/* Maximum number of the results, what a message
				   can contain */

struct browse_data {
	MafwDBusOpCompletedInfo *oci;
	guint timeout_id;	/* timeout GSource ID */
	guint timeout_time;	/* The timeout value of the current message-array */
	guint results;		/* The number of messages already added */
	guint maxresults;	/* The maximal number of messages */
	DBusMessage *message_to_send;
	DBusMessageIter iter_array, iter_msg;
	ExportedComponent *ecomp;
};

static GHashTable *browse_requests;

static gboolean send_browse_res(struct browse_data *bdat)
{
	if (bdat->message_to_send)
	{
		dbus_message_iter_close_container(&bdat->iter_msg,
						&bdat->iter_array);
		mafw_dbus_send(bdat->oci->con, bdat->message_to_send);
		bdat->message_to_send = NULL;
	}
	return TRUE;
}

static gboolean remove_from_hash(guint browse_id)
{
	g_hash_table_remove(browse_requests,
				    GUINT_TO_POINTER(browse_id));
	return FALSE;
}

static void free_browse_req(struct browse_data *bdata)
{
	if (bdata->timeout_id)
		g_source_remove(bdata->timeout_id);
	if (bdata->message_to_send)
		dbus_message_unref(bdata->message_to_send);
	if (bdata->oci)
		mafw_dbus_oci_free(bdata->oci);
	g_free(bdata);
}

static void emit_browse_result(MafwSource *self, guint browse_id,
			       gint remaining_count, guint index,
			       const gchar *object_id, GHashTable *metadata,
			       gpointer user_data, const GError *error)
{
	const gchar *domain_str;
	MafwDBusOpCompletedInfo *info;
	DBusMessageIter istr;
	struct browse_data *bdata = (struct browse_data *)user_data;
	GByteArray *ba = NULL;

	info = (MafwDBusOpCompletedInfo *)bdata->oci;
	g_assert(info != NULL);
	
	if (browse_id == MAFW_SOURCE_INVALID_BROWSE_ID)
	{
		mafw_dbus_send(info->con, mafw_dbus_gerror(info->msg,
					error));
		free_browse_req(bdata);
		return;
	}

	/* $object_id == NULL is valid eg. when browsing an empty container. */
	if (object_id == NULL)
		object_id = "";

	if (!bdata->message_to_send)
	{
		bdata->message_to_send = dbus_message_new_method_call(
				dbus_message_get_sender(info->msg),
				bdata->ecomp->object_path,
				MAFW_SOURCE_INTERFACE,
				MAFW_PROXY_SOURCE_METHOD_BROWSE_RESULT);
		bdata->results = 0;
		dbus_message_iter_init_append(bdata->message_to_send,
						&bdata->iter_msg);
		dbus_message_iter_append_basic(&bdata->iter_msg,
						DBUS_TYPE_UINT32, &browse_id);
		dbus_message_iter_open_container(&bdata->iter_msg,
						DBUS_TYPE_ARRAY,
						"(iusaysus)",
						&bdata->iter_array);
	}
	
	ba = mafw_metadata_freeze_bary(metadata);
	
	dbus_message_iter_open_container(&bdata->iter_array,
						DBUS_TYPE_STRUCT,
						NULL, &istr);
	dbus_message_iter_append_basic(&istr,  DBUS_TYPE_INT32,
					&remaining_count);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_UINT32, &index);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &object_id);
	mafw_dbus_message_append_array(&istr, DBUS_TYPE_BYTE, ba->len,
					ba->data);
	g_byte_array_free(ba, TRUE);

	if (error) {
		domain_str = g_quark_to_string(error->domain);
		dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
						&domain_str);
		dbus_message_iter_append_basic(&istr, DBUS_TYPE_UINT32,
						&error->code);
		dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
						&error->message);
	} else {
		gchar *fakestr="";
		guint fakeint = 0;
		dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
						&fakestr);
		dbus_message_iter_append_basic(&istr,  DBUS_TYPE_UINT32,
						&fakeint);
		dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
						&fakestr);
	}
	dbus_message_iter_close_container(&bdata->iter_array, &istr);
	
	bdata->results++;
	/* Note: This assumes that source must always call 
	   the callback in the end with remaining_count == 0.
	   In case an error happened, no more browse-result
	   should come.*/
	if (remaining_count == 0 || error) {
		dbus_message_iter_close_container(&bdata->iter_msg,
							&bdata->iter_array);
		mafw_dbus_send(bdata->oci->con, bdata->message_to_send);
		bdata->message_to_send = NULL;
		g_source_remove(bdata->timeout_id);
		bdata->timeout_id = 0;
		/* At this point, it could happen, that the browse did not
		return, so the data is not added to the hash-table */
		if (browse_id != MAFW_SOURCE_INVALID_BROWSE_ID)
			g_idle_add((GSourceFunc)remove_from_hash,
				GUINT_TO_POINTER(browse_id));
		else
			free_browse_req(bdata);
		return;
	}
	
	/* In case, the source is very "fast", to keep the UI as responsible
	   as possible, it is better if the wrapper sends huge amount of
	   results in one message. If the browse-results reaches the current
	   maximal result-number, the results are send immediately, and the next
	   max batched browse-results are trippled, and the collection-timeout
	   is increased. The timeout, and the max-results have a defined
	   maximums.
	   The max-browse-results increases so drastically if needed, because if
	   the UI displays these results in a GTKTreeView, the treeview will
	   need more and more time to render the newly added items. So to finish
	   the whole procedure earlier, it is better to send as less
	   browse-results-messages as possible */
	if (bdata->results >= bdata->maxresults)
	{
		g_source_remove(bdata->timeout_id);
		dbus_message_iter_close_container(&bdata->iter_msg,
						&bdata->iter_array);
		mafw_dbus_send(bdata->oci->con, bdata->message_to_send);
		bdata->results = 0;
		bdata->message_to_send = NULL;
		if (bdata->maxresults != MAX_BROWSE_RESULT)
		{
			bdata->maxresults *= 3;
			if (bdata->maxresults > MAX_BROWSE_RESULT)
			{
				bdata->maxresults = MAX_BROWSE_RESULT;
			}
			else
			{
				bdata->timeout_time += TIMEOUT_INCREMENT;
			}
			if (bdata->timeout_time > MAX_TIMEOUT)
			{
				bdata->timeout_time = MAX_TIMEOUT;
			}
		}
		
		bdata->timeout_id = g_timeout_add(bdata->timeout_time,
					(GSourceFunc)send_browse_res, bdata);
	}
}

static void got_metadata(MafwSource *self, const gchar *object_id,
			 GHashTable *metadata, gpointer user_data,
			 const GError *error)
{
	MafwDBusOpCompletedInfo *info;

	info = (MafwDBusOpCompletedInfo *)user_data;
	g_assert(info != NULL);

	mafw_dbus_send(info->con, error
		? mafw_dbus_gerror(info->msg, error)
		: mafw_dbus_reply (info->msg, MAFW_DBUS_METADATA(metadata)));
	mafw_dbus_oci_free(info);
}

/* Appends the metadata-results to the reply-msg */
static void _append_ob_mdata(gchar *key, GHashTable *metadata,
					DBusMessageIter *iter_array)
{
	GByteArray *ba = NULL;
	DBusMessageIter istr;

	dbus_message_iter_open_container(iter_array,
					DBUS_TYPE_STRUCT,
					NULL, &istr);

	ba = mafw_metadata_freeze_bary(metadata);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
						&key);
	mafw_dbus_message_append_array(&istr, DBUS_TYPE_BYTE,
					ba->len,
					ba->data);
	dbus_message_iter_close_container(iter_array,
						&istr);
	g_byte_array_free(ba, TRUE);
}

static void got_metadatas(MafwSource *self,
			 GHashTable *metadatas, gpointer user_data,
			 const GError *error)
{
	MafwDBusOpCompletedInfo *info;
	
	DBusMessage *replmsg;
	const gchar *domain_str = "";
	guint errcode = 0;
	const gchar *err_msg = "";
	DBusMessageIter iter_array, iter_msg;

	info = (MafwDBusOpCompletedInfo *)user_data;
	g_assert(info != NULL);
	
	replmsg = dbus_message_new_method_return(info->msg);
	dbus_message_iter_init_append(replmsg,
					&iter_msg);
	if (metadatas)
	{
		dbus_message_iter_open_container(&iter_msg, DBUS_TYPE_ARRAY,
						 "(say)", &iter_array);

		g_hash_table_foreach(metadatas, (GHFunc)_append_ob_mdata,
					&iter_array);

		dbus_message_iter_close_container(&iter_msg, &iter_array);
	}
	
	if (error)
	{
		domain_str = g_quark_to_string(error->domain);
		errcode = error->code;
		err_msg = error->message;
	}
	dbus_message_iter_append_basic(&iter_msg, DBUS_TYPE_STRING, &domain_str);
	dbus_message_iter_append_basic(&iter_msg, DBUS_TYPE_UINT32, &errcode);
	dbus_message_iter_append_basic(&iter_msg, DBUS_TYPE_STRING, &err_msg);

	mafw_dbus_send(info->con, replmsg);
	mafw_dbus_oci_free(info);
}

/* Called when ->create_object() finished. */
static void object_created(MafwSource *src, const gchar *objectid,
			   gpointer user_data, const GError *error)
{
	MafwDBusOpCompletedInfo *info;

	info = (MafwDBusOpCompletedInfo*) user_data;
	g_assert(info != NULL);
	
	mafw_dbus_send(info->con, error
		? mafw_dbus_gerror(info->msg, error)
		: mafw_dbus_reply (info->msg, MAFW_DBUS_STRING(objectid)));
	mafw_dbus_oci_free(info);
}

/* Called when ->destroy_object() finished. */
static void object_destroyed(MafwSource *src, const gchar *objectid,
			     gpointer user_data, const GError *error)
{
	MafwDBusOpCompletedInfo *info;

	info = (MafwDBusOpCompletedInfo*) user_data;
	g_assert(info != NULL);

	mafw_dbus_send(info->con, error
	       ? mafw_dbus_gerror(info->msg, error)
	       : mafw_dbus_reply (info->msg, MAFW_DBUS_STRING(objectid)));
	mafw_dbus_oci_free(info);
}

/* Called when ->set_metadata() finished. */
static void metadata_set(MafwSource *src, const gchar *objectid,
			 const gchar **failed_keys, gpointer user_data,
			 const GError *error)
{
	MafwDBusOpCompletedInfo *info;
	const gchar *domain_str;

	info = (MafwDBusOpCompletedInfo*) user_data;
	g_assert(info != NULL);

	g_assert(objectid != NULL);

	if (error) {
		domain_str = g_quark_to_string(error->domain);
		mafw_dbus_send(
			info->con,
			mafw_dbus_reply(info->msg,
					MAFW_DBUS_STRING(objectid),
					MAFW_DBUS_STRVZ(failed_keys),
					MAFW_DBUS_STRING(domain_str),
					MAFW_DBUS_INT32(error->code),
					MAFW_DBUS_STRING(error->message))
			);
	} else {
		DBusMessage *m;
		m = mafw_dbus_reply(info->msg,
				    MAFW_DBUS_STRING(objectid),
				    MAFW_DBUS_STRVZ(failed_keys));
		mafw_dbus_send(info->con, m);
	}
	mafw_dbus_oci_free(info);
}

/**
 * handle_source_msg:
 * @conn: the #DBusConnection on which this message arrived.
 * @msg: the #DBusMessage itself.
 * @data: the associated #ExportedComponent struct.
 *
 * D-Bus message filter.  Called by the wrapper on incoming messages
 * for a extension.
 */
DBusHandlerResult handle_source_msg(DBusConnection *conn,
				    DBusMessage *msg,
				    void *data)
{
	ExportedComponent *ecomp;
	MafwSource *source;

	ecomp = (ExportedComponent *)data;
	source = MAFW_SOURCE(ecomp->comp);

	if (dbus_message_has_interface(msg, MAFW_EXTENSION_INTERFACE))
		return handle_extension_msg(conn, msg, data);

	if (dbus_message_has_member(msg, MAFW_SOURCE_METHOD_BROWSE)) {

		const gchar *object_id;
		gboolean recursive;
		const gchar *filter_string;
		const gchar *sort_criteria;
		const gchar **metadata_keys;
		guint skip_count;
		guint item_count;
		guint browse_id;
		MafwFilter *filter = NULL;
		struct browse_data *bdata = g_new0(struct browse_data, 1);

		/* NOTE Though i didn't find it documented, but D-BUS
		 * adds NULL-termination to $metadata_keys.  <relief> */
		mafw_dbus_parse(msg,
				DBUS_TYPE_STRING, &object_id,
				DBUS_TYPE_BOOLEAN, &recursive,
				DBUS_TYPE_STRING, &filter_string,
				DBUS_TYPE_STRING, &sort_criteria,
				MAFW_DBUS_TYPE_STRVZ, &metadata_keys,
				DBUS_TYPE_UINT32, &skip_count,
				DBUS_TYPE_UINT32, &item_count);

		/* Empty criteria? */
		if (filter_string != NULL) {
			filter = mafw_filter_parse(filter_string);
		}
		if (*sort_criteria == '\0')
			sort_criteria = NULL;

		/* Store the message and pass as user data to browse().
		   This is used to route the results to correct 
		   destination. */
		bdata->oci = mafw_dbus_oci_new(conn, msg);
		bdata->maxresults = INITIAL_MAX_RESULTS;
		bdata->timeout_id = g_timeout_add(INITIAL_BROWSE_TIMEOUT,
						(GSourceFunc)send_browse_res,
						bdata);
		bdata->timeout_time = INITIAL_BROWSE_TIMEOUT;
		bdata->ecomp = ecomp;

		/* Invoke real object method and forward reply. */
		browse_id = mafw_source_browse(source, object_id, recursive,
					       filter, sort_criteria,
					       metadata_keys[0]
						       ? metadata_keys
						       : NULL,
					       skip_count, item_count,
					       emit_browse_result, bdata);
		mafw_filter_free(filter);
		if (!browse_requests)
			browse_requests = g_hash_table_new_full(NULL,
							      NULL,
							      NULL,
							      (GDestroyNotify)free_browse_req);
		if (browse_id != MAFW_SOURCE_INVALID_BROWSE_ID)
		{
			g_hash_table_replace(browse_requests,
					     GUINT_TO_POINTER(browse_id),
					     bdata);

			/* Send the browse ID */
			mafw_dbus_send(conn, mafw_dbus_reply(msg,
					     MAFW_DBUS_UINT32(browse_id)));
		}
		
		/* Clean up. */
		g_strfreev((gchar **)metadata_keys);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg,
					   MAFW_SOURCE_METHOD_CANCEL_BROWSE)) {
		guint browse_id;
		GError *error = NULL;

		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &browse_id);
		mafw_source_cancel_browse(source, browse_id, &error);
		g_hash_table_remove(browse_requests,
				    GUINT_TO_POINTER(browse_id));
		mafw_dbus_ack_or_error(conn, msg, error);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg,
					   MAFW_SOURCE_METHOD_GET_METADATA)) {
		const gchar *objectid;
		const gchar **mkeys;
		MafwDBusOpCompletedInfo *oci;

		mafw_dbus_parse(msg,
				DBUS_TYPE_STRING, &objectid,
				MAFW_DBUS_TYPE_STRVZ, &mkeys);
		oci = mafw_dbus_oci_new(conn, msg);
		/* TODO: Remove error (NULL) from MafwSource API */
		mafw_source_get_metadata(source, objectid, mkeys,
					 got_metadata, oci);
		g_strfreev((gchar **)mkeys);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_has_member(msg,
					   MAFW_SOURCE_METHOD_GET_METADATAS)) {
		const gchar **objectids;
		const gchar **mkeys;
		MafwDBusOpCompletedInfo *oci;

		mafw_dbus_parse(msg,
				MAFW_DBUS_TYPE_STRVZ, &objectids,
				MAFW_DBUS_TYPE_STRVZ, &mkeys);
		oci = mafw_dbus_oci_new(conn, msg);
		/* TODO: Remove error (NULL) from MafwSource API */
		mafw_source_get_metadatas(source, objectids, mkeys,
					 got_metadatas, oci);
		g_strfreev((gchar **)mkeys);
		g_strfreev((gchar **)objectids);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_has_member(msg,
					   MAFW_SOURCE_METHOD_SET_METADATA)) {
		const gchar *object_id;
		GHashTable *metadata;
		MafwDBusOpCompletedInfo *oci;

		mafw_dbus_parse(msg, DBUS_TYPE_STRING, &object_id,
				MAFW_DBUS_TYPE_METADATA, &metadata);

		oci = mafw_dbus_oci_new(conn, msg);
		/* TODO: Remove error (NULL) from MafwSource API */
		mafw_source_set_metadata(source, object_id, metadata,
					 metadata_set, oci);
		mafw_metadata_release(metadata);

		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg,
					   MAFW_SOURCE_METHOD_CREATE_OBJECT)) {
		const gchar *parent;
		GHashTable *metadata;
		MafwDBusOpCompletedInfo *oci;

		mafw_dbus_parse(msg,
				DBUS_TYPE_STRING, &parent,
				MAFW_DBUS_TYPE_METADATA, &metadata);

		oci = mafw_dbus_oci_new(conn, msg);
		/* TODO: Remove error (NULL) from MafwSource API */
		mafw_source_create_object(source, parent, metadata,
					  object_created, oci);
		mafw_metadata_release(metadata);

		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg,
					   MAFW_SOURCE_METHOD_DESTROY_OBJECT)) {
		const gchar *objectid;
		MafwDBusOpCompletedInfo *oci;

		mafw_dbus_parse(msg, DBUS_TYPE_STRING, &objectid);
		oci = mafw_dbus_oci_new(conn, msg);
		/* TODO: Remove error (NULL) from MafwSource API */
		mafw_source_destroy_object(source, objectid, object_destroyed,
					   oci);

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void container_changed(MafwSource *source, const gchar *object_id,
				gpointer userdata)
{
	ExportedComponent *ecomp;
	
	ecomp = (ExportedComponent *) userdata;
	mafw_dbus_send(ecomp->connection,
			mafw_dbus_signal_full(
					NULL,
					ecomp->object_path,
					MAFW_SOURCE_INTERFACE,
					MAFW_SOURCE_SIGNAL_CONTAINER_CHANGED,
					MAFW_DBUS_STRING(object_id)));
}

static void metadata_changed(MafwSource *source, const gchar *object_id,
				gpointer userdata)
{
	ExportedComponent *ecomp;
	
	ecomp = (ExportedComponent *) userdata;
	mafw_dbus_send(ecomp->connection,
			mafw_dbus_signal_full(
					NULL,
					ecomp->object_path,
					MAFW_SOURCE_INTERFACE,
					MAFW_SOURCE_SIGNAL_METADATA_CHANGED,
					MAFW_DBUS_STRING(object_id)));

}

void connect_to_source_signals(gpointer ecomp)
{
	connect_signal(ecomp, "metadata-changed", metadata_changed);
	connect_signal(ecomp, "container-changed", container_changed);
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
