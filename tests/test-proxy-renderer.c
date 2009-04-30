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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <libmafw/mafw-registry.h>
#include <libmafw/mafw-metadata.h>

#include <glib.h>
#include <check.h>
#include "libmafw-shared/mafw-proxy-renderer.h"
#include "libmafw-shared/mafw-proxy-playlist.h"
#include "libmafw-shared/mafw-playlist-manager.h"
#include "common/dbus-interface.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>


#include "common/mafw-util.h"
#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"

#include <checkmore.h>
#include "mockbus.h"

/* Set to 1 to get extra messages. */
#if 0
# define VERBOSE	1
# define info		g_debug
#else
# define VERBOSE	0
# define info(...)	/* */
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "test-proxy-renderer"

#define RENDERER_UUID "uuid"

/* For mafw_dbus_*() */
#define MAFW_DBUS_PATH MAFW_RENDERER_OBJECT "/" RENDERER_UUID
#define MAFW_DBUS_DESTINATION MAFW_RENDERER_SERVICE ".fake." RENDERER_UUID
#define MAFW_DBUS_INTERFACE MAFW_RENDERER_INTERFACE

/* Unit tests:
 *
 * x play, stop, get_status
 * x signals
 * - playlist_changed signal with real playlist
 * - compare the received metadata_changed with the sent one
 */

static GMainLoop *Mainloop;

static gboolean play_called;

static void play_cb(MafwRenderer* renderer, gpointer user_data,
                    const GError* error)
{
	play_called = TRUE;
	fail_if(error, "play_cb has error");
}

static gboolean play_error_called;
static void play_cb_error(MafwRenderer* renderer, gpointer user_data,
                          const GError* error)
{
	play_error_called = TRUE;
	fail_if(error == NULL, "play_cb does not have an error");
}

START_TEST(test_play)
{
	MafwProxyRenderer *sp = NULL;

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);

	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY));
	mockbus_reply();
	mafw_renderer_play(MAFW_RENDERER(sp), play_cb, NULL);
	fail_unless(play_called, "Play-cb not called");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY_OBJECT,
				MAFW_DBUS_STRING("Test_id")));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_play_object(MAFW_RENDERER(sp), "Test_id", play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for play_object");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY_URI,
				MAFW_DBUS_STRING("Test_id")));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_play_uri(MAFW_RENDERER(sp), "Test_id", play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for play_uri");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_STOP));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_stop(MAFW_RENDERER(sp), play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for stop");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_PAUSE));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_pause(MAFW_RENDERER(sp), play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for pause");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_RESUME));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_resume(MAFW_RENDERER(sp), play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for resume");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_NEXT));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_next(MAFW_RENDERER(sp), play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for next");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_PREVIOUS));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_previous(MAFW_RENDERER(sp), play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for previous");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GOTO_INDEX,
				MAFW_DBUS_UINT32(1)));
	mockbus_reply();
	play_called = FALSE;
	mafw_renderer_goto_index(MAFW_RENDERER(sp), 1, play_cb, NULL);
	fail_unless(play_called, "Play-cb not called for goto_index");

	/* Error occured */
	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GOTO_INDEX,
				MAFW_DBUS_UINT32(1)));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mafw_renderer_goto_index(MAFW_RENDERER(sp), 1, play_cb_error, NULL);
	fail_unless(play_error_called, "Play-cb not called for goto_index with "
				"error reply");

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

static void set_get_position_cb(MafwRenderer *renderer, gint seconds,
				gpointer user_data, const GError *error)
{
	fail_if(GPOINTER_TO_INT(user_data) != 0xACDCABBA, "Wrong userdata");
	fail_if(seconds != 31337, "Wrong position returned");
	fail_if(error != NULL);
}

static void set_get_position_error_cb(MafwRenderer *renderer, gint seconds,
				gpointer user_data, const GError *error)
{
	fail_if(GPOINTER_TO_INT(user_data) != 0xACDCABBA, "Wrong userdata");
	fail_if(error == NULL);
}

