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
#include <libmafw/mafw-playlist.h>

#include <glib.h>
#include <check.h>
#include "libmafw-shared/mafw-proxy-playlist.h"
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
#define G_LOG_DOMAIN "test-proxy-playlist-msg"

/* The address of the mocked outp renderer. */
#define PLAYLIST_ADDR	"unix:abstract=dbus-test"

/* For mafw_dbus_*() */
#define MAFW_DBUS_PATH MAFW_PLAYLIST_PATH"/1"
#define MAFW_DBUS_INTERFACE MAFW_PLAYLIST_INTERFACE


/* Unit tests:
 *
 * x all the API functions
 */

START_TEST(test_set_get_name)
{
	MafwProxyPlaylist *pl = NULL;
	gchar *name;

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_SET_NAME,
				       DBUS_TYPE_STRING, "Test-name"));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING("Test-name"));

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	mafw_playlist_set_name(MAFW_PLAYLIST(pl), "Test-name");
	name = mafw_playlist_get_name(MAFW_PLAYLIST(pl));
	fail_if(strcmp(name, "Test-name") != 0, "Get_name doesn't work");
	g_free(name);

	/* What happens in case of error... */
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_NAME));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");

	name = mafw_playlist_get_name(MAFW_PLAYLIST(pl));
	fail_if(name);

	g_object_unref(pl);
	mockbus_finish();
}
END_TEST

START_TEST(test_set_get_repeat)
{
	MafwProxyPlaylist *pl = NULL;

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_SET_REPEAT,
				       DBUS_TYPE_BOOLEAN, TRUE));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_REPEAT));
	mockbus_reply(MAFW_DBUS_BOOLEAN(TRUE));

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	mafw_playlist_set_repeat(MAFW_PLAYLIST(pl), TRUE);
	fail_if(mafw_playlist_get_repeat(MAFW_PLAYLIST(pl)) != TRUE,
	       	"Get_repeat doesn't work");

	/*What happens in case of error */
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_REPEAT));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");

	fail_if(mafw_playlist_get_repeat(MAFW_PLAYLIST(pl)) == TRUE,
	       	"Get_repeat doesn't work");

	g_object_unref(pl);

	mockbus_finish();
}
END_TEST


START_TEST(test_shuffle)
{
	MafwProxyPlaylist *pl = NULL;
	GError *err = NULL;

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_IS_SHUFFLED));
	mockbus_reply(MAFW_DBUS_BOOLEAN(FALSE));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_SHUFFLE));
	mockbus_reply();
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_UNSHUFFLE));
	mockbus_reply();

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	fail_if(mafw_playlist_is_shuffled(MAFW_PLAYLIST(pl)) != FALSE,
	       	"Is_shuffled doesn't work");

	fail_if(mafw_playlist_shuffle(MAFW_PLAYLIST(pl), &err) == FALSE,
	       	"shuffle doesn't work");
	fail_if(err);

	/* What happens in case of error */
	fail_if(mafw_playlist_unshuffle(MAFW_PLAYLIST(pl), NULL) == FALSE,
	       	"Unshuffle doesn't work");
	fail_if(err);
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_IS_SHUFFLED));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_SHUFFLE));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_UNSHUFFLE));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");

	fail_if(mafw_playlist_is_shuffled(MAFW_PLAYLIST(pl)) != FALSE,
	       	"Is_shuffled doesn't work");

	fail_if(mafw_playlist_shuffle(MAFW_PLAYLIST(pl), &err) != FALSE,
	       	"shuffle doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	fail_if(mafw_playlist_unshuffle(MAFW_PLAYLIST(pl), &err) != FALSE,
	       	"Unshuffle doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	g_object_unref(pl);

	mockbus_finish();
}
END_TEST

