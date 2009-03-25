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

#ifndef __DBUS_INTERFACE_H__
#define __DBUS_INTERFACE_H__


/*----------------------------------------------------------------------------
  Common Service & Object names
  ----------------------------------------------------------------------------*/

#define MAFW_SERVICE "com.nokia.mafw"
#define MAFW_INTERFACE "com.nokia.mafw"
#define MAFW_OBJECT "/com/nokia/mafw"

/*----------------------------------------------------------------------------
  Common between renderer and source
  ----------------------------------------------------------------------------*/
#define MAFW_EXTENSION_INTERFACE MAFW_INTERFACE ".extension"

/**
 * list_extension_properties:
 *
 * Method for querying available run-time properties of a extension.
 *
 * Returns: a %DBUS_TYPE_ARRAY of %DBUS_TYPE_STRING:s specifying the
 * property names, followed by a %DBUS_TYPE_ARRAY of
 * %DBUS_TYPE_UINT32:s specifying the #GType of the corresponding
 * property.
 */
#define MAFW_EXTENSION_METHOD_LIST_PROPERTIES "list_extension_properties"

/**
 * set_extension_property:
 * @property: the name of the property (%DBUS_TYPE_STRING).
 * @value: the value of the property (%DBUS_TYPE_VARIANT).
 *
 * Sets a run-time property.
 */
#define MAFW_EXTENSION_METHOD_SET_PROPERTY "set_extension_property"

/**
 * get_extension_property:
 * @property: the name of the property (%DBUS_TYPE_STRING).
 *
 * Queries a run-time property of a extension.  Note that this call may
 * block for a longer time.
 *
 * Returns: the name of the property (%DBUS_TYPE_STRING) followed by
 * the value (%DBUS_TYPE_VARIANT), or an error.
 */
#define MAFW_EXTENSION_METHOD_GET_PROPERTY "get_extension_property"

/**
 * property_changed:
 * @property: the name of the property (%DBUS_TYPE_STRING).
 * @value: the value of the property (%DBUS_TYPE_VARIANT).
 *
 * A signal for wrapping MafwExtension::property-changed.
 */
#define MAFW_EXTENSION_SIGNAL_PROPERTY_CHANGED "property_changed"

/**
 * set_name:
 * @name: the new name of the extension (%DBUS_TYPE_STRING).
 *
 * Sets the name (GObject property) of the extension.
 */
#define MAFW_EXTENSION_METHOD_SET_NAME "set_name"

/**
 * get_name:
 * @name: the name of the extension (%DBUS_TYPE_STRING).
 *
 * Returns the name (GObject property) of the extension.
 */
#define MAFW_EXTENSION_METHOD_GET_NAME "get_name"


/**
 * name_changed:
 * @name: the name of the extension (%DBUS_TYPE_STRING).
 *
 * A signal sent after the extension is renamed.
 */
#define MAFW_EXTENSION_SIGNAL_NAME_CHANGED "name_changed"

/**
 * error:
 * @domain: the error domain (%DBUS_TYPE_STRING).
 * @code: the error code (%DBUS_TYPE_INT32).
 * @message: the error message (%DBUS_TYPE_STRING).
 *
 * Signals a GError (MafwExtension::error).
 */
#define MAFW_EXTENSION_SIGNAL_ERROR "error"

/*----------------------------------------------------------------------------
  Renderer 
  ----------------------------------------------------------------------------*/

#define MAFW_RENDERER_INTERFACE MAFW_INTERFACE ".renderer"
#define MAFW_RENDERER_SERVICE MAFW_SERVICE ".renderer"
#define MAFW_RENDERER_OBJECT MAFW_OBJECT "/renderer"

/**
 * play:
 *
 * Starts playback
 */
#define MAFW_RENDERER_METHOD_PLAY "play"

/**
 * play_object:
 * @object_id: (%DBUS_TYPE_STRING) an object ID to play
 *
 * Starts playback of the given object ID
 */
