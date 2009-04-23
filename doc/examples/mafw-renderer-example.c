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
#include <libmafw/mafw-registry.h>
#include <libmafw-shared/mafw-shared.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-example"

#define WANTED_RENDERER     "Mafw-Gst-Renderer"

MafwRenderer *app_renderer = NULL;
GMainLoop * main_loop = NULL;

gchar *state_str[] = {"STOPPED", "PLAYING", "PAUSED", "TRANSITIONING"};

static void
play_cb (MafwRenderer *renderer, 
	 gpointer user_data,
	 const GError *error)
{
        if (error != NULL) {
		g_print ("Play operation failed: %s\n", error->message);
	}
}

static void
error_cb (MafwRenderer *renderer,
	  GQuark domain,
	  gint code,
	  gchar *message,
	  gpointer user_data)
{
	g_print ("Playback error received: %s\n", message);
}

static void
state_changed_cb (MafwRenderer *renderer,
                  MafwPlayState state, 
		  gpointer user_data)
{
        g_print("State changed! New state is %s\n",
		state_str[state]);
}

static void
media_changed_cb (MafwRenderer *renderer,
		  gint index,
		  gchar *object_id,
		  gpointer user_data)
{
	static gboolean started = FALSE;

	g_print ("Media changed: assigned media is %d - %s\n",
		 index, object_id);

	/* Start playback right away! */
	if (started == FALSE) {
		started = TRUE;
		mafw_renderer_play (renderer, play_cb, NULL);
	}
}

static void
metadata_changed_cb (MafwRenderer *renderer, 
		     gchar *key, 
		     GValueArray *value,
		     gpointer user_data)
{
	g_print ("  Got metadata %s: %s\n", 
		 key, 
		 g_strdup_value_contents (&(value->values[0])));
}

static MafwPlaylist *
find_playlist (gchar *playlist_name, GError **error) 
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
 * This function assigned a playlist to the renderer.
 * Receives the playlist name to assign as parameter.
 */
static gboolean
do_assign_playlist_request (gpointer user_data)
{
	GError *error = NULL;
	MafwPlaylist *playlist;
	gchar *playlist_name = (gchar *) user_data;

	g_print ("[INFO] Assigning playlist %s to "  WANTED_RENDERER ".\n", 
		 playlist_name);
	
	playlist = find_playlist (playlist_name, &error);
	
	if (error != NULL)  {
		g_error ("Failed to find playlist\n");
	}

	if (playlist == NULL) {
		g_error ("Playlist not found");
	}

	mafw_renderer_assign_playlist (app_renderer, playlist, &error);

	if (error != NULL) {
		g_error ("Failed to assign playlist: %s\n",
			 error->message);
	}

	return FALSE;
}


/*
 * Hooks for extension added and removed signals
 */

/*
 * Checks for a particular source to be added and 
 * saves a reference to it.
 */
static void 
source_added_cb (MafwRegistry *registry, 
		 GObject *source, 
		 gpointer user_data)
{
	if (MAFW_IS_SOURCE(source)) {
		const gchar *name = 
			mafw_extension_get_name(MAFW_EXTENSION(source));
		g_print("[INFO] Source %s available.\n", name);
	}
}

/*
 * Checks if the referenced source is removed, and if so, exits.
 */
static void 
source_removed_cb (MafwRegistry *registry, 
		   GObject *source, 
		   gpointer user_data)
{
	if (MAFW_IS_SOURCE(source)) {
		g_print("[INFO] Source %s removed.\n", 
			mafw_extension_get_name(MAFW_EXTENSION(source)));
	}
}

static void 
renderer_added_cb (MafwRegistry *registry, 
		   GObject *renderer, 
		   gpointer user_data)
{
	if (MAFW_IS_RENDERER(renderer)) {
		const gchar *name = 
			mafw_extension_get_name (MAFW_EXTENSION(renderer));
		
		g_print("[INFO] Renderer %s available.\n", name);

		if (strcmp (name, WANTED_RENDERER) == 0) {
			g_print ("[INFO]     Wanted renderer found!\n");
			app_renderer = g_object_ref (renderer);
			
			/* Connect to a few interesting signals */
			g_signal_connect (renderer, 
					  "media-changed",
					  G_CALLBACK (media_changed_cb), 
					  NULL);
			g_signal_connect (renderer, 
					  "state-changed",
					  G_CALLBACK (state_changed_cb), 
					  NULL);
			g_signal_connect (renderer, 
					  "metadata-changed",
					  G_CALLBACK (metadata_changed_cb), 
					  NULL);
			g_signal_connect (renderer, 
					  "error",
					  G_CALLBACK (error_cb), 
					  NULL);

			/* When we find the renderer we are interested in,
			   so use it to play something */
			g_timeout_add_seconds (1,
                                               do_assign_playlist_request,
                                               user_data);
		} else {
			g_print ("[INFO]     Not interesting. Skipping...\n");
		}
	}
}

