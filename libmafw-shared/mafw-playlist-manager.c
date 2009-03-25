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

/* Include files */
#include "config.h"

#include <string.h>
#include <dbus/dbus.h>

#include <libmafw/mafw-errors.h>

#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"

#include "mafw-playlist-manager.h"
#include "mafw-marshal.h"

/**
 * SECTION:mafwplaylistmanager
 * @short_description: Maintainer of the framework-wide playlists for
 * a user
 *
 * Framework-wide playlists are #MafwPlaylist objects which represent
 * shared resources among all applications of the framework.  All
 * applications know about the creation and the destruction of those
 * playlists, and all of them can manipulate them equally, and will
 * see the changes made by others.
 *
 * The manager is the only one which can create #MafwPlaylist objects.
 * It ensures that the same playlist is always represented by he same
 * object instance. Initially the list is empty, objects are created
 * and added only when needed.  The list may not contain every
 * existing framework-wide playlists, but it should not have any one
 * which have been destructed (for long times).
 *
 * The manager communicates with a daemon (the playlist daemon)
 * through D-BUS, which is responsible for the central maintenance of
 * playlists.  The daemon sends signals about playlist creations and
 * destructions which are relayed by the manager to the application.
 *
 * Since multiple instances of manager (in multiple applications) may be
 * running at the same time it may happen more than one of them attempts
 * to create or destroy the same list.  This case care is taken to inform
 * every application exactly once about the change in the list of playlists.
 * To achieve this the manager does NOT emit #GObject signals until the
 * daemon confirms the operation.  In the case of creation, the playlist
 * is registered immediately (because its success can be taken for granted
 * when the D-BUS method call returns).  For destruction the playlist is
 * NOT unregistered until comfirmation arrives.
 *
 * One application has at most one instance of the manager, which can
 * be accessed with mafw_playlist_manager_get().  While this object
 * must not be unref:ed by the caller, it is free to keep a copy of
 * the pointer.
 */

/* Standard definitions */
/* For mafw-dbus: */
#define MAFW_DBUS_PATH		MAFW_PLAYLIST_PATH
#define MAFW_DBUS_DESTINATION	MAFW_PLAYLIST_SERVICE
#define MAFW_DBUS_INTERFACE 	MAFW_PLAYLIST_INTERFACE


/* Type definitions */
struct _MafwPlaylistManagerPrivate {
	/* List of #MafwPlaylist:s the manager has already discovered.
	 * It may not contain every framework-wide playlists, but
	 * nonexisting ones should not remain here for long times.
	 * The list own a reference count on every playlists it has. */
	GPtrArray *playlists;
};

static GHashTable *import_requests;

struct _import_req {
	MafwPlaylistManagerImportCb cb;
	gpointer udata;
};

/*
 * This structure binds a GObject signal name and number together.
 * @name is also used as the member name for the corresponding D-BUS
 * signal.
 */
typedef struct {
	gchar const *name;
	guint id;
} GSignalDesc;

/* Function prototypes */
static GArray *do_get_playlists(GError **errp);
static void mafw_playlist_manager_finalize(MafwPlaylistManager *self);
static DBusHandlerResult dbus_handler(DBusConnection *con, DBusMessage *msg,
				      MafwPlaylistManager *self);

/* Private variables */
/*
 * The single instance of the playlist manager for a user.
 * TODO This object is never deallocated, which may be
 * interpreted as a memory leak by some detectors.
 */
static MafwPlaylistManager *PlaylistManager;

static GSignalDesc Signal_list_created   =
	{ MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED		};
static GSignalDesc Signal_list_destroyed =
	{ MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED	};
static GSignalDesc Signal_list_destruction_failed =
	{ MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTRUCTION_FAILED	};

/* Program code */
/* Class construction */
G_DEFINE_TYPE(MafwPlaylistManager, mafw_playlist_manager,
	      G_TYPE_OBJECT);

