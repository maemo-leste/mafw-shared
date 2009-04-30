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
#include <stdio.h>

#include <libmafw/mafw.h>
#include "common/dbus-interface.h"
#include "common/mafw-util.h"
#include "common/mafw-dbus.h"
#include "mafw-proxy-renderer.h"
#include "mafw-proxy-extension.h"
#include "mafw-proxy-playlist.h"
#include "mafw-playlist-manager.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-proxy-renderer"

#define MAFW_DBUS_PATH MAFW_OBJECT
#define MAFW_DBUS_INTERFACE MAFW_RENDERER_INTERFACE

static DBusConnection *connection;

/**
 * mafw_proxy_renderer_handle_signal_state_changed:
 * @self: a #MafwProxyRenderer instance.
 * @msg: the DBus message
 *
 * Handles the received DBus signal "state_changed", i.e.
 * parses the message and emits a corresponding g_signal
 */
static void
mafw_proxy_renderer_handle_signal_state_changed(MafwProxyRenderer *self,
                                                DBusMessage *msg)
{
	MafwPlayState state;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_INT32, &state);

	g_signal_emit_by_name(self, "state_changed",
			      state);
}
/**
 * mafw_proxy_renderer_handle_signal_playlist_changed:
 * @self: a #MafwProxyRenderer instance.
 * @msg: the DBus message
 *
 * Handles the received DBus signal "playlist_changed", i.e.
 * parses the message and emits a corresponding g_signal
 */
static void
mafw_proxy_renderer_handle_signal_playlist_changed(MafwProxyRenderer *self,
                                                               DBusMessage *msg)
{
	guint playlist_id;
	MafwProxyPlaylist *playlist;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_UINT32, &playlist_id);
	playlist = playlist_id != MAFW_PROXY_PLAYLIST_INVALID_ID
		? mafw_playlist_manager_get_playlist(
			mafw_playlist_manager_get(), playlist_id, NULL)
		: NULL;

	g_signal_emit_by_name(self, "playlist_changed",
			      MAFW_PLAYLIST(playlist));
	if (playlist)
		g_object_unref(playlist);
}
/**
 * mafw_proxy_renderer_handle_signal_media_changed:
 * @self: a #MafwProxyRenderer instance.
 * @msg: the DBus message
 *
 * Handles the received DBus signal "media_changed", i.e.
 * parses the message and emits a corresponding g_signal
 */
static void
mafw_proxy_renderer_handle_signal_media_changed(MafwProxyRenderer *self,
                                                DBusMessage *msg)
{
	guint index;
	gchar *object_id;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_INT32, &index,
			DBUS_TYPE_STRING, &object_id);

	if (object_id[0] == '\0')
		object_id = NULL;
	g_signal_emit_by_name(self, "media_changed",
			      index,
			      object_id);
}


/**
 * mafw_proxy_renderer_handle_signal_buffering_info:
 * @self: a #MafwProxyRenderer instance.
 * @msg: the DBus message
 *
 * Handles the received DBus signal "buffering_info", i.e.
 * parses the message and emits a corresponding g_signal
 */
static void
mafw_proxy_renderer_handle_signal_buffering_info(MafwProxyRenderer *self,
                                                 DBusMessage *msg)
{
	gdouble status;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_DOUBLE, &status);
	g_signal_emit_by_name(self, "buffering_info", (gfloat)status);
}

static void
mafw_proxy_renderer_handle_signal_metadata_changed(MafwProxyRenderer *self,
                                                   DBusMessage *msg)
{
	const gchar *key;
	GValueArray *values;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	mafw_dbus_parse(msg,
			DBUS_TYPE_STRING, &key,
			MAFW_DBUS_TYPE_GVALUEARRAY, &values);

	g_signal_emit_by_name(self, "metadata-changed", key, values);
	g_value_array_free(values);
}


/**
 * mafw_proxy_renderer_dispatch_message:
 * @conn: pointer to a private DBus connection
 * @msg: the DBus message
 * @data: User data
 *
 * Handles the received DBus messages
 * Returns: Whether the message was handled or not
 */