START_TEST(test_set_position)
{
	MafwProxyRenderer *sp = NULL;
	guint seconds = 31337;

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_SET_POSITION,
					 MAFW_DBUS_INT32(SeekAbsolute),
					 MAFW_DBUS_INT32(seconds)));
	mockbus_reply(MAFW_DBUS_UINT32(31337));

	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	mafw_renderer_set_position(MAFW_RENDERER(sp), SeekAbsolute,
				seconds, set_get_position_cb,
				GINT_TO_POINTER(0xACDCABBA));

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

START_TEST(test_get_position)
{
	MafwProxyRenderer *sp = NULL;

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GET_POSITION));
	mockbus_reply(MAFW_DBUS_UINT32(31337));

	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	/* Try to get position and check its outcome in the above callback */
	mafw_renderer_get_position(MAFW_RENDERER(sp), set_get_position_cb,
			       GINT_TO_POINTER(0xACDCABBA));

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GET_POSITION));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mafw_renderer_get_position(MAFW_RENDERER(sp), set_get_position_error_cb,
			       GINT_TO_POINTER(0xACDCABBA));
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

static gboolean stat_cb_called;
static void get_status_cb(MafwRenderer *renderer, MafwPlaylist *playlist,
                          guint index, MafwPlayState state,
                          const gchar* object_id, gboolean *is_oid_null,
                          const GError *error)
{
	fail_if(error != NULL, "Get status returned with an error");
	if (!*is_oid_null)
	{
		fail_if(playlist != NULL, "Wrong playlist");
	}
	else
	{
		fail_if(playlist == NULL, "Wrong playlist");
	}
	fail_if(index != 22, "Wrong index");
	fail_if(state != 1, "Wrong play state");
	if (!*is_oid_null)
	{
		fail_if(strcmp(object_id,
                               "All your base are belong to us") != 0,
			"Wrong object ID");
	}
	else
	{
		fail_if(object_id != NULL);
	}
	fail_if(stat_cb_called);
	stat_cb_called = TRUE;
}

static void get_status_error_cb(MafwRenderer *renderer, MafwPlaylist *playlist,
                                guint index, MafwPlayState state,
                                const gchar* object_id, gpointer udata,
                                const GError *error)
{
	fail_if(!error);
	fail_if(playlist);
	fail_if(object_id);
	fail_if(stat_cb_called);
	stat_cb_called = TRUE;
}

