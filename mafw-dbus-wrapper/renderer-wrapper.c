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
#include <glib.h>
#include <stdio.h>
#include <dbus/dbus.h>
#include <libmafw/mafw.h>
#include <libmafw/mafw-registry.h>
#include <libmafw-shared/mafw-proxy-playlist.h>
#include <libmafw-shared/mafw-playlist-manager.h>
#include <libmafw-shared/mafw-shared.h>
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "common/mafw-util.h"
#include "wrapper.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-renderer-wrapper"

#define MAFW_DBUS_PATH MAFW_OBJECT
#define MAFW_DBUS_INTERFACE MAFW_RENDERER_INTERFACE

struct buffering_data {
	guint buffering_info_id;
	gdouble last_state;
	ExportedComponent *ecomp;
};

/*----------------------------------------------------------------------------
  Playback operation success/failure callback
  ----------------------------------------------------------------------------*/

static void playback_cb(MafwRenderer *renderer, gpointer user_data, const GError *error)
{
	MafwDBusOpCompletedInfo* oci;

	oci = (MafwDBusOpCompletedInfo*) user_data;
	g_assert(oci != NULL);
	g_assert(oci->con != NULL);
	g_assert(oci->msg != NULL);

	if (error == NULL)
		mafw_dbus_send(oci->con, mafw_dbus_reply(oci->msg));
	else
		mafw_dbus_send(oci->con, mafw_dbus_gerror(oci->msg, error));

	mafw_dbus_oci_free(oci);
}

/*----------------------------------------------------------------------------
  Get status
  ----------------------------------------------------------------------------*/

static void get_status_cb(MafwRenderer *renderer, MafwPlaylist *playlist, guint index,
			  MafwPlayState state, const gchar* object_id,
			  gpointer user_data, const GError *error)
{
	MafwDBusOpCompletedInfo* oci;
	guint playlist_id;

	oci = (MafwDBusOpCompletedInfo*) user_data;
	g_assert(oci != NULL);
	g_assert(oci->con != NULL);
	g_assert(oci->msg != NULL);

	if (!error) {
		if (MAFW_IS_PROXY_PLAYLIST(playlist)) {
			playlist_id = mafw_proxy_playlist_get_id(
				 MAFW_PROXY_PLAYLIST(playlist));

		} else {

			playlist_id = MAFW_PROXY_PLAYLIST_INVALID_ID;
		}

		mafw_dbus_send(oci->con,
			       mafw_dbus_reply(oci->msg,
					       MAFW_DBUS_UINT32(playlist_id),
					       MAFW_DBUS_UINT32(index),
					       MAFW_DBUS_INT32(state),
					       MAFW_DBUS_STRING(
						object_id ? object_id : "")));
	} else
		mafw_dbus_send(oci->con, mafw_dbus_gerror(oci->msg, error));

	mafw_dbus_oci_free(oci);
}

/*----------------------------------------------------------------------------
  Get position
  ----------------------------------------------------------------------------*/

static void set_get_position_cb(MafwRenderer *renderer, gint seconds,
				gpointer user_data, const GError *error)
{
	MafwDBusOpCompletedInfo* oci;

	oci = (MafwDBusOpCompletedInfo*) user_data;
	g_assert(oci != NULL);
	g_assert(oci->con != NULL);
	g_assert(oci->msg != NULL);

	mafw_dbus_send(oci->con, error
		       ? mafw_dbus_gerror(oci->msg, error)
		       : mafw_dbus_reply(oci->msg, MAFW_DBUS_UINT32(seconds)));
	mafw_dbus_oci_free(oci);
}

/*----------------------------------------------------------------------------
  Dispatch incoming renderer messages.
  ----------------------------------------------------------------------------*/