static DBusHandlerResult
mafw_proxy_renderer_dispatch_message(DBusConnection *conn,
                                     DBusMessage *msg,
                                     gpointer data)
{
	MafwProxyRenderer *self;

	g_assert(conn != NULL);
	g_assert(msg != NULL);

	self = MAFW_PROXY_RENDERER(data);

	if (dbus_message_has_interface(msg, MAFW_EXTENSION_INTERFACE))
		return proxy_extension_dispatch(conn, msg, self);

	/* Dispatch message. */
	if (mafw_dbus_is_signal(msg, MAFW_RENDERER_SIGNAL_STATE_CHANGED)) {
		mafw_proxy_renderer_handle_signal_state_changed(self, msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED)) {
		mafw_proxy_renderer_handle_signal_playlist_changed(self, msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_RENDERER_SIGNAL_ITEM_CHANGED)) {
		mafw_proxy_renderer_handle_signal_media_changed(self, msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_RENDERER_SIGNAL_BUFFERING_INFO)) {
		mafw_proxy_renderer_handle_signal_buffering_info(self, msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (mafw_dbus_is_signal(msg,
                                       MAFW_RENDERER_SIGNAL_METADATA_CHANGED)) {
		mafw_proxy_renderer_handle_signal_metadata_changed(self, msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


/*----------------------------------------------------------------------------
  Param struct for asynchronous renderer callbacks
  ----------------------------------------------------------------------------*/

typedef struct _AsyncParams {
	MafwRenderer* renderer;
	gpointer callback;
	gpointer user_data;
} AsyncParams;

static void mafw_proxy_renderer_async_free(gpointer data)
{
	AsyncParams *params = (AsyncParams*) data;

	if (params != NULL) {
		g_object_unref(params->renderer);
		g_free(params);
	}
}

/*----------------------------------------------------------------------------
  Generic "playback" pending return handler
  ----------------------------------------------------------------------------*/

/**
 * mafw_proxy_renderer_playback_cb:
 * @pending_call: A DBusPendingCall that is the reply for a playback call
 * @user_data:    AsyncParams*
 *
 * Receives the resulting DBus reply message for a playback call and calls
 * the assigned callback function to pass the actual result to the UI layer.
 */
static void mafw_proxy_renderer_playback_cb(DBusPendingCall *pending_call,
					 gpointer user_data)
{
	AsyncParams *ap;
	DBusMessage *reply;
	GError *error;

	ap = (AsyncParams*) user_data;
	g_assert(ap != NULL);

	reply = dbus_pending_call_steal_reply(pending_call);
	error = mafw_dbus_is_error(reply, MAFW_RENDERER_ERROR);

	if (error == NULL) {
		if (ap->callback)
			((MafwRendererPlaybackCB) ap->callback)
				(ap->renderer, ap->user_data, NULL);
	} else {
		if (ap->callback)
			((MafwRendererPlaybackCB) ap->callback)
				(ap->renderer, ap->user_data, error);
		g_error_free(error);
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending_call);
}

/*----------------------------------------------------------------------------
  Playback
  ----------------------------------------------------------------------------*/

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_play(MafwRenderer *self,
                                     MafwRendererPlaybackCB callback,
                                     gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);

	proxy = MAFW_PROXY_RENDERER(self);
	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_PLAY));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_play_object(MafwRenderer *self,
                                            const gchar* object_id,
                                            MafwRendererPlaybackCB callback,
                                            gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);

	proxy = MAFW_PROXY_RENDERER(self);
	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_PLAY_OBJECT,
                                      MAFW_DBUS_STRING(object_id)));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_play_uri(MafwRenderer *self, const gchar* uri,
				      MafwRendererPlaybackCB callback,
				      gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);

	proxy = MAFW_PROXY_RENDERER(self);
	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_PLAY_URI,
                                      MAFW_DBUS_STRING(uri)));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_stop(MafwRenderer *self,
                                     MafwRendererPlaybackCB callback,
                                     gpointer user_data)
{
	AsyncParams* ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);

	proxy = MAFW_PROXY_RENDERER(self);
	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_STOP));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_pause(MafwRenderer *self,
                                      MafwRendererPlaybackCB callback,
                                      gpointer user_data)
{
	AsyncParams* ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);

	proxy = MAFW_PROXY_RENDERER(self);
	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_PAUSE));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_resume(MafwRenderer *self,
                                       MafwRendererPlaybackCB callback,
                                       gpointer user_data)
{
	AsyncParams* ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);

	proxy = MAFW_PROXY_RENDERER(self);
	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_RESUME));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/*----------------------------------------------------------------------------
  Get status
  ----------------------------------------------------------------------------*/

