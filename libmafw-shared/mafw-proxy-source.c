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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libmafw/mafw.h>

#include "common/mafw-util.h"
#include "common/mafw-dbus.h"
#include <libmafw/mafw-metadata-serializer.h>
#include "common/dbus-interface.h"
#include "mafw-proxy-source.h"
#include "mafw-proxy-extension.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-proxy-source"

#define MAFW_DBUS_PATH MAFW_OBJECT
#define MAFW_DBUS_INTERFACE MAFW_SOURCE_INTERFACE

#define MAFW_PROXY_SOURCE_GET_PRIVATE(o)			\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o),			\
				      MAFW_TYPE_PROXY_SOURCE,	\
				      MafwProxySourcePrivate))

typedef struct _MafwProxySourceBrowseReq MafwProxySourceBrowseReq;
typedef struct _MafwProxySourceMetadataReq MafwProxySourceMetadataReq;

/* Communication area between DBusPendingCall issuers and callbacks. */
typedef struct {
	MafwSource *src;

	union {
		gpointer cb;
		MafwSourceMetadataResultCb got_metadata_cb;
		MafwSourceMetadataResultsCb got_metadatas_cb;
		MafwSourceObjectCreatedCb object_created_cb;
		MafwSourceObjectDestroyedCb object_destroyed_cb;
		MafwSourceMetadataSetCb metadata_set_cb;
	};
	gpointer cbdata;

	union {
		char *objectid;
	};
} RequestReplyInfo;

struct _MafwProxySourceMetadataReq {
	MafwSourceMetadataResultCb metadata_cb;
	gpointer user_data;
};

struct _MafwProxySourceBrowseReq {
	MafwSourceBrowseResultCb browse_cb;
	gpointer user_data;
};

struct _MafwProxySourcePrivate {
	GHashTable *browse_requests;
};

static DBusConnection *connection;

static DBusHandlerResult handle_container_changed_signal(MafwProxySource *self,
							 DBusMessage *msg)
{
	gchar *obj_id = NULL;

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_STRING, &obj_id);
	g_signal_emit_by_name(self, "container-changed", obj_id);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_metadata_changed_signal(MafwProxySource *self,
							 DBusMessage *msg)
{
	gchar *obj_id = NULL;

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_STRING, &obj_id);
	g_signal_emit_by_name(self, "metadata-changed", obj_id);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_updating_signal(MafwProxySource *self,
                                                DBusMessage *msg)
{
	gint progress;

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_INT32, &progress);
	g_signal_emit_by_name(self, "updating", progress);

	return DBUS_HANDLER_RESULT_HANDLED;
}


/**
 * SECTION:mafw-proxy-source
 *
 * #MafwProxySource is ...  `not documented', for example.
 * TODO
 */