static void mafw_playlist_manager_class_init(
					MafwPlaylistManagerClass *me)
{
	g_type_class_add_private(me, sizeof(MafwPlaylistManagerPrivate));

	/* Fill in the VMT. */
	G_OBJECT_CLASS(me)->finalize =
	       	(void *)mafw_playlist_manager_finalize;

	/* Register signals */
/**
 * MafwPlaylistManager::playlist-created:
 * @playlist: the playlist object of #MafwPlaylist just created.
 *
 * This signal informs the application that #playlist has been created.
 * It is guaranteed that the framework will always represent the playlist
 * with #playlist (this object).
 */
	Signal_list_created.id = g_signal_new(
		Signal_list_created.name, G_TYPE_FROM_CLASS(me),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		mafw_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, G_TYPE_OBJECT);
/**
 * MafwPlaylistManager::playlist-destroyed:
 * @playlist: the playlist object of #MafwPlaylist just destroyed.
 *
 * This signal informs the application that the framework is no longer
 * handling @playlist, which, after the emission of the signal is
 * unref()ed.  While the application may keep the object by increasing
 * its reference count, all further operations on it will fail.  This
 * signal is only sent about playlists the application has a reference
 * to, ie. it was obtained via mafw_playlist_manager_get_playlist()
 * or mafw_playlist_manager_list_playlists().
 */
	Signal_list_destroyed.id = g_signal_new(
		Signal_list_destroyed.name, G_TYPE_FROM_CLASS(me),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		mafw_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, G_TYPE_OBJECT);

/**
 * MafwPlaylistManager::playlist-destruction-failed:
 * @playlist: the playlist object of #MafwPlaylist which destruction is not
 * allowed.
 *
 * This signal informs the application that the destruction of the playlist is 
 * not allowed. This signal is only sent about playlists the application has a
 * reference to, ie. it was obtained via mafw_playlist_manager_get_playlist() 
 * or mafw_playlist_manager_list_playlists().
 */
	Signal_list_destruction_failed.id = g_signal_new(
		Signal_list_destruction_failed.name, G_TYPE_FROM_CLASS(me),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		mafw_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, G_TYPE_OBJECT);

}

/* Object construction */
/**
 * mafw_playlist_manager_get:
 *
 * Method supporting the singleton.
 *
 * Returns: the playlist manager of a process. A process has only one
 * object like this, and it must not be released by the caller.
 */
MafwPlaylistManager *mafw_playlist_manager_get(void)
{
	if (!PlaylistManager)
		PlaylistManager = g_object_new(
					 MAFW_TYPE_PLAYLIST_MANAGER,
					 NULL);
	return PlaylistManager;
}

static void mafw_playlist_manager_init(MafwPlaylistManager *self)
{
	DBusConnection *dbus;

	/* Initialize .priv.
	 * TODO Is it necessary to clear it? */
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
					 MAFW_TYPE_PLAYLIST_MANAGER,
					 MafwPlaylistManagerPrivate);
	memset(self->priv, 0, sizeof(*self->priv));

	/* NOTE .playlists is not NULL-terminated. */
	self->priv->playlists = g_ptr_array_new();

	/* Let dbus_handler() see all messages we're interested in. */
	if ((dbus = mafw_dbus_session(NULL)) != NULL) {
		DBusError dbe;

		dbus_bus_start_service_by_name(dbus,MAFW_PLAYLIST_SERVICE, 0,
				NULL, NULL);
		dbus_error_init(&dbe);
		dbus_connection_add_filter(dbus,
			(DBusHandleMessageFunction)dbus_handler, self, NULL);
		dbus_bus_add_match(dbus, "type='signal',"
                           "interface='" MAFW_PLAYLIST_INTERFACE "'",
                           &dbe);
		if (dbus_error_is_set(&dbe))
			g_error("dbus_bus_add_match: %s", dbe.name);
		dbus_bus_add_match(dbus, "type='signal',"
				   "interface='" DBUS_INTERFACE_DBUS "',"
				   "member='NameOwnerChanged'",
				   &dbe);
		if (dbus_error_is_set(&dbe))
			g_error("dbus_bus_add_match: %s", dbe.name);
		dbus_connection_setup_with_g_main(dbus, NULL);
		dbus_connection_unref(dbus);
	}
}