/**
 * mafw_proxy_renderer_get_status_cb:
 * @pending_call: A DBusPendingCall that is the reply for a get_status call
 * @user_data:    GetStatusParams*
 *
 * Receives the resulting DBus reply message for a get_status call and calls
 * the assigned callback function to pass the actual result to the UI layer.
 */
static void mafw_proxy_renderer_get_status_cb(DBusPendingCall *pending_call,
					   gpointer user_data)
{
	AsyncParams *params;
	DBusMessage *reply;
	GError *error;

	params = (AsyncParams*) user_data;
	g_assert(params != NULL);

	reply = dbus_pending_call_steal_reply(pending_call);
	error = mafw_dbus_is_error(reply, MAFW_RENDERER_ERROR);

	if (error == NULL) {

		guint playlist_id;
		MafwPlaylistManager* manager;
		MafwPlaylist *playlist;
		const gchar* object_id;
		guint index;
		MafwPlayState state;

		mafw_dbus_parse(reply,
				DBUS_TYPE_UINT32, &playlist_id,
				DBUS_TYPE_UINT32, &index,
				DBUS_TYPE_INT32,  &state,
				DBUS_TYPE_STRING, &object_id);

		if (object_id && object_id[0] == '\0')
			object_id = NULL;

		if (playlist_id != MAFW_PROXY_PLAYLIST_INVALID_ID) {

			manager = mafw_playlist_manager_get();
			g_assert(manager != NULL);

			playlist = MAFW_PLAYLIST(
				mafw_playlist_manager_get_playlist(
					manager, playlist_id, NULL));
		} else {
			playlist = NULL;
		}

		((MafwRendererStatusCB) (params->callback)) (params->renderer,
							 playlist, index,
							 state, object_id,
							 params->user_data,
							 NULL);
	} else {
		if (params->callback)
			((MafwRendererStatusCB) (params->callback))(params->renderer,
                                                                    NULL, 0,
                                                                    0, NULL,
                                                                    params->user_data,
                                                                    error);
		g_error_free(error);
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending_call);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_get_status(MafwRenderer *self,
					MafwRendererStatusCB callback,
					gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);
	proxy = MAFW_PROXY_RENDERER(self);

	g_return_if_fail(connection != NULL);
	g_return_if_fail(callback != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_GET_STATUS));

	dbus_pending_call_set_notify(pending_call,
				     mafw_proxy_renderer_get_status_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/*----------------------------------------------------------------------------
  Playlist
  ----------------------------------------------------------------------------*/

/**
 * See #MafwRenderer for a description.
 */
static gboolean mafw_proxy_renderer_assign_playlist(MafwRenderer *self,
						 MafwPlaylist *playlist,
						 GError **error)
{
	MafwProxyRenderer *proxy;
	guint pls_id;
	DBusMessage *reply;

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(connection != NULL, FALSE);

	if (playlist != NULL)
		g_return_val_if_fail(MAFW_IS_PROXY_PLAYLIST(playlist), FALSE);

	proxy = MAFW_PROXY_RENDERER(self);
	if (!playlist) {
		pls_id = 0;
	} else {
		/* No point in sending if its an in-core playlist.
		 * TODO this emits a g_critical, should it? */
		pls_id = mafw_proxy_playlist_get_id(
			MAFW_PROXY_PLAYLIST(playlist));
	}

	reply = mafw_dbus_call(connection,
				mafw_dbus_method_full(
					proxy_extension_return_service(proxy),
					proxy_extension_return_path(proxy),
					MAFW_RENDERER_INTERFACE,
					MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
					MAFW_DBUS_UINT32(pls_id)),
		MAFW_RENDERER_ERROR, error);

	if (reply) {
		dbus_message_unref(reply);
	 	return TRUE;
	}

	return FALSE;
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_next(MafwRenderer *self,
                                     MafwRendererPlaybackCB callback,
                                     gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);
	proxy = MAFW_PROXY_RENDERER(self);

	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_NEXT));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_previous(MafwRenderer *self,
				      MafwRendererPlaybackCB callback,
				      gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);
	proxy = MAFW_PROXY_RENDERER(self);

	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(connection, &pending_call,
			     mafw_dbus_method_full(
                                     proxy_extension_return_service(proxy),
                                     proxy_extension_return_path(proxy),
                                     MAFW_RENDERER_INTERFACE,
                                     MAFW_RENDERER_METHOD_PREVIOUS));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_goto_index(MafwRenderer *self, guint index,
					MafwRendererPlaybackCB callback,
					gpointer user_data)
{
	AsyncParams *ap;
	MafwProxyRenderer *proxy;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);
	proxy = MAFW_PROXY_RENDERER(self);

	g_return_if_fail(connection != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_GOTO_INDEX,
                                      MAFW_DBUS_UINT32(index)));

	dbus_pending_call_set_notify(pending_call,
                                     mafw_proxy_renderer_playback_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/*----------------------------------------------------------------------------
  Position
  ----------------------------------------------------------------------------*/

static void set_get_position_cb(DBusPendingCall *pending_call,
				gpointer user_data)
{
	AsyncParams *ap;
	DBusMessage *reply;
	GError *error;

	ap = (AsyncParams*) user_data;
	g_assert(ap != NULL);

	reply = dbus_pending_call_steal_reply(pending_call);
	error = mafw_dbus_is_error(reply, MAFW_RENDERER_ERROR);
	if (error == NULL) {

		guint seconds = 0;
		mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &seconds);
		if (ap->callback != NULL)
			((MafwRendererPositionCB) ap->callback)(ap->renderer,
							    seconds,
							    ap->user_data,
							    NULL);
	} else {

		if (ap->callback != NULL)
			((MafwRendererPositionCB) ap->callback)(ap->renderer,
							    0,
							    ap->user_data,
							    error);
		g_error_free(error);
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending_call);
}

