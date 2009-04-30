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
# include "config.h"
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>


#include <glib.h>
#include <glib/gstdio.h>

#include <libmafw/mafw-errors.h>
#include <libmafw/mafw-source.h>
#include <libmafw/mafw-registry.h>
#include <totem-pl-parser.h>

#include "libmafw-shared/mafw-shared.h"
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "mpd-internal.h"

/* Standard definitions */
#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN		"mafw-playlistmanager-wrapper"


/* It must be buried deep inside the D-BUS specs but anyway,
 * if the object path is not set dbusd will disconnect you. */
#define MAFW_DBUS_PATH		MAFW_PLAYLIST_PATH
#define MAFW_DBUS_INTERFACE	MAFW_PLAYLIST_INTERFACE

/* Default location to save playlists. */
#define DEFAULT_PLS_DIR		".mafw-playlists"

/* Globals. */
GMainLoop *Loop;

/* TRUE at startup, so it wont save the loaded playlists */
gboolean initialize = FALSE;

/* Private variables */
static MafwRegistry *Reg;
/* Our playlists, keyed by their ID, we don't expect many of them. */
GTree *Playlists;
/* The same playlists, but keyed by their name. */
GTree *Playlists_by_name;
/* Highest id given out to our playlists. */
static guint Last_id = 1;

/* Program code */

/*
 * GTraverseFunc adding a struct of ($id, $name) to $iter.  Used to construct
 * the reply to list_playlist requests.
 */
static gboolean append_pls(guint id, Pls *pls, DBusMessageIter *iter)
{
	DBusMessageIter istr;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &istr);
	dbus_message_iter_append_basic(&istr,  DBUS_TYPE_UINT32, &pls->id);
	dbus_message_iter_append_basic(&istr,  DBUS_TYPE_STRING, &pls->name);
	dbus_message_iter_close_container(iter, &istr);
	return FALSE;
}

/* Returns the directory where playlists will be saved.  It defaults to
 * $HOME/DEFAULT_PLS_DIR, but can be overridden via the $MAFW_PLAYLIST_DIR
 * environment variable.  The returned string points to a static storage and
 * must not be freed. */
static const gchar *playlist_dir(void)
{
	static gchar *playlist_dir;
	const gchar *home, *pld;

	if (playlist_dir)
		return playlist_dir;
	pld = g_getenv("MAFW_PLAYLIST_DIR");
	if (pld) {
		playlist_dir = g_strdup(pld);
	} else {
		home = g_getenv("HOME");
		if (!home)
			home = g_get_home_dir();
		playlist_dir = g_build_filename(home, DEFAULT_PLS_DIR, NULL);
	}
	return playlist_dir;
}

/* Makes sure that the playlist directory exists, returns FALSE if it can't. */
static gboolean ensure_playlist_dir(void)
{
	if (mkdir(playlist_dir(), 0700) == -1 && errno != EEXIST) {
		g_critical("failed to ensure existence of playlist directory"
			   ", playlists cannot be saved. %s: (%s)",
			   playlist_dir(), g_strerror(errno));
		return FALSE;
	} else
		return TRUE;
}

/* Triggered from aplaylist.c after edit operations have settled on $pls. */
void save_me(Pls *pls)
{
	gchar *fn;

	if (!ensure_playlist_dir())
		return;
	fn = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%u",
			     playlist_dir(), pls->id);
	if (pls_save(pls, fn))
		pls->dirty = FALSE;
	g_free(fn);
}

/* Tree traversal callback for save_all_playlists(). */
static gboolean save_pls_cb(guint id, Pls *pls, gpointer _)
{
	save_me(pls);
	return FALSE;
}

/* Saves every playlist unconditionally.  Used at exit. */
void save_all_playlists(void)
{
	g_tree_foreach(Playlists, (GTraverseFunc)save_pls_cb, NULL);
}