static void mafw_playlist_manager_finalize(
					   MafwPlaylistManager *self)
{
	/* While users are not expected to finalize us
	 * test cases may want to do it. */
	g_warning("PlaylistManager is shutting down");

	/* Free .playlists. */
	g_ptr_array_foreach(self->priv->playlists,
			    (GFunc)g_object_unref, NULL);
	g_ptr_array_free(self->priv->playlists, TRUE);
	
	if (import_requests)
		g_hash_table_destroy(import_requests);

	/* Clear $PlaylistManager so *_get()
	 * won't return invalid object. */
	if (PlaylistManager == self)
		PlaylistManager = NULL;
}

/* Private functions */
/*
 * Search .playlists and add a new object if none of them is identified
 * by $id.  Returns either the found or the newly created playlist.
 * Doesn't alter refcounts.
 */
static MafwProxyPlaylist *register_playlist(
					   MafwPlaylistManager *self,
					   guint id)
{
	guint i;
	GPtrArray *playlists;
	MafwProxyPlaylist *playlist;

	/* Check if an object with $id already exists in .playlists.
	 * Create it only if it doesn't. */
	playlists = self->priv->playlists;
	for (i = 0; i < playlists->len; i++)
		if (mafw_proxy_playlist_get_id(playlists->pdata[i]) == id)
			/* Found it */
			return playlists->pdata[i];

	/* No more playlists, so add the new one. */
	playlist = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(id));
	g_ptr_array_add(playlists, playlist);
	return playlist;
}