static DBusHandlerResult
mafw_proxy_source_dispatch_message(DBusConnection *conn,
                                   DBusMessage *msg,
                                   MafwProxySource *self)
{
	MafwProxySourcePrivate *priv;
	gchar *domain_str;
	gint code;
	gchar *message;
	GError *error = NULL;

	g_assert(conn != NULL);
	g_assert(msg != NULL);

	priv = MAFW_PROXY_SOURCE_GET_PRIVATE(self);

	if (dbus_message_has_interface(msg, MAFW_EXTENSION_INTERFACE))
		return proxy_extension_dispatch(conn, msg, self);

	if (dbus_message_has_member(msg,
				    MAFW_PROXY_SOURCE_METHOD_BROWSE_RESULT)) {
		MafwProxySourceBrowseReq *new_req = NULL;
		guint browse_id;
		gint  remaining_count;
		guint index;
		gchar *object_id;
		GHashTable *metadata;
		DBusMessageIter imsg, iary, istr;

		g_return_val_if_fail(
			priv->browse_requests != NULL,
			DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		dbus_message_iter_init(msg, &imsg);
		dbus_message_iter_get_basic(&imsg, &browse_id);
		dbus_message_iter_next(&imsg);
		g_assert(dbus_message_iter_get_arg_type(&imsg) ==
				DBUS_TYPE_ARRAY);
		dbus_message_iter_recurse(&imsg, &iary);

		while (dbus_message_iter_get_arg_type(&iary) !=
						DBUS_TYPE_INVALID)
		{
			dbus_message_iter_recurse(&iary, &istr);
			g_assert(dbus_message_iter_get_arg_type(&istr)
				== DBUS_TYPE_INT32);
			dbus_message_iter_get_basic(&istr,
						&remaining_count);
			dbus_message_iter_next(&istr);
			dbus_message_iter_get_basic(&istr, &index);
			dbus_message_iter_next(&istr);
			dbus_message_iter_get_basic(&istr, &object_id);
			dbus_message_iter_next(&istr);
			mafw_dbus_message_parse_metadata(&istr,
							&metadata);
			dbus_message_iter_next(&istr);
			dbus_message_iter_get_basic(&istr, &domain_str);
			dbus_message_iter_next(&istr);
			dbus_message_iter_get_basic(&istr, &code);
			dbus_message_iter_next(&istr);
			dbus_message_iter_get_basic(&istr, &message);
			if (domain_str && domain_str[0])
				g_set_error(&error,
					g_quark_from_string(domain_str),
					    code, "%s", message);
			dbus_message_iter_next(&iary);

			if (!new_req)
				new_req = g_hash_table_lookup(
					priv->browse_requests,
					GUINT_TO_POINTER(browse_id));

			if (new_req) {
				new_req->browse_cb(MAFW_SOURCE(self),
						   browse_id,
						   remaining_count,
						   index,
						   object_id[0] ?
							object_id :
							NULL,
						   metadata,
						   new_req->user_data,
						   error);
				if (remaining_count == 0 )
					g_hash_table_remove(
						priv->browse_requests,
						GUINT_TO_POINTER(browse_id));
			}
			else
			{
				g_clear_error(&error);
				mafw_metadata_release(metadata);
				break;
			}

			g_clear_error(&error);
			mafw_metadata_release(metadata);
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (!dbus_message_has_path(msg,
                                          proxy_extension_return_path(self))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_SOURCE_SIGNAL_METADATA_CHANGED)) {
		return handle_metadata_changed_signal(self, msg);
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_SOURCE_SIGNAL_CONTAINER_CHANGED)) {
		return handle_container_changed_signal(self, msg);
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_SOURCE_SIGNAL_UPDATING)) {
                return handle_updating_signal(self, msg);
        }
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*****************************************************************************
 * Methods
 *****************************************************************************/

/**
 * mafw_proxy_source_browse:
 * @self: a #MafwProxySource instance.
 * @object_id: the object id to browse.
 * @recursive: TRUE, if browsing should be recursive.
 * @filter: filter criteria as #MafwFilter.
 * @sort_criteria: sort criteria.
 * @metadata_keys: list of metadata keys to include in the results.
 * @skip_count: skip count.
 * @item_count: item count.
 * Returns: browse session id, or #MAFW_SOURCE_INVALID_BROWSE_ID in
 * case of error.
 *
 * Starts a browse session on the given source.
 */
static guint mafw_proxy_source_browse(MafwSource * self,
			       const gchar * object_id,
			       gboolean recursive, const MafwFilter* filter,
			       const gchar * sort_criteria,
			       const gchar * const * metadata_keys,
			       guint skip_count, guint item_count,
			       MafwSourceBrowseResultCb browse_cb,
			       gpointer user_data)
{
	MafwProxySource *proxy;
	MafwProxySourcePrivate *priv;
	MafwProxySourceBrowseReq *new_req;
	DBusMessage *reply;
	guint browse_id;
	GError *error = NULL;
	gchar *filter_string = NULL;

	/* Return invalid browse id if args are bogus. */
	g_return_val_if_fail(self != NULL, MAFW_SOURCE_INVALID_BROWSE_ID);
	proxy = MAFW_PROXY_SOURCE(self);
	priv = MAFW_PROXY_SOURCE_GET_PRIVATE(proxy);
	g_return_val_if_fail(connection != NULL,
			     MAFW_SOURCE_INVALID_BROWSE_ID);
	g_return_val_if_fail(object_id != NULL,
			     MAFW_SOURCE_INVALID_BROWSE_ID);
	g_return_val_if_fail(browse_cb != NULL,
			     MAFW_SOURCE_INVALID_BROWSE_ID);

	if (!priv->browse_requests) {
		priv->browse_requests = g_hash_table_new_full(NULL,
							      NULL,
							      NULL,
							      g_free);
	}

	/* Prepare arguments and call remote side. */
	if (!metadata_keys)
		metadata_keys = MAFW_SOURCE_NO_KEYS;
	filter_string = mafw_filter_to_string(filter);
	reply = mafw_dbus_call(
		connection,
		mafw_dbus_method_full(proxy_extension_return_service(proxy),
				 proxy_extension_return_path(proxy),
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING(object_id),
				 MAFW_DBUS_BOOLEAN(recursive),
				 MAFW_DBUS_STRING(filter_string != NULL ?
						  filter_string : ""),
				 MAFW_DBUS_STRING(sort_criteria
						  ? sort_criteria : ""),
				 MAFW_DBUS_STRVZ(metadata_keys
						 ? metadata_keys
						 : MAFW_SOURCE_NO_KEYS),
				 MAFW_DBUS_UINT32(skip_count),
				 MAFW_DBUS_UINT32(item_count)),
		MAFW_SOURCE_ERROR, &error);
	g_free(filter_string);

	if (reply) {
		mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &browse_id);
		if (browse_id != MAFW_SOURCE_INVALID_BROWSE_ID)
		{
			/* Remember this new request. */
			new_req = g_new0(MafwProxySourceBrowseReq, 1);
			new_req->browse_cb = browse_cb;
			new_req->user_data = user_data;
			g_hash_table_replace(priv->browse_requests,
					     GUINT_TO_POINTER(browse_id),
					     new_req);
		}
		dbus_message_unref(reply);
	}
	else
	{
		browse_cb(self, MAFW_SOURCE_INVALID_BROWSE_ID, 0, 0, NULL, NULL,
					user_data, error);
		g_error_free(error);
		browse_id = MAFW_SOURCE_INVALID_BROWSE_ID;
	}

	return browse_id;
}