#define MAFW_RENDERER_METHOD_PLAY_OBJECT "play_object"

/**
 * play_uri:
 * @uri: (%DBUS_TYPE_STRING) a URI to play
 *
 * Starts playback of the given uri
 */
#define MAFW_RENDERER_METHOD_PLAY_URI "play_uri"

/**
 * stop:
 *
 * Stops playback
 */
#define MAFW_RENDERER_METHOD_STOP "stop"

/**
 * pause:
 *
 * Suspends playback
 */
#define MAFW_RENDERER_METHOD_PAUSE "pause"

/**
 * resume:
 *
 * Resumes a suspended playback
 */
#define MAFW_RENDERER_METHOD_RESUME "resume"

/**
 * get_status:
 *
 * Requests the renderer's current status information.
 *
 * Returns: Assigned playlist ID(%DBUS_TYPE_UINT32),
 *          Playback index in the playlist (%DBUS_TYPE_UINT32),
 *          Playback state (%DBUS_TYPE_UINT32 -> %MafwPlayState)
 *          Current object ID (%DBUS_TYPE_STRING)
 */
#define MAFW_RENDERER_METHOD_GET_STATUS "get_status"

/**
 * assign_playlist:
 * @playlist_id: the playlist ID to assign (%DBUS_TYPE_UINT32).
 *
 * Assigns the given playlist to the renderer.
 */
#define MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST "assign_playlist"

/**
 * next:
 *
 * Tells the renderer to skip to the next item in its assigned playlist.
 */
#define MAFW_RENDERER_METHOD_NEXT "next"

/**
 * next:
 *
 * Tells the renderer to skip to the previous item in its assigned playlist.
 */
#define MAFW_RENDERER_METHOD_PREVIOUS "previous"

/**
 * next:
 *
 * Tells the renderer to skip to the given index in its assigned playlist
 */
#define MAFW_RENDERER_METHOD_GOTO_INDEX "goto_index"

/**
 * set_position:
 * @mode:    Seek mode (#MafwRendererSeekMode) (%DBUS_TYPE_INT32)
 * @seconds: The position to seek to in seconds (%DBUS_TYPE_INT32)
 *
 * Tells the renderer to seek to the given position within the current media.
 */
#define MAFW_RENDERER_METHOD_SET_POSITION "set_position"

/**
 * set_position:
 *
 * Requests the renderer's current playback position within the current media.
 *
 * Returns: The current playback position in seconds (%DBUS_TYPE_UINT32)
 */
#define MAFW_RENDERER_METHOD_GET_POSITION "get_position"

#define MAFW_RENDERER_SIGNAL_STATE_CHANGED "state_changed"
#define MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED "playlist_changed"
#define MAFW_RENDERER_SIGNAL_ITEM_CHANGED "media_changed"

/**
 * buffering_info:
 * @status: buffering status as a fraction (0.0 - 1.0).
 *
 * Indicates the buffering status. Status 1.0 means buffering complete
 */
#define MAFW_RENDERER_SIGNAL_BUFFERING_INFO "buffering_info"

/**
 * metadata_changed:
 * @name:  name of the changed metadatum (%DBUS_TYPE_STRING).
 * @value: values (%MAFW_DBUS_TYPE_GVALUEARRAY).
 *
 * Wraps MafwRenderer::metadata-changed.
 */
#define MAFW_RENDERER_SIGNAL_METADATA_CHANGED "metadata_changed"

/*----------------------------------------------------------------------------
  Source 
  ----------------------------------------------------------------------------*/

#define MAFW_SOURCE_INTERFACE MAFW_INTERFACE ".source"
#define MAFW_SOURCE_SERVICE MAFW_SERVICE ".source"
#define MAFW_SOURCE_OBJECT MAFW_OBJECT "/source"