START_TEST(test_get_status)
{
	MafwProxyRenderer *sp = NULL;
	gboolean oid_is_NULL = FALSE;

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);

	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GET_STATUS));
	mockbus_reply(MAFW_DBUS_UINT32(MAFW_PROXY_PLAYLIST_INVALID_ID),
		      MAFW_DBUS_UINT32(22),
		      MAFW_DBUS_INT32(1),
		      MAFW_DBUS_STRING("All your base are belong to us"));

	mafw_renderer_get_status(MAFW_RENDERER(sp), (gpointer)get_status_cb,
			     &oid_is_NULL);
	fail_if(!stat_cb_called);
	stat_cb_called = FALSE;

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GET_STATUS));
	mockbus_reply(MAFW_DBUS_UINT32(10),
		      MAFW_DBUS_UINT32(22),
		      MAFW_DBUS_INT32(1),
		      MAFW_DBUS_STRING(""));
	mockbus_expect(mafw_dbus_method_full(
			       DBUS_SERVICE_DBUS,
			       DBUS_PATH_DBUS,
			       DBUS_INTERFACE_DBUS,
			       "StartServiceByName",
			       MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
			       MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(DBUS_START_REPLY_SUCCESS));
	mockbus_expect(mafw_dbus_method_full(
			       MAFW_PLAYLIST_SERVICE,
			       MAFW_PLAYLIST_PATH,
			       MAFW_PLAYLIST_INTERFACE,
			       MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
			       MAFW_DBUS_C_ARRAY(UINT32, guint, 10)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(10),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	oid_is_NULL = TRUE;
	mafw_renderer_get_status(MAFW_RENDERER(sp), (gpointer)get_status_cb,
			     &oid_is_NULL);
	fail_if(!stat_cb_called);
	stat_cb_called = FALSE;

	mockbus_expect(mafw_dbus_method(MAFW_RENDERER_METHOD_GET_STATUS));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mafw_renderer_get_status(MAFW_RENDERER(sp),
                                 (gpointer)get_status_error_cb,
                                 NULL);
	fail_if(!stat_cb_called);
	stat_cb_called = FALSE;

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

START_TEST(test_get_status_invalid)
{
	MafwProxyRenderer *sp = NULL;

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);

	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	/* This should just assert, but not crash */
	expect_ignore(mafw_renderer_get_status(MAFW_RENDERER(sp), NULL, NULL));

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

static gboolean st_chd, med_chd, pllist_chd, buff_inf, metada_inf;

static void check_signals(void)
{
	if (st_chd && med_chd && pllist_chd && buff_inf && metada_inf)
	{
		g_main_loop_quit(Mainloop);
	}
}

static void sp_state_changed(MafwRenderer *self,
			       MafwPlayState state)
{
	fail_if(state != 1, "Wrong state");
	st_chd = TRUE;
	check_signals();
}

static void sp_media_changed(MafwRenderer *self, gint index,
						const gchar *object_id)
{
	static gboolean called_once;
	fail_if(index != 10, "Wrong index");
	if (called_once)
	{
		fail_if(object_id == NULL, "Object id is NULL");
		fail_if(strcmp(object_id, "str") != 0, "Wrong object_id");
		fail_if(med_chd);
		med_chd = TRUE;
	}
	else
	{
		fail_if(object_id != NULL);
		called_once = TRUE;
	}
	check_signals();
}

static void sp_playlist_changed(MafwRenderer *self,
			       MafwPlaylist *playlist)
{
	fail_if(pllist_chd);
	pllist_chd = TRUE;
	fail_if(!playlist);
	check_signals();
}

static void sp_buffering_info(MafwRenderer *self, gfloat status)
{
	fail_if(status != 0.2f, "Wrong buffer info");
	fail_if(buff_inf);
	buff_inf = TRUE;
	check_signals();
}

static void sp_metadata_changed(MafwRenderer *self,
				const gchar *key, GValueArray *value)
{
	fail_if(strcmp(key, "date"));
	fail_unless(value->n_values == 2);
	fail_unless(g_value_get_uint(&value->values[0]) == 2008);
	fail_unless(g_value_get_uint(&value->values[1]) == 05);
	fail_if(metada_inf);
	metada_inf = TRUE;
	check_signals();
}

START_TEST(test_signals)
{
	MafwProxyRenderer *sp;
	GValueArray *value;
	GValue v = {0};

	Mainloop = g_main_loop_new(NULL, FALSE);
	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");
	g_signal_connect(sp, "state-changed", G_CALLBACK(sp_state_changed),
			 NULL);
	g_signal_connect(sp, "media-changed", G_CALLBACK(sp_media_changed),
			 NULL);
	g_signal_connect(sp, "playlist-changed",
                         G_CALLBACK(sp_playlist_changed),
			 NULL);
	g_signal_connect(sp, "buffering-info", G_CALLBACK(sp_buffering_info),
			 NULL);
	g_signal_connect(sp, "metadata-changed",
                         G_CALLBACK(sp_metadata_changed),
			 NULL);

	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_STATE_CHANGED,
					  MAFW_DBUS_INT32(1)));

	mockbus_expect(mafw_dbus_method_full(
			       DBUS_SERVICE_DBUS,
			       DBUS_PATH_DBUS,
			       DBUS_INTERFACE_DBUS,
			       "StartServiceByName",
			       MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
			       MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(DBUS_START_REPLY_SUCCESS));
	mockbus_expect(mafw_dbus_method_full(
			       MAFW_PLAYLIST_SERVICE,
			       MAFW_PLAYLIST_PATH,
			       MAFW_PLAYLIST_INTERFACE,
			       MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
			       MAFW_DBUS_C_ARRAY(UINT32, guint, 10)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(10),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	mockbus_incoming(mafw_dbus_signal_full(NULL,
				MAFW_DBUS_PATH "2",
				MAFW_RENDERER_INTERFACE,
				MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED,
				MAFW_DBUS_UINT32(30)));

	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED,
					  MAFW_DBUS_UINT32(10)));
	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED"2",
					  MAFW_DBUS_UINT32(10)));

	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_ITEM_CHANGED,
					  MAFW_DBUS_INT32(10),
					  MAFW_DBUS_STRING("")));
	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_ITEM_CHANGED,
					  MAFW_DBUS_INT32(10),
					  MAFW_DBUS_STRING("str")));

	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_BUFFERING_INFO,
					  MAFW_DBUS_DOUBLE(0.2f)));
	value = g_value_array_new(2);
	g_value_init(&v, G_TYPE_UINT);
	g_value_set_uint(&v, 2008);
	g_value_array_append(value, &v);
	g_value_set_uint(&v, 05);
	g_value_array_append(value, &v);
	mockbus_incoming(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_METADATA_CHANGED,
					  MAFW_DBUS_STRING("date"),
					  MAFW_DBUS_GVALUEARRAY(value)));
	g_main_loop_run(Mainloop);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	g_value_array_free(value);
	mockbus_finish();
}
END_TEST