static gboolean mafw_proxy_source_cancel_browse(MafwSource * self,
                                                guint browse_id,
                                                GError **error)
{
	MafwProxySource *proxy;
	MafwProxySourcePrivate *priv;
	MafwProxySourceBrowseReq *req;

	proxy = MAFW_PROXY_SOURCE(self);
	priv = MAFW_PROXY_SOURCE_GET_PRIVATE(proxy);

	/* We couldn't possible have had any requests if .browse_requests
	 * does not exist, so $browse_id must be invalid this case. */
	if (priv->browse_requests == NULL) {
		g_set_error(error, MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_INVALID_BROWSE_ID,
			    "Browse id not found.");
		return FALSE;
	}

	req = g_hash_table_lookup(priv->browse_requests,
				  GUINT_TO_POINTER(browse_id));
	if (req != NULL) {
		DBusMessage *reply;
		/* The request is still in progress. */

		/* $req doesn't contain dynamically allocated data
		 * we care about. */
		g_hash_table_remove(priv->browse_requests,
				    GUINT_TO_POINTER(browse_id));

		/* Tell our mate to cancel. */
		reply = mafw_dbus_call(
                        connection,
                        mafw_dbus_method_full(
                                proxy_extension_return_service(proxy),
                                proxy_extension_return_path(proxy),
                                MAFW_SOURCE_INTERFACE,
                                MAFW_SOURCE_METHOD_CANCEL_BROWSE,
                                MAFW_DBUS_UINT32(browse_id)),
                        MAFW_SOURCE_ERROR, error);
		if (reply) dbus_message_unref(reply);
		else return FALSE;
	} else {
		g_set_error(error, MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_INVALID_BROWSE_ID,
			    "Browse id not found.");
		return FALSE;
	}

	return TRUE;
}