static void load_playlists(void)
{
	GDir *d;
	const gchar *fn;

	d = g_dir_open(playlist_dir(), 0, NULL);
	if (!d) {
		if (errno != ENOENT)
			g_critical("failed to open playlist directory: %s",
				   g_strerror(errno));
		return;
	}
	/* Handle the case where the final rename() of a previously written
	 * playlist failed: if $fn ends with '.tmp' do the renaming now, then
	 * load the playlists. */
	while ((fn = g_dir_read_name(d))) {
		gchar *dot;

		if ((dot = strrchr(fn, '.')) && dot != fn) {
			gchar *nufn;
			struct stat sb;

			if (strcmp(dot, ".tmp"))
				continue;
			/* Minor sanity check: we accept only nonempty regular
			 * files.  Otherwise we try to unlink the file, to avoid
			 * future hassle. */
			if (stat(fn, &sb) == -1 ||
			    !S_ISREG(sb.st_mode) ||
			    sb.st_size == 0)
			{
				g_unlink(fn);
				continue;
			}
			nufn = g_strndup(fn, dot - fn);
			rename(fn, nufn);
			g_free(nufn);
		}
	}
	g_dir_close(d);

	/* The second open should succeed unconditionally... */
	d = g_dir_open(playlist_dir(), 0, NULL);
	g_assert(d);
	initialize = TRUE;
	while ((fn = g_dir_read_name(d))) {
		Pls *pls;
		gchar *fullfn;

		fullfn = g_build_filename(playlist_dir(), fn, NULL);
		pls = pls_load(fullfn);
		g_free(fullfn);
		if (!pls) {
			g_warning("failed to load from: %s", fn);
			continue;
		}
		/* We cannot issue lower playlist id:s than any existing. */
		if (Last_id <= pls->id)
			Last_id = pls->id + 1;
		g_tree_insert(Playlists, GUINT_TO_POINTER(pls->id), pls);
		g_tree_insert(Playlists_by_name, g_strdup(pls->name), pls);
	}
	initialize = FALSE;
	g_dir_close(d);
}

static void signal_playlist_created(DBusConnection *con, guint new_id)
{
	mafw_dbus_send(con, mafw_dbus_signal(
                               MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
                               MAFW_DBUS_UINT32(new_id)));
}

static GHashTable *import_requests;

static guint get_next_import_id(void)
{
	static guint next_id = 0;
	return ++next_id;
}

struct plparse_data {
	gchar *pl_uri;
	gchar *base;
	MafwDBusOpCompletedInfo *oci;
	guint import_id;
	GList *urilist;
	gboolean list_from_browse;
	MafwSource *source;
	guint browse_id;
	gboolean cancel;
};

static void free_plparse_data(struct plparse_data *pl_dat)
{
	GList *cur_item = pl_dat->urilist;
	if (pl_dat->pl_uri)
		g_free(pl_dat->pl_uri);
	if (pl_dat->base)
		g_free(pl_dat->base);
	if (pl_dat->oci)
		mafw_dbus_oci_free(pl_dat->oci);
	while(cur_item)
	{
		g_free(cur_item->data);
		cur_item = g_list_delete_link(cur_item, cur_item);
	}
	g_free(pl_dat);
}

