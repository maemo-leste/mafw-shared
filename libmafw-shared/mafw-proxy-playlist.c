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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libmafw/mafw-playlist.h>
#include <libmafw/mafw-db.h>
#include "mafw-proxy-playlist.h"
#include "mafw-playlist-manager.h"
#include "mafw-marshal.h"
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"

#define MAFW_DBUS_DESTINATION	MAFW_PLAYLIST_SERVICE
#define MAFW_DBUS_INTERFACE	MAFW_PLAYLIST_INTERFACE

struct _MafwProxyPlaylistPrivate {
	guint id;
	DBusConnection *connection;
	gchar *obj_path;
};

#define MAFW_PROXY_PLAYLIST_GET_PRIVATE(o)			\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o),			\
				      MAFW_TYPE_PROXY_PLAYLIST,	\
				      MafwProxyPlaylistPrivate))

/**
 * SECTION:mafwproxyplaylist
 * @short_description: Proxy for shared playlist
 *
 * #MafwProxyPlaylist is a shared playlist which can be shared among
 * multiple processes.  Changes are announced on D-Bus.
 */

/*---------------------------------------------------------------------------
  Static prototypes
  ---------------------------------------------------------------------------*/


static DBusHandlerResult dispatch_message(DBusConnection *conn,
						DBusMessage *msg,
						MafwProxyPlaylist *self);

/* Common properties */

static void mafw_proxy_playlist_set_name(
					  MafwProxyPlaylist *playlist,
					  const gchar *name);
static gchar *mafw_proxy_playlist_get_name(
					MafwProxyPlaylist *playlist,
				       	GError **error);

static void mafw_proxy_playlist_set_repeat(
					  MafwProxyPlaylist *playlist,
					  gboolean repeat);
static gboolean mafw_proxy_playlist_get_repeat(
					  MafwProxyPlaylist *playlist,
					  GError **error);

static gboolean mafw_proxy_playlist_shuffle(MafwPlaylist *playlist,
						 GError **error);
static gboolean mafw_proxy_playlist_is_shuffled(
					  MafwProxyPlaylist *playlist,
					  GError **error);
static gboolean mafw_proxy_playlist_unshuffle(MafwPlaylist *self,
						   GError **error);

/* Use count manipulation */

static gboolean mafw_proxy_playlist_increment_use_count(
	MafwPlaylist *playlist,
	GError **error);

static gboolean mafw_proxy_playlist_decrement_use_count(
	MafwPlaylist *playlist,
	GError **error);

/* Item manipulation */

static gboolean mafw_proxy_playlist_insert_item(MafwPlaylist *playlist,
						     guint index,
						     const gchar *objectid,
						     GError **error);
static gboolean mafw_proxy_playlist_append_item(MafwPlaylist *playlist,
						 const gchar *objectid,
						 GError **error);
static gboolean mafw_proxy_playlist_insert_items(MafwPlaylist *playlist,
						     guint index,
						     const gchar **objectid,
						     GError **error);
static gboolean mafw_proxy_playlist_append_items(MafwPlaylist *playlist,
						 const gchar **objectid,
						 GError **error);
static gboolean mafw_proxy_playlist_remove_item(MafwPlaylist *playlist,
						     guint index,
						     GError **error);

static gchar *mafw_proxy_playlist_get_item(MafwPlaylist *playlist,
					       	guint index, GError **error);
static gchar **mafw_proxy_playlist_get_items(MafwPlaylist *playlist,
					       	guint first_index,
						guint last_index,
						GError **error);

static void mafw_proxy_playlist_get_starting_index(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error);

static void mafw_proxy_playlist_get_last_index(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error);

static gboolean mafw_proxy_playlist_get_next(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error);

static gboolean mafw_proxy_playlist_get_prev(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error);

static gboolean mafw_proxy_playlist_move_item(MafwPlaylist *playlist,
						   guint from, guint to,
						   GError **error);

static guint mafw_proxy_playlist_get_size(MafwPlaylist *playlist,
					       GError **error);

static gboolean mafw_proxy_playlist_clear(MafwPlaylist *playlist,
					       GError **error);