START_TEST(test_manipulation)
{
	MafwProxyPlaylist *pl = NULL;
	GError *err = NULL;
	const gchar *oids[] = {"test::objid", NULL};

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_INSERT_ITEM,
				       DBUS_TYPE_UINT32, 0,
				       MAFW_DBUS_STRVZ(oids)));
	mockbus_reply(MAFW_DBUS_BOOLEAN(TRUE));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_APPEND_ITEM,
				       MAFW_DBUS_STRVZ(oids)));
	mockbus_reply();
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_REMOVE_ITEM,
				       DBUS_TYPE_UINT32, 0));
	mockbus_reply(MAFW_DBUS_BOOLEAN(TRUE));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_CLEAR
				       ));
	mockbus_reply(MAFW_DBUS_BOOLEAN(TRUE));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_MOVE,
				       DBUS_TYPE_UINT32, 0,
				       DBUS_TYPE_UINT32, 1));
	mockbus_reply(MAFW_DBUS_BOOLEAN(TRUE));

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	fail_if(mafw_playlist_insert_item(MAFW_PLAYLIST(pl),0, "test::objid",
			&err) == FALSE,
	       	"insert_item doesn't work");
	fail_if(err);
	fail_if(mafw_playlist_append_item(MAFW_PLAYLIST(pl),"test::objid",
			&err) == FALSE,
	       	"append_item doesn't work");
	fail_if(err);
	fail_if(mafw_playlist_remove_item(MAFW_PLAYLIST(pl),0, &err) == FALSE,
	       	"remove_item doesn't work");
	fail_if(err);
	fail_if(mafw_playlist_clear(MAFW_PLAYLIST(pl), &err) == FALSE,
	       	"clear doesn't work");
	fail_if(err);
	fail_if(mafw_playlist_move_item(MAFW_PLAYLIST(pl),0,1, &err) == FALSE,
	       	"move_item doesn't work");
	fail_if(err);

	/* What happens in case of errors... */
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_INSERT_ITEM,
				       DBUS_TYPE_UINT32, 0,
				       MAFW_DBUS_STRVZ(oids)));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_APPEND_ITEM,
				       MAFW_DBUS_STRVZ(oids)));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_REMOVE_ITEM,
				       DBUS_TYPE_UINT32, 0));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_CLEAR
				       ));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_MOVE,
				       DBUS_TYPE_UINT32, 0,
				       DBUS_TYPE_UINT32, 1));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");

	fail_if(mafw_playlist_insert_item(MAFW_PLAYLIST(pl),0, "test::objid",
			&err) != FALSE,
	       	"insert_item doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(mafw_playlist_append_item(MAFW_PLAYLIST(pl),"test::objid",
			&err) != FALSE,
	       	"append_item doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(mafw_playlist_remove_item(MAFW_PLAYLIST(pl),0, &err) != FALSE,
	       	"remove_item doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(mafw_playlist_clear(MAFW_PLAYLIST(pl), &err) != FALSE,
	       	"clear doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(mafw_playlist_move_item(MAFW_PLAYLIST(pl),0,1, &err) != FALSE,
	       	"move_item doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	g_object_unref(pl);
	mockbus_finish();
}
END_TEST


START_TEST(test_state_functions)
{
	MafwProxyPlaylist *pl = NULL;
	gchar *item;
	GError *err = NULL;

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_ITEM,
				       DBUS_TYPE_UINT32, 1));
	mockbus_reply(MAFW_DBUS_STRING("test::objid"));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_SIZE));
	mockbus_reply(MAFW_DBUS_UINT32(3));

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	item = mafw_playlist_get_item(MAFW_PLAYLIST(pl),1, &err);
	fail_if(strcmp(item, "test::objid")
			!= 0,
	       	"get_item doesn't work");
	g_free(item);
	fail_if(err);

	fail_if(mafw_playlist_get_size(MAFW_PLAYLIST(pl), &err) != 3,
	       	"get_size doesn't work");
	fail_if(err);

	/* What happens in case of errors... */
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_ITEM,
				       DBUS_TYPE_UINT32, 1));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_SIZE));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");

	item = mafw_playlist_get_item(MAFW_PLAYLIST(pl),1, &err);
	fail_if(item, "get_item doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	fail_if(mafw_playlist_get_size(MAFW_PLAYLIST(pl), &err) != 0,
	       	"get_size doesn't work");
	g_object_unref(pl);
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	mockbus_finish();
}
END_TEST