/* Watch incomming D-BUS signals and keep .playlists updated. */
static DBusHandlerResult dbus_handler(DBusConnection *con, DBusMessage *msg,
				      MafwPlaylistManager *self)
{
	char const *iface, *member;

	/* First of all, if the daemon died (and hopefully restarted), our list
	 * of playlists might be outdated.  We detect this case by listening to
	 * NameOwnerChanged messages. */
	if (dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS,
				   "NameOwnerChanged"))
	{
		guint i;
		gchar *name, *oldname, *newname;
		GPtrArray *playlists;
		GArray *ids;
		GError *err = NULL;

		name = oldname = newname = NULL;
		mafw_dbus_parse(msg,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_STRING, &oldname,
				DBUS_TYPE_STRING, &newname);

		if (strcmp(name, MAFW_PLAYLIST_SERVICE) ||
		    !newname || !newname[0])
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		/* An UI may have a `reference' to our priv->playlists array, so
		 * we must not change it suddenly.  We query the list of
		 * playlist ids from the new playlist daemon first... */
		if (!(ids = do_get_playlists(&err))) {
			g_warning("Cannot re-fetch playlist ids: %s",
				  err->message);
			g_error_free(err);
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		/* ... and now we have the new list of playlists.  Remove each
		 * playlist (and emit a signal) from the old list which don't
		 * exist in the new.  It can be slow, as this is not expected to
		 * happen often. */
		playlists = self->priv->playlists;
		for (i = 0; i < playlists->len; ++i) {
			guint j, id;
			MafwProxyPlaylist *pls;

			id = mafw_proxy_playlist_get_id(playlists->pdata[i]);
			for (j = 0; j < ids->len; ++j)
				if (g_array_index(ids, guint, i) == id)
					break;
			if (j < ids->len)
				continue;
			pls = g_ptr_array_remove_index(playlists, i);
			g_assert(pls);
			if (G_OBJECT(pls)->ref_count > 1)
				g_signal_emit(self, Signal_list_destroyed.id,
					      0, pls);
			g_object_unref(pls);
		}
		g_array_free(ids, TRUE);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	iface = dbus_message_get_interface(msg);
	if (!iface || strcmp(iface, MAFW_PLAYLIST_INTERFACE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	/* Let's see what to do. */
	member = dbus_message_get_member(msg);
	g_assert(member != NULL);

	if (!strcmp(member, Signal_list_created.name)) {
		guint id;

		/* Someone has created a playlist.  See its
		 * identification parameters and cast the word. */
		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &id);
		g_signal_emit(self, Signal_list_created.id, 0,
			      register_playlist(self, id));
	} else if (!strcmp(member, Signal_list_destroyed.name)) {
		guint i;
		GPtrArray *playlists;
		guint id;
		MafwProxyPlaylist *playlist;

		/* Parse error is an impossible event, because
		 * the message must have been sent by us. */
		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &id);

		/* Do we have an object for this playlist?
		 * Remove it from .playlists at once. */
		playlist = NULL;
		playlists = self->priv->playlists;
		for (i = 0; i < playlists->len; i++)
			if (mafw_proxy_playlist_get_id(playlists->pdata[i])
			    == id) {
				playlist = g_ptr_array_remove_index(playlists,
								    i);
				break;
			}

		/*
		 * Don't send the signal unless we found it in our repo
		 * because this case the UI cannot possible be interested
		 * in its destruction because if it had a reference to it
		 * we'd have it too.
		 */
		if (playlist) {
			if (G_OBJECT(playlist)->ref_count > 1)
				g_signal_emit(self, Signal_list_destroyed.id, 0,
					      playlist);
			g_object_unref(playlist);
		}
	} else if (!strcmp(member, Signal_list_destruction_failed.name)) {
		guint i;
		GPtrArray *playlists;
		guint id;
		MafwProxyPlaylist *playlist;

		/* Parse error is an impossible event, because
		 * the message must have been sent by us. */
		mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &id);

		/* Do we have an object for this playlist? */
		
		playlist = NULL;
		playlists = self->priv->playlists;
		for (i = 0; i < playlists->len; i++)
			if (mafw_proxy_playlist_get_id(playlists->pdata[i])
			    == id) {
				playlist = g_ptr_array_index(playlists, i);
				break;
			}
		/*
		 * Don't send the signal unless we found it in our repo
		 * because this case the UI cannot possible be interested
		 * in its destruction because if it had a reference to it
		 * we'd have it too.
		 */
		if (playlist) {
			if (G_OBJECT(playlist)->ref_count > 1)
				g_signal_emit(self, Signal_list_destruction_failed.id, 0,
					      playlist);
			g_object_unref(playlist);
		}
	} else if (!strcmp(member, MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED)) {
		guint new_id, import_id;
		struct _import_req *req;
		gchar *domain_str;
		gint code;
		gchar *message;
		GError *error = NULL;

		/* Check whether the message contains error */
		if (mafw_dbus_count_args(msg) == 2) {
			/* Playlist imported */
			mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &import_id,
						DBUS_TYPE_UINT32, &new_id);
		}
		else
		{
			mafw_dbus_parse(msg, DBUS_TYPE_UINT32, &import_id,
					DBUS_TYPE_STRING, &domain_str,
					DBUS_TYPE_INT32, &code,
					DBUS_TYPE_STRING, &message);
			g_set_error(&error,g_quark_from_string(domain_str),
				    code, "%s", message);
		}
		
		req = g_hash_table_lookup(import_requests,
				  GUINT_TO_POINTER(import_id));
		if (!req)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		req->cb(self, import_id, error ? NULL :
				register_playlist(self, new_id),
			req->udata, error);
		if (error)
			g_error_free(error);
		g_hash_table_remove(import_requests,
				    GUINT_TO_POINTER(import_id));
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	/*
	 * Pretend we didn't handle the message, so other filters in
	 * the application can see it.  May come handy while debugging.
	 *
	 * Also, everyone listening to DBus session bus is affected by this
	 * result (not just THIS application), so keep the signals available
	 * to them as well.
	 *
	 * TODO Is it really so?
	 */
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Methods */
/**
 * mafw_playlist_manager_create_playlist:
 * @self: a #MafwPlaylistManager instance.
 * @name: name of the playlist.
 * @errp: a #GError to store an error if needed
 *
 * Creates a playlist with @name (an UTF-8 string), makes it available
 * to other applications, and returns the object, to be
 * g_object_unref()ed by the caller.  If a playlist with @name already
 * existed it is not created again, but the same object is returned.
 * If a new playlist was created, the
 * #MafwPlaylistManager::playlist_created signal is emitted.  On
 * error @errp is set.
 *
 * Returns: a #MafwProxyPlaylist which has @name or %NULL on error.
 * In this case no new playlist is created.
 */
MafwProxyPlaylist *mafw_playlist_manager_create_playlist(
					   MafwPlaylistManager *self,
					   gchar const *name,
					   GError **errp)
{
	DBusMessage *reply;
	DBusConnection *dbus;
	guint id;

	/*
	 * Ask the playlist daemon to create the playlist without checking
	 * the existence of $name in .playlists.  This is because neither
	 * shared nor persistent playlists know about their name and they
	 * would need to consult external resources (daemon, db) anyway.
	 */
	if (!(dbus = mafw_dbus_session(errp)))
		return NULL;
	reply = mafw_dbus_call(dbus, mafw_dbus_method(
				      MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
				      MAFW_DBUS_STRING(name)),
			       MAFW_PLAYLIST_ERROR, errp);
	dbus_connection_unref(dbus);
	if (!reply)
		return NULL;

	mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &id);
	dbus_message_unref(reply);

	/* Signal the creation of the playlist if we get the word from the
	 * playlist daemon.  This is different from signaling here because
	 * we cannot tell if the playlists existed before, which is what
	 * the signal is supposed to inform us about. */
	return g_object_ref(register_playlist(self, id));
}