DBusHandlerResult handle_renderer_msg(DBusConnection *conn,
				      DBusMessage *msg, void *data)
{
	ExportedComponent *ecomp;
	MafwRenderer *renderer;
	ecomp = (ExportedComponent *)data;
	renderer = MAFW_RENDERER(ecomp->comp);

	if (dbus_message_has_interface(msg, MAFW_EXTENSION_INTERFACE))
		return handle_extension_msg(conn, msg, data);

	/* Dispatch based on member. */
	/* TODO: handle the error wrapping in all these cases. */
	if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_PLAY)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_play(renderer, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_PLAY_OBJECT)) {

		MafwDBusOpCompletedInfo *oci;
		const gchar* object_id = NULL;

		mafw_dbus_parse(msg, DBUS_TYPE_STRING, &object_id);
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_play_object(renderer, object_id, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_PLAY_URI)) {

		MafwDBusOpCompletedInfo *oci;
		const gchar* uri = NULL;

		mafw_dbus_parse(msg, DBUS_TYPE_STRING, &uri);
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_play_uri(renderer, uri, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_STOP)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_stop(renderer, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_PAUSE)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_pause(renderer, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_RESUME)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_resume(renderer, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_NEXT)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_next(renderer, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_PREVIOUS)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_previous(renderer, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_GOTO_INDEX)) {

		guint index;
		MafwDBusOpCompletedInfo *oci;
		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &index);
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_goto_index(renderer, index, playback_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_GET_STATUS)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_get_status(renderer, get_status_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST)) {

		guint pls_id;
		MafwPlaylist *playlist;
		MafwPlaylistManager *pm;
		GError *errp = NULL;

		/* Ask someone to create the MafwPlaylist object from pls_id.
		 */
		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &pls_id);
		if (pls_id == 0) {
			g_debug("Unassigning playlist...");
			playlist = NULL;
			
		} else {
			pm = mafw_playlist_manager_get();
			playlist = MAFW_PLAYLIST(
				mafw_playlist_manager_get_playlist(pm, pls_id,
								    &errp));
		}

		if (errp) {
			g_critical("Could not get playlist instance: %s",
				   errp->message);
		}
		else
		{
			mafw_renderer_assign_playlist(renderer, playlist, &errp);
		}
		mafw_dbus_ack_or_error(conn, msg, errp);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_SET_POSITION)) {

		MafwDBusOpCompletedInfo *oci;
		MafwRendererSeekMode mode;
		guint seconds;

		mafw_dbus_parse(msg, DBUS_TYPE_INT32, &mode, DBUS_TYPE_INT32, &seconds);
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_set_position(renderer, mode, seconds, set_get_position_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_has_member(msg, MAFW_RENDERER_METHOD_GET_POSITION)) {

		MafwDBusOpCompletedInfo *oci;
		oci = mafw_dbus_oci_new(conn, msg);
		mafw_renderer_get_position(renderer, set_get_position_cb, oci);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void _remove_buffering_tout(struct buffering_data *bdata)
{
	if (bdata->buffering_info_id)
	{
		if (bdata->buffering_info_id)
			g_source_remove(bdata->buffering_info_id);
		bdata->buffering_info_id = 0;
		bdata->last_state = 0.0;
	}
}

/**
 * state_changed:
 *
 * Handle state-changed signal from a renderer.  Forward it to
 * all UI connections.
 */
static void state_changed(MafwRenderer * self,
			   MafwPlayState state,
			   gpointer userdata)
{
	struct buffering_data *bufdata = (struct buffering_data*) userdata;
	ExportedComponent *ecomp;

	_remove_buffering_tout(bufdata);

	ecomp = bufdata->ecomp;
	mafw_dbus_send(ecomp->connection,
		       mafw_dbus_signal_full(
				NULL,
				ecomp->object_path,
				MAFW_RENDERER_INTERFACE,
				MAFW_RENDERER_SIGNAL_STATE_CHANGED,
				MAFW_DBUS_INT32(state)));
}
/**
 * playlist_changed:
 *
 * Handle playlist_changed signal from a renderer.  Forward it to
 * all UI connections.
 */
static void playlist_changed(MafwRenderer * self,
			   MafwPlaylist *playlist,
			   gpointer userdata)
{
	ExportedComponent *ecomp;
	guint playlist_id;

	playlist_id = MAFW_IS_PROXY_PLAYLIST(playlist)
	       	? mafw_proxy_playlist_get_id(
				  MAFW_PROXY_PLAYLIST(playlist))
		: MAFW_PROXY_PLAYLIST_INVALID_ID;
	ecomp = (ExportedComponent *) userdata;

	mafw_dbus_send(ecomp->connection,
		       mafw_dbus_signal_full(
				NULL,
				ecomp->object_path,
				MAFW_RENDERER_INTERFACE,
				MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED,
				MAFW_DBUS_UINT32(playlist_id)));
}
/**
 * media_changed:
 *
 * Handle media_changed signal from a renderer.  Forward it to
 * all UI connections.
 */
static void media_changed(MafwRenderer * self,
			   gint index, const gchar * object_id, 
			   gpointer userdata)
{
	ExportedComponent *ecomp;

	ecomp = (ExportedComponent *) userdata;
	mafw_dbus_send(ecomp->connection,
		       mafw_dbus_signal_full(
					     NULL,
					     ecomp->object_path,
					     MAFW_RENDERER_INTERFACE,
					     MAFW_RENDERER_SIGNAL_ITEM_CHANGED,
					     MAFW_DBUS_INT32(index),
					     MAFW_DBUS_STRING( object_id ?
							       object_id :
							       "")));
}
static void _emit_buffering_info(ExportedComponent *ecomp, gfloat status)
{
	mafw_dbus_send(ecomp->connection,
		       mafw_dbus_signal_full(
					NULL,
					ecomp->object_path,
					MAFW_RENDERER_INTERFACE,
					MAFW_RENDERER_SIGNAL_BUFFERING_INFO,
					MAFW_DBUS_DOUBLE(status)));
	
}

static gboolean emit_buffering_info_tout(struct buffering_data *bufdata)
{
	_emit_buffering_info(bufdata->ecomp, bufdata->last_state);
	bufdata->buffering_info_id = 0;
	return FALSE;
}

#define BUFFERING_EMIT_INTERVAL 750	/* buffering status emit interval in ms */

/**
 * buffering_info:
 *
 * Handle buffering_info signal from a renderer. Forward it to
 * all UI connections.
 */
static void buffering_info(MafwRenderer * self, gfloat status, gpointer userdata)
{
	struct buffering_data *bufdata = (struct buffering_data*) userdata;
	
	if (status != 100.0 && bufdata->buffering_info_id == 0)
	{
		_emit_buffering_info(bufdata -> ecomp, status);
		bufdata->buffering_info_id = g_timeout_add_full(
			G_PRIORITY_HIGH_IDLE,
			BUFFERING_EMIT_INTERVAL,
			(GSourceFunc)emit_buffering_info_tout,
			bufdata, NULL);
	}
	else if (status == 1.0)
	{
		_remove_buffering_tout(bufdata);
		_emit_buffering_info(bufdata -> ecomp, status);
	}

	bufdata->last_state = status;
}


/**
 * metadata_changed:
 *
 * Handle metadata_changed signal from a renderer.  Forward it to
 * session bus.
 */
static void metadata_changed(MafwRenderer *self,
			     const gchar *name, GValueArray *values,
			     gpointer userdata)
{
	ExportedComponent *ecomp;

	ecomp = (ExportedComponent *) userdata;
	mafw_dbus_send(ecomp->connection,
		       mafw_dbus_signal_full(
					NULL,
					ecomp->object_path,
					MAFW_RENDERER_INTERFACE,
					MAFW_RENDERER_SIGNAL_METADATA_CHANGED,
					MAFW_DBUS_STRING(name),
					MAFW_DBUS_GVALUEARRAY(values)));
}

static void _destroy_bufdata(struct buffering_data *bufdata, GClosure *closure)
{
	_remove_buffering_tout(bufdata);
	g_free(bufdata);
}

void connect_to_renderer_signals(gpointer ecomp)
{
	struct buffering_data *bufdata = g_new0(struct buffering_data, 1);
	gulong id;
	GError *err = NULL;
	MafwRegistry *registry;

	bufdata->ecomp = ecomp;


	id = g_signal_connect_data(bufdata->ecomp->comp, "buffering-info", 
				(GCallback)buffering_info,
				bufdata, (GClosureNotify)_destroy_bufdata, 0);
	g_array_append_val(bufdata->ecomp->sighandlers, id);

	id = g_signal_connect(bufdata->ecomp->comp, "state-changed",
				(GCallback)state_changed,
				bufdata);
	g_array_append_val(bufdata->ecomp->sighandlers, id);

	connect_signal(ecomp, "playlist-changed", playlist_changed);
	connect_signal(ecomp, "media-changed", media_changed);
	connect_signal(ecomp, "metadata-changed", metadata_changed);

	registry = mafw_registry_get_instance();
        mafw_shared_init(registry, &err);
	if (err) {
		g_warning("mafw_shared_init() failed, other exported "
			  "extensions won't be available: %s", err->message);
		g_error_free(err);
	}

}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