/*---------------------------------------------------------------------------
  Object/class initialization
  ---------------------------------------------------------------------------*/

enum {
	PROP_0,
	PROP_NAME,
	PROP_REPEAT,
	PROP_IS_SHUFFLED,
};

static void playlist_iface_init(MafwPlaylistIface *iface)
{
	iface->shuffle = mafw_proxy_playlist_shuffle;
	iface->unshuffle = mafw_proxy_playlist_unshuffle;
	iface->increment_use_count = mafw_proxy_playlist_increment_use_count;
	iface->decrement_use_count = mafw_proxy_playlist_decrement_use_count;
	iface->get_item = mafw_proxy_playlist_get_item;
	iface->get_items = mafw_proxy_playlist_get_items;
	iface->get_starting_index = mafw_proxy_playlist_get_starting_index;
	iface->get_last_index = mafw_proxy_playlist_get_last_index;
	iface->get_next = mafw_proxy_playlist_get_next;
	iface->get_prev = mafw_proxy_playlist_get_prev;
	iface->insert_item = mafw_proxy_playlist_insert_item;
	iface->append_item = mafw_proxy_playlist_append_item;
	iface->insert_items = mafw_proxy_playlist_insert_items;
	iface->append_items = mafw_proxy_playlist_append_items;
	iface->remove_item = mafw_proxy_playlist_remove_item;
	iface->clear = mafw_proxy_playlist_clear;
	iface->get_size = mafw_proxy_playlist_get_size;
	iface->move_item = mafw_proxy_playlist_move_item;
}

static void set_prop(MafwProxyPlaylist *playlist, guint prop,
		     const GValue *value, GParamSpec *spec)
{
	if (prop == PROP_NAME) {
		mafw_proxy_playlist_set_name(playlist,
						  g_value_get_string(value));
	} else if (prop == PROP_REPEAT) {
		mafw_proxy_playlist_set_repeat(playlist,
						    g_value_get_boolean(value));
	} else
		G_OBJECT_WARN_INVALID_PROPERTY_ID(playlist, prop, spec);
}

static void get_prop(MafwProxyPlaylist *playlist, guint prop,
		     GValue *value, GParamSpec *spec)
{
	if (prop == PROP_NAME) {
		gchar *name;

		name = mafw_proxy_playlist_get_name(playlist, NULL);
		g_value_take_string(value, name);
	} else if (prop == PROP_REPEAT) {
		g_value_set_boolean(value,
			mafw_proxy_playlist_get_repeat(playlist, NULL));
	} else if (prop == PROP_IS_SHUFFLED) {
		g_value_set_boolean(value,
				    mafw_proxy_playlist_is_shuffled(
							  playlist, NULL));
	} else
		G_OBJECT_WARN_INVALID_PROPERTY_ID(playlist, prop, spec);
}

static void mafw_proxy_playlist_finalize(GObject *object)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(object);
	MafwProxyPlaylistPrivate *priv;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);

	dbus_connection_unregister_object_path(priv->connection,
                                               priv->obj_path);

	dbus_connection_unref(priv->connection);
	g_free(priv->obj_path);
}

static void mafw_proxy_playlist_class_init(
					MafwProxyPlaylistClass *klass)
{
	GObjectClass *oclass = NULL;

	oclass = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(MafwProxyPlaylistPrivate));
	oclass->set_property = (gpointer)set_prop;
	oclass->get_property = (gpointer)get_prop;
	g_object_class_override_property(oclass, PROP_NAME, "name");
	g_object_class_override_property(oclass, PROP_REPEAT, "repeat");
	g_object_class_override_property(oclass,
					 PROP_IS_SHUFFLED, "is-shuffled");
	oclass -> finalize = mafw_proxy_playlist_finalize;
}

static void mafw_proxy_playlist_init(MafwProxyPlaylist *self)
{

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
						 MAFW_TYPE_PROXY_PLAYLIST,
						 MafwProxyPlaylistPrivate);
	memset(self->priv, 0, sizeof(*self->priv));

}



G_DEFINE_TYPE_WITH_CODE(MafwProxyPlaylist, mafw_proxy_playlist,
			G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE(MAFW_TYPE_PLAYLIST,
					      playlist_iface_init));