/**
 * mafw_playlist_manager_destroy_playlist:
 * @self: the #MafwPlaylistManager object
 * @playlist: the playlist to destroy.
 * @errp: a #GError to store an error if needed
 *
 * Decreases the refcount of @playlist and begins its removal from the
 * list of framework-wide playlists.  @playlist may remain included in
 * response to inquires about the list of playlists for a while.  When
 * the removal is complete a #MafwPlaylistManager::playlist_destroyed
 * signal is emitted.  If the playlist cannot be removed because it is
 * being used by some renderer a
 * #MafwPlaylistManager::playlist_destruction_failed signal is
 * emitted.  Attempts to destroy nonexisting playlists result in no
 * operation.
 *
 * Returns: the outcome of the request.  On failure no playlist is
 * destroyed.
 */
gboolean mafw_playlist_manager_destroy_playlist(
					   MafwPlaylistManager *self, 
					   MafwProxyPlaylist *playlist,
					   GError **errp)
{
	DBusMessage *msg;
	DBusConnection *dbus;

	/* Send the destroy command to the daemon.  The NO_REPLY flag
	 * needs to be set otherwise dbusd or someone becomes upset
	 * and denies further message passing between us and the daemon. */
	if (!(dbus = mafw_dbus_session(errp)))
		return FALSE;
	msg = mafw_dbus_method(
		  MAFW_PLAYLIST_METHOD_DESTROY_PLAYLIST,
		  MAFW_DBUS_UINT32(mafw_proxy_playlist_get_id(playlist)));
	dbus_message_set_no_reply(msg, TRUE);
	mafw_dbus_send(dbus, msg);
	dbus_connection_unref(dbus);

	/* Don't remove $playlist from .playlists
	 * and don't signal until the daemon reacts. */
	g_object_unref(playlist);
	return TRUE;
}

/**
 * mafw_playlist_manager_get_playlist:
 * @self: the #MafwPlaylistManager
 * @id: ID of a playlist for which the object to be returned.
 * @errp: a #GError to store an error if needed
 *
 * Finds out whether a playlist with @id exists in the framework.
 *
 * Returns: the #MafwProxyPlaylist representation of the playlist
 * or %NULL if it doesn't exist or an error encountered.  This case
 * @errp is set.  Otherwise it is guaranteed that a playlist with
 * a given ID is always represented by the same object.
 */
