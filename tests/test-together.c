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

/*
 * Test proxies and wrapper together.
 *
 * Forks in the beginning, and executes two sets of tests in parallel:
 * the parent runs as a UI, the child as a wrapped extension.
 *
 * NOTE: Ordering of UI and wrapper test cases has to be the same!
 * Use the mainloop cleverly to synchronize test cases.
 */

#include <string.h>
#include <sys/wait.h>

#include <check.h>
#include <glib-object.h>
#include <stdio.h>
#include <unistd.h>

#include <libmafw/mafw.h>
#include <libmafw/mafw-log.h>
#include "libmafw-shared/mafw-proxy-playlist.h"
#include "libmafw-shared/mafw-playlist-manager.h"
#include <libmafw/mafw-renderer.h>

#include "libmafw-shared/mafw-shared-playlist.h"
#include "libmafw-shared/mafw-scared-playlist-manager.h"
#include "libmafw-shared/mafw-shared-registry.h"

#include "mafw-dbus-wrapper/wrapper.h"
#include "mafw-dbus-wrapper/source-wrapper.h"
#include "mafw-dbus-wrapper/renderer-wrapper.h"

#include "common/mafw-util.h"
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"

#include <checkmore.h>

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN	"test-together"

#define TEST_DONE_SIGNAL "test_done"
#define MAFW_TEST_UI_INTERFACE MAFW_INTERFACE ".test_ui"
#define TEST_PREPARED_SIGNAL "test_prepared"
#define PREPARE_TEST_SIGNAL "prepare_test"
#define METADATA_CHD_OBJ_ID "test_object_id"

static guint ui_prepare_idle_id = 0;

#define FAKE_SOURCE_NAME "DEADBEEF"
#define FAKE_RENDERER_NAME "CAFEBABE"


#if 0
# define VERBOSE	1
# define info		g_debug
#else
# define VERBOSE	0
# define info(...)	/* */
#endif

#ifndef MAFW_DB_PATH
#define MAFW_DB_PATH "mafw.db"
#endif

static DBusConnection *s_bus;

static gboolean signal_wrapper(gpointer user_data);
static gboolean ui_send_prepare_command(gpointer data);
static void ui_wait_for_prepared_state(void);

static gboolean wrapper_add_extensions(gpointer data);

/* Test case ideas
 *
 * status_changed
 * 	test_sc_initial - implemented
 * 	preconditions: no db
 * 	status_changed with all options set (1,1,1, "id")
 * 	expected: valid db row with playlist_id and index NULL
 *
 * 	test_sc_update_objid - implemented
 * 	preconditions: db with valid row playlist_id, index and objectid not
 * 	NULL
 * 	status changed with object_id not NULL
 * 	expected: playlist_id and index not changed, objectid updated
 *
 * 	test_sc_update_null_objid - implemented
 * 	preconditions: db with valid row playlist_id, index and objectid not
 * 	NULL
 * 	status changed with object_id NULL
 * 	expected: playlist_id and index updated, object_id NULL
 *
 * 	test_sc_limit_max - not enabled
 * 	preconditions: no db
 * 	status_changed with options (G_MAXUINT64, G_MAXUINT, 3, NULL)
 * 	expected: valid db row
 *
 * 	test_sc_invalid_args - not implemented
 * 	preconditions: db with valid row playlist_id, index and objectid not
 *      NULL
 * 	status_changed with self NULL others valid
 * 	... (repeat for all vars)
 * 	expected: db stays unthouched
 *
 * get_status
 * 	test_gs_no_db - implemented
 * 	preconditions: no db
 * 	get_status with valid arguments
 * 	expected: return with FALSE, arguments untouched
 *
 * 	test_gs_null_playlist_id - implemented
 * 	preconditions: db with valid row with playlist_id and index NULL,
 * 	object_id not NULL
 * 	get_status with valid arguments
 * 	expected: return with TRUE, playlist_id and index 0, valid objectid
 *
 * 	test_gs_not_null - implemented
 * 	preconditions: db with valid row (1,1,1, "id")
 * 	get_status with valid arguments
 * 	expected: return with TRUE, predefined values in plid, index and
 * 	objectid
 *
 * 	test_gs_limit_max - not enabled
 * 	preconditions: db with valid row (G_MAXUINT64, G_MAXUINT, 3, NULL)
 * 	get_status with valid arguments
 * 	expected: return with TRUE, arguments (G_MAXUINT64, G_MAXUINT, 3,
 * 	NULL)
 *
 * 	test_gs_null_objid - implemented
 * 	preconditions: db with valid row (1,1,1, NULL)
 * 	get_status with valid arguments
 * 	expected: return with TRUE, arguments (1,1,1, NULL)
 *
 * 	test_gs_invalid_args - implemented
 * 	preconditions: nothing
 * 	get_status with self NULL others valid
 * 	get_status with plid NULL others valid
 * 	get_status with index NULL others valid
 * 	get_status with state NULL others valid
 * 	expected: fail with error return
 */


/* Renderer mock-up */
typedef struct {
	MafwRendererClass parent_class;
} MockedRendererClass;

typedef struct {
	MafwRenderer parent;
} MockedRenderer;

GType mocked_renderer_get_type(void);
G_DEFINE_TYPE(MockedRenderer, mocked_renderer, MAFW_TYPE_RENDERER);

/* Globals */
static MafwRegistry *Reg;
static MafwSource *Src;
static MafwRenderer *Renderer;
static GMainLoop *Mainloop;

/* GMQ doesn't quit if this is TRUE.
 * `Everything is fair in war, love and unit tests.' */
static gboolean Dont_quit;
static int Play_called, Play_object_called, Play_uri_called,
	Stop_called, Pause_called, Resume_called, Get_status_called,
	Assign_playlist_called, Next_called, Previous_called,
	Goto_index_called, Get_item_relative_called, Set_position_called,
	Get_position_called, Buffering_info_called,
	Metadata_changed_called;

static void quit_main_loop(gchar const *fun)
{
	if (!Dont_quit)
		g_main_loop_quit(Mainloop);
}

static void fill_db(void)
{
	MafwPlaylistManager *pl_manager = NULL;
        MafwProxyPlaylist *pl1 = NULL;
        MafwProxyPlaylist *pl2 = NULL;

	pl_manager = mafw_playlist_manager_get();
        pl1 = mafw_playlist_manager_create_playlist(pl_manager, "t pl 1", NULL);
        pl2 = mafw_playlist_manager_create_playlist(pl_manager, "t pl 2", NULL);

        mafw_playlist_insert_item(MAFW_PLAYLIST(pl1), 0, "t item 1", NULL);
        mafw_playlist_insert_item(MAFW_PLAYLIST(pl1), 1, "t item 2", NULL);
        mafw_playlist_insert_item(MAFW_PLAYLIST(pl1), 2, "t item 3", NULL);
/*
        mafw_playlist_insert_item(MAFW_PLAYLIST(pl2), 0, "t item 1", NULL);
        mafw_playlist_insert_item(MAFW_PLAYLIST(pl2), 1, "t item 2", NULL);
        mafw_playlist_insert_item(MAFW_PLAYLIST(pl2), 2, "t item 3", NULL);
*/

	g_object_unref(G_OBJECT(pl1));
	g_object_unref(G_OBJECT(pl2));
}