START_TEST(test_assign_playlist)
{
	MafwProxyRenderer *sp;
	gpointer playlist;
	GError *err = NULL;

	Mainloop = g_main_loop_new(NULL, FALSE);
	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	sp = MAFW_PROXY_RENDERER(mafw_proxy_renderer_new(
                                         RENDERER_UUID, "fake",
                                         mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	mockbus_expect(mafw_dbus_method_full(
			       DBUS_SERVICE_DBUS,
			       DBUS_PATH_DBUS,
			       DBUS_INTERFACE_DBUS,
			       "StartServiceByName",
			       MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
			       MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(DBUS_START_REPLY_SUCCESS));
	mockbus_expect(mafw_dbus_method_full(
			       MAFW_PLAYLIST_SERVICE,
			       MAFW_PLAYLIST_PATH,
			       MAFW_PLAYLIST_INTERFACE,
			       MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
			       MAFW_DBUS_C_ARRAY(UINT32, guint, 10)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(10),
			MAFW_DBUS_STRING("Entyem-pentyem"))));

	playlist = mafw_playlist_manager_get_playlist(
                mafw_playlist_manager_get(),
                10, NULL);
	fail_if(!playlist);

	mockbus_expect(mafw_dbus_method(
			       MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
					MAFW_DBUS_UINT32(10)));
	mockbus_reply();
	fail_if(!mafw_renderer_assign_playlist(MAFW_RENDERER(sp), playlist,
                                               &err));
	fail_if(err);
	mockbus_expect(mafw_dbus_method(
			       MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
					MAFW_DBUS_UINT32(10)));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	fail_if(mafw_renderer_assign_playlist(MAFW_RENDERER(sp), playlist,
                                              &err));
	fail_if(!err);
	g_error_free(err);

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

/*****************************************************************************
 * Test case management
 *****************************************************************************/
static Suite *mafw_proxy_renderer_suite_new(void)
{
	Suite *suite;

	suite = suite_create("MafwProxyRenderer");
	checkmore_add_tcase(suite, "Play", test_play);
	checkmore_add_tcase(suite, "Set Position", test_set_position);
	checkmore_add_tcase(suite, "Get Position", test_get_position);
	checkmore_add_tcase(suite, "Signals", test_signals);
	checkmore_add_tcase(suite, "Get status", test_get_status);
	checkmore_add_tcase(suite, "Get status invalid",
                            test_get_status_invalid);
	checkmore_add_tcase(suite, "Get status invalid", test_assign_playlist);
	return suite;
}

/*****************************************************************************
 * Test case execution
 *****************************************************************************/

int main(void)
{
	SRunner *r;
	g_type_init();
	r = srunner_create(mafw_proxy_renderer_suite_new());
	return checkmore_run(r, FALSE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