static void import_done(struct plparse_data *pl_dat, const GError *err)
{
	GList  *cur_uri = pl_dat->urilist;
	Pls *new_pl;
	gint count = 0;
	gchar *temp;
	if (err)
	{
		const gchar *domain_str;

		domain_str = g_quark_to_string(err->domain);

		mafw_dbus_send(pl_dat->oci->con, mafw_dbus_method_full(
				dbus_message_get_sender(pl_dat->oci->msg),
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(pl_dat->import_id),
				MAFW_DBUS_STRING(domain_str),
				MAFW_DBUS_INT32(err->code),
				MAFW_DBUS_STRING(err->message)));
		g_hash_table_remove(import_requests,
				    GUINT_TO_POINTER(pl_dat->import_id));
		free_plparse_data(pl_dat);
		return;
	}
	temp = g_strdup(pl_dat->pl_uri);
	while (g_tree_lookup(Playlists_by_name, temp) != NULL)
	{
		count++;
		g_free(temp);

		temp = g_strdup_printf("%s (%d)", pl_dat->pl_uri, count);
	}

	new_pl = pls_new(Last_id++, temp);
	g_free(temp);
	g_tree_insert(Playlists, GUINT_TO_POINTER(new_pl->id), new_pl);
	g_tree_insert(Playlists_by_name, g_strdup(new_pl->name), new_pl);

	while (cur_uri)
	{
		gchar *new_oid;
		if (!pl_dat->list_from_browse)
		{
			new_oid = mafw_source_create_objectid(cur_uri->data);
			pls_append(new_pl, new_oid);
			g_free(new_oid);
		}
		else
		{
			pls_append(new_pl, cur_uri->data);
		}
		g_free(cur_uri->data);
		cur_uri = g_list_delete_link(cur_uri, cur_uri);
	}
	pl_dat->urilist = NULL;
	/* Inform the proxy about the new playlist */
	mafw_dbus_send(pl_dat->oci->con, mafw_dbus_method_full(
				dbus_message_get_sender(pl_dat->oci->msg),
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(pl_dat->import_id),
				MAFW_DBUS_UINT32(new_pl->id)));

	/* signal pl-created */
	signal_playlist_created(pl_dat->oci->con, new_pl->id);
	g_hash_table_remove(import_requests,
				    GUINT_TO_POINTER(pl_dat->import_id));
	free_plparse_data(pl_dat);
}


static void plparser_entry_parsed_cb(TotemPlParser *parser, gchar *uri,
				     gpointer metadata,
				     struct plparse_data *pl_dat)
{
	pl_dat->urilist = g_list_append(pl_dat->urilist, g_strdup(uri));
}

static gboolean import_from_file(struct plparse_data *pl_dat, GError **err)
{
	TotemPlParser *parser = totem_pl_parser_new ();
	gboolean retval = TRUE;

	g_object_set (parser, "recurse", FALSE, "disable-unsafe", TRUE, NULL);

	g_signal_connect(parser, "entry-parsed",
				(GCallback)plparser_entry_parsed_cb,
				pl_dat);

	if (totem_pl_parser_parse_with_base (parser, pl_dat->pl_uri,
						pl_dat->base, FALSE)
			!= TOTEM_PL_PARSER_RESULT_SUCCESS)
	{
		g_set_error(err, MAFW_PLAYLIST_ERROR,
					  MAFW_PLAYLIST_ERROR_IMPORT_FAILED,
					  "Playlist parsing failed.");
		retval = FALSE;
	}
	else
		import_done(pl_dat, NULL);

	g_object_unref (parser);

	return retval;
}

static void browse_res_cb(MafwSource *self, guint browse_id,
					 gint remaining_count, guint index,
					 const gchar *object_id,
					 GHashTable *metadata,
					 struct plparse_data *pl_data,
					 const GError *error)
{
	if (!error)
	{
		pl_data->urilist = g_list_append(pl_data->urilist,
							g_strdup(object_id));
		if (remaining_count)
		{
			return;
		}
	}

	import_done(pl_data, error);
	g_object_unref(self);
}