static void play(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	Play_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void play_object(MafwRenderer *self, const gchar *object_id,
			MafwRendererPlaybackCB callback, gpointer user_data)
{
	Play_object_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void play_uri(MafwRenderer *self, const gchar *uri,
		     MafwRendererPlaybackCB callback, gpointer user_data)
{
	Play_uri_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void stop(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	Stop_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void _pause(MafwRenderer *self, MafwRendererPlaybackCB callback,
		   gpointer user_data)
{
	Pause_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void resume(MafwRenderer *self, MafwRendererPlaybackCB callback,
		   gpointer user_data)
{
	Resume_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void get_status(MafwRenderer *self, MafwRendererStatusCB callback,
		       gpointer user_data)
{
	fail_if(callback == NULL, "Get status callback is NULL.");

	/* This fails on second time so that we can test synchronous
	   method error wrapping */

	if (Get_status_called != 0) {

		MafwPlaylist *playlist;
		guint index;
		MafwPlayState state;

		/* Prepare reply. */
		playlist = MAFW_PLAYLIST(
			mafw_playlist_manager_create_playlist(
				mafw_playlist_manager_get(),
				"0xFA52", NULL));
		index = 42;
		state = Paused;

		callback(self, playlist, index, state, "DEADBEEF::foo",
			 user_data, NULL);
	} else {

		GError* error = NULL;
		g_set_error(&error,
			    MAFW_EXTENSION_ERROR,
			    MAFW_EXTENSION_ERROR_FAILED,
			    "Failed because the test case requires it.");
		callback(self, NULL, 0, 0, NULL, user_data, error);
		g_error_free(error);
	}

	Get_status_called++;
	quit_main_loop(G_STRFUNC);
}

static void next(MafwRenderer *self, MafwRendererPlaybackCB callback,
		 gpointer user_data)
{
	Next_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void previous(MafwRenderer *self, MafwRendererPlaybackCB callback,
		     gpointer user_data)
{
	Previous_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static gboolean assign_playlist(MafwRenderer *self, MafwPlaylist *playlist,
				GError **error)
{
	/* Validate what we got. */
	fail_if(strcmp(mafw_playlist_get_name(playlist), "maj playlist"));
	Assign_playlist_called++;

	if (error)
	{
		g_set_error(error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
	}

	quit_main_loop(G_STRFUNC);

	return FALSE;
}

static void goto_index(MafwRenderer *self, guint index,
                       MafwRendererPlaybackCB callback,
		       gpointer user_data)
{
	Goto_index_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void get_item_relative(MafwRenderer *self, gint offset,
			      MafwRendererItemCB callback, gpointer user_data)
{
	Get_item_relative_called++;

	if (callback != NULL)
	{
		GError* error = NULL;
		/* This also returns an error, so that we can test error
		   signal wrapping */
		g_set_error(&error,
			    MAFW_RENDERER_ERROR,
			    MAFW_RENDERER_ERROR_CANNOT_PLAY,
			    "Failed because the test case requires it.");
		callback(self, NULL, 0, user_data, error);
		g_error_free(error);
	}

	quit_main_loop(G_STRFUNC);
}

static void set_position(MafwRenderer *self, guint seconds,
			 MafwRendererPositionCB callback, gpointer user_data)
{
	Set_position_called++;
	if (callback != NULL)
		callback(self, seconds, user_data, NULL);
	quit_main_loop(G_STRFUNC);
}

static void get_position(MafwRenderer *self, MafwRendererPositionCB callback,
			 gpointer user_data)
{
	Get_position_called++;
	if (callback != NULL)
		callback(self, 42, user_data, NULL);
	quit_main_loop(G_STRFUNC);
}

static void buffering_info(MafwRenderer *self, gfloat status)
{
	Buffering_info_called++;
	quit_main_loop(G_STRFUNC);
}

static void metadata_changed(MafwRenderer *self,
			     const GList *metadata)
{
	Metadata_changed_called++;
	quit_main_loop(G_STRFUNC);
}

static void mocked_renderer_class_init(MockedRendererClass *klass)
{
	klass->parent_class.play = play;
	klass->parent_class.play_object = play_object;
	klass->parent_class.play_uri = play_uri;
	klass->parent_class.stop = stop;
	klass->parent_class.pause = _pause;
	klass->parent_class.resume = resume;
	klass->parent_class.get_status = get_status;
	klass->parent_class.assign_playlist = assign_playlist;
	klass->parent_class.next = next;
	klass->parent_class.previous = previous;
	klass->parent_class.goto_index = goto_index;
	klass->parent_class.get_item_relative = get_item_relative;
	klass->parent_class.set_position = set_position;
	klass->parent_class.get_position = get_position;
	klass->parent_class.buffering_info = buffering_info;
	klass->parent_class.metadata_changed = metadata_changed;
}

static void mocked_renderer_init(MockedRenderer *src)
{
}

/* Source mock-up. */

static int Browse_called, Get_metadata_called, Set_metadata_called,
	   Create_object_called, Destroy_object_called;

typedef struct {
	MafwSourceClass parent_class;
} MockedSourceClass;

typedef struct {
	MafwSource parent;
} MockedSource;

GType mocked_source_get_type(void);
G_DEFINE_TYPE(MockedSource, mocked_source, MAFW_TYPE_SOURCE);

struct foo {
	MafwSource *self;
	MafwSourceBrowseResultCb cb;
	GError *error;
	gpointer data;
};

typedef struct {
	MafwSource *self;
	gchar *object_id;
	MafwSourceMetadataSetCb cb;
	gpointer cbdata;
} SetMetadataInfo;

typedef struct {
	MafwSource *self;
	gchar *parent;
	GHashTable *metadata;
	MafwSourceObjectCreatedCb cb;
	gpointer cbdata;
} CreateObjectInfo;

typedef struct {
	MafwSource *self;
	gchar *objectid;
	MafwSourceObjectDestroyedCb cb;
	gpointer cbdata;
} DestroyObjectInfo;

static gboolean emit_browse_result(struct foo *d)
{
	GHashTable *md;

	md = mafw_metadata_new();
	mafw_metadata_add_str(md, "uri", "oh.no");

	/* Result. */
	d->cb(d->self, 123, 0, 0, "DEADBEEF::bar", md, d->data, d->error);
	if (d->error)
		g_error_free(d->error);
	g_free(d);
	g_hash_table_unref(md);
	quit_main_loop(G_STRFUNC);
	return FALSE;
}

static guint browse(MafwSource *self,
		    const gchar *object_id,
		    gboolean recursive, const gchar *filter,
		    const gchar *sort_criteria,
		    const gchar *const *metadata_keys,
		    guint skip_count, guint item_count,
		    MafwSourceBrowseResultCb cb, gpointer user_data,
		    GError **error)
{
	struct foo *d;
	Browse_called++;

	/* Validate arguments. */
	if (strcmp(object_id, "DEADBEEF::foo") == 0) {
		/* This is the correct object id */
		fail_unless(recursive == FALSE);
		fail_if(strcmp(filter, "(egyik=masik)"));
		fail_if(!metadata_keys || !metadata_keys[0]
			|| strcmp(metadata_keys[0], "uri")
			|| metadata_keys[1] != NULL);
		fail_unless(skip_count == 11);
		fail_unless(item_count == 99);

		if(strcmp(sort_criteria, "+x,-y") == 0) {
			/* Pass lots of stuff to idle function. */
			d = g_new0(struct foo, 1);
			d->self = self;
			d->cb = cb;
			d->error = NULL;
			d->data = user_data;
			g_idle_add((GSourceFunc)emit_browse_result, d);
			return 123;
		}
		else {
			/* Set this error for the call */
			g_set_error(error, MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_INVALID_SORT_STRING,
				    "Invalid sort string");
			g_main_loop_quit(Mainloop);
			return MAFW_SOURCE_INVALID_BROWSE_ID;
		}
	} else {
		d = g_new0(struct foo, 1);
		d->self = self;
		d->cb = cb;
		/* Set this error for the browse result */
		g_set_error(&(d->error), MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_OBJECT_ID_NOT_AVAILABLE,
			    "Object id not found");
		d->data = user_data;
		g_idle_add((GSourceFunc)emit_browse_result, d);

		return 123;
	}
}

static gboolean get_metadata(MafwSource *self,
			     const gchar *object_id,
			     const gchar *const *metadata_keys,
			     MafwSourceMetadataResultCb cb, gpointer data,
			     GError **error)
{
	GHashTable *md;
	GError *result_error = NULL;

	Get_metadata_called++;

	if (strcmp(object_id, "DEADBEEF::foo") == 0) {
		/* This is correct object id */
		/* Validate arguments. */
		fail_if(strcmp(metadata_keys[0], "uri"));
		fail_if(strcmp(metadata_keys[1], "bar"));

		/* Results. */
		md = mafw_metadata_new();
		mafw_metadata_add_str(md, "uri", "oh.no");
		mafw_metadata_add_str(md, "bar", "holy hand grenade");
		cb(self, object_id, md, data, NULL);
		g_hash_table_unref(md);
		quit_main_loop(G_STRFUNC);
	} else {

		md = mafw_metadata_new();
		mafw_metadata_add_str(md, "uri", "oh.no");
		mafw_metadata_add_str(md, "bar", "holy hand grenade");
		g_set_error(&result_error, MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_OBJECT_ID_NOT_AVAILABLE,
			    "Object id not found");

		cb(self, "DEADBEEF::ehe", md, data, result_error);
		g_hash_table_unref(md);
		g_error_free(result_error);
		quit_main_loop(G_STRFUNC);
	}

	return TRUE;
}

static gboolean set_metadata_bg(SetMetadataInfo *info)
{
	if (info->object_id) {
		/* No errors, all keys accepted */
		info->cb(info->self, info->object_id, NULL,
			 info->cbdata, NULL);

		g_free(info->object_id);
	} else {
		GError *error = NULL;
		gchar *failed_keys[] = {"failed", "keys", NULL };

		error = g_error_new_literal(MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_UNSUPPORTED_METADATA_KEY,
				    "Unsupported metadata");
		info->cb(info->self, "gimmi_error_later",
			 (const gchar**)failed_keys,
			 info->cbdata, error);
		g_error_free(error);
	}

	g_object_unref(info->self);
	g_free(info);

	quit_main_loop(G_STRFUNC);
	return FALSE;
}

static gboolean set_metadata(MafwSource *self,
			     const gchar *object_id, GHashTable *metadata,
			     MafwSourceMetadataSetCb cb, gpointer cbdata,
			     GError **errp)
{
	SetMetadataInfo *smi;

	Set_metadata_called++;

	if (!strcmp(object_id, "gimmi_error")) {
		g_set_error(errp, MAFW_SOURCE_ERROR,
			    MAFW_EXTENSION_ERROR_NETWORK_DOWN, "lfszp");
		quit_main_loop(G_STRFUNC);
		return FALSE;
	}

	smi = g_new0(SetMetadataInfo, 1);
	smi->self = g_object_ref(self);
	smi->cb	= cb;
	smi->cbdata = cbdata;
       	if (strcmp(object_id, "gimmi_error_later") != 0) {
		smi->object_id = g_strdup(object_id);

		fail_if(g_hash_table_lookup(metadata, "artist") == NULL);
		fail_if(g_hash_table_lookup(metadata, "title") == NULL);
	}
	g_idle_add((GSourceFunc)set_metadata_bg, smi);
	return TRUE;
}


static gboolean create_object_bg(CreateObjectInfo *info)
{
	if (info->parent) {
		gchar *objectid;

		if (info->metadata) {
			objectid = g_strdup_printf("%ss_%s_child",
				info->parent,
				g_value_get_string(mafw_metadata_first(
				       info->metadata, "compliment")));
		} else
			objectid = g_strdup_printf("%ss_child", info->parent);
		info->cb(info->self, objectid, info->cbdata, NULL);
		g_free(objectid);

		g_free(info->parent);
		mafw_metadata_release(info->metadata);
	} else {
		GError *error;

		error = g_error_new_literal(
                        MAFW_SOURCE_ERROR,
                        MAFW_EXTENSION_ERROR_EXTENSION_NOT_RESPONDING,
                        "dead");
		info->cb(info->self, NULL, info->cbdata, error);
		g_error_free(error);
	}

	g_object_unref(info->self);
	g_free(info);

	quit_main_loop(G_STRFUNC);
	return FALSE;
}

static gboolean create_object(MafwSource *self,
			      const gchar *parent, GHashTable *metadata,
			      MafwSourceObjectCreatedCb cb, gpointer cbdata,
			      GError **errp)
{
	CreateObjectInfo *coci;

	Create_object_called++;

	if (!strcmp(parent, "gimmi_error")) {
		g_set_error(errp, MAFW_SOURCE_ERROR,
			    MAFW_EXTENSION_ERROR_NETWORK_DOWN, "lfszp");
		quit_main_loop(G_STRFUNC);
		return FALSE;
	}

	coci = g_new0(CreateObjectInfo, 1);
	coci->self	= g_object_ref(self);
	coci->cb	= cb;
	coci->cbdata	= cbdata;
       	if (strcmp(parent, "gimmi_error_later") != 0) {
		coci->parent = g_strdup(parent);
		if (metadata)
			coci->metadata = g_hash_table_ref(metadata);
	}

	g_idle_add((GSourceFunc)create_object_bg, coci);
	return TRUE;
}

static gboolean destroy_object_bg(DestroyObjectInfo *info)
{
	if (!strcmp(info->objectid, "gimmi_error_later")) {
		GError *error;

		error = g_error_new_literal(MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_OBJECT_ID_NOT_AVAILABLE,
				    "someone_stole_it");
		info->cb(info->self, info->objectid, info->cbdata, error);
		g_error_free(error);
	} else
		info->cb(info->self, info->objectid, info->cbdata, NULL);

	g_object_unref(info->self);
	g_free(info->objectid);
	g_free(info);

	quit_main_loop(G_STRFUNC);
	return FALSE;
}

static gboolean destroy_object(MafwSource *self, const gchar *objectid,
			       MafwSourceObjectDestroyedCb cb, gpointer cbdata,
			       GError **errp)
{
	DestroyObjectInfo *doci;

	Destroy_object_called++;

	if (!strcmp(objectid, "gimmi_error")) {
		g_set_error(errp, MAFW_SOURCE_ERROR,
			    MAFW_EXTENSION_ERROR_INVALID_PARAMS,
			    "sikeresevagy");
		quit_main_loop(G_STRFUNC);
		return FALSE;
	}

	doci = g_new0(DestroyObjectInfo, 1);
	doci->self	= g_object_ref(self);
	doci->cb	= cb;
	doci->cbdata	= cbdata;
	doci->objectid	= g_strdup(objectid);
	g_idle_add((GSourceFunc)destroy_object_bg, doci);
	return TRUE;
}

static void mocked_source_class_init(MockedSourceClass *klass)
{
	klass->parent_class.browse = browse;
	klass->parent_class.get_metadata = get_metadata;
	klass->parent_class.set_metadata = set_metadata;
	klass->parent_class.create_object = create_object;
	klass->parent_class.destroy_object = destroy_object;
}

static void mocked_source_init(MockedSource *src)
{
}

/* Common for both wrapper and UI. */

static gboolean stop_mainloop(gpointer udata)
{
	quit_main_loop(udata);
	return FALSE;
}

/* UI part. */

static void ui_browse_result(MafwSource *src, guint browse_id,
			     gint remaining_count, guint index,
			     const gchar *object_id, GHashTable *metadata,
			     gpointer user_data, const GError *error)
{
	GValue *mi;

	/* Validate arguments. */
	fail_unless(error == NULL);
	fail_unless(browse_id == 123);
	fail_unless(remaining_count == 0);
	fail_unless(index == 0);
	fail_if(strcmp(object_id, "DEADBEEF::bar"));

	mi = g_hash_table_lookup(metadata, "uri");
	fail_if(mafw_metadata_nvalues(mi) != 1);
	fail_if(strcmp(g_value_get_string(mi), "oh.no"));

	quit_main_loop(G_STRFUNC);
}

static void ui_browse_result_with_errors(MafwSource *src, guint browse_id,
					 gint remaining_count, guint index,
					 const gchar *object_id,
					 GHashTable *metadata,
					 gpointer user_data,
					 const GError *error)
{
	fail_if(error == NULL, "Browse result should have an error");
	fail_if(error->domain != MAFW_SOURCE_ERROR,
		"Wrong error domain");
	fail_if(error->code != MAFW_SOURCE_ERROR_OBJECT_ID_NOT_AVAILABLE,
		"Wrong error code");
	fail_if(strcmp(error->message, "Object id not found") != 0,
		"Wrong error message");
	quit_main_loop(G_STRFUNC);
}

static void ui_metadata_result(MafwSource *src, const gchar *object_id,
			       GHashTable *metadata, gpointer user_data,
			       const GError *error)
{
	GValue *mi;

	/* Validate arguments. */
	fail_if(strcmp(object_id, "DEADBEEF::foo"));

	mi = g_hash_table_lookup(metadata, "uri");
	fail_if(mafw_metadata_nvalues(mi) != 1);
	fail_if(strcmp(g_value_get_string(mi), "oh.no"));

	mi = g_hash_table_lookup(metadata, "bar");
	fail_if(mafw_metadata_nvalues(mi) != 1);
	fail_if(strcmp(g_value_get_string(mi), "holy hand grenade"));

	quit_main_loop(G_STRFUNC);
}

static void ui_metadata_result_with_error(MafwSource *src,
                                          const gchar *object_id,
					  GHashTable *metadata,
                                          gpointer user_data,
					  const GError *error)
{
	/* Validate arguments. */
	fail_if(error == NULL, "Error should have been set");
	fail_if(error->domain != MAFW_SOURCE_ERROR,
		"Wrong error domain");
	fail_if(error->code != MAFW_SOURCE_ERROR_OBJECT_ID_NOT_AVAILABLE,
		"Wrong error code");
	fail_if(strcmp(error->message, "Object id not found") != 0,
		"Wrong error message");

	quit_main_loop(G_STRFUNC);
}


static void error_signal(MafwExtension *self, GQuark domain, gint code,
			 gchar *message, gpointer userdata)
{

	fail_if(domain != MAFW_EXTENSION_ERROR, "Wrong error domain");
	fail_if(code != MAFW_EXTENSION_ERROR_FAILED, "Wrong error code");
	fail_if(strcmp(message,
		       "Failed because the test case requires it.") != 0,
		"Wrong error message");

	quit_main_loop(G_STRFUNC);
}

static void ui_src_added(MafwRegistry *reg, MafwSource *src, void *udata)
{
	/* Save the discovered source globally. */
	Src = src;
	fail_unless(MAFW_IS_SOURCE(Src), "source_added parameter is not a "
						"source");
	fail_unless(!strcmp(mafw_extension_get_uuid(MAFW_EXTENSION(Src)),
				FAKE_SOURCE_NAME),
				"Wrong uuid for source");
	if (Src && Renderer) quit_main_loop(G_STRFUNC);
}

static void ui_renderer_added(MafwRegistry *reg, MafwRenderer *renderer,
                              void *udata)
{
	Renderer = renderer;
	fail_unless(MAFW_IS_RENDERER(Renderer),
                    "renderer_added parameter is not a renderer");
	fail_unless(!strcmp(mafw_extension_get_uuid(MAFW_EXTENSION(Renderer)),
				FAKE_RENDERER_NAME),
				"Wrong uuid for renderer");
	g_signal_connect(MAFW_EXTENSION(Renderer), "error",
                         G_CALLBACK(error_signal), NULL);
	if (Src && Renderer) quit_main_loop(G_STRFUNC);
}

static DBusHandlerResult ui_dbus_handler(DBusConnection* conn, DBusMessage* msg,
				      gpointer* udata)
{
	if (dbus_message_has_member(msg, TEST_PREPARED_SIGNAL))
	{
		if (ui_prepare_idle_id)
		{
			g_source_remove(ui_prepare_idle_id);
			ui_prepare_idle_id = 0;
		}
		quit_main_loop(G_STRFUNC);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void setup_ui_dbus(void)
{
	DBusError err;
	int ret;

	dbus_error_init(&err);
	/* Get a reference to the session bus and add a filter. */
	s_bus = dbus_bus_get(DBUS_BUS_SESSION, &err);

	ret = dbus_bus_request_name(s_bus, MAFW_TEST_UI_INTERFACE,
                                    DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) {
		g_critical("Name Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	g_assert (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER == ret);

	dbus_connection_add_filter(s_bus,
				   (DBusHandleMessageFunction) ui_dbus_handler,
				   NULL,
				   NULL);
	dbus_bus_add_match(s_bus,
                           "type='signal'",/*member='" TEST_PREPARED_SIGNAL"'"*/
                           NULL);

	dbus_connection_setup_with_g_main(s_bus, NULL);
}

static void setup_ui(void)
{
	/* Set up the registry, and find the source. */
	Reg = MAFW_REGISTRY(mafw_registry_get_instance());
	Mainloop = g_main_loop_new(NULL, FALSE);

	setup_ui_dbus();

	g_signal_connect(Reg, "source-added", G_CALLBACK(ui_src_added), NULL);
	g_signal_connect(Reg, "renderer-added", G_CALLBACK(ui_renderer_added),
                         NULL);
	g_main_loop_run(Mainloop);
}

/* Source test cases (UI). */

START_TEST(test_ui_source_browse)
{
	guint bid;
	GError *error = NULL;

	bid = mafw_source_browse(Src, "DEADBEEF::foo", FALSE,
				 "(egyik=masik)", "+x,-y",
				 MAFW_SOURCE_LIST("uri"), 11, 99,
				 ui_browse_result, NULL, &error);
	fail_if(bid != 123, "wrong browse id");
	fail_if(error != NULL, "Error was set");
	g_main_loop_run(Mainloop);

	/* Browse with object id which doesn't exist. It should fail
	   in browse result */
	bid = mafw_source_browse(Src, "DEADBEEF::bar", FALSE,
				 "(egyik=masik)", NULL,
				 MAFW_SOURCE_LIST("uri"), 11, 99,
				 ui_browse_result_with_errors, NULL, &error);
	fail_if(bid != 123, "wrong browse id");
	fail_if(error != NULL, "Error was set");
	g_main_loop_run(Mainloop);

	/* Browse with unsupported sort criteria.
	   It should fail immediately. */
	bid = mafw_source_browse(Src, "DEADBEEF::foo", FALSE,
				 "(egyik=masik)", "wrong_sort_string",
				 MAFW_SOURCE_LIST("uri"), 11, 99,
				 ui_browse_result, NULL, &error);
	fail_if(bid != MAFW_SOURCE_INVALID_BROWSE_ID,
                "Browse should have faild");
	fail_if(error == NULL, "Error was not set");
	fail_if(error->domain != MAFW_SOURCE_ERROR,
		"Wrong error domain");
	fail_if(error->code != MAFW_SOURCE_ERROR_INVALID_SORT_STRING,
		"Wrong error code");
	fail_if(strcmp(error->message, "Invalid sort string") != 0,
		"Wrong error message");

	g_error_free(error);
	error = NULL;
}
END_TEST

START_TEST(test_ui_source_get_metadata)
{
	gboolean rv;
	GError *error = NULL;

	/* Successful get_metadata */
	rv = mafw_source_get_metadata(Src,
				 "DEADBEEF::foo",
				 MAFW_SOURCE_LIST("uri", "bar"),
				 (MafwSourceMetadataResultCb)ui_metadata_result,
				 NULL, &error);
	fail_if(rv == FALSE, "get metadata should not have failed");
	fail_if(error != NULL, "Error should not be set");
	g_main_loop_run(Mainloop);

	/* Then get_metadata with invalid object id. Since it is wrapped,
	   the call succeeds and callback contains the error */
	rv = mafw_source_get_metadata(
		Src,
		"DEADBEEF::ehe",
		MAFW_SOURCE_LIST("uri", "bar"),
		(MafwSourceMetadataResultCb)ui_metadata_result_with_error,
		NULL, &error);
	fail_if(rv == FALSE, "get metadata should have failed");
	fail_if(error != NULL, "Error should not be set");
	g_main_loop_run(Mainloop);
}
END_TEST

static void metadata_set(MafwSource *self, const gchar *objectid,
			 const gchar **failed_keys,
			 guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid != NULL);
	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code   != MAFW_EXTENSION_ERROR_NETWORK_DOWN);
	fail_if(strcmp(error->message, "lfszp") != 0);

	quit_main_loop(G_STRFUNC);
}

static void metadata_set_all_ok(MafwSource *self, const gchar *objectid,
				const gchar **failed_keys,
				guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(g_strv_length((gchar**)failed_keys) != 0);
	fail_if(error != NULL);

	quit_main_loop(G_STRFUNC);
}

static void metadata_set_failed_keys(MafwSource *self, const gchar *objectid,
				     const gchar **failed_keys,
				     guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(g_strv_length((gchar**)failed_keys) != 2);
	fail_if(strcmp(failed_keys[0], "failed") != 0);
	fail_if(strcmp(failed_keys[1], "keys") != 0);
	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code   != MAFW_SOURCE_ERROR_UNSUPPORTED_METADATA_KEY);
	fail_if(strcmp(error->message, "Unsupported metadata") != 0);

	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_source_set_metadata)
{
	GError *error;
	gboolean called;
	GHashTable *md;

	/* Immediate failure */
	md = mafw_metadata_new();
	mafw_metadata_add_str(md, "artist", "juice");
	mafw_metadata_add_str(md, "title", "Marilyn");

	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_set_metadata(
			    Src, "gimmi_error", md,
			    (MafwSourceMetadataSetCb)metadata_set,
			    &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);
	/* OK, no failed keys */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_set_metadata(
			    Src, "no_errors", md,
			    (MafwSourceMetadataSetCb)metadata_set_all_ok,
			    &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);

	/* OK, some failed keys */
	called = FALSE;
	error = NULL;
	mafw_metadata_add_str(md, "color", "blue");
	mafw_metadata_add_str(md, "resolution", "50x50");
	fail_unless(mafw_source_set_metadata(
			    Src, "gimmi_error_later", md,
			    (MafwSourceMetadataSetCb)metadata_set_failed_keys,
			    &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);
	mafw_metadata_release(md);
}
END_TEST

/* MafwSource::create_object() */
static void object_not_created_1(MafwSource *self, const gchar *objectid,
				 guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid != NULL);
	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code   != MAFW_EXTENSION_ERROR_NETWORK_DOWN);
	fail_if(strcmp(error->message, "lfszp") != 0);

	quit_main_loop(G_STRFUNC);
}

static void object_not_created_2(MafwSource *self, const gchar *objectid,
				 guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid != NULL);
	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code != MAFW_EXTENSION_ERROR_EXTENSION_NOT_RESPONDING);
	fail_if(strcmp(error->message, "dead") != 0);

	quit_main_loop(G_STRFUNC);
}

static void object_created_1(MafwSource *self, const gchar *objectid,
			     guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(strcmp(objectid, "mummys_child") != 0);
	fail_if(error != NULL);

	quit_main_loop(G_STRFUNC);
}

static void object_created_2(MafwSource *self, const gchar *objectid,
			     guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(strcmp(objectid, "daddys_dearest_child") != 0);
	fail_if(error != NULL);

	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_source_create_object)
{
	GError *error;
	gboolean called;
	GHashTable *md;

	/* Immediate failure */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_create_object(Src, "gimmi_error", NULL,
			      (MafwSourceObjectCreatedCb)object_not_created_1,
			      &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);

	/* Deferred failure */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_create_object(Src, "gimmi_error_later", NULL,
			      (MafwSourceObjectCreatedCb)object_not_created_2,
			      &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);

	/* Success without metadata */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_create_object(Src, "mummy", NULL,
			      (MafwSourceObjectCreatedCb)object_created_1,
			      &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);

	/* Success with metadata */
	md = mafw_metadata_new();
	mafw_metadata_add_str(md, "compliment", "dearest");
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_create_object(Src, "daddy", md,
			      (MafwSourceObjectCreatedCb)object_created_2,
			      &called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);
	mafw_metadata_release(md);
}
END_TEST

/* MafwSource::destroy_object() */
static void object_not_destroyed_1(MafwSource *self, const gchar *objectid,
				   guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(strcmp(objectid, "gimmi_error") != 0);

	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code   != MAFW_EXTENSION_ERROR_INVALID_PARAMS);
	fail_if(strcmp(error->message, "sikeresevagy") != 0);

	quit_main_loop(G_STRFUNC);
}

static void object_not_destroyed_2(MafwSource *self, const gchar *objectid,
				   guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(strcmp(objectid, "gimmi_error_later") != 0);

	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code != MAFW_SOURCE_ERROR_OBJECT_ID_NOT_AVAILABLE);
	fail_if(strcmp(error->message, "someone_stole_it") != 0);

	quit_main_loop(G_STRFUNC);
}

static void object_destroyed(MafwSource *self, const gchar *objectid,
			     guint *called, const GError *error)
{
	fail_if(*called);
	*called = TRUE;

	fail_if(objectid == NULL);
	fail_if(strcmp(objectid, "nokes") != 0);
	fail_if(error != NULL);

	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_source_destroy_object)
{
	GError *error;
	gboolean called;

	/* Immediate failure */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_destroy_object(Src, "gimmi_error",
			(MafwSourceObjectDestroyedCb)object_not_destroyed_1,
		       	&called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);

	/* Deferred failure */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_destroy_object(Src, "gimmi_error_later",
			(MafwSourceObjectDestroyedCb)object_not_destroyed_2,
		       	&called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);

	/* Success failure */
	called = FALSE;
	error = NULL;
	fail_unless(mafw_source_destroy_object(Src, "nokes",
			(MafwSourceObjectDestroyedCb)object_destroyed,
		       	&called, &error));
	fail_if(error != NULL);
	g_main_loop_run(Mainloop);
	fail_if(!called);
}
END_TEST

static void ui_source_metadata_changed(MafwSource *src, const gchar *object_id,
		gboolean *called)
{
	*called = TRUE;
	fail_if(strcmp(object_id, METADATA_CHD_OBJ_ID) != 0);
	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_source_metadata_chaged)
{
	gboolean called = FALSE;

	g_signal_connect(Src,"metadata_changed",
				G_CALLBACK(ui_source_metadata_changed),
				(gpointer)&called);

	g_main_loop_run(Mainloop);

	fail_if(!called);
}
END_TEST

static void ui_source_container_changed(MafwSource *src, const gchar *object_id,
                                        gboolean *called)
{
	*called = TRUE;
	fail_if(strcmp(object_id, METADATA_CHD_OBJ_ID) != 0);
	quit_main_loop(G_STRFUNC);
}


START_TEST(test_ui_source_container_chaged)
{
	gboolean called = FALSE;
	g_signal_connect(Src,"container_changed",
				G_CALLBACK(ui_source_container_changed),
				(gpointer)&called);

	g_main_loop_run(Mainloop);

	fail_if(!called);
}
END_TEST

/* Renderer test cases (UI). */

static void test_ui_renderer_playback_cb(MafwRenderer* renderer,
                                         gpointer user_data,
                                         const GError* error)
{
	/* Play will return with an error */
	fail_if(error == NULL);
	fail_unless(error->domain == MAFW_RENDERER_ERROR);
	fail_unless(error->code == MAFW_RENDERER_ERROR_CANNOT_PLAY);
	fail_if(strcmp(error->message,
		       "Failed because the test case requires it."));
	fail_if(user_data != GINT_TO_POINTER(0x00BADA55));
}

START_TEST(test_ui_renderer_play)
{
	mafw_renderer_play(Renderer, test_ui_renderer_playback_cb,
		       GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_play_object)
{
	mafw_renderer_play_object(Renderer, "foo", test_ui_renderer_playback_cb,
			      GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_play_uri)
{
	mafw_renderer_play_uri(Renderer, "oh.no", test_ui_renderer_playback_cb,
			   GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_stop)
{
	mafw_renderer_stop(Renderer, test_ui_renderer_playback_cb,
		       GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_pause)
{
	mafw_renderer_pause(Renderer, test_ui_renderer_playback_cb,
			GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_resume)
{
	mafw_renderer_resume(Renderer, test_ui_renderer_playback_cb,
			 GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_assign_playlist)
{
	MafwProxyPlaylist *pl;
	GError *error;

	pl = mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(),
		"maj playlist", NULL);

	/* Assign playlist will return with an error */
	mafw_renderer_assign_playlist(Renderer, MAFW_PLAYLIST(pl),
				   &error);
	fail_if(error == NULL);
	fail_unless(error->domain == MAFW_RENDERER_ERROR);
	fail_unless(error->code == MAFW_RENDERER_ERROR_CANNOT_PLAY);
	fail_if(strcmp(error->message,
		       "Failed because the test case requires it."));
	g_error_free(error);
}
END_TEST

START_TEST(test_ui_renderer_next)
{
	mafw_renderer_next(Renderer, test_ui_renderer_playback_cb,
		       GINT_TO_POINTER(0x00BADA55));
}
END_TEST

START_TEST(test_ui_renderer_previous)
{
	mafw_renderer_previous(Renderer, test_ui_renderer_playback_cb,
			   GINT_TO_POINTER(0x00BADA55));
}
END_TEST

static void set_get_position_cb(MafwRenderer *renderer, gint seconds,
				gpointer user_data, const GError* error)
{
	if (seconds != 42)
	{
		printf("Wrong position: %d\n", seconds);
		fail_if(1);
	}
}

START_TEST(test_ui_renderer_set_position)
{
	mafw_renderer_set_position(Renderer, 42, set_get_position_cb, NULL);
}
END_TEST

START_TEST(test_ui_renderer_get_position)
{
	/* This is checked in the above set_get_position_cb function */
	mafw_renderer_get_position(Renderer, set_get_position_cb, NULL);
}
END_TEST

START_TEST(test_ui_renderer_get_metadata)
{
	g_timeout_add_seconds(2, stop_mainloop, (gpointer)G_STRFUNC);
	g_main_loop_run(Mainloop);
}
END_TEST

static gboolean signal_wrapper(gpointer user_data)
{
	mafw_dbus_send(s_bus,
		       mafw_dbus_signal_full(MAFW_REGISTRY_SERVICE, MAFW_OBJECT,
			MAFW_REGISTRY_INTERFACE,TEST_DONE_SIGNAL));
	return FALSE;
}

static gboolean ui_send_prepare_command(gpointer data)
{
	mafw_dbus_send(s_bus,
		       mafw_dbus_signal_full(MAFW_REGISTRY_SERVICE, MAFW_OBJECT,
			MAFW_REGISTRY_INTERFACE,PREPARE_TEST_SIGNAL));
	return TRUE;
}

static void ui_wait_for_prepared_state(void)
{
	ui_prepare_idle_id = g_timeout_add_seconds(1,
                                                   ui_send_prepare_command,
                                                   NULL);
	g_main_loop_run(Mainloop);
}


static void get_status_gs_no_db_cb(MafwRenderer *self,
				   MafwPlaylist *playlist,
				   guint index,
				   MafwPlayState state,
				   const gchar *object_id,
				   gpointer user_data,
				   const GError *error)
{
	quit_main_loop(G_STRFUNC);
	fail_if(0 != index, "Wrong index (%u)", index);
	fail_if(NULL != playlist, "Wrong playlist");
	fail_if(Stopped != state, "Wrong state (%d)", state);
	fail_if(NULL != object_id, "Object id cleared ( %s )", object_id);
}

static gboolean test_ui_gs_no_db_idle(gpointer data)
{
	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
                                 get_status_gs_no_db_cb, NULL);
	return FALSE;
}

START_TEST(test_ui_gs_no_db)
{
	/* Preconditions setup end */
	ui_wait_for_prepared_state();
	g_idle_add(test_ui_gs_no_db_idle, NULL);
	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);
}
END_TEST

static void get_status_gs_no_null_playlist_id_cb(MafwRenderer *self,
						 MafwPlaylist *playlist,
						 guint index,
						 MafwPlayState state,
						 const gchar *object_id,
						 gpointer user_data,
						 const GError *error)
{

	fail_if(0 != index, "Wrong index (%u)", index);
	fail_if(NULL == playlist, "Wrong playlist");
	fail_if(1 != state, "Wrong state (%d)", state);
	fail_if(strncmp(object_id, "id", 3), "Wrong object id (%s)", object_id);

	if (playlist != NULL)
		g_object_unref(playlist);
	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_gs_null_playlist_id)
{
	ui_wait_for_prepared_state();
	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
			     get_status_gs_no_null_playlist_id_cb, NULL);
	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);
}
END_TEST

static void get_status_gs_not_null_cb(MafwRenderer *self,
				      MafwPlaylist *playlist,
				      guint index,
				      MafwPlayState state,
				      const gchar *object_id,
				      gpointer user_data,
				      const GError *error)
{
	fail_if(0 != index, "Wrong index (%u)", index);
	fail_if(playlist != user_data, "Wrong playlist");
	fail_if(1 != state, "Wrong state (%d)", state);
	fail_if(strncmp(object_id, "id", 3), "Wrong object id (%s)", object_id);

	g_object_unref(playlist);

	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_gs_not_null)
{
	ui_wait_for_prepared_state();
	MafwPlaylist *playlist;

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));

	/* Preconditions setup end */

	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
                                 get_status_gs_not_null_cb,
			     playlist);

	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);

}
END_TEST

static void get_status_gs_null_objid_cb(MafwRenderer *self,
					MafwPlaylist *playlist,
					guint index,
					MafwPlayState state,
					const gchar *object_id,
					gpointer user_data,
					const GError *error)
{
	fail_if(0 != index, "Wrong index (%u)", index);
	fail_if(playlist != user_data, "Wrong playlist");
	fail_if(1 != state, "Wrong state (%d)", state);
	fail_if(strncmp(object_id, "t item 1", 9),
		"Wrong object id (%s)", object_id);

	g_object_unref(playlist);

	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_gs_null_objid)
{
	ui_wait_for_prepared_state();
	MafwPlaylist *playlist;

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));

	/* Preconditions setup end */

	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
                                 get_status_gs_null_objid_cb,
                                 playlist);

	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);

}
END_TEST