MafwProxyPlaylist *mafw_playlist_manager_get_playlist(
					   MafwPlaylistManager *self,
					   guint id,
					   GError **errp)
{
	guint i;
	GPtrArray *playlists;
	MafwProxyPlaylist *playlist;
	DBusConnection *dbus;
	DBusMessage *reply;
	DBusMessageIter imsg, iary;

	/* Shortcut if we can already tell $id is invalid. */
	if (id == MAFW_PROXY_PLAYLIST_INVALID_ID)
		return NULL;

	/* Check if we already have an object with $id in .playlists. */
	playlists = self->priv->playlists;
	for (i = 0; i < playlists->len; i++)
		if (mafw_proxy_playlist_get_id(playlists->pdata[i]) == id)
			return g_object_ref(playlists->pdata[i]);

	/* Not found, ask the daemon. */
	if (!(dbus = mafw_dbus_session(errp)))
		return NULL;
	reply = mafw_dbus_call(dbus, mafw_dbus_method(
					MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32, guint, id)),
				MAFW_PLAYLIST_ERROR, errp);
	dbus_connection_unref(dbus);
	if (!reply)
		return NULL;

	/* We're only interested in the presence of return arguments,
	 * which signifies the existence of the inquired playlist id. */
	dbus_message_iter_init(reply, &imsg);
	g_assert(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);
	playlist = dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_INVALID
		? g_object_ref(register_playlist(self, id)) : NULL;
	dbus_message_unref(reply);

	return playlist;
}

/* Returns a #GArray of guint's containing the playlist ids known by the
 * playlist daemon.  @errp is set in case of errors. */
static GArray *do_get_playlists(GError **errp)
{
	DBusMessage *reply;
	DBusConnection *dbus;
	DBusMessageIter imsg, iary, istr;
	GArray *ids;

	if (!(dbus = mafw_dbus_session(errp)))
		return NULL;
	reply = mafw_dbus_call(dbus, mafw_dbus_method(
					MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS),
				MAFW_PLAYLIST_ERROR, errp);
	dbus_connection_unref(dbus);
	if (!reply)
		return NULL;

	dbus_message_iter_init(reply, &imsg);
	g_assert(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);
	ids = g_array_new(FALSE, FALSE, sizeof(guint));
	while (dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_INVALID)
	{
		guint id;

		dbus_message_iter_recurse(&iary, &istr);
		g_assert(dbus_message_iter_get_arg_type(&istr)
			== DBUS_TYPE_UINT32);
		dbus_message_iter_get_basic(&istr, &id);
		g_array_append_val(ids, id);

		dbus_message_iter_next(&iary);
	}
	dbus_message_unref(reply);
	return ids;
}

/**
 * mafw_playlist_manager_get_playlists:
 * @self: the #MafwPlaylistManager
 * @errp: a #GError to store the error if needed
 *
 * Gets the playlists stored on the manager
 *
 * Returns: %NULL on error and sets #errp.  Otherwise an array of
 * #MafwPlaylist objects enumerating all the framework-wide playlists.
 * The array is kept up to date with changes in the list of known
 * playlists.  The caller may not modify nor free the returned
 * array.  Objects in the list are the same as one would get from
 * mafw_playlist_manager_get_playlist().
 */
GPtrArray *mafw_playlist_manager_get_playlists(
					   MafwPlaylistManager *self,
					   GError **errp)
{
	GArray *ids;
	guint i;

	/* Query the daemon and create all playlists missing from .playlists.
	 * After this call .playlist will be up to date as long as we exist. */
	if (!(ids = do_get_playlists(errp)))
		return NULL;

	for (i = 0; i < ids->len; ++i)
		register_playlist(self, g_array_index(ids, guint, i));
	g_array_free(ids, TRUE);

	/* Ownership of the list is retained. */
	return self->priv->playlists;
}

/**
 * mafw_playlist_manager_list_playlists:
 * @self: a #MafwPlaylistManager
 * @errp: a #GError to store an error if needed
 *
 * List the playlist stored in the #MiasPlaylistManager
 *
 * Returns: %NULL on error and sets @errp.  Otherwise a zero-terminated
 * array of #MafwPlaylistManagerItem objects of the shared playlists the
 * framework currently knows about.  The list is not kept up to date with
 * respect to changes in the shared playlists and is not ordered by any
 * means.  It is the caller's responsibility to free the returned array,
 * for example with mafw_playlist_manager_free_list_of_playlists().
 */
