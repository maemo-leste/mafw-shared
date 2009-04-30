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

#ifndef __MAFW_PLAYLIST_MANAGER_H__
#define __MAFW_PLAYLIST_MANAGER_H__

/* Include files */
#include <glib.h>
#include <glib-object.h>

#include <libmafw-shared/mafw-proxy-playlist.h>

/* Macros */
#define MAFW_TYPE_PLAYLIST_MANAGER				\
	mafw_playlist_manager_get_type()
#define MAFW_PLAYLIST_MANAGER(obj)				\
	G_TYPE_CHECK_INSTANCE_CAST((obj),				\
				   MAFW_TYPE_PLAYLIST_MANAGER,	\
				   MafwPlaylistManager)
#define MAFW_PLAYLIST_MANAGER_GET_CLASS(obj)			\
	G_TYPE_INSTANCE_GET_CLASS((obj),				\
				  MAFW_TYPE_PLAYLIST_MANAGER,	\
				  MafwPlaylistManagerClass)

/**
 * MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID:
 *
 * Invalid playlist import session id.
 */
#define MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID (~0)


/* Type definitions */
typedef struct
	_MafwPlaylistManagerPrivate
	 MafwPlaylistManagerPrivate;

/**
 * MafwPlaylistManager:
 *
 * Object structure
 */
typedef struct _MafwPlaylistManager {
	GObject parent;
	MafwPlaylistManagerPrivate *priv;
} MafwPlaylistManager;

typedef GObjectClass MafwPlaylistManagerClass;

/**
 * MafwPlaylistManagerItem:
 * @id: playlist id
 * @name: a dynamically allocated UTF-8 string.
 *
 * mafw_playlist_manager_list_playlists() returns a #GList
 * of this structures.
 */
typedef struct _MafwPlaylistManagerItem {
	guint id;
	gchar *name;
} MafwPlaylistManagerItem;

/* Function prototypes */
G_BEGIN_DECLS

/**
 * MafwPlaylistManagerImportCb:
 * @self:      The emitting MafwPlaylistManager.
 * @import_id: The import-ID of the import session
 * @playlist: The created playlist
 * @user_data: Optional user data pointer passed to mafw_source_create_object().
 * @error:     Non-%NULL if an error occurred.
 *
 * Callback prototype for playlist import result. If any errors were encountered
 * during playlist importing, @error is set non-%NULL, while @playlist might
 * also be %NULL.
 */
typedef void (*MafwPlaylistManagerImportCb)(MafwPlaylistManager *self,
					  guint import_id,
					  MafwProxyPlaylist *playlist,
					  gpointer user_data,
					  const GError *error);


extern GType mafw_playlist_manager_get_type(void);

extern MafwPlaylistManager *mafw_playlist_manager_get(void);

/* Methods */
extern MafwProxyPlaylist *mafw_playlist_manager_create_playlist(
					   MafwPlaylistManager *self,
					   gchar const *name,
					   GError **errp);
extern gboolean mafw_playlist_manager_destroy_playlist(
					   MafwPlaylistManager *self,
					   MafwProxyPlaylist *playlist,
					   GError **errp);

extern MafwProxyPlaylist *mafw_playlist_manager_dup_playlist(
					   MafwPlaylistManager *self,
					   MafwProxyPlaylist *playlist,
					   gchar const *new_name,
					   GError **errp);

extern MafwProxyPlaylist *mafw_playlist_manager_get_playlist(
					   MafwPlaylistManager *self,
					   guint id,
					   GError **errp);
extern GPtrArray *mafw_playlist_manager_get_playlists(
					   MafwPlaylistManager *self,
					   GError **errp);
extern GArray *mafw_playlist_manager_list_playlists(
					   MafwPlaylistManager *self,
					   GError **errp);
extern void mafw_playlist_manager_free_list_of_playlists(
					   GArray *playlist_list);
extern guint mafw_playlist_manager_import(MafwPlaylistManager *self,
					   const gchar *playlist,
					   const gchar *base_uri,
					   MafwPlaylistManagerImportCb cb,
					   gpointer user_data,
					   GError **error);
extern gboolean mafw_playlist_manager_cancel_import(MafwPlaylistManager *self,
					  guint import_id,
					  GError **error);
G_END_DECLS

#endif /* ! __MAFW_PLAYLIST_MANAGER_H__ */
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