static void get_status_initial_cb(MafwRenderer *self,
				  MafwPlaylist *playlist,
				  guint index,
				  MafwPlayState state,
				  const gchar *object_id,
				  gpointer user_data,
				  const GError *error)
{
	fail_if(0 != index, "Wrong index (%u)", index);
	fail_if(NULL == playlist, "Wrong playlist");
	fail_if(2 != state, "Wrong state (%d)", state);
	fail_if(strncmp(object_id, "id", 3), "Wrong object id (%s)", object_id);
	quit_main_loop(G_STRFUNC);
}


START_TEST(test_ui_sc_initial)
{
	ui_wait_for_prepared_state();

	MafwPlaylist *playlist;

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));

	/* Preconditions setup end */

	/* Should update only objid and state */

	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
                                 get_status_initial_cb, NULL);

	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);

}
END_TEST

static void get_status_update_objid_cb(MafwRenderer *self,
				       MafwPlaylist *playlist,
				       guint index,
				       MafwPlayState state,
				       const gchar *object_id,
				       gpointer user_data,
				       const GError *error)
{
	fail_if(0 != index, "Wrong index (%u)", index);
	fail_if(user_data != playlist, "Wrong playlist");
	fail_if(2 != state, "Wrong state (%d)", state);
	fail_if(strncmp(object_id, "tid", 4),
		"Wrong object id (%s)", object_id);

	g_object_unref(playlist);
	quit_main_loop(G_STRFUNC);
}


