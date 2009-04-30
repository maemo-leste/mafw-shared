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

#ifndef ERRORRENDERER_H
#define ERRORRENDERER_H

#include <glib-object.h>
#include <libmafw/mafw.h>

typedef struct {
	MafwRendererClass parent;
} ErrorRendererClass;

typedef struct {
	MafwRenderer parent;

	int play_called, play_object_called, play_uri_called,
	stop_called, pause_called, resume_called, get_status_called,
	assign_playlist_called, next_called, previous_called,
	goto_index_called, set_position_called,
	get_position_called, state_changed_called, media_changed_called,
	playlist_changed_called, buffering_info_called,
		metadata_changed_called;

	GMainLoop *mainloop;
	gboolean dont_quit;
} ErrorRenderer;

extern GType error_renderer_get_type(void);
#define ERROR_RENDERER(o) (G_TYPE_CHECK_INSTANCE_CAST((o),\
                           error_renderer_get_type(), ErrorRenderer))

extern gpointer error_renderer_new(const gchar *name, const gchar *uuid,
			       GMainLoop *mainloop);
#endif
