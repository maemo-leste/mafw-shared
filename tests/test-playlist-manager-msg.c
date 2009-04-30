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

#include <string.h>
#include <stdio.h>

#include <check.h>
#include <glib.h>

#include <libmafw/mafw-errors.h>
#include "libmafw-shared/mafw-playlist-manager.h"


#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"

#include <checkmore.h>
#include "mockbus.h"

/* Standard definitions */
/* For mafw-dbus: */
#define MAFW_DBUS_PATH		MAFW_PLAYLIST_PATH
#define MAFW_DBUS_INTERFACE	MAFW_PLAYLIST_INTERFACE

/* Program code */
/* Test mafw_playlist_manager_create_playlist(). {{{ */
START_TEST(test_create_playlist)
{
	GError *error;
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2, *playlist3;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(DBUS_START_REPLY_SUCCESS));
	/* Check that mafw_playlist_manager_create_playlist()
	 * constructs the D-BUS request correctly and returns the
	 * same object when it already exists. */
	manager = mafw_playlist_manager_get();

	/* Create a playlist, expect success. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Entyem-pentyem")));
	mockbus_reply(MAFW_DBUS_UINT32(101));
	error = NULL;
	playlist1 = mafw_playlist_manager_create_playlist(manager,
							  "Entyem-pentyem",
							  &error);
	fail_if(!playlist1);
	fail_if(G_OBJECT(playlist1)->ref_count != 2);
	fail_if(mafw_proxy_playlist_get_id(playlist1) != 101);
	fail_if(error);

	/* See if we get the same playlist from its internal store. */
	playlist2 = mafw_playlist_manager_get_playlist(manager, 101,
							      NULL);
	fail_if(playlist2 != playlist1);
	g_object_unref(playlist2);

	/* Tell the manager to create the same playlist,
	 * which should return the same object as it did previously. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Entyem-pentyem")));
	mockbus_reply(MAFW_DBUS_UINT32(101));
	playlist2 = mafw_playlist_manager_create_playlist(manager,
							  "Entyem-pentyem",
							  NULL);
	fail_if(playlist2 != playlist1);

	/* Create another playlist, expect another object. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Etyepetyelepetye")));
	mockbus_reply(MAFW_DBUS_UINT32(404));
	playlist3 = mafw_playlist_manager_create_playlist(manager,
							"Etyepetyelepetye",
						       	NULL);
	fail_if(!playlist3);
	fail_if(mafw_proxy_playlist_get_id(playlist3) != 404);
	fail_if(playlist3 == playlist1);

	/* Simulate an error condition, expect error. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Elqrtuk")));
	mockbus_error(MAFW_PLAYLIST_ERROR,
		      MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND, "Hihi");
	fail_if(mafw_playlist_manager_create_playlist(manager,
							     "Elqrtuk",
							     &error));
	fail_if(!error);
	fail_if(error->domain != MAFW_PLAYLIST_ERROR);
	fail_if(error->code   != MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND);
	fail_if(strcmp(error->message, "Hihi") != 0);
	g_error_free(error);
	mockbus_finish();
}
END_TEST /* }}} */

/* Test mafw_playlist_manager_destroy_playlist(). {{{ */
START_TEST(test_destroy_playlist)
{
	GError *error;
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	/* Check that mafw_scare_playlist_manager_destroy_playlist()
	 * constructs the D-BUS request and maintains its internal
	 * store as expected. */
	manager = mafw_playlist_manager_get();

	/* Create a playlist to be destroyed. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Entyem-pentyem")));
	mockbus_reply(MAFW_DBUS_UINT32(101));
	playlist1 = mafw_playlist_manager_create_playlist(manager,
							  "Entyem-pentyem",
							  NULL);

	/* Destroy $playlist1, expect no complaints, though its refcount
	 * must be decremented by the manager. */
	g_object_ref(playlist1);
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_DESTROY_PLAYLIST,
					MAFW_DBUS_UINT32(101)));
	error = NULL;
	fail_if(!mafw_playlist_manager_destroy_playlist(manager,
							       playlist1,
							       &error));
	fail_if(error);
	fail_if(G_OBJECT(playlist1)->ref_count != 2);

	/* $playlist1 is not expected to be removed from the
	 * internal store until the daemon confirms. */
	playlist2 = mafw_playlist_manager_get_playlist(manager, 101,
							      NULL);
	fail_if(playlist2 != playlist1);
	g_object_unref(playlist2);

	/* Check that destroying the same playlist is not an error. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_DESTROY_PLAYLIST,
					MAFW_DBUS_UINT32(101)));
	fail_if(!mafw_playlist_manager_destroy_playlist(manager,
							       playlist1,
							       &error));
	fail_if(error);

	mockbus_finish();
}
END_TEST /* }}} */

