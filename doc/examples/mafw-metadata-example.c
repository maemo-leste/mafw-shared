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

#define WANTED_SOURCE     "Mafw-Tracker-Source"

MafwSource *app_source = NULL;
GMainLoop * main_loop = NULL;

/*
 * This callback is invoked whenever a browse result is available
 */
static void
metadata_request_cb (MafwSource *source,
		     const gchar *object_id,
		     GHashTable *metadata,
		     gpointer user_data,
		     const GError *error)
{
	const gchar *title, *artist, *album, *genre;

	if (error != NULL) {
		g_error ("Metadata error: %s\n", error->message);
	}

	g_print ("[INFO]     Got metadata:\n");
	if (metadata == NULL) {
		title = "Unknown";
		artist = "Unknown";
		album = "Unknown";
		genre = "Unknown";
	} else {
		GValue *v;
		v = mafw_metadata_first (metadata,
					 MAFW_METADATA_KEY_TITLE);
		title = v ? g_value_get_string (v) : "Unknown";
		v = mafw_metadata_first (metadata,
					 MAFW_METADATA_KEY_ARTIST);
		artist = v ? g_value_get_string (v) : "Unknown";
		v = mafw_metadata_first (metadata,
					 MAFW_METADATA_KEY_ALBUM);
		album = v ? g_value_get_string (v) : "Unknown";
		v = mafw_metadata_first (metadata,
					 MAFW_METADATA_KEY_GENRE);
		genre = v ? g_value_get_string (v) : "Unknown";
	}

	g_print ("[INFO]           Title: %s\n", title);
	g_print ("[INFO]          Artist: %s\n", artist);
	g_print ("[INFO]           Album: %s\n", album);
	g_print ("[INFO]           Genre: %s\n", genre);
}


/*
 * This function executes a metadata request on the selected source
 * for the object identifier passed as user data.
 */
static gboolean
do_metadata_request (gpointer user_data)
{
	guint browse_id;
	const gchar *const *keys;
	gchar *object_id = (gchar *) user_data;

	g_print ("[INFO] Requesting metadata for %s on "
		 WANTED_SOURCE ".\n", object_id);

	keys = MAFW_SOURCE_LIST(
	       MAFW_METADATA_KEY_TITLE,
	       MAFW_METADATA_KEY_ARTIST,
	       MAFW_METADATA_KEY_ALBUM,
	       MAFW_METADATA_KEY_GENRE);

	mafw_source_get_metadata (app_source,
				  object_id,
				  keys,
				  metadata_request_cb,
				  NULL);

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

		if (strcmp (name, WANTED_SOURCE) == 0) {
			g_print ("[INFO]     Wanted source found!\n");
			app_source = g_object_ref (source);

			/* When we find the source we are interested in,
			   do a metadata request */
			g_timeout_add_seconds (1,
                                               do_metadata_request,
                                               user_data);
		} else {
			g_print ("[INFO]     Not interesting. Skipping...\n");
		}
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

		if (MAFW_SOURCE (source) == app_source) {
			g_print ("[INFO]     Wanted source removed!"
				 " Exiting...\n");
			g_object_unref (app_source);
			g_main_loop_quit (main_loop);
		}
	}
}

static void
renderer_added_cb (MafwRegistry *registry,
		   GObject *renderer,
		   gpointer user_data)
{
	if (MAFW_IS_RENDERER(renderer)) {
		g_print("[INFO] Renderer %s available.\n",
			mafw_extension_get_name(MAFW_EXTENSION(renderer)));
	}
}

static void
renderer_removed_cb (MafwRegistry * registry,
		     GObject *renderer,
		     gpointer user_data)
{
	if (MAFW_IS_RENDERER(renderer)) {
		g_print("Renderer %s removed.\n",
			mafw_extension_get_name(MAFW_EXTENSION(renderer)));
	}
}

/*
 * Loads MAFW plugins.
 *
 * This function lods out-of-process extensions and hooks to
 * source-added and source-removed signals for dynamic extension
 * discovery and removal.
 *
 * Also, this function allows loading of in-process extensions
 * defined through an environment variable.
 *
 * The object_id parameter is used to request metadata as soon
 * as the source of interest is loaded.
 */
gboolean static
app_init (gchar *object_id)
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
			  G_CALLBACK(renderer_added_cb), NULL);

	g_signal_connect (registry,
			  "renderer_removed",
			  G_CALLBACK(renderer_removed_cb), NULL);

	g_signal_connect (registry,
			  "source_added",
			  G_CALLBACK(source_added_cb), object_id);

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
			 "the object identifier of the item to get "
			 "metadata from.");
	}

	g_print ("[INFO] Starting example...\n");
	app_init (argv[1]);
	g_print ("[INFO] Example started.\n");

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}