/* General async D-BUS request-reply processing. */
/* Unless $pendelum is NULL returns a new RequestReplyInfo set up with
 * $self, $cb and $cbdata.  Otherwise sets an appropriate error in *$errp. */
static RequestReplyInfo *new_request_reply_info(DBusPendingCall *pendelum,
		MafwSource *self, gpointer cb, gpointer cbdata)
{
	RequestReplyInfo *rri;

	if (!pendelum) {


		return NULL;
	}

	/* We can't set up $pendelum right here because the caller may need
	 * to add more details to $rri, but mockbus tends to fire the callback
	 * at the moment it's set. */
	rri = g_new0(RequestReplyInfo, 1);
	rri->src	= g_object_ref(self);
	rri->cb		= cb;
	rri->cbdata	= cbdata;
	return rri;
}

/* DBusPendingCallback cbdata free()ing function.  Note that it only does
 * free() the common portion of $info, but nothing from the union at the
 * end of the structure. */
static void free_request_reply_info(RequestReplyInfo *info)
{
	g_object_unref(info->src);
	g_free(info);
}

static void free_reply_info_and_oid(RequestReplyInfo *info)
{
	g_free(info->objectid);
	free_request_reply_info(info);
}

/* MafwSource::get_metadata */
static void got_metadata(DBusPendingCall *pendelum, RequestReplyInfo *info)
{
	GError *error;
	DBusMessage *reply;

	reply = dbus_pending_call_steal_reply(pendelum);
	if (!(error = mafw_dbus_is_error(reply, MAFW_SOURCE_ERROR))) {
		GHashTable *metadata;

		g_assert(dbus_message_get_type(reply) ==
                         DBUS_MESSAGE_TYPE_METHOD_RETURN);
		mafw_dbus_parse(reply,
				MAFW_DBUS_TYPE_METADATA, &metadata);
		info->got_metadata_cb(info->src,
				      info->objectid, metadata,
				      info->cbdata, NULL);
		mafw_metadata_release(metadata);
	} else {
		info->got_metadata_cb(info->src,
				      info->objectid, NULL,
				      info->cbdata, error);
		g_error_free(error);
	}
	dbus_message_unref(reply);
	dbus_pending_call_unref(pendelum);
}

static void mafw_proxy_source_get_metadata(MafwSource *self,
                                           const gchar *object_id,
                                           const gchar *const *metadata_keys,
                                           MafwSourceMetadataResultCb cb,
                                           gpointer cbdata)
{
	MafwProxySource *proxy;
	DBusPendingCall *pendelum = NULL;
	RequestReplyInfo *rri;

	/* We consider calling this with metadata_keys==NULL an error. */
	g_assert(metadata_keys);
	g_return_if_fail(cb);

	proxy = MAFW_PROXY_SOURCE(self);

	mafw_dbus_send_async(
                connection, &pendelum,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_SOURCE_INTERFACE,
                                      MAFW_SOURCE_METHOD_GET_METADATA,
                                      MAFW_DBUS_STRING(object_id),
                                      MAFW_DBUS_STRVZ(metadata_keys)));
	if (!pendelum)
	{
		GError *errp = NULL;
		g_set_error(&errp, MAFW_SOURCE_ERROR,
			    MAFW_EXTENSION_ERROR_EXTENSION_NOT_AVAILABLE,
			    "Source disconnected.");
		cb(self, object_id, NULL, cbdata, errp);
		g_error_free(errp);
		return;
	}

	rri = new_request_reply_info(pendelum, self, cb, cbdata);
	rri->objectid = g_strdup(object_id);
	dbus_pending_call_set_notify(pendelum,
				     (gpointer)got_metadata,
				     rri, (gpointer)free_reply_info_and_oid);
}