START_TEST(test_ui_sc_update_objid)
{
	ui_wait_for_prepared_state();

	MafwPlaylist *playlist1;


	playlist1 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 2", NULL));


	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
                                 get_status_update_objid_cb,
                                 playlist1);

	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);
}
END_TEST

static void get_status_update_null_objid_cb(MafwRenderer *self,
					    MafwPlaylist *playlist,
					    guint index,
					    MafwPlayState state,
					    const gchar *object_id,
					    gpointer user_data,
					    const GError *error)
{
	fail_if(1 != index, "Wrong index (%u)", index);
	fail_if(user_data != playlist, "Wrong playlist");
	fail_if(2 != state, "Wrong state (%d)", state);
	fail_if(strncmp(object_id, "t item 2", 9),
		"Wrong object id (%s)", object_id);

	g_object_unref(playlist);

	quit_main_loop(G_STRFUNC);
}

START_TEST(test_ui_sc_update_null_objid)
{
	ui_wait_for_prepared_state();

	MafwPlaylist *playlist1, *playlist2;


	playlist1 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));
	playlist2 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 2", NULL));

	/* Preconditions setup end */

	mafw_renderer_get_status(MAFW_RENDERER(Renderer),
                                 get_status_update_null_objid_cb,
                                 playlist2);

	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);
}
END_TEST