static void
renderer_removed_cb (MafwRegistry * registry, 
		     GObject *renderer, 
		     gpointer user_data)
{
	if (MAFW_IS_RENDERER(renderer)) {
		const gchar *name = 
			mafw_extension_get_name (MAFW_EXTENSION(renderer));
		
		g_print("[INFO] Renderer %s removed.\n", name);

		if (MAFW_RENDERER (renderer) == app_renderer) {
			g_print ("[INFO]     Wanted renderer removed!"
				 " Exiting...\n");
			g_object_unref (app_renderer);
			g_main_loop_quit (main_loop);
		}
	}
}

/*
 * Loads MAFW plugins.
 *
 * This function lods out-of-process extensions and hooks to
 * renderer/source-added and renderer/source-removed signals 
 * for dynamic extension discovery and removal.
 *
 * Also, this function allows loading of in-process extensions
 * defined through an environment variable.
 *
 * The object_id parameter is used to play that object as soon
 * as the renderer of interest is loaded.
 */
gboolean static
app_init (gchar *playlist_name) 
{
        GError *error = NULL;
	gchar **plugins = NULL;
	GList *extension_list = NULL;
	MafwRegistry *registry = NULL;

	/* ----- Basic MAFW setup ---- */

	/* Init GType */
        g_type_init ();

	/* Init MAFW log (show all messages) */
	mafw_log_init (G_LOG_DOMAIN ":ALL");

	/* ---- Start out-of-process plugin loading ---- */

	g_print ("[INFO] Checking for out-of-process plugins...\n");

	/* Check available plugins  */
	registry = MAFW_REGISTRY (mafw_registry_get_instance());
	if (registry == NULL) {
		g_error ("app_init: Failed to get MafwRegistry reference\n");
		return FALSE;
	}

	/* Start out-of-process extension discovery */
	mafw_shared_init (registry, &error);
	if (error != NULL)
	{
		g_warning ("Ext. discovery failed: %s",
			   error->message);
		g_error_free(error);
		error = NULL;
	} 

	/* Connect to extension discovery signals. These signals will be
	   emitted when new extensions are started or removed */
	g_signal_connect (registry,
			  "renderer_added", 
			  G_CALLBACK(renderer_added_cb), playlist_name);

	g_signal_connect (registry,
			  "renderer_removed", 
			  G_CALLBACK(renderer_removed_cb), NULL);

	g_signal_connect (registry,
			  "source_added", 
			  G_CALLBACK(source_added_cb), NULL);

	g_signal_connect (registry,
			  "source_removed", 
			  G_CALLBACK(source_removed_cb), NULL);

	/* Also, check for already started extensions */
	extension_list = mafw_registry_get_renderers(registry);
	while (extension_list)
	{
		renderer_added_cb (registry, 
				   G_OBJECT(extension_list->data), NULL);
		extension_list = g_list_next(extension_list);
	}
	
	extension_list = mafw_registry_get_sources(registry);
	while (extension_list)
	{
		source_added_cb (registry, 
				 G_OBJECT(extension_list->data), NULL);
		extension_list = g_list_next(extension_list);
	}

	
	/* ---- Start in-process plugin loading ---- */

	/* MAFW_INP_PLUGINS shold contain a list of paths
	   to plugin files to be loaded in-process */

	g_print ("[INFO] Checking for in-process plugins...\n");
	if (g_getenv("MAFW_INP_PLUGINS") != NULL) {
		plugins = g_strsplit (g_getenv ("MAFW_INP_PLUGINS"),
				      G_SEARCHPATH_SEPARATOR_S, 
				      0);

		for (; NULL != *plugins; plugins++) {
			g_print ("[INFO] Loading in-process plugin %s...\n",
				 *plugins);

			mafw_registry_load_plugin (MAFW_REGISTRY(registry),
						   *plugins, 
						   &error);

			if (error != NULL) {
				gchar* msg;
				msg = g_strdup_printf (
					"Unable to load inp. plugin %s: %s", 
					*plugins,
					error->message);

				g_warning ("Plugin loading failed: %s", msg);

				g_free(msg);
				g_error_free(error);
				error = NULL;
			} 
		}
	} else {
		g_print ("[INFO]     No in-process plugins requested.\n");
	}
}

int
main (int argc, gchar *argv[])
{
	if (argc != 2) {
		g_error ("Please, provide exactly one argument specifying "
			 "the name of the playlist to assign and play.");
	}

	g_print ("[INFO] Starting example...\n");
	app_init (argv[1]);
	g_print ("[INFO] Example started.\n");

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}