/* MafwSource::get_metadatas */
static void got_metadatas(DBusPendingCall *pendelum, RequestReplyInfo *info)
{
	GError *error;
	DBusMessage *msg;

	msg = dbus_pending_call_steal_reply(pendelum);
	if (!(error = mafw_dbus_is_error(msg, MAFW_SOURCE_ERROR))) {
		gchar *object_id;
		GHashTable *cur_metadata, *metadatas = NULL;
		DBusMessageIter imsg, iary, istr;
		GError *error = NULL;
		gchar *domain_str;
		gint code;
		gchar *message;

		dbus_message_iter_init(msg, &imsg);
		if (dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_ARRAY)
		{
			dbus_message_iter_recurse(&imsg, &iary);
			metadatas =
                                g_hash_table_new_full(
                                        g_str_hash,
                                        g_str_equal,
                                        (GDestroyNotify)g_free,
                                        (GDestroyNotify)mafw_metadata_release);

			while (dbus_message_iter_get_arg_type(&iary) !=
						DBUS_TYPE_INVALID)
			{
				dbus_message_iter_recurse(&iary, &istr);
				dbus_message_iter_get_basic(&istr, &object_id);
				dbus_message_iter_next(&istr);
				mafw_dbus_message_parse_metadata(&istr,
							&cur_metadata);
				g_hash_table_insert(metadatas,
                                                    g_strdup(object_id),
                                                    cur_metadata);
				dbus_message_iter_next(&iary);
			}
			dbus_message_iter_next(&imsg);
		}
		dbus_message_iter_get_basic(&imsg, &domain_str);
		dbus_message_iter_next(&imsg);
		dbus_message_iter_get_basic(&imsg, &code);
		dbus_message_iter_next(&imsg);
		dbus_message_iter_get_basic(&imsg, &message);
		if (domain_str && domain_str[0])
				g_set_error(&error,
					g_quark_from_string(domain_str),
					    code, "%s", message);

		info->got_metadatas_cb(info->src,
				      metadatas,
				      info->cbdata, error);
		if (metadatas)
			g_hash_table_unref(metadatas);
	} else {
		info->got_metadatas_cb(info->src,
				      NULL,
				      info->cbdata, error);
		g_error_free(error);
	}
	dbus_message_unref(msg);
	dbus_pending_call_unref(pendelum);
}

static void
mafw_proxy_source_get_metadatas(MafwSource *self,
                                const gchar **object_ids,
                                const gchar *const *metadata_keys,
                                MafwSourceMetadataResultsCb cb,
                                gpointer cbdata)
{
	MafwProxySource *proxy;
	DBusPendingCall *pendelum = NULL;
	RequestReplyInfo *rri;

	/* We consider calling this with metadata_keys==NULL an error. */
	g_assert(metadata_keys);
	g_return_if_fail(cb);

	proxy = MAFW_PROXY_SOURCE(self);

	mafw_dbus_send_async(
                connection, &pendelum,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_SOURCE_INTERFACE,
                                      MAFW_SOURCE_METHOD_GET_METADATAS,
                                      MAFW_DBUS_STRVZ(object_ids),
                                      MAFW_DBUS_STRVZ(metadata_keys)));
	if (!pendelum)
	{
		GError *errp = NULL;
		g_set_error(&errp, MAFW_SOURCE_ERROR,
			    MAFW_EXTENSION_ERROR_EXTENSION_NOT_AVAILABLE,
			    "Source disconnected.");
		cb(self, NULL, cbdata, errp);
		g_error_free(errp);
		return;
	}

	rri = new_request_reply_info(pendelum, self, cb, cbdata);
	dbus_pending_call_set_notify(pendelum,
				     (gpointer)got_metadatas,
				     rri, (gpointer)free_reply_info_and_oid);
}