static void renderer_plist_changed(MafwRenderer *renderer,
                                   MafwPlaylist *playlist,
		gpointer udata)
{
	MafwPlaylist *playlist1 =
		MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 2", NULL));
	static gboolean called = FALSE;
	fail_if(called, "playlist_changed called more than once");
	fail_if(playlist == NULL, "Playlist is NULL");
	fail_if(mafw_proxy_playlist_get_id(
				MAFW_PROXY_PLAYLIST(playlist)) !=
			mafw_proxy_playlist_get_id(
				MAFW_PROXY_PLAYLIST(playlist1)),
			"Playlist is not the same");
	called = TRUE;
}

static void renderer_state_changed(MafwRenderer *renderer, MafwPlayState state,
		gpointer udata)
{
	static gboolean called = FALSE;
	fail_if(called, "state_changed called more than once");
	fail_if(state != 2, "Wrong state in the callback");
	called = TRUE;
	quit_main_loop(G_STRFUNC);
}

static void renderer_media_changed(MafwRenderer *renderer, guint index,
		const gchar *object_id,
		gpointer udata)
{
	static guint called = 0;

	if (!called)
	{
		fail_unless(index == 2 && !object_id,
				"Wrong media_changed parameters no.1");
	} else
	{
		fail_unless(index == 0 && object_id &&
				strncmp(object_id, "id", 3) == 0,
				"Wrong media_changed parameters no.2");
	}
	fail_if(called > 1, "media_changed was emmited more times");
	called++;
}


