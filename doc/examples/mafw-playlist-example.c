/* 
The code examples copyrighted by Nokia Corporation that are included to
this material are licensed to you under following MIT-style License:

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <libmafw/mafw.h>
#include <libmafw/mafw-log.h>
#include <libmafw-shared/mafw-shared.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-example"

#define COMMAND_CREATE       1
#define COMMAND_REMOVE       2
#define COMMAND_SHOW         3
#define COMMAND_ADD_ITEM     4
#define COMMAND_REMOVE_ITEM  5

GMainLoop *main_loop = NULL;

gint command;
gchar *playlist_name;
gchar *object_id;

/*
 * This function looks for a particular playlist using its name
 */
static MafwPlaylist *
find_playlist (gchar *name, GError **error) 
{
 	MafwPlaylistManager *manager;
	MafwPlaylist *playlist = NULL;
	GPtrArray *list = NULL;
	gint i;

	manager = mafw_playlist_manager_get ();

	list = mafw_playlist_manager_get_playlists (manager, error);

	if (*error != NULL) {
		return NULL;
	}

	for (i = 0; i < list->len; i++) {
		playlist = (MafwPlaylist *) g_ptr_array_index (list, i);
		gchar *name = mafw_playlist_get_name (playlist);
		if (strcmp (playlist_name, name) == 0) {
			return playlist;
		}
	}

	return NULL;
}

/*
 * This function creates a new playlist
 */
static GError *
create_playlist (void)
{
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist;
	GError *error = NULL;
	
	manager = mafw_playlist_manager_get ();
	mafw_playlist_manager_create_playlist (manager,
					       playlist_name,
					       &error);
	return error;
}

/*
 * This function removes a playlist
 */
static GError *
remove_playlist (void)
{
	MafwPlaylistManager *manager;
	MafwPlaylist *playlist;
	GError *error = NULL;
	GPtrArray *list = NULL;
	gint i;

	manager = mafw_playlist_manager_get ();

	playlist = find_playlist (playlist_name, &error);

	if (error != NULL)  {
		return error;
	}

	if (playlist == NULL) {
		return g_error_new (MAFW_ERROR, 0, "Playlist not found");
	}

	mafw_playlist_manager_destroy_playlist (manager, 
						playlist,
						&error);

	return error;
}

/*
 * This function shows the object identifiers contained in a playlist
 */
static GError *
show_playlist (void)
{
	MafwPlaylist *playlist;
	GError *error = NULL;
	gint i;
	guint size;

	playlist = find_playlist (playlist_name, &error);

	if (error != NULL)  {
		return error;
	}

	if (playlist == NULL) {
		return g_error_new (MAFW_ERROR, 0, "Playlist not found");
	}

	g_print ("Showing contents for playlist %s...:\n", playlist_name);
	size = mafw_playlist_get_size (playlist, &error);

	if (error != NULL) {
		return error;
	}

	if (size > 0) {
		for (i = 0; i < size; i++) {
			gchar *id = 
				mafw_playlist_get_item (playlist, i, &error);
			if (error != NULL) {
				g_warning ("Error getting item %d "
					   "from playlist: %s\n",
					   i, error->message);
				g_error_free (error);
				error = NULL;
			} 
			
			g_print ("  %d %s\n", i, id);
		}
	} else {
		g_print ("Playlist is empty\n");
	}

	return error;
}

/*
 * This function adds an object identifier to a playlist
 */
static GError *
add_item_to_playlist (void)
{
	MafwPlaylist *playlist;
	GError *error = NULL;

	playlist = find_playlist (playlist_name, &error);

	if (error != NULL)  {
		return error;
	}

	if (playlist == NULL) {
		return g_error_new (MAFW_ERROR, 0, "Playlist not found");
	}

	mafw_playlist_insert_item (playlist, 0, object_id, &error);
	return error;
}

/*
 * This function removes an item from a playlist
 */