static void mdata_res_cb(MafwSource *self,
				   const gchar *object_id,
				   GHashTable *metadata,
				   struct plparse_data *plp_data,
				   const GError *error)
{
	GValue *cur_value;
	GError *err = NULL;
	const gchar *mime_type = NULL;
	gboolean got_uri = FALSE;

	if (plp_data->cancel)
	{
		g_hash_table_remove(import_requests,
				    GUINT_TO_POINTER(plp_data->import_id));
		g_object_unref(self);
		free_plparse_data(plp_data);
		return;
	}

	if (error)
	{/* error during get-metadata */
		import_done(plp_data, error);
		g_object_unref(self);
		return;
	}

	cur_value = mafw_metadata_first(metadata, MAFW_METADATA_KEY_URI);
	if (!cur_value ||
		!(G_IS_VALUE(cur_value) && G_VALUE_HOLDS_STRING(cur_value)))
	{/* No URI?? */
	}
	else
	{
		got_uri = TRUE;
		g_free(plp_data->pl_uri);
		plp_data->pl_uri = g_value_dup_string(cur_value);
	}

	cur_value = mafw_metadata_first(metadata, MAFW_METADATA_KEY_MIME);
	if (cur_value &&
		(G_IS_VALUE(cur_value) && G_VALUE_HOLDS_STRING(cur_value)))
	{
		mime_type = g_value_get_string(cur_value);
	}

	if (!got_uri || (mime_type && !strcmp(mime_type,
			MAFW_METADATA_VALUE_MIME_CONTAINER)))
	{/* it is a container.... browse it */
		plp_data->list_from_browse = TRUE;
		plp_data->source = self;
		plp_data->browse_id = mafw_source_browse(self, object_id,
			FALSE,
			NULL, NULL,
			MAFW_SOURCE_NO_KEYS,
			0, 0, (MafwSourceBrowseResultCb)browse_res_cb,
			plp_data);
	}
	else
	{/* it is a simple file */
		g_object_unref(self);
		if (!import_from_file(plp_data, &err))
		{/* error during parse? */
			import_done(plp_data, err);
			g_error_free(err);
			return;
		}
	}
}

static guint import_playlist(const gchar *pl, const gchar *base,
			MafwDBusOpCompletedInfo *oci,GError **err)
{
	gchar *src_uuid;
	guint import_id;
	MafwSource *src;
	struct plparse_data *pl_dat = g_new0(struct plparse_data, 1);

	import_id = pl_dat->import_id = get_next_import_id();
	pl_dat->oci = oci;
	pl_dat->pl_uri = g_strdup(pl);
	if (base && base[0])
	{
		pl_dat->base = g_strdup(base);
	}

	/* Check whether pl is an object-id */
	if (mafw_source_split_objectid(pl, &src_uuid, NULL))
	{
		if ((src = MAFW_SOURCE(
			mafw_registry_get_extension_by_uuid(Reg, src_uuid))))
		{
			g_object_ref(src);

			if (!import_requests) {
				import_requests = g_hash_table_new_full(NULL,
							      		NULL,
							      		NULL,
							      		NULL);
			}

			g_hash_table_replace(import_requests,
						GUINT_TO_POINTER(import_id),
						pl_dat);
			/* if it was an object-id, check if it is a container or
                         * not */
			mafw_source_get_metadata(src, pl,
				MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME),
				(MafwSourceMetadataResultCb)mdata_res_cb,
				pl_dat);
			g_free(src_uuid);
			return import_id;
		}
		else
		{
			g_set_error(err, MAFW_PLAYLIST_ERROR,
					  MAFW_PLAYLIST_ERROR_IMPORT_FAILED,
					  "Source not found");
		}
		g_free(src_uuid);
	}
	else
	{
		if (import_from_file(pl_dat, err))
			return import_id;
	}

	free_plparse_data(pl_dat);
	return ~0;
}