START_TEST(test_ui_sc_signals)
{
	ui_wait_for_prepared_state();

	g_signal_connect(Renderer,"playlist_changed",
                         G_CALLBACK(renderer_plist_changed),
		NULL);

	g_signal_connect(Renderer,"state_changed",
                         G_CALLBACK(renderer_state_changed),
		NULL);

	g_signal_connect(Renderer,"media_changed",
                         G_CALLBACK(renderer_media_changed),
		NULL);
	g_idle_add(signal_wrapper,NULL);
	ui_wait_for_prepared_state();
	g_main_loop_run(Mainloop);
	signal_wrapper(NULL);
}
END_TEST

static gboolean signal_ui(gpointer data)
{
	mafw_dbus_send(s_bus,
		       mafw_dbus_signal_full(MAFW_TEST_UI_INTERFACE,
                                             MAFW_OBJECT,
                                             MAFW_TEST_UI_INTERFACE,
                                             TEST_PREPARED_SIGNAL));
	return FALSE;
}


static void wrapper_start_wrapping(void)
{
	Reg = MAFW_REGISTRY(mafw_registry_get_instance());

	wrapper_init();
	source_wrapper_init(MAFW_REGISTRY(Reg));
	renderer_wrapper_init(MAFW_REGISTRY(Reg));

	Renderer = g_object_new(mocked_renderer_get_type(),
			    "uuid", "CAFEBABE",
			    "name", "mock-snk",
			    NULL);
	Src = g_object_new(mocked_source_get_type(),
			   "uuid", "DEADBEEF",
			   "name", "mock-src",
			   NULL);

	g_idle_add(wrapper_add_extensions, 0);
	g_main_loop_run(Mainloop);

}