/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_set_position(MafwRenderer *self,
                                             MafwRendererSeekMode mode,
                                             gint seconds,
                                             MafwRendererPositionCB callback,
                                             gpointer user_data)
{
	MafwProxyRenderer *proxy;
	AsyncParams *ap;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);
	proxy = MAFW_PROXY_RENDERER(self);

	g_return_if_fail(connection != NULL);
	g_return_if_fail(callback != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(connection, &pending_call,
			      mafw_dbus_method_full(
				      proxy_extension_return_service(proxy),
				      proxy_extension_return_path(proxy),
				      MAFW_RENDERER_INTERFACE,
				      MAFW_RENDERER_METHOD_SET_POSITION,
				      MAFW_DBUS_INT32(mode),
				      MAFW_DBUS_INT32(seconds)));

	dbus_pending_call_set_notify(pending_call, set_get_position_cb,
				     ap, mafw_proxy_renderer_async_free);
}


/**
 * See #MafwRenderer for a description.
 */
static void mafw_proxy_renderer_get_position(MafwRenderer *self,
					  MafwRendererPositionCB callback,
					  gpointer user_data)
{
	MafwProxyRenderer *proxy;
	AsyncParams *ap;
	DBusPendingCall *pending_call = NULL;

	g_return_if_fail(self != NULL);
	proxy = MAFW_PROXY_RENDERER(self);

	g_return_if_fail(connection != NULL);
	g_return_if_fail(callback != NULL);

	ap = g_new0(AsyncParams, 1);
	ap->renderer = g_object_ref(self);
	ap->callback = callback;
	ap->user_data = user_data;

	mafw_dbus_send_async(
                connection, &pending_call,
                mafw_dbus_method_full(proxy_extension_return_service(proxy),
                                      proxy_extension_return_path(proxy),
                                      MAFW_RENDERER_INTERFACE,
                                      MAFW_RENDERER_METHOD_GET_POSITION));

	dbus_pending_call_set_notify(pending_call, set_get_position_cb,
				     ap, mafw_proxy_renderer_async_free);
}

/*----------------------------------------------------------------------------
  GObject necessities
  ----------------------------------------------------------------------------*/