/* Test MafwPlaylistManager::playlist-created. {{{ */
/* MafwPlaylistManager::playlist-created signal handler. */
static void playlist_created(MafwPlaylistManager *manager,
			     MafwProxyPlaylist  *playlist,
			     MafwProxyPlaylist **playlistp)
{
	/* Verify that it runs for the first time last checked
	 * and store $playlist for further reference. */
	fail_if(!playlist);
	g_assert(playlistp);
	fail_if(*playlistp);
	*playlistp = g_object_ref(playlist);
}

START_TEST(test_playlist_created)
{
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	/* Test that MafwPlaylistManager::playlist_created is emitted
	 * exatly the right time and its parameter is consistent with the
	 * return values of *_create_playlist() and *_get_playlist(). */
	playlist2 = NULL;
	manager = mafw_playlist_manager_get();
	g_signal_connect(manager, "playlist_created",
			 G_CALLBACK(playlist_created), &playlist2);

	/* First test: create a list then simulate the D-BUS confirmation
	 * from the daemon and check that the GObject signal is emitted
	 * the right time. */
	/* Create */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Entyem-pentyem")));
	mockbus_reply(MAFW_DBUS_UINT32(101));
	playlist1 = mafw_playlist_manager_create_playlist(manager,
							  "Entyem-pentyem",
							  NULL);
	fail_if(!playlist1);
	fail_if( playlist2);

	/* Signal */
	mockbus_incoming(mafw_dbus_signal(MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
					  MAFW_DBUS_UINT32(101)));
	mockbus_deliver(NULL);
	fail_unless(playlist2 == playlist1);

	g_object_unref(playlist1);
	g_object_unref(playlist2);
	playlist2 = NULL;

	/* Second test: notify the manager about the creation of a playlist,
	 * anticipate the GObject signal and see that the new playlist is
	 * indeed in the manager's repo. */
	/* Notify */
	mockbus_incoming(mafw_dbus_signal(MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
					  MAFW_DBUS_UINT32(404)));
	mockbus_deliver(NULL);
	fail_if(!playlist2);

	/* Get */
	playlist1 = mafw_playlist_manager_get_playlist(manager,
							      404, NULL);
	fail_if(playlist1 != playlist2);

	/* Clean up */
	g_object_unref(playlist1);
	g_object_unref(playlist2);
	mockbus_finish();
}
END_TEST /* }}} */

/* Test MafwPlaylistManager::playlist-destroyed. {{{ */
/* MafwPlaylistManager::playlist-destroyed signal handler. */
static void playlist_destroyed(MafwPlaylistManager *manager,
			       MafwProxyPlaylist  *playlist,
			       MafwProxyPlaylist **playlistp)
{
	/* Do the exact opposit of playlist_created(). */
	fail_if(!playlist);
	g_assert(playlistp);
	fail_if(*playlistp != playlist);
	g_object_unref(*playlistp);
	*playlistp = NULL;
}

static void playlist_destruction_failed(MafwPlaylistManager *manager,
			       MafwProxyPlaylist  *playlist,
			       MafwProxyPlaylist **playlistp)
{
	/* Do the exact opposit of playlist_created(). */
	fail_if(!playlist);
	g_assert(playlistp);
	fail_if(*playlistp != playlist);
	*playlistp = NULL;
}