/* D-BUS filter to process a request to the daemon. */
static DBusHandlerResult request(DBusConnection *con, DBusMessage *req,
				 void *unused)
{
	DBusMessage *reply;
	const gchar *iface, *member, *path;

	/* Are we the addressee? */
        if (dbus_message_get_sender(req) &&
            !strcmp(dbus_bus_get_unique_name(con),
                    dbus_message_get_sender(req)))
		/* It's our own signal echoed back by dbusd. */
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (!(iface = dbus_message_get_interface(req))
	    || strcmp(iface, MAFW_PLAYLIST_INTERFACE) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (!(member = dbus_message_get_member(req)))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	/* Handle signals. */
	if (dbus_message_get_type(req) == DBUS_MESSAGE_TYPE_SIGNAL) {
		if (!strcmp(member, "die")) {
			g_info("bye-bye");
			g_main_loop_quit(Loop);
		} else
			g_assert_not_reached();
	}

	/* Handle method calls. */
	if (dbus_message_get_type(req) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if ((path = dbus_message_get_path(req)) != NULL
	     && strcmp(path, MAFW_PLAYLIST_PATH) != 0)
		return handle_playlist_request(con, req, path);

	/* Process $req and construct a $reply. */
	reply = NULL;
	if (!strcmp(member, MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST)) {
		const gchar *name;
		Pls *pls;

		mafw_dbus_parse(req, DBUS_TYPE_STRING, &name);
		g_assert(name);
		if (*name == '\0') {
			reply = mafw_dbus_error(
                                req,
                                MAFW_PLAYLIST_ERROR,
                                MAFW_PLAYLIST_ERROR_INVALID_NAME,
                                "name cannot be empty");
			goto out;
		}
		pls = g_tree_lookup(Playlists_by_name, name);
		if (!pls) {
			pls = pls_new(Last_id++, name);
			g_tree_insert(Playlists, GUINT_TO_POINTER(pls->id),
                                      pls);
			g_tree_insert(Playlists_by_name, g_strdup(pls->name),
                                      pls);
			/*
			 * Sending the playlist_created signal here is quite all
			 * right because the receiver will queue up everything
			 * until it receives the reply to its method call.
			 */
			signal_playlist_created(con, pls->id);
		}
		reply = mafw_dbus_reply(req, MAFW_DBUS_UINT32(pls->id));
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_DESTROY_PLAYLIST)) {
		guint id;
		Pls *pls;

		mafw_dbus_parse(req, DBUS_TYPE_UINT32, &id);
		pls = g_tree_lookup(Playlists, GUINT_TO_POINTER(id));
		if (pls) {
			g_assert(id == pls->id);

			/* Check if the playlist is being used */
			if (pls->use_count != 0)
			{
				/* Destroy playlists that are being used is not
				 * allowed, so send a signal to inform about
				 * that */
				mafw_dbus_send(
					con,
					mafw_dbus_signal(
						MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTRUCTION_FAILED,
						MAFW_DBUS_UINT32(id)));
			} else {
				/* Unlink the playlist, (it's not an error if it
				 * hasn't been saved yet).  Then remove $pls
				 * from our data structures.  NOTE: that
				 * removing from $Playlists causes the playlist
				 * to be free()d. */

				gchar *fn;

				fn = g_strdup_printf("%s"
                                                     G_DIR_SEPARATOR_S "%u",
						     playlist_dir(), pls->id);
				if (g_unlink(fn) == -1 && errno != ENOENT)
					g_warning(
                                                "error while deleting '%s': %s",
                                                fn, g_strerror(errno));
				g_free(fn);
				g_assert(g_tree_remove(Playlists_by_name,
                                                       pls->name));
				g_assert(g_tree_remove(
                                                 Playlists,
                                                 GUINT_TO_POINTER(pls->id)));
				mafw_dbus_send(
					con,
					mafw_dbus_signal(
						MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED,
						MAFW_DBUS_UINT32(id)));
			}
		}
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_DUP_PLAYLIST)) {
                const gchar *new_name = NULL;
                Pls *pls, *new_pls;
                int i;
		guint src_id;

                mafw_dbus_parse(req, DBUS_TYPE_UINT32, &src_id,
					DBUS_TYPE_STRING, &new_name);
                g_assert(new_name);
                if (*new_name == '\0') {
                        reply = mafw_dbus_error(
                                req,
                                MAFW_PLAYLIST_ERROR,
                                MAFW_PLAYLIST_ERROR_INVALID_NAME,
                                "name cannot be empty");
                        goto out;
                }
                /*Check if playlist with new name already exits*/
                new_pls = g_tree_lookup(Playlists_by_name, new_name);
                if(new_pls) {
                        reply = mafw_dbus_error(
                                req,
                                MAFW_PLAYLIST_ERROR,
                                MAFW_PLAYLIST_ERROR_INVALID_NAME,
                                "Playlist already exists");
                        goto out;
                }
                pls = g_tree_lookup(Playlists, GUINT_TO_POINTER(src_id));
                if (!pls) {
                        reply = mafw_dbus_error(req,
                                         MAFW_PLAYLIST_ERROR,
                                         MAFW_PLAYLIST_ERROR_INVALID_NAME,
                                         "playlist does not exist");
                        goto out;
		 }
		/* copy the plst*/
                new_pls = pls_new(Last_id++, new_name);
                new_pls->shuffled = pls->shuffled;
                new_pls->alloc = pls->alloc ;
                new_pls->len = pls->len ;
                new_pls->vidx = g_realloc(new_pls->vidx, new_pls->alloc *
		 				sizeof(*new_pls->vidx));
                new_pls->pidx = g_realloc(new_pls->pidx, new_pls->alloc *
		 				sizeof(*new_pls->pidx));
                for (i = 0; i < pls->len; ++i) {
                         new_pls->vidx[i] = g_strdup(pls->vidx[i]);
                         new_pls->pidx[i] = pls->pidx[i];
		 }
                pls_set_repeat(new_pls, pls->repeat);
                g_tree_insert(Playlists, GUINT_TO_POINTER(new_pls->id),
				new_pls);
                g_tree_insert(Playlists_by_name, g_strdup(new_pls->name),
				new_pls);
                signal_playlist_created(con, new_pls->id);
                /*
                 * Sending the playlist_created signal here is quite all
                 * right because the receiver will queue up everything
                 * until it receives the reply to its method call.
                 */
        	reply = mafw_dbus_reply(req, MAFW_DBUS_UINT32(new_pls->id));
	}else if (!strcmp(member, MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS)) {
		DBusMessageIter imsg, iary;

		reply = mafw_dbus_reply(req);
		dbus_message_iter_init_append(reply, &imsg);
		dbus_message_iter_open_container(&imsg, DBUS_TYPE_ARRAY,
						 "(us)", &iary);
		if (dbus_message_get_signature(req)[0] != '\0') {
			guint nids, i;
			guint *ids;

			/* Retrieve information about the playlists
			 * whose ID are specified in the array. */
			mafw_dbus_parse(req, DBUS_TYPE_ARRAY,
					 DBUS_TYPE_UINT32, &ids, &nids);

			for (i = 0; i < nids; i++) {
				Pls *pls;

				pls = g_tree_lookup(Playlists,
						    GUINT_TO_POINTER(ids[i]));
				/* It may happen that there's no playlist with
				 * the given id; for example when the playlist
				 * manager's (or someone else's) idea of
				 * playlists is outdated. */
				if (pls)
					append_pls(ids[i], pls, &iary);
			}
		} else {
			/* Return information about all known playlists. */
			g_tree_foreach(Playlists,
				       (GTraverseFunc)append_pls, &iary);
		}
		dbus_message_iter_close_container(&imsg, &iary);
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST)) {
		gchar *pl, *base;
		guint import_id;
		GError *err = NULL;
		MafwDBusOpCompletedInfo *oci;

		/* Store the message and pass as user data to browse().
		   This is used to route the results to correct
		   destination. */
		oci = mafw_dbus_oci_new(con, req);
		mafw_dbus_parse(req, DBUS_TYPE_STRING, &pl,
				DBUS_TYPE_STRING, &base);
		import_id = import_playlist(pl, base, oci, &err);
		if (err)
		{
			reply =  mafw_dbus_gerror(req, err);
			g_error_free(err);
		}
		else
		{
			reply = mafw_dbus_reply(req,
					     MAFW_DBUS_UINT32(import_id));
		}
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_CANCEL_IMPORT)) {
		guint import_id;
		struct plparse_data *pl_dat = NULL;
		GError *err = NULL;

		mafw_dbus_parse(req, DBUS_TYPE_UINT32, &import_id);
		if (import_requests)
			pl_dat = g_hash_table_lookup(import_requests,
				  		GUINT_TO_POINTER(import_id));
		if (pl_dat)
		{
			if (pl_dat->source != NULL)
			{/* browse is ongoing.... cancel it */
				mafw_source_cancel_browse(pl_dat->source,
						pl_dat->browse_id, NULL);
			}
			else
			{/* waiting for get_metadata-cb only... */
				pl_dat->cancel = TRUE;
			}
		}
		else
		{
			g_set_error(&err, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_INVALID_IMPORT_ID,
					"ImportID not found");
		}
		mafw_dbus_ack_or_error(con, req, err);
		return DBUS_HANDLER_RESULT_HANDLED;
	}else
		/* Unknown request. */
		g_assert_not_reached();