/**
 * browse:
 * @object_id: the object id to start from (%DBUS_TYPE_STRING).
 * @recursive: whether it should be recursive (%DBUS_TYPE_BOOLEAN).
 * @filter: the filter expression (%DBUS_TYPE_STRING).
 * @sort_criteria: the sort criteria (%DBUS_TYPE_STRING).
 * @metadata_keys: the metadata keys to retrieve (array of %DBUS_TYPE_STRING).
 * @skip_count: number of elements to skip from the resultset
 *              (%DBUS_TYPE_UINT32).
 * @item_count: number of items to retrieve (%DBUS_TYPE_UINT32).
 * @domain: In case of error, the error domain (%DBUS_TYPE_STRING)
 * @code: In case of error, the error code (%DBUS_TYPE_INT32)
 * @message: In case of error, the error message (%DBUS_TYPE_STRING)
 *
 * Starts a browse session.  Results will arrive in browse_result()
 * methods via the source proxy interface. Note, in case of error 
 * the domain, code and message arguments are added to the message.
 *
 * Returns: the session id (%DBUS_TYPE_UINT32).
 */
#define MAFW_SOURCE_METHOD_BROWSE "browse"

/**
 * cancel_browse:
 * @browse_id: the identification number of the request to cancel
 *	       (%DBUS_TYPE_UINT32)
 *
 * Cancels a browse request intiated by a "browse" message.
 * No answer is expected to this call.
 */
#define MAFW_SOURCE_METHOD_CANCEL_BROWSE "cancel_browse"

/**
 * get_metadata:
 * @object_ids: the list of object id:s to query (array of
 *              %DBUS_TYPE_STRING).
 * @metadata_keys: list of metadata keys to return (array of
 *                 %DBUS_TYPE_STRING).
 * @domain: In case of error, the error domain (%DBUS_TYPE_STRING)
 * @code: In case of error, the error code (%DBUS_TYPE_INT32)
 * @message: In case of error, the error message (%DBUS_TYPE_STRING)
 *
 * Gets metadata of given objects.
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @metadata: serialized metadata (%MAFW_DBUS_TYPE_METADATA)
 */
#define MAFW_SOURCE_METHOD_GET_METADATA "get_metadata"

/**
 * set_metadata: %DBUS_MESSAGE_TYPE_METHOD
 * @object_id:   objectid of the object whose metadata is edited
 *            (%DBUS_TYPE_STRING)
 * @metadata: serialized metadata which will be set to the object
 *            (%DBUS_TYPE_ARRAY of %DBUS_TYPE_BYTE)
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @objectid: of the modified object (%DBUS_TYPE_STRING)
 * @failed_keys: array of metadata keys which failed (array 
 *               of %DBUS_TYPE_STRING) or an empty array.
 * @domain:  In case of error, the error domain (%DBUS_TYPE_STRING)
 * @code: In case of error, the error code   (%DBUS_TYPE_INT32)
 * @message: In case of error, the error message (%DBUS_TYPE_STRING)
 */
#define MAFW_SOURCE_METHOD_SET_METADATA "set_metadata"

/**
 * create_object: %DBUS_MESSAGE_TYPE_METHOD
 * @parent:   objectid of the container to create the object in
 *            (%DBUS_TYPE_STRING)
 * @metadata: serialized metadata of the object to be created
 *            (%DBUS_TYPE_ARRAY of %DBUS_TYPE_BYTE)
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @objectid: of the newly created object (%DBUS_TYPE_STRING)
 */
#define MAFW_SOURCE_METHOD_CREATE_OBJECT "create_object"

/**
 * destroy_object: %DBUS_MESSAGE_TYPE_METHOD
 * @objectid: whom to destroy (%DBUS_TYPE_STRING)
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * Arguments void.
 */
#define MAFW_SOURCE_METHOD_DESTROY_OBJECT "destroy_object"

/**
 * metadata_changed:
 * @object_id: object id (%DBUS_TYPE_STRING).
 *
 * Metdata changed signal
 */
#define MAFW_SOURCE_SIGNAL_METADATA_CHANGED "metadata_changed"

