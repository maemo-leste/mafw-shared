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
#ifndef __MAFW_PLAYLIST_DAEMON_H__
#define __MAFW_PLAYLIST_DAEMON_H__

/* Internal declarations for MPD. */

#include <glib.h>
#include <dbus/dbus.h>

/* From aplaylist.c: */

extern guint Settle_time;

/*
 * Array based playlist storage.
 *
 * @id:          playlist identifier
 * @name:        playlist name
 * @repeat:      repeat mode
 * @shuffled:    playlist is shuffled
 * @use_count:   a reference count for the playlist
 * @len:         length of playlist
 * @alloc:       number of elements allocated (>= len)
 * @poolst:      the first element of the pool (>= len if pool is empty)
 * @vidx:        array of object id:s
 * @pidx:        array containing both shuffled elements and un-shuffled ones
 *               {0..poolst-1}: shuffled elements
 *               {poolst..len-1}: pool with still unshuffled elements
 *               Resolves the query "which element will be played at postion
 *               i-th?"
 * @iidx:        Relates both vidx and pidx. Resolves the query "in which
 *               position will be played element i-th?". That is, it stores
 *               where is placed in pidx each element of vidx. Only makes sense
 *               when playlist is shuffled.
 * @dirty:       set to 1 if a playlist is modified (cleared manually)
 * @dirty_timer: each time the playlist is dirtied, a timer is started (or
 *               elongated), and when it expires, triggers save_me().  This
 *               variable stores its id.
 */
typedef struct {
	guint id;
	gchar *name;
	gboolean repeat;
	gboolean shuffled;
	guint use_count;
	guint len;
	guint alloc;
        guint poolst;
	gchar **vidx;
	guint *pidx;
        gint *iidx;
	gboolean dirty;
	guint dirty_timer;
} Pls;

extern gboolean pls_check(Pls *pls);
extern void pls_dump(Pls *pls, gboolean items);
extern Pls *pls_new(guint id, const gchar *name);
extern gboolean pls_set_name(Pls *pls, const gchar *name);
extern void pls_clear(Pls *pls);
extern void pls_free(Pls *pls);
extern gboolean pls_append(Pls *pls, const gchar *oid);
extern gboolean pls_appends(Pls *pls, const gchar **oid, guint len);
extern gboolean pls_inserts(Pls *pls, guint idx, const gchar **oids, guint len);
gboolean pls_insert(Pls *pls, guint idx, const gchar *oid);
extern gboolean pls_remove(Pls *pls, guint idx);
extern void pls_shuffle(Pls *pls);
extern void pls_unshuffle(Pls *pls);
extern gchar *pls_get_item(Pls *pls, guint idx);
extern gchar **pls_get_items(Pls *pls, guint fidx, guint lidx);
void pls_get_starting(Pls *pls, guint *index, gchar **oid);
void pls_get_last(Pls *pls, guint *index, gchar **oid);
gboolean pls_get_next(Pls *pls, guint *index, gchar **oid);
gboolean pls_get_prev(Pls *pls, guint *index, gchar **oid);
extern gboolean pls_is_shuffled(Pls *pls);
extern void pls_set_repeat(Pls *pls, gboolean repeat);
extern void pls_set_use_count(Pls *pls, guint use_count);
extern gboolean pls_move(Pls *pls, guint from, guint to);
extern gint pls_cmpids(gconstpointer a, gconstpointer b, gpointer unused);
extern gboolean pls_save(Pls *pls, const gchar *fn);
extern Pls *pls_load(const gchar *fn);

/* From mafw-playlist-daemon.c: */
extern void save_me(Pls *pls);

/* From playlist-wrapper.c: */
extern DBusHandlerResult handle_playlist_request(DBusConnection *con,
						 DBusMessage *msg,
                                                 const gchar *path);

/* From playlist-manager-wrapper.c: */
extern GMainLoop *Loop;
extern GTree *Playlists;
extern GTree *Playlists_by_name;

extern void init_playlist_wrapper(DBusConnection *dbus,
				  gboolean opt_stayalive,
				  gboolean opt_kill);
extern void save_all_playlists(void);

#endif