GArray *mafw_playlist_manager_list_playlists(
					   MafwPlaylistManager *self,
					   GError **errp)
{
	GArray *playlists;
	DBusMessage *reply;
	DBusConnection *dbus;
	DBusMessageIter imsg, iary, istr;

	if (!(dbus = mafw_dbus_session(errp)))
		return NULL;
	reply = mafw_dbus_call(dbus, mafw_dbus_method(
				     MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS),
			       MAFW_PLAYLIST_ERROR, errp);
	dbus_connection_unref(dbus);
	if (!reply)
		return NULL;

	/* Parse $reply and fill $playlists with its contents. */
	playlists = g_array_new(TRUE, FALSE,
			       	sizeof(MafwPlaylistManagerItem));
	dbus_message_iter_init(reply, &imsg);
	g_assert(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);
	while (dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_INVALID)
	{
		MafwPlaylistManagerItem plit;

		/* Playlist ID */
		dbus_message_iter_recurse(&iary, &istr);
		g_assert(dbus_message_iter_get_arg_type(&istr)
			== DBUS_TYPE_UINT32);
		dbus_message_iter_get_basic(&istr, &plit.id);

		/* Playlist name */
		dbus_message_iter_next(&istr);
		g_assert(dbus_message_iter_get_arg_type(&istr)
			== DBUS_TYPE_STRING);
		dbus_message_iter_get_basic(&istr, &plit.name);

		/* Save $plit in $playlists and move on. */
		plit.name = g_strdup(plit.name);
		g_array_append_val(playlists, plit);
		dbus_message_iter_next(&iary);
	}
	dbus_message_unref(reply);

	return playlists;
}

/**
 * mafw_playlist_manager_free_list_of_playlists:
 * @playlist_list: an array of #MafwPlaylistManagerItem to free
 * 		   or %NULL.
 *
 * Releases @playlist_list and all dynamically allocated structures
 * contained therein.  If @playlist_list is %NULL it does nothing.
 */
void mafw_playlist_manager_free_list_of_playlists(
					   GArray *playlist_list)
{
	guint i;

	if (!playlist_list)
		return;
	for (i = 0; i < playlist_list->len; i++)
		g_free(g_array_index(playlist_list,
				     MafwPlaylistManagerItem, i).name);
	g_array_free(playlist_list, TRUE);
}

/**
 * mafw_playlist_manager_import:
 * @self: a #MafwPlaylistManager instance.
 * @playlist: Uri to playlist, playlist object id or container objectid.
 * @cb: callback to be called, when it finished with the import, or error occured
 * @base_uri: If not %NULL, used as prefix to resolve relative paths found from
 * playlist.
 * @user_data:     Optional user data pointer passed along with @browse_cb.
 * @error:	   Return location for a #GError, or %NULL
 *
 * Imports external playlists files and shares them in mafw environment.
 * Returns: The identifier of the import session (which is also passed
 *          to @cb). If some arguments were invalid,
 *          %MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID is returned.
 */
guint mafw_playlist_manager_import(MafwPlaylistManager *self,
				    const gchar *playlist,
				    const gchar *base_uri,
				    MafwPlaylistManagerImportCb cb,
				    gpointer user_data,
				    GError **error)
{
	DBusConnection *dbus;
	DBusMessage *reply;
	guint import_id;
	
	g_return_val_if_fail(self != NULL, MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID);
	g_return_val_if_fail(playlist != NULL && playlist[0] != '\0',
				MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID);
	g_return_val_if_fail(cb != NULL, MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID);
	
	if (!(dbus = mafw_dbus_session(error)))
		return MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID;

	reply = mafw_dbus_call(
		dbus,
		mafw_dbus_method(MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(playlist),
				  MAFW_DBUS_STRING(base_uri ? base_uri : "")),
				  MAFW_SOURCE_ERROR, error);

	if (reply) {
		struct _import_req *new_req;
		mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &import_id);
		/* Remember this new request. */
		dbus_message_unref(reply);
		
		if (!import_requests) {
			import_requests = g_hash_table_new_full(NULL,
							      	NULL,
							      	NULL,
							      	g_free);
		}
		
		new_req = g_new0(struct _import_req, 1);
		new_req->cb = cb;
		new_req->udata = user_data;
		g_hash_table_replace(import_requests,
				     GUINT_TO_POINTER(import_id),
				     new_req);
		
	}
	else
		import_id = MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID;
	
	dbus_connection_unref(dbus);
	
	return import_id;
}