/* MafwSource::create_object() */
static void object_created(DBusPendingCall *pendelum, RequestReplyInfo *info)
{
	GError *error;
	DBusMessage *reply;

	reply = dbus_pending_call_steal_reply(pendelum);
	if (!(error = mafw_dbus_is_error(reply, MAFW_SOURCE_ERROR))) {
		const gchar *objectid;

		g_assert(dbus_message_get_type(reply) ==
                         DBUS_MESSAGE_TYPE_METHOD_RETURN);
		mafw_dbus_parse(reply, DBUS_TYPE_STRING, &objectid);
		if (info->object_created_cb)
			info->object_created_cb(info->src, objectid,
                                                info->cbdata,
                                                NULL);
	} else {
		if (info->object_created_cb)
			info->object_created_cb(info->src, NULL, info->cbdata,
                                                error);
		g_error_free(error);
	}
	dbus_message_unref(reply);
	dbus_pending_call_unref(pendelum);
}

static void mafw_proxy_source_create_object(MafwSource *self,
					 const gchar *parent,
					 GHashTable *metadata,
					 MafwSourceObjectCreatedCb cb,
					 gpointer cbdata)
{
	MafwProxySource *proxy;
	DBusPendingCall *pendelum = NULL;
	RequestReplyInfo *rri;

	proxy = MAFW_PROXY_SOURCE(self);

	mafw_dbus_send_async(
                connection, &pendelum,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_SOURCE_INTERFACE,
                                      MAFW_SOURCE_METHOD_CREATE_OBJECT,
                                      MAFW_DBUS_STRING(parent),
                                      MAFW_DBUS_METADATA(metadata)));
	if (!pendelum)
	{
		if (cb)
		{
			GError *errp = NULL;
			g_set_error(
                                &errp, MAFW_SOURCE_ERROR,
                                MAFW_EXTENSION_ERROR_EXTENSION_NOT_AVAILABLE,
                                "Source disconnected.");
			cb(self, parent, cbdata, errp);
			g_error_free(errp);
		}
		return;
	}

	rri = new_request_reply_info(pendelum, self, cb, cbdata);

	dbus_pending_call_set_notify(pendelum,
				(DBusPendingCallNotifyFunction)object_created,
				rri, (DBusFreeFunction)free_request_reply_info);
}

/* MafwSource::destroy_object() */

/* Don't be bothered by the fact that this function is almost exactly
 * the same as object_created().  The little difference is that it
 * gives the callback the objectid on error too. */
static void object_destroyed(DBusPendingCall *pendelum, RequestReplyInfo *info)
{
	GError *error;
	DBusMessage *reply;

	reply = dbus_pending_call_steal_reply(pendelum);
	if (!(error = mafw_dbus_is_error(reply, MAFW_SOURCE_ERROR))) {
		g_assert(dbus_message_get_type(reply) ==
                         DBUS_MESSAGE_TYPE_METHOD_RETURN);
		if (info->object_destroyed_cb)
			info->object_destroyed_cb(info->src,
					  info->objectid,
					  info->cbdata, NULL);
	} else {
		if (info->object_destroyed_cb)
			info->object_destroyed_cb(info->src,
					  info->objectid,
					  info->cbdata, error);
		g_error_free(error);
	}
	dbus_message_unref(reply);
	dbus_pending_call_unref(pendelum);
}

static void mafw_proxy_source_destroy_object(MafwSource *self,
					  const gchar *objectid,
					  MafwSourceObjectDestroyedCb cb,
					  gpointer cbdata)
{
	MafwProxySource *proxy;
	DBusPendingCall *pendelum = NULL;
	RequestReplyInfo *rri;

	proxy = MAFW_PROXY_SOURCE(self);

	mafw_dbus_send_async(
                connection, &pendelum,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_SOURCE_INTERFACE,
                                      MAFW_SOURCE_METHOD_DESTROY_OBJECT,
                                      MAFW_DBUS_STRING(objectid)));
	if (!pendelum)
	{
		if (cb)
		{
			GError *errp = NULL;
			g_set_error(
                                &errp, MAFW_SOURCE_ERROR,
                                MAFW_EXTENSION_ERROR_EXTENSION_NOT_AVAILABLE,
                                "Source disconnected.");
			cb(self, objectid, cbdata, errp);
			g_error_free(errp);
		}
		return;
	}

	rri = new_request_reply_info(pendelum, self, cb, cbdata);

	rri->objectid = g_strdup(objectid);
	dbus_pending_call_set_notify(pendelum,
			(DBusPendingCallNotifyFunction)object_destroyed,
			rri, (DBusFreeFunction)free_reply_info_and_oid);
}