/**
 * container_changed:
 * @object_id: object id (%DBUS_TYPE_STRING).
 *
 * Container changed signal
 */
#define MAFW_SOURCE_SIGNAL_CONTAINER_CHANGED "container_changed"


/*----------------------------------------------------------------------------
  Source proxy
  ----------------------------------------------------------------------------*/

/**
 * browse_result:
 * @browse_id: the session id (%DBUS_TYPE_UINT32).
 * @remaining_count: items remaining in the session, -1 if not known
 *                   (%DBUS_TYPE_INT32).
 * @index: index of the element in this relative to the whole
 *         resultset (%DBUS_TYPE_UINT32).
 * @object_id: an object id (%DBUS_TYPE_STRING).
 * @metadata: the queried metadata (array of %DBUS_TYPE_STRING);
 *            (key1, value1, key2, value2, ...).
 *
 * A chunk of results in the given browse session.
 */
#define MAFW_PROXY_SOURCE_METHOD_BROWSE_RESULT "browse_result"

/*******************************************************************
 * MAFW Playlist daemon interface
 *******************************************************************/
#define MAFW_PLAYLIST_SERVICE	MAFW_SERVICE	".playlist"
#define MAFW_PLAYLIST_INTERFACE	MAFW_INTERFACE	".playlist"
#define MAFW_PLAYLIST_PATH	MAFW_OBJECT	"/playlist"

/**
 * create_playlist: %DBUS_MESSAGE_TYPE_METHOD
 * @name: name of the playlist to create (%DBUS_TYPE_STRING)
 *
 * Makes sure a playlist @name exists.  If it doesn't it will be created
 * and subsequent %MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED signal is sent.
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @id: ID of the playlist with the requested name.  By the time
 * reply comes the playlist must be fully usable.
 */
#define MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST	"create_playlist"

/**
 * duplicate_playlist: %DBUS_MESSAGE_TYPE_METHOD
 * @playlist_id: ID of the original playlist (%DBUS_TYPE_UINT32)
 * @new_name: name for the duplicate playlist to create (%DBUS_TYPE_STRING)
 *
 * Duplicates the playlist @playlist_id with the name @name if it does not
 * exist, and subsequent %MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED signal is sent.
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 */
#define MAFW_PLAYLIST_METHOD_DUP_PLAYLIST	"duplicate_playlist"

/**
 * import_playlist: %DBUS_MESSAGE_TYPE_METHOD
 * @playlist: Uri to playlist, playlist object id or container objectid.
 *            (%DBUS_TYPE_STRING)
 * @base_uri: If not null, used as prefix to resolve relative paths found from
 *            playlist. (%DBUS_TYPE_STRING)
 *
 * Imports external playlists files and shares them in mafw environment.
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @id: ID of the playlist-import
 */
#define MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST	"import_playlist"

/**
 * playlist_imported:
 * @import_id: the import id (%DBUS_TYPE_UINT32).
 * @playlist_id: new playlist's ID
 *                   (%DBUS_TYPE_INT32).
 *
 * The result of the import
 */
#define MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED	"playlist_imported"

/**
 * cancel_import:
 * @import_id: the identification number of the request to cancel
 *	       (%DBUS_TYPE_UINT32)
 *
 * Cancels a import request intiated by a "import_playlist" message.
 * No answer is expected to this call.
 */
#define MAFW_PLAYLIST_METHOD_CANCEL_IMPORT	"cancel_import"

/**
 * playlist_created: %DBUS_MESSAGE_TYPE_SIGNAL
 * @id: the ID of the playlist just created (%DBUS_TYPE_UINT32).
 *
 * Informs about the creation of a playlist.
 */
#define MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED	"playlist_created"

/**
 * destroy_playlist: %DBUS_MESSAGE_TYPE_METHOD
 * @id: the ID of the playlist to destroy.
 *
 * Tells the daemon to destroy the playlist identified by @id.
 * It is not an error if there is no such playlist.  Otherwise
 * the daemon emit a MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED
 * signal.
 *
 * reply: doesn't reply.
 */