out:
	if (reply)
		mafw_dbus_send(con, reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static void
renderer_added_cb(MafwRegistry * registry, GObject *renderer,
                  gpointer user_data)
{
	/* We are not interested about the renderers */
	mafw_registry_remove_extension(registry, MAFW_EXTENSION(renderer));
}

void init_playlist_wrapper(DBusConnection *dbus, gboolean opt_stayalive,
				gboolean opt_kill)
{
	gboolean name_acquired;
	GError *gerr = NULL;
	DBusError dbe;
	DBusObjectPathVTable path_vtable;

	memset(&path_vtable, 0, sizeof(DBusObjectPathVTable));
	path_vtable.message_function = request;

	dbus_error_init(&dbe);

	/* Demand our name. */
	name_acquired = FALSE;
	do {
		switch (dbus_bus_request_name(dbus, MAFW_PLAYLIST_SERVICE,
				      DBUS_NAME_FLAG_DO_NOT_QUEUE, &dbe)) {
		case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
			if (!opt_stayalive)
				exit(0);
			name_acquired = TRUE;
			break;
		case -1:
			g_error("dbus_bus_request_name(%s): %s",
				MAFW_PLAYLIST_SERVICE, dbe.name);
		default:
			/* Another daemon is running. */
			if (opt_kill) {
				mafw_dbus_send(dbus, mafw_dbus_signal("die"));
				if (!opt_stayalive)
					/* Mission completed */
					exit(0);
				g_warning("Hijacking already running daemon");
			} else {
				/* We need that name but cannot get it.
				 * Make this event distinguishable so
				 * the parent won't panic.  Also make
				 * the exit code different from whatever
				 * used to launch the daemon.
				 */
				g_warning("dbus_bus_request_name(%s): %s",
					  MAFW_PLAYLIST_SERVICE,
					  "service already running");
				exit(11);
			}
		} /* while */
	} while (!name_acquired);

	/* Set up playlist storage. */
	Playlists = g_tree_new_full((GCompareDataFunc)pls_cmpids,
				    NULL, NULL,
				    (GDestroyNotify)pls_free);
	/* Set up a secondary tree, mapping names => playlists.  We don't ever
	 * free the values, as all are aliases of the Playlists tree.  Keys
	 * (playlist names) are however duplicated. */
	Playlists_by_name = g_tree_new_full((GCompareDataFunc)strcmp, NULL,
					    (GDestroyNotify)g_free, NULL);

	/* Load existing playlists. */
	load_playlists();
	dbus_bus_add_match(dbus, "type='signal',"
                          "interface='" MAFW_PLAYLIST_INTERFACE "'",
                          &dbe);
       if (dbus_error_is_set(&dbe))
               g_error("dbus_bus_add_match: %s", dbe.name);
	/* Set up $dbus. */
	if (!dbus_connection_register_fallback(dbus, MAFW_PLAYLIST_PATH,
                                               &path_vtable, NULL))
		g_error("dbus_connection_register_fallback: failed");

	g_type_init();
	Reg = mafw_registry_get_instance();
	g_signal_connect(Reg, "renderer-added", (GCallback)renderer_added_cb,
                         NULL);
	if (!mafw_shared_init(Reg, &gerr))
		g_error("Error during discover init: %s", gerr->message);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