static GError *
remove_item_from_playlist (void)
{
	MafwPlaylist *playlist;
	GError *error = NULL;
	guint size;
	gint i;

	playlist = find_playlist (playlist_name, &error);

	if (error != NULL)  {
		return error;
	}

	if (playlist == NULL) {
		return g_error_new (MAFW_ERROR, 0, "Playlist not found");
	}

	g_print ("  Searching for %s in playlist %s\n", 
		 object_id, playlist_name);
	size = mafw_playlist_get_size (playlist, &error);

	if (error != NULL) {
		return error;
	}

	for (i = 0; i < size; i++) {
		gchar *id = mafw_playlist_get_item (playlist, i, &error);
		if (error != NULL) {
			g_warning ("Error getting item %d "
				   "from playlist: %s\n",
				   i, error->message);
			g_error_free (error);
			error = NULL;
		} else if (strcmp (id, object_id) == 0) {
			mafw_playlist_remove_item (playlist, i, &error);
			g_print ("  Element found at position %d\n", i);
			break;
		}
	}

	if (i == size) {
		g_print ("  Element not found\n");
	} else {
		g_print ("Playlist %s removed\n", playlist_name);
	}
}

/*
 * This function executes the command specified in the command line
 */
static gboolean
execute_command (gpointer user_data)
{
	GError *error;

	switch (command) {
	case COMMAND_CREATE:
		error = create_playlist ();
		if (error == NULL) {
			g_print ("Playlist %s created\n", playlist_name);
		}
		break;
	case COMMAND_REMOVE:
		error = remove_playlist ();
		if (error == NULL) {
			g_print ("Playlist %s removed\n", playlist_name);
		}
		break;
	case COMMAND_SHOW:
		error = show_playlist ();
		break;
	case COMMAND_ADD_ITEM:	
		error = add_item_to_playlist ();
		if (error == NULL) {
			g_print ("Item %s added to playlist %s\n", 
				 object_id, playlist_name);
		}
		break;
	case COMMAND_REMOVE_ITEM:
		error = remove_item_from_playlist ();
		break;
	}

	if (error != NULL) {
		g_print ("Operation failed: %s\n", error->message);
	} else {
		g_print ("Operation executed successfully.\n");
	}

	g_main_loop_quit (main_loop);

	return FALSE;
}

/*
 * This function parses the command line and extracts
 * info on the requested command and arguments.
 */
static gboolean
check_command_line (int argc, 
		    gchar *argv[], 
		    gint *command, 
		    gchar **playlist_name,
		    gchar **object_id)
{
	switch (argc) {
	case 3:
		*playlist_name = argv[2];
		if (!strcmp (argv[1], "create")) {
			*command = COMMAND_CREATE;
		} else if (!strcmp (argv[1], "remove")) {
			*command = COMMAND_REMOVE;
		} else if (!strcmp (argv[1], "show")) {
			*command = COMMAND_SHOW;
		} else {
			return FALSE;
		}
		break;
	case 4:
		*playlist_name = argv[2];
		*object_id = argv[3];
		if (!strcmp (argv[1], "add-item")) {
			*command = COMMAND_ADD_ITEM;
		} else if (!strcmp (argv[1], "remove-item")) {
			*command = COMMAND_REMOVE_ITEM;		
		} else {
			return FALSE;
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

int
main (int argc, gchar *argv[])
{
	if (!check_command_line (argc, argv, 
				 &command, &playlist_name, &object_id)) {
		g_error ("Please, provide one of these sets of arguments:\n"
			 "  create <playlist-name>\n"
			 "  remove <playlist-name>\n"
			 "  show <playlist-name>\n"
                         "  add-item <playlist-name> <object-id>\n",
			 "  remove-item <playlist-name> <object-id>\n");
	}

	g_type_init ();
	mafw_log_init (G_LOG_DOMAIN ":ALL");

	g_timeout_add (100, execute_command, NULL);
	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}