/* Source test cases (Wrapper). */

START_TEST(test_wrapper_source_browse)
{
	/* Normal browse */
	g_main_loop_run(Mainloop);
	fail_if(Browse_called != 1);
	/* Invalid object id browse */
	g_main_loop_run(Mainloop);
	fail_if(Browse_called != 2);
	/* Invalid sort string browse */
	g_main_loop_run(Mainloop);
	fail_if(Browse_called != 3);
}
END_TEST

START_TEST(test_wrapper_source_get_metadata)
{
	/* OK get_metadata */
	g_main_loop_run(Mainloop);
	fail_if(Get_metadata_called != 1);
	/* Failing get_metadata (invalid object id) */
	g_main_loop_run(Mainloop);
	fail_if(Get_metadata_called != 2);
}
END_TEST

START_TEST(test_wrapper_source_set_metadata)
{
	fail_if(Create_object_called != 0);
	/* Error out immedeately */
	g_main_loop_run(Mainloop);
	fail_if(Set_metadata_called != 1);
	/* OK, no failed keys */
	g_main_loop_run(Mainloop);
	fail_if(Set_metadata_called != 2);
	/* OK, with failed keys */
	g_main_loop_run(Mainloop);
	fail_if(Set_metadata_called != 3);
}
END_TEST

START_TEST(test_wrapper_source_create_object)
{
	fail_if(Create_object_called != 0);
	/* Error out immedeately */
	g_main_loop_run(Mainloop);
	fail_if(Create_object_called != 1);
	/* Error out in the background */
	g_main_loop_run(Mainloop);
	fail_if(Create_object_called != 2);
	/* Ok without metadata */
	g_main_loop_run(Mainloop);
	fail_if(Create_object_called != 3);
	/* Ok with metadata */
	g_main_loop_run(Mainloop);
	fail_if(Create_object_called != 4);
}
END_TEST

START_TEST(test_wrapper_source_destroy_object)
{
	fail_if(Destroy_object_called != 0);
	/* Error out immedeately */
	g_main_loop_run(Mainloop);
	fail_if(Destroy_object_called != 1);
	/* Error out in the background */
	g_main_loop_run(Mainloop);
	fail_if(Destroy_object_called != 2);
	/* Ok */
	g_main_loop_run(Mainloop);
	fail_if(Destroy_object_called != 3);
}
END_TEST

START_TEST(test_wrapper_source_metadata_changed)
{
	g_signal_emit_by_name(Src,"metadata_changed",METADATA_CHD_OBJ_ID);
}
END_TEST

START_TEST(test_wrapper_source_container_changed)
{
	g_signal_emit_by_name(Src,"container_changed",METADATA_CHD_OBJ_ID);
}
END_TEST

/* Renderer test cases (Wrapper). */

START_TEST(test_wrapper_renderer_play)
{
	g_main_loop_run(Mainloop);
	fail_if(Play_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_play_object)
{
	g_main_loop_run(Mainloop);
	fail_if(Play_object_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_play_uri)
{
	g_main_loop_run(Mainloop);
	fail_if(Play_uri_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_stop)
{
	g_main_loop_run(Mainloop);
	fail_if(Stop_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_pause)
{
	g_main_loop_run(Mainloop);
	fail_if(Pause_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_resume)
{
	g_main_loop_run(Mainloop);
	fail_if(Resume_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_next)
{
	g_main_loop_run(Mainloop);
	fail_if(Next_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_previous)
{
	g_main_loop_run(Mainloop);
	fail_if(Previous_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_gs_no_db)
{
	g_idle_add(signal_ui, NULL);
	g_main_loop_run(Mainloop);
}
END_TEST

START_TEST(test_wrapper_renderer_gs_null_playlist_id)
{
	MafwPlaylist *playlist;

	fill_db();

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  "id");

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);


	/* Preconditions setup end */

	g_idle_add(signal_ui, NULL);

	g_main_loop_run(Mainloop);

	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist),
				       	NULL);
}
END_TEST

START_TEST(test_wrapper_renderer_gs_not_null)
{
	MafwPlaylist *playlist;

	fill_db();

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));


	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  NULL);

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);


	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  "id");

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);
	/* Preconditions setup end */

	g_idle_add(signal_ui, NULL);

	g_main_loop_run(Mainloop);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist),
				       	NULL);

}
END_TEST

START_TEST(test_wrapper_renderer_gs_null_objid)
{
	MafwPlaylist *playlist;

	fill_db();

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));


	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  NULL);

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);

	/* Preconditions setup end */

	g_idle_add(signal_ui, NULL);

	g_main_loop_run(Mainloop);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist),
				       	NULL);

}
END_TEST

START_TEST(test_wrapper_renderer_sc_initial)
{
	MafwPlaylist *playlist;

	fill_db();

	playlist = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));

	/* Preconditions setup end */

	/* Should update only objid and state */
	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist);

	g_signal_emit_by_name(Renderer, "media_changed",
						  2,
						  "id");

	g_signal_emit_by_name(Renderer, "state_changed",
						  2);

	/* Preconditions setup end */

	g_idle_add(signal_ui, NULL);

	g_main_loop_run(Mainloop);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist),
				       	NULL);
}
END_TEST

START_TEST(test_wrapper_renderer_sc_update_objid)
{
	MafwPlaylist *playlist1, *playlist2;

	fill_db();

	playlist1 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));
	playlist2 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 2", NULL));

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist1);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  "id");

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist1);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  NULL);

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);
	/* Preconditions setup end */

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist2);

	g_signal_emit_by_name(Renderer, "media_changed",
						  2,
						  "tid");

	g_signal_emit_by_name(Renderer, "state_changed",
						  2);

	/* Preconditions setup end */

	g_idle_add(signal_ui, NULL);
	g_main_loop_run(Mainloop);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist1),
				       	NULL);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist2),
				       	NULL);

}
END_TEST

START_TEST(test_wrapper_renderer_sc_update_null_objid)
{
	MafwPlaylist *playlist1, *playlist2;

	fill_db();

	playlist1 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));
	playlist2 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 2", NULL));

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist1);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  "id");

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist1);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  NULL);

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);

	/* Preconditions setup end */

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist2);

	g_signal_emit_by_name(Renderer, "media_changed",
						  2,
						  NULL);

	g_signal_emit_by_name(Renderer, "state_changed",
						  2);



	g_idle_add(signal_ui, NULL);
	g_main_loop_run(Mainloop);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist1),
				       	NULL);
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(playlist2),
				       	NULL);

}
END_TEST

START_TEST(test_wrapper_renderer_sc_signals)
{
	MafwPlaylist *playlist1, *playlist2;

	fill_db();

	playlist1 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 1", NULL));
	playlist2 = MAFW_PLAYLIST(mafw_playlist_manager_create_playlist(
		mafw_playlist_manager_get(), "t pl 2", NULL));

	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist1);

	g_signal_emit_by_name(Renderer, "media_changed",
						  1,
						  "id");

	g_signal_emit_by_name(Renderer, "state_changed",
						  1);
	g_idle_add(signal_ui, NULL);
	g_main_loop_run(Mainloop);
	g_signal_emit_by_name(Renderer, "playlist_changed",
						  playlist2);
	g_signal_emit_by_name(Renderer, "media_changed",
						  2,
						  NULL);
	g_signal_emit_by_name(Renderer, "media_changed",
						  0,
						  "id");
	g_signal_emit_by_name(Renderer, "state_changed",
						  2);
	g_main_loop_run(Mainloop);

	mafw_playlist_manager_destroy_playlist(mafw_playlist_manager_get(),
					       MAFW_PROXY_PLAYLIST(playlist1),
					       NULL);
	mafw_playlist_manager_destroy_playlist(mafw_playlist_manager_get(),
					       MAFW_PROXY_PLAYLIST(playlist2),
					       NULL);

}
END_TEST