/*---------------------------------------------------------------------------
  New with ID
  ---------------------------------------------------------------------------*/

/**
 * mafw_proxy_playlist_new:
 * @id: the ID of the playlist the new object will represent.
 *
 * Create a new shared playlist object and bind it to the given existing
 * playlist ID.
 *
 * Returns: a new #MafwProxyPlaylist.
 **/
GObject *mafw_proxy_playlist_new(guint id)
{
	MafwProxyPlaylist *self;
	GError *err = NULL;
	DBusObjectPathVTable path_vtable;

	memset(&path_vtable, 0, sizeof(DBusObjectPathVTable));
	path_vtable.message_function =
		(DBusObjectPathMessageFunction)dispatch_message;

	self = g_object_new(MAFW_TYPE_PROXY_PLAYLIST, NULL);
	self->priv->id = id;
	self->priv->connection = mafw_dbus_session(&err);

	if (err)
	{
		g_error_free(err);
		return NULL;
	}

	self->priv->obj_path = g_strdup_printf("%s/%u",MAFW_PLAYLIST_PATH,id);
	dbus_connection_register_object_path(self->priv->connection,
			self->priv->obj_path,
			&path_vtable,
			self);

	return G_OBJECT(self);
}

/*---------------------------------------------------------------------------
  Get ID
  ---------------------------------------------------------------------------*/
/**
 * mafw_proxy_playlist_get_id:
 * @self: a #MafwProxyPlaylist
 *
 * Gets the proxy playlist id.
 *
 * Returns: a #guint with the given id.
 */
guint mafw_proxy_playlist_get_id(MafwProxyPlaylist *self)
{
	return self->priv->id;
}

/*---------------------------------------------------------------------------
  Set name
  ---------------------------------------------------------------------------*/

static void mafw_proxy_playlist_set_name(MafwProxyPlaylist *self,
					   const gchar *name)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);

	g_return_if_fail(priv->connection != NULL);
	g_return_if_fail(name != NULL);

	mafw_dbus_send(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
					MAFW_PLAYLIST_METHOD_SET_NAME,
					DBUS_TYPE_STRING, name));
}

/*---------------------------------------------------------------------------
  Get name
  ---------------------------------------------------------------------------*/

static gchar *mafw_proxy_playlist_get_name(MafwProxyPlaylist *self,
					 GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gchar *retval = NULL;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, NULL);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_GET_NAME),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		mafw_dbus_parse(reply, DBUS_TYPE_STRING, &retval);
		dbus_message_unref(reply);
		retval = g_strdup(retval);
	}


	return retval;
}

/*---------------------------------------------------------------------------
  Set repeat
  ---------------------------------------------------------------------------*/

static void mafw_proxy_playlist_set_repeat(MafwProxyPlaylist *self,
					     gboolean repeat)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_if_fail(priv->connection != NULL);

	mafw_dbus_send(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
					MAFW_PLAYLIST_METHOD_SET_REPEAT,
					DBUS_TYPE_BOOLEAN, repeat));

}

/*---------------------------------------------------------------------------
  Get repeat
  ---------------------------------------------------------------------------*/

gboolean mafw_proxy_playlist_get_repeat(MafwProxyPlaylist *self,
					     GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gboolean retval = FALSE;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
					MAFW_PLAYLIST_METHOD_GET_REPEAT),
				MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		mafw_dbus_parse(reply, DBUS_TYPE_BOOLEAN, &retval);
		dbus_message_unref(reply);
	}


	return retval;
}

/*---------------------------------------------------------------------------
  Set shuffle
  ---------------------------------------------------------------------------*/

/**
 * Changes all item's playing indices such that after the operation
 * none of them remains the same (unless the playlist has only one item).
 * On error diagnostics are printed and the playlist remains unmodified.
 */
gboolean mafw_proxy_playlist_shuffle(MafwPlaylist *self, GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_SHUFFLE),
			       MAFW_PLAYLIST_ERROR, error);
	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}

	return FALSE;
}