START_TEST(test_iterator)
{
	MafwProxyPlaylist *pl = NULL;
	guint new_idx = 2;
	gchar *oid;
	GError *err = NULL;

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_GET_STARTING_INDEX));
	mockbus_reply(MAFW_DBUS_UINT32(10), MAFW_DBUS_STRING("test::objid"));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_LAST_INDEX));
	mockbus_reply(MAFW_DBUS_UINT32(10), MAFW_DBUS_STRING("test::objid"));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_NEXT,
				       DBUS_TYPE_UINT32, 2));
	mockbus_reply(MAFW_DBUS_UINT32(10), MAFW_DBUS_STRING("test::objid2"));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_PREV,
				       DBUS_TYPE_UINT32, 2));
	mockbus_reply(MAFW_DBUS_UINT32(10), MAFW_DBUS_STRING("test::objid3"));

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	mafw_playlist_get_starting_index(MAFW_PLAYLIST(pl), &new_idx,
					&oid, &err);
	fail_if(err);
	fail_if(new_idx != 10);
	fail_if(!oid);
	fail_if(strcmp(oid, "test::objid"));
	new_idx = 2;
	g_free(oid);
	oid = NULL;

	mafw_playlist_get_last_index(MAFW_PLAYLIST(pl), &new_idx,
					&oid, &err);
	fail_if(err);
	fail_if(new_idx != 10);
	fail_if(!oid);
	fail_if(strcmp(oid, "test::objid"));
	new_idx = 2;
	g_free(oid);
	oid = NULL;

	fail_if(!mafw_playlist_get_next(MAFW_PLAYLIST(pl), &new_idx,
					&oid,&err));
	fail_if(err);
	fail_if(new_idx != 10);
	fail_if(!oid);
	fail_if(strcmp(oid, "test::objid2"));
	new_idx = 2;
	g_free(oid);
	oid = NULL;

	fail_if(!mafw_playlist_get_prev(MAFW_PLAYLIST(pl), &new_idx,
					&oid,&err));
	fail_if(err);
	fail_if(new_idx != 10);
	fail_if(!oid);
	fail_if(strcmp(oid, "test::objid3"));
	new_idx = 2;
	g_free(oid);
	oid = NULL;

	/* Cases, where the oid should not be touched */
	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_GET_STARTING_INDEX));
	mockbus_reply(MAFW_DBUS_UINT32(1), MAFW_DBUS_STRING(""));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_LAST_INDEX));
	mockbus_reply(MAFW_DBUS_UINT32(1), MAFW_DBUS_STRING(""));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_NEXT,
				       DBUS_TYPE_UINT32, 2));
	mockbus_reply(MAFW_DBUS_UINT32(1), MAFW_DBUS_STRING(""));
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_PREV,
				       DBUS_TYPE_UINT32, 2));
	mockbus_reply(MAFW_DBUS_UINT32(1), MAFW_DBUS_STRING(""));

	mafw_playlist_get_starting_index(MAFW_PLAYLIST(pl), &new_idx,
					&oid, &err);
	fail_if(err);
	fail_if(oid);
	mafw_playlist_get_last_index(MAFW_PLAYLIST(pl), &new_idx,
					&oid, &err);
	fail_if(err);
	fail_if(oid);

	fail_if(mafw_playlist_get_next(MAFW_PLAYLIST(pl), &new_idx,
					&oid,&err));
	fail_if(err);
	fail_if(oid);

	fail_if(mafw_playlist_get_prev(MAFW_PLAYLIST(pl), &new_idx,
					&oid,&err));
	fail_if(err);
	fail_if(oid);

	/* In case of error ... */
	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_GET_STARTING_INDEX));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_LAST_INDEX));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_NEXT,
				       DBUS_TYPE_UINT32, 2));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
				       MAFW_PLAYLIST_METHOD_GET_PREV,
				       DBUS_TYPE_UINT32, 2));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");

	mafw_playlist_get_starting_index(MAFW_PLAYLIST(pl), &new_idx,
					&oid, &err);
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(oid);
	mafw_playlist_get_last_index(MAFW_PLAYLIST(pl), &new_idx,
					&oid, &err);
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(oid);

	fail_if(mafw_playlist_get_next(MAFW_PLAYLIST(pl), &new_idx,
					&oid,&err));
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(oid);

	fail_if(mafw_playlist_get_prev(MAFW_PLAYLIST(pl), &new_idx,
					&oid,&err));
	fail_if(!err);
	g_error_free(err);
	err = NULL;
	fail_if(oid);

	g_object_unref(pl);
	mockbus_finish();
}
END_TEST