START_TEST(test_playlist_destroyed)
{
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	/* Like test_playlist_created(), but for the destroy event. */
	playlist2 = NULL;
	manager = mafw_playlist_manager_get();
	g_signal_connect(manager, "playlist_destroyed",
			 G_CALLBACK(playlist_destroyed), &playlist2);
	g_signal_connect(manager, "playlist_destruction_failed",
			 G_CALLBACK(playlist_destruction_failed), &playlist2);

	/* Create a playlist we'll destroy. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Entyem-pentyem")));
	mockbus_reply(MAFW_DBUS_UINT32(101));
	playlist1 = mafw_playlist_manager_create_playlist(manager,
							  "Entyem-pentyem",
							  NULL);

	/* Ask the manager to destroy it. */
	g_assert(!playlist2);
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_DESTROY_PLAYLIST,
					MAFW_DBUS_UINT32(101)));
	fail_unless(mafw_playlist_manager_destroy_playlist(manager,
								  playlist1,
								  NULL));

	/* Unconfirmed, $playlist1 must still be in the manager's store.
	 * Now confirm the destruction and expect playlist_destroyed()
	 * to notice it. */
	fail_unless(G_OBJECT(playlist1)->ref_count == 1);

	playlist2 = g_object_ref(playlist1);
	mockbus_incoming(mafw_dbus_signal(MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTRUCTION_FAILED,
					  MAFW_DBUS_UINT32(101)));
	mockbus_deliver(NULL);
	fail_unless(!playlist2);

	playlist2 = g_object_ref(playlist1);
	mockbus_incoming(mafw_dbus_signal(
				   MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED,
				   MAFW_DBUS_UINT32(101)));
	mockbus_deliver(NULL);
	fail_unless(!playlist2);

	/* By this time $playlist must have disappeared from the manager. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  101)));
	mockbus_reply(MAFW_DBUS_AST("us"));
	fail_if(mafw_playlist_manager_get_playlist(manager, 101,
							  NULL));

	/* Finally simulate the daemon notifying the manager about the
	 * destruction of a playlist we never heard about.  Our signal
	 * handler is not expected to be called. */
	mockbus_incoming(mafw_dbus_signal(
				   MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED,
				   MAFW_DBUS_UINT32(404)));
	mockbus_deliver(NULL);

	/* Do the same to an existing, but never referenced playlist. */
	mockbus_incoming(mafw_dbus_signal(
				   MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
				   MAFW_DBUS_UINT32(404)));
	mockbus_deliver(NULL);
	mockbus_incoming(mafw_dbus_signal(
				   MAFW_PLAYLIST_SIGNAL_PLAYLIST_DESTROYED,
				   MAFW_DBUS_UINT32(404)));
	mockbus_deliver(NULL);

	mockbus_finish();
}
END_TEST
/* }}} */

