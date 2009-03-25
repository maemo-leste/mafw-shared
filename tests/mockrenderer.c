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

#include "mockrenderer.h"

/* Mocked renderer
 *
 * has looots of variables which are set when the appropriate method
 * is invoked, and can be checked later
 * parameter validation?
 */

static void quit_main_loop(MafwRenderer *self, gchar const *fun)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (!ms->dont_quit) {
		g_debug("g_main_loop_quit in '%s'", fun);
		g_main_loop_quit(ms->mainloop);
	}
}

static void play(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->play_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void play_object(MafwRenderer *self, const gchar *object_id,
			MafwRendererPlaybackCB callback, gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->play_object_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void play_uri(MafwRenderer *self, const gchar *uri,
		     MafwRendererPlaybackCB callback, gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->play_uri_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void stop(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->stop_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void _pause(MafwRenderer *self, MafwRendererPlaybackCB callback,
		   gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->pause_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void resume(MafwRenderer *self, MafwRendererPlaybackCB callback,
		   gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->resume_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void get_status(MafwRenderer *self, MafwRendererStatusCB callback,
		       gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, ms->get_stat_pl, 42, Paused, "bar", user_data, NULL);

	ms->get_status_called++;
	quit_main_loop(self, G_STRFUNC);
}

static gboolean assign_playlist(MafwRenderer *self, MafwPlaylist *playlist,
				GError **error)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	ms->assign_playlist_called++;
	quit_main_loop(self, G_STRFUNC);

	return TRUE;
}

static void next(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->next_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void previous(MafwRenderer *self, MafwRendererPlaybackCB callback,
		     gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->previous_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void goto_index(MafwRenderer *self, guint index,
		       MafwRendererPlaybackCB callback, gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(self, user_data, NULL);

	ms->goto_index_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void set_position(MafwRenderer *self, MafwRendererSeekMode mode, gint seconds,
			 MafwRendererPositionCB callback, gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	if (callback != NULL)
		callback(MAFW_RENDERER(self), seconds, user_data, NULL);

	ms->set_position_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void get_position(MafwRenderer *self, MafwRendererPositionCB callback,
			 gpointer user_data)
{
	MockedRenderer* ms = (MockedRenderer*) self;
	
	if (callback != NULL)
		callback(MAFW_RENDERER(self), 1337, user_data, NULL);
	
	ms->get_position_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void state_changed(MafwRenderer *self,
			   MafwPlayState state)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	ms->state_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void media_changed(MafwRenderer *self, gint index,
			   const gchar *object_id)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	ms->media_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void playlist_changed(MafwRenderer *self, MafwPlaylist *playlist)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	ms->playlist_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void buffering_info(MafwRenderer *self, gfloat status)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	ms->buffering_info_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void metadata_changed(MafwRenderer *self, const GHashTable *metadata)
{
	MockedRenderer* ms = (MockedRenderer*) self;

	ms->metadata_changed_called++;
	quit_main_loop(self, G_STRFUNC);
}

G_DEFINE_TYPE(MockedRenderer, mocked_renderer, MAFW_TYPE_RENDERER);

static void mocked_renderer_class_init(MockedRendererClass *klass)
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

static void mocked_renderer_init(MockedRenderer *src) { /*nop*/ }

gpointer mocked_renderer_new(const gchar *name, const gchar *uuid,
			 GMainLoop *mainloop)
{
	MockedRenderer *ms;
	
	ms = g_object_new(mocked_renderer_get_type(),
			  "plugin", "mockland",
			  "uuid", uuid,
			  "name", name,
			  NULL);
	ms->mainloop = mainloop;
	return ms;
}