/*---------------------------------------------------------------------------
  Is shuffled
  ---------------------------------------------------------------------------*/

/**
 * Returns whether any item in the playlist has a different playing index
 * than a visual one. */
static gboolean mafw_proxy_playlist_is_shuffled(MafwProxyPlaylist *self,
                                                GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gboolean retval = FALSE;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_IS_SHUFFLED),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		mafw_dbus_parse(reply, DBUS_TYPE_BOOLEAN, &retval);
		dbus_message_unref(reply);
	}

	return retval;
}

/*---------------------------------------------------------------------------
  Unshuffle
  ---------------------------------------------------------------------------*/

/**
 * Restores all item's playing indices to their visual index.
 */
gboolean mafw_proxy_playlist_unshuffle(MafwPlaylist *self, GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_UNSHUFFLE),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}

	return FALSE;
}


/*---------------------------------------------------------------------------
  Increment use count
  ---------------------------------------------------------------------------*/

static gboolean mafw_proxy_playlist_increment_use_count(MafwPlaylist *self,
							 GError **error)
{

	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	g_return_val_if_fail(self != NULL, FALSE);
	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(
		priv->connection,
		mafw_dbus_method_full(
			MAFW_DBUS_DESTINATION,
			priv->obj_path,
			MAFW_DBUS_INTERFACE,
			MAFW_PLAYLIST_METHOD_INCREMENT_USE_COUNT),
		MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}

	return FALSE;
}

/*---------------------------------------------------------------------------
  Decrement use count
  ---------------------------------------------------------------------------*/

static gboolean mafw_proxy_playlist_decrement_use_count(MafwPlaylist *self,
							 GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(
		priv->connection,
		mafw_dbus_method_full(
			MAFW_DBUS_DESTINATION,
			priv->obj_path,
			MAFW_DBUS_INTERFACE,
			MAFW_PLAYLIST_METHOD_DECREMENT_USE_COUNT),
		MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}

	return FALSE;
}

/*---------------------------------------------------------------------------
  Insert Item
  ---------------------------------------------------------------------------*/

gboolean mafw_proxy_playlist_insert_item(MafwPlaylist *self, guint index,
					      const gchar *objectid,
					      GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	const gchar *oids[2] = {objectid, NULL};

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_INSERT_ITEM,
				       MAFW_DBUS_UINT32(index),
				       MAFW_DBUS_STRVZ(oids)),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}
	return FALSE;
}

gboolean mafw_proxy_playlist_insert_items(MafwPlaylist *self, guint index,
					      const gchar **objectids,
					      GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_INSERT_ITEM,
				       MAFW_DBUS_UINT32(index),
				       MAFW_DBUS_STRVZ(objectids)),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}
	return FALSE;
}

gboolean mafw_proxy_playlist_append_item(MafwPlaylist *self,
					  const gchar *objectid,
					  GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	const gchar *oids[2] = {objectid, NULL};

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
					MAFW_PLAYLIST_METHOD_APPEND_ITEM,
					MAFW_DBUS_STRVZ(oids)),
				MAFW_PLAYLIST_ERROR, error);
	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}
	return FALSE;
}

gboolean mafw_proxy_playlist_append_items(MafwPlaylist *self,
					  const gchar **objectids,
					  GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
					MAFW_PLAYLIST_METHOD_APPEND_ITEM,
					MAFW_DBUS_STRVZ(objectids)),
				MAFW_PLAYLIST_ERROR, error);
	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}
	return FALSE;
}


/*---------------------------------------------------------------------------
  Remove item
  ---------------------------------------------------------------------------*/

gboolean mafw_proxy_playlist_remove_item(MafwPlaylist *self, guint index,
					      GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gboolean retval;

	g_return_val_if_fail(self != NULL, FALSE);
	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_REMOVE_ITEM,
				       DBUS_TYPE_UINT32, index),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		mafw_dbus_parse(reply,DBUS_TYPE_BOOLEAN, &retval);
		dbus_message_unref(reply);
		return retval;
	}

	return FALSE;
}

/*---------------------------------------------------------------------------
  Get item
  ---------------------------------------------------------------------------*/