/* Test mafw_playlist_manager_get_playlist(). {{{ */
START_TEST(test_get_playlist)
{
	GError *error;
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	manager = mafw_playlist_manager_get();

	/* Look up an unknown playlist; expect a D-BUS call. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  101)));
	mockbus_error(MAFW_PLAYLIST_ERROR,
		      MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND, "Hihi");
	error = NULL;
	playlist1 = mafw_playlist_manager_get_playlist(manager,
							      101, &error);
	fail_if(playlist1);
	fail_if(!error);
	g_error_free(error);
	error=NULL;


	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  101)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	error = NULL;
	playlist1 = mafw_playlist_manager_get_playlist(manager,
							      101, &error);
	fail_if(!playlist1);
	fail_if(mafw_proxy_playlist_get_id(playlist1) != 101);
	fail_if(error);

	/* By this time $playlist1 is known.  Look it up again
	 * but expect no D-BUS activity this time. */
	playlist2 = mafw_playlist_manager_get_playlist(manager,
							      101, NULL);
	fail_if(playlist2 != playlist1);
	g_object_unref(playlist2);

	/* Try to look up an nonexisting playlist.  This is not an error. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  404)));
	mockbus_reply(MAFW_DBUS_AST("us"));
	playlist2 = mafw_playlist_manager_get_playlist(manager,
							      404, &error);
	fail_if(playlist2);
	fail_if(error);

	mockbus_finish();
}
END_TEST /* }}} */

/* Test mafw_playlist_manager_get_playlists(). {{{ */
START_TEST(test_get_playlists)
{
	GError *error;
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2;
	GPtrArray *list;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	manager = mafw_playlist_manager_get();

	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_error(MAFW_PLAYLIST_ERROR,
		      MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND, "Hihi");
	error = NULL;
	list = mafw_playlist_manager_get_playlists(manager, &error);
	fail_if(list);
	fail_if(!error);
	g_error_free(error);

	/* Does manager accept the empty reply? */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_reply(MAFW_DBUS_AST("us"));
	error = NULL;
	list = mafw_playlist_manager_get_playlists(manager, &error);
	fail_if(!list);
	fail_if(error);
	fail_if(list->len > 0);

	/* Create two playlist to be returned lated. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Entyem-pentyem")));
	mockbus_reply(MAFW_DBUS_UINT32(101));
	playlist1 = mafw_playlist_manager_create_playlist(manager,
							  "Entyem-pentyem",
							  NULL);
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CREATE_PLAYLIST,
					MAFW_DBUS_STRING("Etyepetyelepetye")));
	mockbus_reply(MAFW_DBUS_UINT32(404));
	playlist2 = mafw_playlist_manager_create_playlist(manager,
							"Etyepetyelepetye",
						       	NULL);
	g_object_unref(playlist1);
	g_object_unref(playlist2);

	/* Return the two playlists we created plus one more.
	 * See if the objects are the same. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(404),
			MAFW_DBUS_STRING("Etyepetyelepetye")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(303),
			MAFW_DBUS_STRING("Ratata"))));
	list = mafw_playlist_manager_get_playlists(manager, NULL);
	fail_if(!list);
	fail_unless(list->len == 3);
	fail_unless(list->pdata[0] == playlist1);
	fail_unless(list->pdata[1] == playlist2);
	fail_unless(mafw_proxy_playlist_get_id(list->pdata[2]) == 303);

	mockbus_finish();
}
END_TEST /* }}} */

/* Test mafw_playlist_manager_list_playlists(). {{{ */
START_TEST(test_list_playlists)
{
	GError *error;
	MafwPlaylistManager *manager;
	GArray *list;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	/* This test is almost the same as test_get_playlists(),
	 * but simpler. */
	manager = mafw_playlist_manager_get();

	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_error(MAFW_PLAYLIST_ERROR,
		      MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND, "Hihi");
	error = NULL;
	list = mafw_playlist_manager_list_playlists(manager, &error);
	fail_if(list);
	fail_if(!error);
	g_error_free(error);

	/* Does manager accept the empty reply? */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_reply(MAFW_DBUS_AST("us"));
	error = NULL;
	list = mafw_playlist_manager_list_playlists(manager, &error);
	fail_if(!list);
	fail_if(error);
	fail_if(list->len > 0);
	mafw_playlist_manager_free_list_of_playlists(list);
	/* See if the manager can parse an ordinary reply. */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(404),
			MAFW_DBUS_STRING("Etyepetyelepetye")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(303),
			MAFW_DBUS_STRING("Ratata"))));
	list = mafw_playlist_manager_list_playlists(manager, NULL);
	fail_if(!list);
	fail_unless(list->len == 3);
	fail_unless(g_array_index(list, MafwPlaylistManagerItem, 0).id
		    == 101);
	fail_unless(!strcmp(
		g_array_index(list, MafwPlaylistManagerItem, 0).name,
	       	"Entyem-pentyem"));
	fail_unless(g_array_index(list, MafwPlaylistManagerItem, 1).id
		    == 404);
	fail_unless(!strcmp(
		g_array_index(list, MafwPlaylistManagerItem, 1).name,
	       	"Etyepetyelepetye"));
	fail_unless(g_array_index(list, MafwPlaylistManagerItem, 2).id
		    == 303);
	fail_unless(!strcmp(
		g_array_index(list, MafwPlaylistManagerItem, 2).name,
	       	"Ratata"));

	/* Does *_free_list_of_playlists() blow up?
	 * Shouldn't complain about NULL argument either. */
	mafw_playlist_manager_free_list_of_playlists(list);
	mafw_playlist_manager_free_list_of_playlists(NULL);

	mockbus_finish();
}
END_TEST /* }}} */