static void metadata_set(DBusPendingCall *pendelum, RequestReplyInfo *info)
{
	GError *error;
	DBusMessage *reply;
	const gchar **failed_keys;
	gint n_elements;

	reply = dbus_pending_call_steal_reply(pendelum);
	if (!(error = mafw_dbus_is_error(reply, MAFW_SOURCE_ERROR))) {
		const gchar *objectid;
		gchar *domain_str = NULL;
		gint code;
		gchar *message = NULL;
		GError *error_in_keys = NULL;

		g_assert(dbus_message_get_type(reply) ==
                         DBUS_MESSAGE_TYPE_METHOD_RETURN);
		if (mafw_dbus_count_args(reply) == 2) {
			/* No errors */
			mafw_dbus_parse(reply, DBUS_TYPE_STRING, &objectid,
					DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
					&failed_keys, &n_elements);
			if (info->metadata_set_cb)
				info->metadata_set_cb(info->src, objectid,
                                                      failed_keys,
                                                      info->cbdata, NULL);
		} else {
			/* Error included in message */
			mafw_dbus_parse(reply, DBUS_TYPE_STRING, &objectid,
					MAFW_DBUS_TYPE_STRVZ, &failed_keys,
					DBUS_TYPE_STRING, &domain_str,
					DBUS_TYPE_INT32, &code,
					DBUS_TYPE_STRING, &message);

			g_set_error(&error_in_keys,
				    g_quark_from_string(domain_str),
				    code, "%s", message);
			if (info->metadata_set_cb)
				info->metadata_set_cb(info->src, objectid,
                                                      failed_keys,
                                                      info->cbdata,
                                                      error_in_keys);
			g_error_free(error_in_keys);
		}
		g_strfreev((gchar **)failed_keys);
	} else {
		if (info->metadata_set_cb)
			info->metadata_set_cb(info->src, NULL, NULL,
                                              info->cbdata,
                                              error);
		g_error_free(error);
	}
	dbus_message_unref(reply);
	dbus_pending_call_unref(pendelum);
}

static void mafw_proxy_source_set_metadata(MafwSource *self,
					const gchar *object_id,
					GHashTable *metadata,
					MafwSourceMetadataSetCb cb,
					gpointer cbdata)
{
	MafwProxySource *proxy;
	DBusPendingCall *pendelum;
	RequestReplyInfo *rri;

	proxy = MAFW_PROXY_SOURCE(self);

	mafw_dbus_send_async(
                connection, &pendelum,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_SOURCE_INTERFACE,
                                      MAFW_SOURCE_METHOD_SET_METADATA,
                                      MAFW_DBUS_STRING(object_id),
                                      MAFW_DBUS_METADATA(metadata)));
	if (!pendelum)
	{
		if (cb)
		{
			GError *errp = NULL;
			g_set_error(
                                &errp, MAFW_SOURCE_ERROR,
                                MAFW_EXTENSION_ERROR_EXTENSION_NOT_AVAILABLE,
                                "Source disconnected.");
			cb(self, object_id, NULL, cbdata, errp);
			g_error_free(errp);
		}
		return;
	}

	rri = new_request_reply_info(pendelum, self, cb, cbdata);

	dbus_pending_call_set_notify(pendelum,
				(DBusPendingCallNotifyFunction)metadata_set,
				rri, (DBusFreeFunction)free_request_reply_info);
}

/* GObject necessities. */

G_DEFINE_TYPE(MafwProxySource, mafw_proxy_source, MAFW_TYPE_SOURCE);