#define MAFW_PLAYLIST_METHOD_DESTROY_PLAYLIST	"destroy_playlist"

/**
 * playlist_destroyed: %DBUS_MESSAGE_TYPE_SIGNAL
 * @id: the ID of the playlist just destroyed (%DBUS_TYPE_UINT32).
 *
 * Informs about the destruction of a playlist.
 */
#define MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED	"playlist_destroyed"

/**
 * playlist_destruction_failed: %DBUS_MESSAGE_TYPE_SIGNAL
 * @id: the ID of the playlist which destruction is not allowed
 * (%DBUS_TYPE_UINT32).
 *
 * Informs about the destruction of a playlist is not allowed because the 
 * playlist is being used.
 */
#define MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTRUCTION_FAILED   "playlist_destruction_failed"

/**
 * list_playlists: %DBUS_MESSAGE_TYPE_METHOD
 * @inargs: an optional %DBUS_TYPE_ARRAY
 *          of playlist IDs (%DBUS_TYPE_UINT32).
 *
 * Queries the existence and name of the playlists specified in @inargs,
 * otherwise returns information about all playlists the daemon knows
 * about.
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @outargs: a %DBUS_TYPE_ARRAY of %DBUS_TYPE_STRUCT of playlist ID
 * (%DBUS_TYPE_UINT32) and name (%DBUS_TYPE_STRING).  Information is
 * returned about all but nonexisting playlists.
 */
#define MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS	"list_playlists"

/*----------------------------------------------------------------------------
  Playlist interface
  ----------------------------------------------------------------------------*/

/**
 * set_name:
 * @name: (%DBUS_TYPE_STRING) name of the playlist
 *
 * Sets playlist's name
 */
#define MAFW_PLAYLIST_METHOD_SET_NAME "set_name"

/**
 * get_name:
 *
 * Returns the name of the playlist
 */
#define MAFW_PLAYLIST_METHOD_GET_NAME "get_name"

/**
 * set_repeat:
 * @repeat: (%DBUS_TYPE_BOOLEAN) new repeat state
 *
 * Sets the repepat state of the playlist
 */
#define MAFW_PLAYLIST_METHOD_SET_REPEAT "set_repeat"

/**
 * get_repeat:
 *
 * Returns the repeat state of the playlist
 */
#define MAFW_PLAYLIST_METHOD_GET_REPEAT "get_repeat"

/**
 * shuffle:
 *
 * Shuffles the playlist
 */
#define MAFW_PLAYLIST_METHOD_SHUFFLE "shuffle"

/**
 * is_shuffled:
 *
 * Returns the shuffled state of the playlist
 */
#define MAFW_PLAYLIST_METHOD_IS_SHUFFLED "is_shuffled"

/**
 * unshuffle:
 *
 * Unshuffles the playlist
 */
#define MAFW_PLAYLIST_METHOD_UNSHUFFLE "unshuffle"

/**
 * increment_use_count:
 *
 * Increments the use count of the playlist
 */
#define MAFW_PLAYLIST_METHOD_INCREMENT_USE_COUNT "increment_use_count"

/**
 * decrement_use_count:
 *
 * Decrements the use count of the playlist
 */
#define MAFW_PLAYLIST_METHOD_DECREMENT_USE_COUNT "decrement_use_count"

/**
 * insert_item:
 * @index:    the position to insert the item at.  Valid value range
 *            is between 0 (insert before all existing items) and
 *            playlist size (append).
 * @objectid: the ID of the item to insert into the playlist.
 *
 * Inserts an item at the given position in the playlist.  The @index
 * parameter should be an absolute visual index (i.e. not in playing
 * order).  If @objectid is appended to the list it will be played last.
 * Otherwise it will inherit the playing position of the @index:th item,
 * and all subsequent items are moved downwards.
 */
#define MAFW_PLAYLIST_METHOD_INSERT_ITEM "insert_item"