static gboolean it_mvd_called;

static void item_moved(MafwPlaylist *playlist, guint from, guint to)
{
	fail_if(from != 1, "Wrong from variable");
	fail_if(to != 2, "Wrong to variable");
	it_mvd_called = TRUE;
}

START_TEST(test_signals)
{
	MafwProxyPlaylist *pl = NULL;

	mockbus_reset();

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	g_signal_connect(pl, "item-moved", G_CALLBACK(item_moved),
			 NULL);


	mockbus_incoming(mafw_dbus_signal(MAFW_PLAYLIST_ITEM_MOVED,
					  MAFW_DBUS_UINT32(1),
					  MAFW_DBUS_UINT32(2)));

	mockbus_deliver(mafw_dbus_session(NULL));

	fail_if(it_mvd_called != TRUE, "item-moved signal not emitted");

	g_object_unref(pl);

	mockbus_finish();
}
END_TEST

START_TEST(test_usecount)
{
	MafwProxyPlaylist *pl = NULL;
	GError *err = NULL;

	mockbus_reset();

	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_INCREMENT_USE_COUNT));
	mockbus_reply();
	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_DECREMENT_USE_COUNT));
	mockbus_reply();

	pl = MAFW_PROXY_PLAYLIST(mafw_proxy_playlist_new(1));
	fail_if(pl == NULL, "Failed to create MafwProxyPlaylist");

	fail_if(mafw_playlist_increment_use_count(
                        MAFW_PLAYLIST(pl), &err) == FALSE,
	       	"increment_use_count doesn't work");
	fail_if(err);

	fail_if(mafw_playlist_decrement_use_count(
                        MAFW_PLAYLIST(pl), &err) == FALSE,
	       	"decrement_use_count doesn't work");
	fail_if(err);

	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_INCREMENT_USE_COUNT));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");
	mockbus_expect(mafw_dbus_method(
                               MAFW_PLAYLIST_METHOD_DECREMENT_USE_COUNT));
	mockbus_error(MAFW_RENDERER_ERROR, 2, "testproblem");


	fail_if(mafw_playlist_increment_use_count(
                        MAFW_PLAYLIST(pl), &err) != FALSE,
	       	"increment_use_count doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	fail_if(mafw_playlist_decrement_use_count(
                        MAFW_PLAYLIST(pl), &err)  != FALSE,
	       	"decrement_use_count doesn't work");
	fail_if(!err);
	g_error_free(err);
	err = NULL;

	g_object_unref(pl);

	mockbus_finish();
}
END_TEST

/*****************************************************************************
 * Test case management
 *****************************************************************************/
static Suite *mafw_proxy_playlist_suite_new(void)
{
	Suite *suite;

	suite = suite_create("MafwProxyPlaylist-msg");
if (1)	checkmore_add_tcase(suite, "Set/Get name", test_set_get_name);
if (1)	checkmore_add_tcase(suite, "Set/Get repeat", test_set_get_repeat);
if (1)	checkmore_add_tcase(suite, "Shuffle", test_shuffle);
if (1)	checkmore_add_tcase(suite, "Playlist manipulation", test_manipulation);
if (1)	checkmore_add_tcase(suite, "State functions", test_state_functions);
if (1)	checkmore_add_tcase(suite, "Signals", test_signals);
if (1)	checkmore_add_tcase(suite, "Iterator", test_iterator);
if (1)	checkmore_add_tcase(suite, "Use count", test_usecount);

	return suite;
}

/*****************************************************************************
 * Test case execution
 *****************************************************************************/

int main(void)
{
	SRunner *r;

	g_type_init();
	r = srunner_create(mafw_proxy_playlist_suite_new());
	return checkmore_run(r, TRUE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