/**
 * mafw_playlist_manager_cancel_import:
 * @self:      A MafwPlaylistManager instance.
 * @import_id: The import session to cancel.
 * @error:     Location for a #GError, or %NULL
 *
 * Cancels an already running import session.
 *
 * Returns: %FALSE if the operation failed (in which case @error is also set).
 */
gboolean mafw_playlist_manager_cancel_import(MafwPlaylistManager *self,
					  guint import_id, 
					  GError **error)
{
	struct _import_req *req;

	req = g_hash_table_lookup(import_requests,
				  GUINT_TO_POINTER(import_id));
	if (!req)
	{
		g_set_error(error,MAFW_PLAYLIST_ERROR,
			    MAFW_PLAYLIST_ERROR_INVALID_IMPORT_ID,
			    "Invalid import-ID");

		return FALSE;
	}
	else
	{
		DBusConnection *dbus;
		DBusMessage *reply;
		/* The request is still in progress. */
		
		dbus = mafw_dbus_session(NULL);

		/* $req doesn't contain dynamically allocated data
		 * we care about. */
		g_hash_table_remove(import_requests,
				    GUINT_TO_POINTER(import_id));

		/* Tell our mate to cancel. */
		reply = mafw_dbus_call(dbus, mafw_dbus_method(
					       MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
					       MAFW_DBUS_UINT32(import_id)),
				       MAFW_PLAYLIST_ERROR, error);
		if (reply)
		{
			dbus_message_unref(reply);
		}
		else
		{
			dbus_connection_unref(dbus);
			return FALSE;
		}
		dbus_connection_unref(dbus);
	}
	return TRUE;
}
/**
 * mafw_playlist_manager_dup_playlist:
 * @self:      A MafwPlaylistManager instance.
 * @playlist: playlist to duplicate
 * @new_name: name for the duplicated playlist.
 * @errp: a #GError to store an error if needed
 *
 * Duplicates the playist to @new_name.
 * If a playlist with @new_name already
 * existed it is not created again.
 * If playlist was duplicated, the
 * #MafwPlaylistManager::playlist_created signal is emitted.  On
 * error @errp is set.
 *
 * Returns: The new duplicated playlist, or %NULL if there was any error. Unref
 * it, if you don't need this object.
 */
MafwProxyPlaylist *mafw_playlist_manager_dup_playlist(
					   MafwPlaylistManager *self,
                                           MafwProxyPlaylist *playlist,
                                           gchar const *new_name,
                                           GError **errp)
{
        DBusMessage *reply;
        DBusConnection *dbus;
        guint id, new_id;

	g_return_val_if_fail(playlist, NULL);
	g_return_val_if_fail(new_name, NULL);

        if (!(dbus = mafw_dbus_session(errp)))
                return NULL;
	id = mafw_proxy_playlist_get_id(playlist);

        reply = mafw_dbus_call(dbus, mafw_dbus_method(
                                      MAFW_PLAYLIST_METHOD_DUP_PLAYLIST,
                                      MAFW_DBUS_UINT32(id), MAFW_DBUS_STRING(new_name)),
                               MAFW_PLAYLIST_ERROR, errp);
        dbus_connection_unref(dbus);
        if (!reply)
                return NULL;
	mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &new_id);
	dbus_message_unref(reply);

        return g_object_ref(register_playlist(self, new_id));
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