/**
 * append_item:
 * @objectid: the ID of the item to append (%DBUS_TYPE_STRING).
 *
 * Appends the item to the playlist.
 */
#define MAFW_PLAYLIST_METHOD_APPEND_ITEM "append_item"

/**
 * remove_item:
 * @index:    position of an element to remove in the playlist.
 *            Valid value is between 0 and (playlist size - 1).
 *
 * Removes an item from a playlist.  The @index parameter is an
 * absolute visual index.
 */
#define MAFW_PLAYLIST_METHOD_REMOVE_ITEM "remove_item"

/**
 * get_item:
 * @index:    an index of an item to get from playlist.  Valid value
 *            range is between 0 and (playlist size - 1).
 *
 * Gets the item at the given playlist index.
 */
#define MAFW_PLAYLIST_METHOD_GET_ITEM "get_item"

/**
 * get_items:
 * @first_index:    First index to return.
 * @last_index:	    Last index to return
 *
 * Gets the items between the specified indicies form the given playlist.
 */
#define MAFW_PLAYLIST_METHOD_GET_ITEMS "get_items"

/**
 * get_starting:
 *
 * Gets the objectid and the visual index of the first playable item.
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @idx: visual index of the first playable item
 * @objectid: object id of the first playable item
 */
#define MAFW_PLAYLIST_METHOD_GET_STARTING_INDEX "get_starting"

/**
 * get_last:
 *
 * Gets the objectid and the visual index of the last playable item.
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @idx: visual index of the last playable item
 * @objectid: object id of the last playable item
 */
#define MAFW_PLAYLIST_METHOD_GET_LAST_INDEX "get_last"


/**
 * get_next:
 * @index:    visual index of the current item
 *
 * Gets the objectid and the visual index of the next playable item.
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @idx: visual index of the next playable item
 * @objectid: object id of the next playable item
 */
#define MAFW_PLAYLIST_METHOD_GET_NEXT "get_next"

/**
 * get_prev:
 * @index:    visual index of the current item
 *
 * Gets the objectid and the visual index of the previous playable item.
 *
 * reply: %DBUS_MESSAGE_TYPE_METHOD_RETURN or %DBUS_MESSAGE_TYPE_ERROR
 * @idx: visual index of the previous playable item
 * @objectid: object id of the previous playable item
 */
#define MAFW_PLAYLIST_METHOD_GET_PREV "get_prev"


/**
 * move:
 * @from:     a position in the playlist to move an item from.  Valid
 *            value range is between 0 and (playlist size - 1).
 * @to:       a position in the playlist move the item to.  Valid
 *            value range is between 0 and playlist size, and must not
 *            be equal to @from.
 *
 * Moves an item in the playlist from one position to another.
 * Notice that both locations must be valid for the given playlist
 * (i.e. within the boundaries of the playlist).
 */
#define MAFW_PLAYLIST_METHOD_MOVE "move"

/**
 * get_size:
 *
 * Gets the number of items in the playlist.
 */
#define MAFW_PLAYLIST_METHOD_GET_SIZE "get_size"

/**
 * clear:
 *
 * Removes all entries from the given playlist.
 */
#define MAFW_PLAYLIST_METHOD_CLEAR "clear"

/**
 * MAFW_PLAYLIST_CONTENTS_CHANGED:
 * A signal telling that the contents of a shared playlist have changed.
 */
#define MAFW_PLAYLIST_CONTENTS_CHANGED "contents_changed"

/**
 * MAFW_PLAYLIST_ITEM_MOVED:
 * A signal telling that an item has been moved to a new place
 */
#define MAFW_PLAYLIST_ITEM_MOVED "item_moved"

/**
 * MAFW_PLAYLIST_PROPERTY_CHANGED:
 * A signal telling that one or more properties of a playlist
 * (name, or repeating mode) of a shared playlist have changed.
 */
#define MAFW_PLAYLIST_PROPERTY_CHANGED "property_changed"

#endif
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