/* Test mafw_playlist_manager_dup_playlist(). {{{ */
START_TEST(test_dup_playlist)
{
GError *error;
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist1, *playlist2;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	manager = mafw_playlist_manager_get();

	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  101)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	error = NULL;
	playlist1 = mafw_playlist_manager_get_playlist(manager,
							      101, &error);
	fail_if(!playlist1);
	fail_if(mafw_proxy_playlist_get_id(playlist1) != 101);
	fail_if(error);

	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_DUP_PLAYLIST,
					MAFW_DBUS_UINT32(101),
					MAFW_DBUS_STRING("newname")));
	mockbus_reply(MAFW_DBUS_UINT32(202));
	playlist2 = mafw_playlist_manager_dup_playlist(manager, playlist1,
                                                       "newname",
                                                       &error);
	fail_if(error);
	fail_if(!playlist2);

	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_DUP_PLAYLIST,
					MAFW_DBUS_UINT32(101),
					MAFW_DBUS_STRING("newname")));
	mockbus_error(MAFW_PLAYLIST_ERROR,
		      MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND, "Hihi");
	playlist2 = mafw_playlist_manager_dup_playlist(manager, playlist1,
                                                       "newname",
                                                       &error);
	fail_if(!error);
	fail_if(playlist2);
	g_error_free(error);

	mockbus_finish();
}
END_TEST /* }}} */

static guint n_import_id;
static gboolean import_cb_called;

static void pl_import_cb(MafwPlaylistManager *self,
					  guint import_id,
					  MafwProxyPlaylist *playlist,
					  gpointer user_data,
					  const GError *error)
{
	fail_if(import_id != n_import_id);
	fail_if(!playlist);
	fail_if(error);
	import_cb_called = TRUE;
	checkmore_stop_loop();
}

static void pl_import_error_cb(MafwPlaylistManager *self,
					  guint import_id,
					  MafwProxyPlaylist *playlist,
					  gpointer user_data,
					  const GError *error)
{
	fail_if(import_id != n_import_id);
	fail_if(playlist);
	fail_if(!error);
	import_cb_called = TRUE;
	checkmore_stop_loop();
}


/* Test mafw_playlist_manager_list_playlists(). {{{ */
START_TEST(test_import_playlist)
{
	GError *error;
	MafwPlaylistManager *manager;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	/* This test is almost the same as test_get_playlists(),
	 * but simpler. */
	manager = mafw_playlist_manager_get();

	/* No crash tests... */

	expect_ignore(mafw_playlist_manager_import(NULL, NULL, NULL, NULL,
						NULL, NULL));
	expect_ignore(mafw_playlist_manager_import(manager, "test", NULL, NULL,
						NULL, NULL));
	expect_ignore(mafw_playlist_manager_import(manager, NULL, NULL,
						pl_import_cb, NULL, NULL));

	/* Error returned */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("test"),
				  MAFW_DBUS_STRING("")));
	mockbus_error(MAFW_PLAYLIST_ERROR, 1, "testmsg");

	error = NULL;
	fail_if((n_import_id = mafw_playlist_manager_import(manager, "test",
					NULL, pl_import_cb, NULL, &error))
				!= MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID);
	fail_if(error == NULL);
	g_error_free(error);

	/* Check an error-free import*/
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("test"),
				  MAFW_DBUS_STRING("")));
	mockbus_reply(MAFW_DBUS_UINT32(11));

	error = NULL;
	fail_if((n_import_id = mafw_playlist_manager_import(manager, "test",
					NULL, pl_import_cb, NULL, &error))
				== MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID);
	fail_if(error);

	mockbus_incoming(
		mafw_dbus_method(
			MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
			MAFW_DBUS_UINT32(11),
			MAFW_DBUS_UINT32(2)));

	checkmore_spin_loop(-1);

	fail_unless(import_cb_called);

	/* What happens if a wrong cb comes? */
	import_cb_called = FALSE;
	mockbus_incoming(
		mafw_dbus_method(
			MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
			MAFW_DBUS_UINT32(15),
			MAFW_DBUS_UINT32(6)));

	checkmore_spin_loop(500);

	fail_unless(!import_cb_called);

	/* Good call, not error-free cb */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("test"),
				  MAFW_DBUS_STRING("")));
	mockbus_reply(MAFW_DBUS_UINT32(11));

	fail_if((n_import_id = mafw_playlist_manager_import(manager, "test",
					NULL, pl_import_error_cb, NULL, &error))
				== MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID);
	fail_if(error);

	mockbus_incoming(
		mafw_dbus_method(
			MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
			MAFW_DBUS_UINT32(11),
			MAFW_DBUS_STRING("SS"),
			MAFW_DBUS_INT32(10),
			MAFW_DBUS_STRING("ERRORMSG")));

	checkmore_spin_loop(-1);

	fail_unless(import_cb_called);

	/* Test cancel */
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("test"),
				  MAFW_DBUS_STRING("")));
	mockbus_reply(MAFW_DBUS_UINT32(11));

	fail_if((n_import_id = mafw_playlist_manager_import(manager, "test",
					NULL, pl_import_cb, NULL, &error))
				!= 11);
	fail_if(error);
	fail_if((mafw_playlist_manager_cancel_import(manager, 2, &error))
				== TRUE);
	fail_if(error == NULL);
	g_error_free(error);
	error = NULL;
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
				  MAFW_DBUS_UINT32(11)));
	mockbus_reply();
	fail_if((mafw_playlist_manager_cancel_import(manager, 11, &error))
				== FALSE);
	fail_if(error != NULL);

	mockbus_finish();
}
END_TEST /* }}} */