G_DEFINE_TYPE(MafwProxyRenderer, mafw_proxy_renderer, MAFW_TYPE_RENDERER);

static void mafw_proxy_renderer_dispose (GObject *obj);

static void mafw_proxy_renderer_class_init(MafwProxyRendererClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	MafwRendererClass *renderer_class = MAFW_RENDERER_CLASS(klass);
	MafwExtensionClass *extension_class = MAFW_EXTENSION_CLASS(klass);

	gobject_class->dispose = mafw_proxy_renderer_dispose;

	extension_class->list_extension_properties =
                (gpointer)proxy_extension_list_properties;
	extension_class->get_extension_property =
                (gpointer)proxy_extension_get_extension_property;
	extension_class->set_extension_property =
                (gpointer)proxy_extension_set_extension_property;

	/* Playback */

	renderer_class->play_uri = mafw_proxy_renderer_play_uri;
	renderer_class->play_object = mafw_proxy_renderer_play_object;
	renderer_class->play = mafw_proxy_renderer_play;
	renderer_class->stop = mafw_proxy_renderer_stop;
	renderer_class->pause = mafw_proxy_renderer_pause;
	renderer_class->resume = mafw_proxy_renderer_resume;
	renderer_class->get_status = mafw_proxy_renderer_get_status;

	/* Playlist */

	renderer_class->next = mafw_proxy_renderer_next;
	renderer_class->previous = mafw_proxy_renderer_previous;
	renderer_class->goto_index = mafw_proxy_renderer_goto_index;
	renderer_class->assign_playlist = mafw_proxy_renderer_assign_playlist;

	/* Position */

	renderer_class->set_position = mafw_proxy_renderer_set_position;
	renderer_class->get_position = mafw_proxy_renderer_get_position;
}

static void mafw_proxy_renderer_init(MafwProxyRenderer *self)
{
}

static void mafw_proxy_renderer_dispose (GObject *obj)
{
	MafwProxyRenderer *renderer_obj = MAFW_PROXY_RENDERER(obj);

	if (connection)
	{
		dbus_connection_unregister_object_path(connection,
				proxy_extension_return_path(renderer_obj));

		dbus_connection_unref(connection);
	}
}

/**
 * mafw_proxy_renderer_new:
 *
 * Creates a new instance of #MafwProxyRenderer.
 *
 * Returns: a new #MafwProxyRenderer object.
 */
GObject *mafw_proxy_renderer_new(const gchar *uuid, const gchar *plugin,
				MafwRegistry *registry)
{
	GObject *new_obj = g_object_new(MAFW_TYPE_PROXY_RENDERER,
					"uuid", uuid,
					"plugin", plugin,
					NULL);
	MafwProxyRenderer *renderer_obj;
	DBusError err;
	gchar *match_str = NULL, *path;
	DBusObjectPathVTable path_vtable;

	memset(&path_vtable, 0, sizeof(DBusObjectPathVTable));
	path_vtable.message_function =
		(DBusObjectPathMessageFunction)mafw_proxy_renderer_dispatch_message;


	if (!new_obj)
		return NULL;

	renderer_obj = MAFW_PROXY_RENDERER(new_obj);

	path = g_strdup_printf(MAFW_RENDERER_OBJECT "/%s", uuid);

	connection = mafw_dbus_session(NULL);

	if (!connection) goto renderer_new_error;

	dbus_error_init(&err);

	match_str = g_strdup_printf(MAFW_EXTENSION_MATCH,
                                    MAFW_RENDERER_INTERFACE,
                                    path);

	dbus_bus_add_match(connection, match_str, &err);

	g_free(match_str);

	if (dbus_error_is_set(&err)) goto renderer_new_error;

	if (!dbus_connection_register_object_path(connection,
			path,
			&path_vtable,
			renderer_obj))
		goto renderer_new_error;

	/* See mafw-proxy-source.c as to why setup_with_g_main() is here. */
	dbus_connection_setup_with_g_main(connection, NULL);
	proxy_extension_attach(G_OBJECT(new_obj), connection, plugin, registry);

	return new_obj;

renderer_new_error:

	if (connection)
		dbus_connection_unref(connection);

	g_object_unref(new_obj);
	return NULL;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