gchar *mafw_proxy_playlist_get_item(MafwPlaylist *self, guint index,
					 GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gchar *retval = NULL;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, NULL);
	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_GET_ITEM,
				       DBUS_TYPE_UINT32, index),
			       MAFW_PLAYLIST_ERROR, error);
	g_return_val_if_fail(reply, NULL);
	mafw_dbus_parse(reply, DBUS_TYPE_STRING, &retval);
	dbus_message_unref(reply);
	if (strlen(retval) == 0) return NULL;
	retval = g_strdup(retval);

	return retval;
}

gchar **mafw_proxy_playlist_get_items(MafwPlaylist *self,
					       	guint first_index,
						guint last_index,
						GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gchar **retval = NULL;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, NULL);
	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_GET_ITEMS,
				       DBUS_TYPE_UINT32, first_index,
				       DBUS_TYPE_UINT32, last_index),
			       MAFW_PLAYLIST_ERROR, error);
	g_return_val_if_fail(reply, NULL);
	mafw_dbus_parse(reply, MAFW_DBUS_TYPE_STRVZ, &retval);
	dbus_message_unref(reply);

	if (!retval[0])
	{
		g_free(retval);
		retval = NULL;
	}

	return retval;
}

static gboolean send_method_get_uint_str_params(gboolean send_param,
				MafwPlaylist *self, const gchar *command,
				guint *index, gchar **oid,
				GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gchar *retoid = NULL;
	guint retidx;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);
	if (send_param)
		reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       command,
					MAFW_DBUS_UINT32(*index)),
			       MAFW_PLAYLIST_ERROR, error);
	else
		reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       command),
			       MAFW_PLAYLIST_ERROR, error);
	g_return_val_if_fail(reply, FALSE);
	mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &retidx,
				DBUS_TYPE_STRING, &retoid);

	if (strlen(retoid) == 0) {
		dbus_message_unref(reply);
		return FALSE;
	}
	if (oid)
		*oid = g_strdup(retoid);
	if (index)
		*index = retidx;

	dbus_message_unref(reply);

	return TRUE;
}

void mafw_proxy_playlist_get_starting_index(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error)
{
	g_return_if_fail(index || oid);
	send_method_get_uint_str_params(FALSE, playlist,
			MAFW_PLAYLIST_METHOD_GET_STARTING_INDEX,
			index, oid, error);
}

void mafw_proxy_playlist_get_last_index(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error)
{
	g_return_if_fail(index || oid);
	send_method_get_uint_str_params(FALSE, playlist,
			MAFW_PLAYLIST_METHOD_GET_LAST_INDEX,
			index, oid, error);
}

gboolean mafw_proxy_playlist_get_next(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error)
{
	g_return_val_if_fail(index, FALSE);
	return send_method_get_uint_str_params(TRUE, playlist,
			MAFW_PLAYLIST_METHOD_GET_NEXT,
			index, oid, error);
}

gboolean mafw_proxy_playlist_get_prev(MafwPlaylist *playlist,
					       	guint *index, gchar **oid,
						GError **error)
{
	g_return_val_if_fail(index, FALSE);
	return send_method_get_uint_str_params(TRUE, playlist,
			MAFW_PLAYLIST_METHOD_GET_PREV,
			index, oid, error);
}

/*---------------------------------------------------------------------------
  Move Item
  ---------------------------------------------------------------------------*/

gboolean mafw_proxy_playlist_move_item(MafwPlaylist *self, guint from,
					    guint to, GError **error)
{
	MafwProxyPlaylist* playlist;
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	gboolean retval;

	g_return_val_if_fail(self, FALSE);
	playlist = MAFW_PROXY_PLAYLIST(self);
	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_MOVE,
				       DBUS_TYPE_UINT32, from,
				       DBUS_TYPE_UINT32, to),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		mafw_dbus_parse(reply,DBUS_TYPE_BOOLEAN, &retval);
		dbus_message_unref(reply);
		return retval;
	}

	return FALSE;

}

/*---------------------------------------------------------------------------
  Get list size
  ---------------------------------------------------------------------------*/

