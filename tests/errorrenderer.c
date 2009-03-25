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

#include "errorrenderer.h"
#include "libmafw/mafw-playlist.h"


/* Error renderer
 *
 * Returns with an error to all operations
 */

static void quit_main_loop(MafwRenderer *self, gchar const *fun)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (!es->dont_quit) {
		g_debug("g_main_loop_quit in '%s'", fun);
		g_main_loop_quit(es->mainloop);
	}
}
	
static void play(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->play_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void play_object(MafwRenderer *self, const gchar *object_id,
			MafwRendererPlaybackCB callback, gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->play_object_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void play_uri(MafwRenderer *self, const gchar *uri,
		     MafwRendererPlaybackCB callback, gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;
	
	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->play_uri_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void stop(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->stop_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void _pause(MafwRenderer *self, MafwRendererPlaybackCB callback,
		   gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->pause_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void resume(MafwRenderer *self, MafwRendererPlaybackCB callback,
		   gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->resume_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void get_status(MafwRenderer *self, MafwRendererStatusCB callback,
		       gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, NULL, 0, 0, NULL, user_data, error);
		g_error_free(error);
	}

	es->get_status_called++;
	quit_main_loop(self, G_STRFUNC);
}

static gboolean assign_playlist(MafwRenderer *self, MafwPlaylist *playlist, 
				GError **error)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (error)
	{
		g_set_error(error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
	}
	es->assign_playlist_called++;
	quit_main_loop(self, G_STRFUNC);

	return FALSE;
}

static void next(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->next_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void previous(MafwRenderer *self, MafwRendererPlaybackCB callback,
		     gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->previous_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void goto_index(MafwRenderer *self, guint index,
		       MafwRendererPlaybackCB callback, gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	es->goto_index_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void set_position(MafwRenderer *self, MafwRendererSeekMode mode, gint seconds,
			 MafwRendererPositionCB callback, gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->set_position_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, seconds, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(self, G_STRFUNC);
}

static void get_position(MafwRenderer *self, MafwRendererPositionCB callback,
			 gpointer user_data)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->get_position_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error renderer fails in everything it does.");
		callback(self, 0, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(self, G_STRFUNC);
}

static void state_changed(MafwRenderer *self,
			   MafwPlayState state)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->state_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void media_changed(MafwRenderer *self, gint index,
			   const gchar *object_id)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->media_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void playlist_changed(MafwRenderer *self, MafwPlaylist *playlist)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->playlist_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void buffering_info(MafwRenderer *self, gfloat status)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->buffering_info_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void metadata_changed(MafwRenderer *self, const GHashTable *metadata)
{
	ErrorRenderer* es = (ErrorRenderer*) self;

	es->metadata_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

G_DEFINE_TYPE(ErrorRenderer, error_renderer, MAFW_TYPE_RENDERER);

static void error_renderer_class_init(ErrorRendererClass *klass)
{
	MafwRendererClass *sclass = MAFW_RENDERER_CLASS(klass);

	sclass->play = play;
	sclass->play_object = play_object;
	sclass->play_uri = play_uri;
	sclass->stop = stop;
	sclass->pause = _pause;
	sclass->resume = resume;
	sclass->get_status = get_status;
	sclass->assign_playlist = assign_playlist;
	sclass->next = next;
	sclass->previous = previous;
	sclass->goto_index = goto_index;
	sclass->set_position = set_position;
	sclass->get_position = get_position;
	sclass->media_changed = media_changed;
	sclass->state_changed = state_changed;
	sclass->playlist_changed = playlist_changed;
	sclass->buffering_info = buffering_info;
	sclass->metadata_changed = metadata_changed;
}

static void error_renderer_init(ErrorRenderer *src) { /*nop*/ }

gpointer error_renderer_new(const gchar *name, const gchar *uuid,
			GMainLoop *mainloop)
{
	ErrorRenderer *ms;

	ms = g_object_new(error_renderer_get_type(),
			  "plugin", "mockland",
			  "uuid", uuid,
			  "name", name,
			  NULL);
	ms->mainloop = mainloop;
	return ms;
}