START_TEST(test_wrapper_renderer_assign_playlist)
{
	g_main_loop_run(Mainloop);
	fail_if(Assign_playlist_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_set_position)
{
	g_main_loop_run(Mainloop);
	fail_if(Set_position_called != 1);
}
END_TEST

START_TEST(test_wrapper_renderer_get_position)
{
	g_main_loop_run(Mainloop);
	fail_if(Get_position_called != 1);
}
END_TEST

static void gm_cb(const gchar *objectid, GHashTable *metadata,
	       	gpointer udata, const GError *error)
{
	GValue *mi;

	fail_if(strcmp(objectid, "DEADBEEF::foo") != 0);
	fail_if(error != NULL);

	fail_unless((int) udata == 44);

	mi = g_hash_table_lookup(metadata, "uri");
	fail_if(mafw_metadata_nvalues(mi) != 1);
	fail_if(strcmp(g_value_get_string(mi), "oh.no"));

	mi = g_hash_table_lookup(metadata, "bar");
	fail_if(mafw_metadata_nvalues(mi) != 1);
	fail_if(strcmp(g_value_get_string(mi), "holy hand grenade"));

	Dont_quit = FALSE;
	quit_main_loop(G_STRFUNC);
}

START_TEST(test_wrapper_renderer_get_metadata)
{
	g_timeout_add(500, stop_mainloop, (gpointer)G_STRFUNC);
	g_main_loop_run(Mainloop);

	mafw_registry_get_metadata(MAFW_REGISTRY(mafw_registry_get_instance()),
			       "DEADBEEF::foo",
			       MAFW_SOURCE_LIST("uri", "bar"),
			       gm_cb, (gpointer) 44, NULL);
	Dont_quit = TRUE;
	g_main_loop_run(Mainloop);
}
END_TEST

/* Test suites. */

/* Wrapper test suite creation. */
static Suite *wrapped_test_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("Wrapper part");

	tc = tcase_create("Source");
	tcase_add_test(tc, test_wrapper_source_browse);
	tcase_add_test(tc, test_wrapper_source_get_metadata);
	tcase_add_test(tc, test_wrapper_source_set_metadata);
	tcase_add_test(tc, test_wrapper_source_create_object);
	tcase_add_test(tc, test_wrapper_source_destroy_object);
	tcase_add_test(tc, test_wrapper_source_metadata_changed);
	tcase_add_test(tc, test_wrapper_source_container_changed);
	suite_add_tcase(s, tc);

	tc = tcase_create("Renderer");
	tcase_add_test(tc, test_wrapper_renderer_play);
	tcase_add_test(tc, test_wrapper_renderer_play_object);
	tcase_add_test(tc, test_wrapper_renderer_play_uri);
	tcase_add_test(tc, test_wrapper_renderer_stop);
	tcase_add_test(tc, test_wrapper_renderer_pause);
	tcase_add_test(tc, test_wrapper_renderer_resume);
	tcase_add_test(tc, test_wrapper_renderer_next);
	tcase_add_test(tc, test_wrapper_renderer_previous);
	tcase_add_test(tc, test_wrapper_renderer_assign_playlist);
	tcase_add_test(tc, test_wrapper_renderer_get_metadata);
	tcase_add_test(tc, test_wrapper_renderer_set_position);
	tcase_add_test(tc, test_wrapper_renderer_get_position);
	tcase_add_test(tc, test_wrapper_renderer_gs_no_db);
	tcase_add_test(tc, test_wrapper_renderer_gs_null_playlist_id);
	tcase_add_test(tc, test_wrapper_renderer_gs_not_null);
	tcase_add_test(tc, test_wrapper_renderer_gs_null_objid);
	tcase_add_test(tc, test_wrapper_renderer_sc_initial);
	tcase_add_test(tc, test_wrapper_renderer_sc_update_objid);
	tcase_add_test(tc, test_wrapper_renderer_sc_update_null_objid);
	tcase_add_test(tc, test_wrapper_renderer_sc_signals);
	suite_add_tcase(s, tc);

	return s;
}

/* Start the playlist daemon.  Needs to be in a fixture
 * because `check' needs some kind of ``messaging setup''. */
static void start_daemon(void)
{
	checkmore_start("../mafw-playlist-daemon/mafw-playlist-daemon", 0,
		MAFW_SOURCE_LIST("mafw-playlist-daemon", "-kf"));
	usleep(200000);
}

/* UI test suite creation. */
static Suite *ui_test_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("UI part");

	tc = tcase_create("Source");
	tcase_add_test(tc, test_ui_source_browse);
	tcase_add_test(tc, test_ui_source_get_metadata);
	tcase_add_test(tc, test_ui_source_set_metadata);
	tcase_add_test(tc, test_ui_source_create_object);
	tcase_add_test(tc, test_ui_source_destroy_object);
	tcase_add_test(tc, test_ui_source_metadata_chaged);
	tcase_add_test(tc, test_ui_source_container_chaged);
	suite_add_tcase(s, tc);

	tc = tcase_create("Renderer");
	tcase_add_unchecked_fixture(tc, NULL, checkmore_stop);
	tcase_add_test(tc, test_ui_renderer_play);
	tcase_add_test(tc, test_ui_renderer_play_object);
	tcase_add_test(tc, test_ui_renderer_play_uri);
	tcase_add_test(tc, test_ui_renderer_stop);
	tcase_add_test(tc, test_ui_renderer_pause);
	tcase_add_test(tc, test_ui_renderer_resume);
	tcase_add_test(tc, test_ui_renderer_next);
	tcase_add_test(tc, test_ui_renderer_previous);
	tcase_add_test(tc, test_ui_renderer_assign_playlist);
	tcase_add_test(tc, test_ui_renderer_get_metadata);
	tcase_add_test(tc, test_ui_renderer_set_position);
	tcase_add_test(tc, test_ui_renderer_get_position);
	tcase_add_test(tc, test_ui_gs_no_db);
	tcase_add_test(tc, test_ui_gs_null_playlist_id);
	tcase_add_test(tc, test_ui_gs_not_null);
	tcase_add_test(tc, test_ui_gs_null_objid);
	tcase_add_test(tc, test_ui_sc_initial);
	tcase_add_test(tc, test_ui_sc_update_objid);
	tcase_add_test(tc, test_ui_sc_update_null_objid);
	tcase_add_test(tc, test_ui_sc_signals);
	suite_add_tcase(s, tc);

	return s;
}

/* Wrapper process. */

static gboolean wrapper_add_extensions(gpointer data)
{
	mafw_registry_add_extension(Reg, MAFW_EXTENSION(Src));
	mafw_registry_add_extension(Reg, MAFW_EXTENSION(Renderer));
	g_assert(wrapper_get(Src) != NULL);
	g_assert(wrapper_get(Renderer) != NULL);
	quit_main_loop(G_STRFUNC);
	return FALSE;
}

static DBusHandlerResult wrapper_dbus_handler(DBusConnection* conn,
                                              DBusMessage* msg,
                                              gpointer* udata)
{
	if (dbus_message_has_member(msg, TEST_DONE_SIGNAL))
	{
		MafwPlaylistManager *pl_manager = mafw_playlist_manager_get();
		GArray *pl_list = NULL;
		pl_list = mafw_playlist_manager_list_playlists(pl_manager,NULL);
		mafw_playlist_manager_free_list_of_playlists(pl_manager,
			pl_list);
		quit_main_loop(G_STRFUNC);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, PREPARE_TEST_SIGNAL))
	{
		if (g_main_loop_is_running(Mainloop))
		{
			g_idle_add(signal_ui,NULL);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void setup_wrapper_dbus(void)
{
	DBusError err;
	gint ret;

	dbus_error_init(&err);
	/* Get a reference to the session bus and add a filter. */
	s_bus = dbus_bus_get(DBUS_BUS_SESSION, &err);

	ret = dbus_bus_request_name(s_bus, MAFW_REGISTRY_INTERFACE,
                                    DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) {
		g_critical("Name Error (%s)\n", err.message);
		dbus_error_free(&err);
	}

	dbus_connection_add_filter(
                s_bus,
                (DBusHandleMessageFunction) wrapper_dbus_handler,
                NULL,
                NULL);
	dbus_bus_add_match(s_bus, "type='signal'member='" TEST_DONE_SIGNAL"'",
                           NULL);

	dbus_connection_setup_with_g_main(s_bus, NULL);
}


/* Exports a mockup renderer and a mockup source. */
static void setup_wrapper(void)
{
	Mainloop = g_main_loop_new(NULL, FALSE);
	setup_wrapper_dbus();
	wrapper_start_wrapping();
}

/* Forks into a UI and a wrapper process. */
int main(void)
{
	int fate;
	pid_t mate;

	mafw_log_init(NULL);
	unlink("test-together.db");
	setenv("MAFW_DB", "test-together.db", 1);

	g_type_init();
	start_daemon();
	if ((mate = check_fork()) != 0) {
		int status;

		setup_ui();
		fate = checkmore_run(srunner_create(ui_test_suite()), TRUE);
		fate = waitpid(mate, &status, 0) != mate || fate != 0
			|| !WIFEXITED(status) || WEXITSTATUS(status) != 0;
	} else {
		setup_wrapper();
		fate = checkmore_run(srunner_create(wrapped_test_suite()),
			       	TRUE);
	}

	return fate;
}