static void mafw_proxy_source_dispose (GObject *obj)
{
	MafwProxySource *source_obj = MAFW_PROXY_SOURCE(obj);

	if (connection)
	{
		dbus_connection_unregister_object_path(connection,
				proxy_extension_return_path(source_obj));

		dbus_connection_unref(connection);
	}
	if (source_obj->priv->browse_requests)
		g_hash_table_destroy(source_obj->priv->browse_requests);
}


static void mafw_proxy_source_class_init(MafwProxySourceClass *klass)
{
	MafwSourceClass *source_class = MAFW_SOURCE_CLASS(klass);
	MafwExtensionClass *extension_class = MAFW_EXTENSION_CLASS(klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	extension_class->list_extension_properties =
                (gpointer)proxy_extension_list_properties;
	extension_class->get_extension_property =
                (gpointer)proxy_extension_get_extension_property;
	extension_class->set_extension_property =
                (gpointer)proxy_extension_set_extension_property;
	source_class->browse = mafw_proxy_source_browse;
	source_class->cancel_browse = mafw_proxy_source_cancel_browse;
	source_class->get_metadata = mafw_proxy_source_get_metadata;
	source_class->get_metadatas = mafw_proxy_source_get_metadatas;
	source_class->set_metadata = mafw_proxy_source_set_metadata;
	source_class->create_object = mafw_proxy_source_create_object;
	source_class->destroy_object = mafw_proxy_source_destroy_object;
	g_type_class_add_private(klass, sizeof(MafwProxySourcePrivate));

	gobject_class->dispose = mafw_proxy_source_dispose;
}

static void mafw_proxy_source_init(MafwProxySource *self)
{
	g_return_if_fail(MAFW_IS_PROXY_SOURCE(self));

	self->priv = MAFW_PROXY_SOURCE_GET_PRIVATE(self);
}

/**
 * mafw_proxy_source_new:
 *
 * Creates a new instance of #MafwProxySource.
 *
 * Returns: a new #MafwProxySource object.
 */
GObject *mafw_proxy_source_new(const gchar *uuid, const gchar *plugin,
                               MafwRegistry *registry)
{
	GObject *new_obj = g_object_new(MAFW_TYPE_PROXY_SOURCE,
					"uuid", uuid,
					"plugin", plugin,
					NULL);
	MafwProxySource *source_obj;
	DBusError err;
	gchar *match_str = NULL, *path = NULL;
	DBusObjectPathVTable path_vtable;

	memset(&path_vtable, 0, sizeof(DBusObjectPathVTable));
	path_vtable.message_function =
		(DBusObjectPathMessageFunction)mafw_proxy_source_dispatch_message;

	if (!new_obj)
		return NULL;

	source_obj = MAFW_PROXY_SOURCE(new_obj);


	connection = mafw_dbus_session(NULL);

	if (!connection) goto source_new_error;

	dbus_error_init(&err);

	path = g_strdup_printf(MAFW_SOURCE_OBJECT "/%s", uuid);

	match_str = g_strdup_printf(MAFW_EXTENSION_MATCH, MAFW_SOURCE_INTERFACE,
					path);
	dbus_bus_add_match(connection, match_str, &err);
	g_free(match_str);
	if (dbus_error_is_set(&err)) goto source_new_error;

	if (!dbus_connection_register_object_path(connection,
			path,
			&path_vtable,
			source_obj))
		goto source_new_error;

	g_free(path);
	/* It is harmless to setup the same context multiple times.
	 * On the other hand it is required if someone just calls
	 * mafw_proxy_source_new() without mafw_dbus_discover_init()
	 * for testing purposes. */
	dbus_connection_setup_with_g_main(connection, NULL);

	proxy_extension_attach(G_OBJECT(new_obj), connection, plugin, registry);

	return new_obj;

source_new_error:

	g_free(path);
	if (connection)
		dbus_connection_unref(connection);

	g_object_unref(new_obj);
	return NULL;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