static gboolean check_destroyed_called;
static void check_destroyed(MafwPlaylistManager *manager,
			       MafwProxyPlaylist  *playlist,
			       gpointer data)
{
	/* Do the exact opposit of playlist_created(). */
	fail_if(!playlist);
	fail_if(mafw_proxy_playlist_get_id(playlist) != 404);
	fail_if(check_destroyed_called);
	check_destroyed_called = TRUE;
}

/* What happens, if pld restarts {{{ */
START_TEST(test_crash)
{
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist, *pl2;

	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             "StartServiceByName",
                                             MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                                             MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(0));

	/* This test is almost the same as test_get_playlists(),
	 * but simpler. */
	manager = mafw_playlist_manager_get();
	g_signal_connect(manager, "playlist_destroyed",
			 G_CALLBACK(check_destroyed), NULL);
	mockbus_incoming(
		mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
				      DBUS_INTERFACE_DBUS,
				      "NameOwnerChanged",
				      MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
				      MAFW_DBUS_STRING(""),
				      MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE)));
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_error(MAFW_PLAYLIST_ERROR,
		      MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND, "Hihi");
	mockbus_deliver(NULL);

	mockbus_incoming(
		mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
				      DBUS_INTERFACE_DBUS,
				      "NameOwnerChanged",
				      MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
				      MAFW_DBUS_STRING(""),
				      MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE)));
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(404),
			MAFW_DBUS_STRING("Etyepetyelepetye")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(303),
			MAFW_DBUS_STRING("Ratata"))));
	mockbus_deliver(NULL);
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  404)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(404),
			MAFW_DBUS_STRING("Etyepetyelepetye"))));
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  101)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	playlist = mafw_playlist_manager_get_playlist(manager,
							      404, NULL);
	pl2 = mafw_playlist_manager_get_playlist(manager,
							      101, NULL);

	mockbus_incoming(
		mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
				      DBUS_INTERFACE_DBUS,
				      "NameOwnerChanged",
				      MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
				      MAFW_DBUS_STRING(""),
				      MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE)));
	mockbus_expect(mafw_dbus_method(MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(101),
			MAFW_DBUS_STRING("Entyem-pentyem")),
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(303),
			MAFW_DBUS_STRING("Ratata"))));
	mockbus_deliver(NULL);
	fail_if(!check_destroyed_called);

	mockbus_finish();
}
END_TEST /* }}} */

/* The main function */
int main(void)
{ /* {{{ */
	TCase *tc;
	Suite *suite;

	suite = suite_create("MafwPlaylistManager-msg");
	tc = tcase_create("Message construction");
if (1)	tcase_add_test(tc, test_create_playlist);
if (1)	tcase_add_test(tc, test_destroy_playlist);
if (1)	tcase_add_test(tc, test_playlist_created);
if (1)	tcase_add_test(tc, test_playlist_destroyed);
if (1)	tcase_add_test(tc, test_get_playlist);
if (1)	tcase_add_test(tc, test_get_playlists);
if (1)	tcase_add_test(tc, test_list_playlists);
if (1)	tcase_add_test(tc, test_dup_playlist);
if (1)	tcase_add_test(tc, test_import_playlist);
if (1)	tcase_add_test(tc, test_crash);
	suite_add_tcase(suite, tc);

	return checkmore_run(srunner_create(suite), FALSE);
} /* }}} */

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0 foldmethod=marker: */