guint mafw_proxy_playlist_get_size(MafwPlaylist *self, GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;
	guint retval = 0;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, 0);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_GET_SIZE),
			       MAFW_PLAYLIST_ERROR, error);

	if (reply) {
		mafw_dbus_parse(reply, DBUS_TYPE_UINT32, &retval);
		dbus_message_unref(reply);
	}


	return retval;
}

/*---------------------------------------------------------------------------
  Playlist clear
  ---------------------------------------------------------------------------*/


gboolean mafw_proxy_playlist_clear(MafwPlaylist *self, GError **error)
{
	MafwProxyPlaylist* playlist = MAFW_PROXY_PLAYLIST(self);
	MafwProxyPlaylistPrivate *priv;
	DBusMessage *reply;

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(playlist);
	g_return_val_if_fail(priv->connection != NULL, FALSE);

	reply = mafw_dbus_call(priv->connection, mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					priv->obj_path,
					MAFW_DBUS_INTERFACE,
				       MAFW_PLAYLIST_METHOD_CLEAR
				       ),
			       MAFW_PLAYLIST_ERROR, error);
	if (reply) {
		dbus_message_unref(reply);
		return TRUE;
	}

	return FALSE;
}

/**
 * mafw_proxy_playlist_handle_signal_contents_changed:
 * @self: a MafwProxyPlaylist instance.
 * @msg: the DBus message
 *
 * Handles the received DBus signal "contents_changed", i.e.
 * parses the message and emits a corresponding g_signal
 */
static void mafw_proxy_playlist_handle_signal_contents_changed(
						MafwProxyPlaylist *self,
						DBusMessage *msg)
{
	guint64 id = 0;
	guint from = 0;
	guint nremove = 0;
	guint nreplace = 0;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_UINT32, &id,
			DBUS_TYPE_UINT32, &from,
			DBUS_TYPE_UINT32, &nremove,
			DBUS_TYPE_UINT32, &nreplace);

	g_signal_emit_by_name(self, "contents-changed",
			      from, nremove, nreplace);
}

/**
 * mafw_proxy_playlist_handle_signal_property_changed:
 * @self: a MafwProxyPlaylist instance.
 * @msg: the DBus message
 *
 * Handles the received DBus property changed signal
 */
static void mafw_proxy_playlist_handle_signal_property_changed(
						MafwProxyPlaylist *self,
						DBusMessage *msg)
{
	gchar *prop;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_STRING, &prop);

	g_object_notify(G_OBJECT(self), prop);
}

/**
 * handle_signal_item_moved:
 * @self: a MafwProxyPlaylist instance.
 * @msg: the DBus message
 *
 * Handles the received DBus signal "item-moved", i.e.
 * parses the message and emits a corresponding g_signal
 */
static void handle_signal_item_moved(MafwProxyPlaylist *self, DBusMessage *msg)
{
	guint from = 0;
	guint to = 0;

	g_assert(self != NULL);
	g_assert(msg != NULL);

	/* Read the message and signal the values */
	mafw_dbus_parse(msg,
			DBUS_TYPE_UINT32, &from,
			DBUS_TYPE_UINT32, &to);

	g_signal_emit_by_name(self, "item-moved",
			      from, to);
}

static DBusHandlerResult dispatch_message(DBusConnection *conn,
					  DBusMessage *msg,
					  MafwProxyPlaylist *self)
{
	MafwProxyPlaylistPrivate *priv;

	g_assert(conn != NULL);
	g_assert(msg != NULL);

	priv = MAFW_PROXY_PLAYLIST_GET_PRIVATE(self);

	if (mafw_dbus_is_signal(msg, MAFW_PLAYLIST_CONTENTS_CHANGED)) {
		mafw_proxy_playlist_handle_signal_contents_changed(self, msg);
	} else if (mafw_dbus_is_signal(msg, MAFW_PLAYLIST_PROPERTY_CHANGED)) {
		mafw_proxy_playlist_handle_signal_property_changed(self, msg);
	} else if (mafw_dbus_is_signal(msg, MAFW_PLAYLIST_ITEM_MOVED)) {
		handle_signal_item_moved(self, msg);
	}

	//Let the other apps receive the signal
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
