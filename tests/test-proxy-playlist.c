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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <check.h>
#include <checkmore.h>

#include <libmafw/mafw-playlist.h>
#include <libmafw/mafw-db.h>
#include <libmafw/mafw-uri-source.h>
#include <libmafw/mafw-metadata.h>

#include "libmafw-shared/mafw-playlist-manager.h"

/* Standard definitions */
/* Path to the playlist daemon. */
#define MAFW_PLAYLIST_DAEMON	"../mafw-playlist-daemon/mafw-playlist-daemon"

/*----------------------------------------------------------------------------
  Globals
  ----------------------------------------------------------------------------*/

static MafwPlaylist *g_playlist = NULL;
static GMainLoop *Loop;
static gboolean dont_quit;

/* Playlist test content. */
static const gchar *Contents[] = {
	MAFW_URI_SOURCE_UUID "::" "file:///alpha",
	MAFW_URI_SOURCE_UUID "::" "file:///beta",
	MAFW_URI_SOURCE_UUID "::" "file:///gamma",
	MAFW_URI_SOURCE_UUID "::" "file:///delta",
	MAFW_URI_SOURCE_UUID "::" "file:///epsilon",
	MAFW_URI_SOURCE_UUID "::" "file:///whiskey",
	MAFW_URI_SOURCE_UUID "::" "file:///tango",
	MAFW_URI_SOURCE_UUID "::" "file:///foxtrot",
};
#define PLS_SIZE G_N_ELEMENTS(Contents)

/*----------------------------------------------------------------------------
  Fixtures
  ----------------------------------------------------------------------------*/

static void fx_start_daemon(void)
{
	checkmore_start(MAFW_PLAYLIST_DAEMON, 11, NULL);
	g_usleep(500000);
}

static void fx_shared_setup(void)
{
	g_playlist = MAFW_PLAYLIST(
		mafw_playlist_manager_create_playlist(
			mafw_playlist_manager_get(), "myplaylist", NULL));

	/* Check whether the playlist was created successfully */
	fail_unless(MAFW_IS_PROXY_PLAYLIST(g_playlist),
		    "Playlist creation failed.");

	/* Start with a clean slate */
	mafw_playlist_clear(g_playlist, NULL);
}

/* Additional setup of a playlist for test cases of
 * mafw_playlist_get_items_md(). */
static void fx_miwmd_setup(void)
{
	gint i;

	fx_shared_setup();
	for (i = PLS_SIZE - 1; i >= 0; --i)
		mafw_playlist_insert_item(g_playlist, 0, Contents[i], NULL);
}

static void fx_shared_teardown(void)
{
	mafw_playlist_manager_destroy_playlist(
					mafw_playlist_manager_get(),
				       	MAFW_PROXY_PLAYLIST(g_playlist),
				       	NULL);
	g_playlist = NULL;
}

/*----------------------------------------------------------------------------
  Test cases
  ----------------------------------------------------------------------------*/

START_TEST(test_insert_and_get_item)
{
	gchar *item = NULL;
	const gchar *plitems[] = {"ab", "cd", "ef", NULL};
	
	/* Insert some items, including invalid indices */
	fail_unless(mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL));
	fail_unless(mafw_playlist_insert_item(g_playlist, 1, "item 2", NULL));
	expect_fallback(mafw_playlist_insert_item(g_playlist, 1, NULL, NULL),
			FALSE);
	fail_if(mafw_playlist_insert_item(g_playlist, 6, "item 3", NULL));
	fail_if(mafw_playlist_insert_item(g_playlist, -1, "item 4", NULL));
	fail_unless(mafw_playlist_insert_item(g_playlist, 0, "item 0", NULL));
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != 3);

	/* Verify insertion order */
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)),
		"Unable to get item inserted at position 0");
	fail_if(strcmp("item 0", item) != 0,
		"First item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)),
		"Unable to get item inserted at position 1");
	fail_if(strcmp("item 1", item) != 0,
		"Second item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 2, NULL)),
		"Unable to get item inserted at position 2");
	fail_if(strcmp("item 2", item) != 0,
		"Third item in playlist is not the one expected.");
	g_free(item);
	
	/* insert_items */
	fail_unless(mafw_playlist_insert_items(g_playlist, 1, plitems, NULL));

	/* Verify insertion order */
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)),
		"Unable to get item inserted at position 0");
	fail_if(strcmp("item 0", item) != 0,
		"First item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)),
		"Unable to get item inserted at position 1");
	fail_if(strcmp("ab", item) != 0,
		"Second item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 2, NULL)),
		"Unable to get item inserted at position 2");
	fail_if(strcmp("cd", item) != 0,
		"Third item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 3, NULL)),
		"Unable to get item inserted at position 3");
	fail_if(strcmp("ef", item) != 0,
		"Fourth item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 4, NULL)),
		"Unable to get item inserted at position 4");
	fail_if(strcmp("item 1", item) != 0,
		"Fifth item in playlist is not the one expected.");
	g_free(item);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 5, NULL)),
		"Unable to get item inserted at position 5");
	fail_if(strcmp("item 2", item) != 0,
		"Sixth item in playlist is not the one expected.");
	g_free(item);

	/* Get item at an invalid index */
	fail_unless(mafw_playlist_get_item(g_playlist, 17, NULL) == NULL);
	/* Try to insert in a NULL playlist (must not segfault) */
	expect_fallback(mafw_playlist_insert_item(NULL, 0, "item", NULL),
			FALSE);
	/* Try to get item from NULL playlist (must not segfault) */
	expect_fallback(mafw_playlist_get_item(NULL, 0, NULL), NULL);
}
END_TEST

START_TEST(test_append)
{
	gchar *item;
	const gchar *plitems[] = {"ab", "cd", "ef", NULL};
	gchar **items;
	GError *err = NULL;

	fail_unless(mafw_playlist_append_item(g_playlist, "item 1", NULL));
	fail_unless(mafw_playlist_append_item(g_playlist, "item 2", NULL));
	fail_unless(mafw_playlist_append_items(g_playlist, plitems, NULL));
	expect_fallback(mafw_playlist_append_item(g_playlist, NULL, NULL),
			FALSE);
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != 5);

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)));
	fail_if(strcmp("item 1", item) != 0);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)));
	fail_if(strcmp("item 2", item) != 0);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 2, NULL)));
	fail_if(strcmp("ab", item) != 0);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 3, NULL)));
	fail_if(strcmp("cd", item) != 0);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 4, NULL)));
	fail_if(strcmp("ef", item) != 0);
	g_free(item);
	
	fail_if(!(items = mafw_playlist_get_items(g_playlist, 1, 3, NULL)));
	fail_if(strcmp("item 2", items[0]) != 0, "Got: %s", items[0]);
	fail_if(strcmp("ab", items[1]) != 0, "Got: %s", items[1]);
	fail_if(strcmp("cd", items[2]) != 0, "Got: %s", items[2]);
	fail_if(items[3] != NULL);
	g_strfreev(items);
	items = mafw_playlist_get_items(g_playlist, 5, 6, &err);
	fail_if(items);
	fail_if(err == NULL);
	g_error_free(err);
}
END_TEST

START_TEST(test_clear)
{
	/* Clear list */
	mafw_playlist_clear(g_playlist, NULL);
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != 0,
		"After clearing a playlist its size is not 0");

	/* Insert some items */
	mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL);
	mafw_playlist_insert_item(g_playlist, 1, "item 2", NULL);

	/* Clear list */
	fail_unless(mafw_playlist_clear(g_playlist, NULL));
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != 0,
		"After clearing the playlist its size is not 0");
}
END_TEST

START_TEST(test_set_get_name)
{
	const gchar *name = "foobar";
	const gchar *name2 = "NAME2";
	gchar *returned_name = NULL;

	/* Check the initial value */
	returned_name = mafw_playlist_get_name(g_playlist);
	fail_if(returned_name == NULL,
		"Unable to get playlist name");
	fail_if(strcmp(returned_name, "myplaylist") != 0,
		"The initial value of name is not myplaylist as it should be");
	g_free(returned_name);

	/* Set name to the playlist */
	mafw_playlist_set_name(g_playlist, name);

	/* Get the name of the playlist */
	returned_name = mafw_playlist_get_name(g_playlist);
	fail_if(returned_name == NULL,
		"Unable to get playlist name");
	fail_if(strcmp(returned_name, name) != 0,
		"The returned name of the playlist is not correct");
	g_free(returned_name);
	/* Replace the name of the playlist */
	mafw_playlist_set_name(g_playlist, name2);
	returned_name = mafw_playlist_get_name(g_playlist);
	fail_if(returned_name == NULL,
		"Unable to get playlist name");
	fail_if(strcmp(returned_name, name2) != 0,
		"The returned name of the playlist is not correct no. 2");
	g_free(returned_name);
	/* Try to replace the name of the playlist with NULL value */
	expect_ignore(mafw_playlist_set_name(g_playlist, NULL));
	returned_name = mafw_playlist_get_name(g_playlist);
	fail_if(returned_name == NULL,
		"Unable to get playlist name");
	fail_if(strcmp(returned_name, name2) != 0,
		"The returned name of the playlist is not correct no. 3");
	g_free(returned_name);
	/* Try to get/set the name from the NULL playlist (must not segfault) */
	expect_ignore(mafw_playlist_set_name(NULL, ""));
}
END_TEST

START_TEST(test_get_size)
{
	guint size = 0;

	/* Insert some items and get the size */
	mafw_playlist_insert_item(g_playlist, 0, "item 4", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 2", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 3", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL);

	/* Check the initial value */
	size = mafw_playlist_get_size(g_playlist, NULL);
	fail_if(size != 4, "Inserted 4 elements and the size is not 4");

	/* Remove element and check the size */
	mafw_playlist_remove_item(g_playlist, 2, NULL);
	size = mafw_playlist_get_size(g_playlist, NULL);
	fail_if(size != 3,
		"Inserted 4 elements, then removed 1 and the size is not 3");

	/* FIXME: Change the list contents and check the size */

	/* Check that the result of NULL playlist does not segfault */
	expect_fallback(mafw_playlist_get_size(NULL, NULL), 0);

	/* Clear the list and check the size */
	mafw_playlist_clear(g_playlist, NULL);
	size = mafw_playlist_get_size(g_playlist, NULL);
	fail_if(size != 0, "We have cleared the list and the size is not zero");
}
END_TEST

START_TEST(test_iterator)
{
	guint new_idx = 2;
	gchar *oid = NULL;

	mafw_playlist_get_starting_index(g_playlist, &new_idx, &oid,
				NULL);
	fail_if(oid);
	
	fail_if(mafw_playlist_get_next(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(oid);

	fail_if(mafw_playlist_get_prev(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(oid);
	
	/* Insert some items */
	mafw_playlist_insert_item(g_playlist, 0, "item 4", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 2", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 3", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL);
	
	mafw_playlist_get_last_index(g_playlist, &new_idx, &oid,
				NULL);
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "item 4"));
	g_free(oid);
	oid = NULL;
	
	mafw_playlist_get_starting_index(g_playlist, &new_idx, &oid,
				NULL);
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "item 1"));
	g_free(oid);
	oid = NULL;
	
	fail_if(!mafw_playlist_get_next(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(new_idx != 1);
	fail_if(strcmp(oid, "item 3"));
	g_free(oid);
	oid = NULL;

	fail_if(!mafw_playlist_get_prev(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "item 1"));
	g_free(oid);
	oid = NULL;

	fail_if(mafw_playlist_get_prev(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(oid);
	
	new_idx = 3;
	fail_if(mafw_playlist_get_next(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(oid);
	
	mafw_playlist_set_repeat(g_playlist, TRUE);
	fail_if(!mafw_playlist_get_next(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "item 1"));
	g_free(oid);
	oid = NULL;

	fail_if(!mafw_playlist_get_prev(g_playlist, &new_idx, &oid,
				NULL));
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "item 4"));
	g_free(oid);
	oid = NULL;
	
	mafw_playlist_get_last_index(g_playlist, &new_idx, &oid,
				NULL);
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "item 4"));
	g_free(oid);
	oid = NULL;

}
END_TEST

START_TEST(test_remove_items)
{
	guint size = 0;
	gchar *item = NULL;

	/* Insert some items and get the size */
	mafw_playlist_insert_item(g_playlist, 0, "item 4", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 3", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 2", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL);

	/* Check what happens when we remove one element */
	fail_unless(mafw_playlist_remove_item(g_playlist, 2, NULL));

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 2, NULL)),
		"Unable to get item inserted at position 2");
	fail_if(strcmp(item, "item 3") == 0,
		"We have removed one item but it is still there");
	g_free(item);

	/* Try to remove an index that does not exist */
	fail_if(mafw_playlist_remove_item(g_playlist, 22, NULL));
	size = mafw_playlist_get_size(g_playlist, NULL);
	fail_if(size != 3, "After removing a non-existing element from a " \
		"playlist, the number of elements is not correct");

	/* Check that the NULL playlist does not segfault */
	expect_fallback(mafw_playlist_remove_item(NULL, 0, NULL), FALSE);
}
END_TEST

START_TEST(test_move_item)
{
	gchar *item = NULL;

	/* Insert some items */
	mafw_playlist_insert_item(g_playlist, 0, "item 4", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 3", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 2", NULL);
	mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL);

	/* Swap the first two elements */
	fail_unless(mafw_playlist_move_item(g_playlist, 0, 1, NULL));
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)),
		"Unable to get moved item at position 0");
	fail_if(strcmp(item, "item 2"),
		"Element at position 0 is not the one expected: %s", item);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)),
		"Unable to get moved item at position 1");
	fail_if(strcmp(item, "item 1"),
		"Element at position 1 is not the on expected: %s", item);
	g_free(item);

	/* Swap them back */
	fail_unless(mafw_playlist_move_item(g_playlist, 1, 0, NULL));

	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)),
		"Unable to get item moved back to position 0");
	fail_if(strcmp(item, "item 1"),
		"Element at position 0 is not the one expected: %s", item);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)),
		"Unable to get item moved back to position 1");
	fail_if(strcmp(item, "item 2"),
		"Element at position 1 is not the one expected: %s", item);
	g_free(item);

	/* Move second element to the end  */
	fail_unless(mafw_playlist_move_item(g_playlist, 1, 3, NULL));
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 3, NULL)),
		"Unable to get item moved to position 3");
	fail_if(strcmp(item, "item 2"),
		"Element at position 3 is not the on expected: %s", item);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)),
		"Unable to get item moved to position 1");
	fail_if(strcmp(item, "item 3"),
		"Element at position 1 is not the on expected: %s", item);
	g_free(item);

	/* Move it back again */
	fail_unless(mafw_playlist_move_item(g_playlist, 3, 1, NULL));
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 3, NULL)),
		"Unable to get item moved back to position 3");
	fail_if(strcmp(item, "item 4"),
		"Element at position 3 is not the on expected: %s", item);
	g_free(item);
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 1, NULL)),
		"Unable to get item moved back to position 1");
	fail_if(strcmp(item, "item 2"),
		"Element at position 1 is not the on expected: %s", item);
	g_free(item);

	/* Try to move from/to an invalid index to a valid index */
	fail_if(mafw_playlist_move_item(g_playlist, 50, 0, NULL));
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)),
		"Moving an item from an invalid index into a valid index " \
		"modifies the list (should not)");
	fail_if(strcmp(item, "item 1"),
		"Move from invalid index changes the playlist (should not)");
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != 4,
		"Move from invalid index changes the playlist (should not)");
	g_free(item);

	/* Try to move to an invalid index from a valid index */
	fail_if(mafw_playlist_move_item(g_playlist, 0, 50, NULL));
	fail_if(!(item = mafw_playlist_get_item(g_playlist, 0, NULL)),
		"Moving an item to an invalid index from a valid index " \
		"modified the list (should not)");
	fail_if(strcmp(item, "item 1"),
		"Move to invalid index changes the playlist (should not)");
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != 4,
		"Move to invalid index changes the playlist (should not)");
	g_free(item);

	/* Try to pass in a NULL playlist */
	expect_ignore(mafw_playlist_move_item(NULL, 0, 1, NULL));
}
END_TEST

START_TEST(test_set_get_repeat)
{
	gboolean repeat;

	mafw_playlist_set_repeat(g_playlist, TRUE);
	repeat = mafw_playlist_get_repeat(g_playlist);

	fail_if(repeat != TRUE,
		"Repeat setting is not correct");

}
END_TEST

enum {
	CONTENT_CHD_SIGNAL,
	ITEM_MVD_SIGNAL,
};

typedef struct {
	guint from, nremove, nreplace;
	gint signal_type;
} PlaylistChangedInfo;

static void on_playlist_changed(MafwPlaylist *playlist, guint from,
				guint nremove, guint nreplace, GArray *comm)
{
	PlaylistChangedInfo info = { from, nremove, nreplace,
					CONTENT_CHD_SIGNAL};
	g_array_prepend_val(comm, info);
	if (!dont_quit)
		g_main_loop_quit(Loop);
}

static void on_item_moved(MafwPlaylist *playlist, guint from,
				guint to, GArray *comm)
{
	PlaylistChangedInfo info = { from, to, 0, ITEM_MVD_SIGNAL};

	g_array_prepend_val(comm, info);
	if (!dont_quit)
		g_main_loop_quit(Loop);
}


START_TEST(test_contents_changed_signal)
{
	GArray *comm;
	const PlaylistChangedInfo *current;

	Loop = g_main_loop_new(NULL, TRUE);

	comm = g_array_new(FALSE, FALSE, sizeof(PlaylistChangedInfo));
	g_signal_connect(G_OBJECT(g_playlist), "contents-changed",
			 G_CALLBACK(on_playlist_changed), comm);
	g_signal_connect(G_OBJECT(g_playlist), "item-moved",
			 G_CALLBACK(on_item_moved), comm);

	g_main_loop_run(Loop);
	current = &g_array_index(comm, PlaylistChangedInfo, 0);
	fail_if(current->from != 0 || current->nremove != 0 ||
		current->nreplace != 0 ||
		current->signal_type != CONTENT_CHD_SIGNAL,
		"Signal parameters are incorrect. from:%u,remove:%u,replace:%u",
		current->from, current->nremove, current->nreplace);

	/* Insert some items and check the signal */
	mafw_playlist_insert_item(g_playlist, 0, "item 2", NULL);
	g_main_loop_run(Loop);
	fail_if(comm->len != 2,	"The signal was not emitted after insertion");
	current = &g_array_index(comm, PlaylistChangedInfo, 0);
	fail_if(current->from != 0 || current->nremove != 0 ||
		current->nreplace != 1 ||
		current->signal_type != CONTENT_CHD_SIGNAL,
		"Signal parameters are incorrect. from:%u,remove:%u,replace:%u",
		current->from, current->nremove, current->nreplace);

	mafw_playlist_insert_item(g_playlist, 0, "item 1", NULL);
	g_main_loop_run(Loop);
	fail_if(comm->len != 3, "The signal was not emitted after insertion");
	current = &g_array_index(comm, PlaylistChangedInfo, 0);
	fail_if(current->from != 0 || current->nremove != 0 ||
		current->nreplace != 1 ||
		current->signal_type != CONTENT_CHD_SIGNAL,
		"Signal parameters are incorrect. from:%u,remove:%u,replace:%u",
		current->from, current->nremove, current->nreplace);

	/* Check the movement of one item */
	mafw_playlist_move_item(g_playlist, 0, 1, NULL);
	g_main_loop_run(Loop);
	fail_if(comm->len != 4,	"The signal was not emitted after moving");
	current = &g_array_index(comm, PlaylistChangedInfo, 0);
	fail_if(current->from != 0 || current->nremove != 1 ||
		current->nreplace != 0 ||
		current->signal_type != ITEM_MVD_SIGNAL,
		"Signal parameters are incorrect. from:%u,remove:%u,replace:%u "
		"type: %d",
		current->from, current->nremove, current->nreplace,
		current->signal_type);

	/* Check the signal when removing an index */
	mafw_playlist_remove_item(g_playlist, 1, NULL);
	g_main_loop_run(Loop);
	fail_if(comm->len != 5,	"The signal was not emitted after removal");
	current = &g_array_index(comm, PlaylistChangedInfo, 0);
	fail_if(current->from != 1 || current->nremove != 1 ||
		current->nreplace != 0 ||
		current->signal_type != CONTENT_CHD_SIGNAL,
		"Signal parameters are incorrect. from:%u,remove:%u,replace:%u",
		current->from, current->nremove, current->nreplace);

	/* Check the signal when clearing the list */
	mafw_playlist_clear(g_playlist, NULL);
	g_main_loop_run(Loop);
	fail_if(comm->len != 6,	"The signal was not emitted after clearing");
	current = &g_array_index(comm, PlaylistChangedInfo, 0);
	fail_if(current->from != 0 || current->nremove != 1 ||
		current->nreplace != 0 ||
		current->signal_type != CONTENT_CHD_SIGNAL,
		"Signal parameters are incorrect. from:%u,remove:%u,replace:%u",
		current->from, current->nremove, current->nreplace);

	/* Disconnect now because the tear down fixture *_clear()s the list. */
	g_signal_handlers_disconnect_by_func(G_OBJECT(g_playlist),
					     G_CALLBACK(on_playlist_changed),
					     comm);
	g_array_free(comm, TRUE);
}
END_TEST

static void on_renamed(MafwPlaylist *self, GParamSpec *pspec, guint *called)
{
	if (!strcmp(pspec->name, "name"))
		called[0]++;
}

static void on_repeated(MafwPlaylist *self, GParamSpec *pspec, guint *called)
{
	if (!strcmp(pspec->name, "repeat"))
		called[0]++;
}

START_TEST(test_property_changed_signal)
{
	guint nrenames, nrepeats;
	gchar *orig_name, *new_name;

	nrenames = nrepeats = 0;
	g_signal_connect(G_OBJECT(g_playlist), "notify",
			 G_CALLBACK(on_renamed),  &nrenames);
	g_signal_connect(G_OBJECT(g_playlist), "notify",
			 G_CALLBACK(on_repeated), &nrepeats);

	/* Save the current name of the playlist so we can restore it. */
	orig_name = mafw_playlist_get_name(MAFW_PLAYLIST(g_playlist));
	new_name  = g_strdup_printf("%.8X", g_random_int());

	mafw_playlist_set_name(MAFW_PLAYLIST(g_playlist), new_name);
	fail_unless(nrenames == 1 && nrepeats == 0);

	/* Restore $name (though the end-test fixture should be able
	 * to destroy it anyway). */
	mafw_playlist_set_name(MAFW_PLAYLIST(g_playlist), orig_name);
	fail_unless(nrenames == 2 && nrepeats == 0);

	/* The signal is expected to be emitted even if the property,
	 * in efffect, didn't change. */
	mafw_playlist_set_repeat(MAFW_PLAYLIST(g_playlist), TRUE);
	fail_unless(nrenames == 2 && nrepeats == 1);
	mafw_playlist_set_repeat(MAFW_PLAYLIST(g_playlist), TRUE);
	fail_unless(nrenames == 2 && nrepeats == 2);
	mafw_playlist_set_repeat(MAFW_PLAYLIST(g_playlist), FALSE);
	fail_unless(nrenames == 2 && nrepeats == 3);

	g_free(orig_name);
	g_free(new_name);

}
END_TEST

static void on_shuffled(MafwPlaylist *self, GParamSpec *pspec, guint *called)
{
	if (!strcmp(pspec->name, "is-shuffled"))
		called[0]++;
	if (!dont_quit)
		g_main_loop_quit(Loop);
}

static void test_shuffle_stress(guint nturns, guint nitems)
{
	guint i, not_shuffled, next_id;
	GHashTable *before, *after;
	gchar *item;


	/*
	 * Strategy: fill the playlist then permute it a number of times.
	 * In each iteration check that all the playing indices have
	 * changed.  For this store the latest known indices in $before.
	 * ($after will contain the indices after the operation.)
	 */
	before = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	after  = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* Fill the playlist and prepare $before.  In the beginning
	 * the visual and playling indices match.  Note that "item 0"
	 * has been added by test_shuffle(). */
	g_hash_table_insert(before, mafw_playlist_get_item(g_playlist, 0, NULL),
			    GINT_TO_POINTER(nitems-1));
	for (i = 1; i < nitems; i++) {
		gchar *item;

		item = g_strdup_printf("item %u", i);
		mafw_playlist_insert_item(g_playlist, 0, item, NULL);
		g_hash_table_insert(before, item, GINT_TO_POINTER(nitems-i-1));
	}

	fail_if(g_hash_table_size(before) != nitems);
	fail_if(mafw_playlist_get_size(g_playlist, NULL) != nitems);

	/* $g_playlist is still not shuffled. */
	fail_if(mafw_playlist_is_shuffled(g_playlist));

	not_shuffled = 0;
	for (i = 0; i < nturns; i++) {
		guint newpos;
		GHashTable *tmp;

		fail_unless(mafw_playlist_shuffle(g_playlist, NULL));
		/*
		 * After a shuffle() one would expect the list is_shuffled().
		 * Thus said, but it may happen the operation just happened to
		 * permute all the playing indices back to the visual index.
		 * However, this condition should be extremely rare -- no more
		 * than once in our lifetime with a sufficiently large list.
		 */
		if (!mafw_playlist_is_shuffled(g_playlist)) {
			not_shuffled++;
			fail_if(not_shuffled > 1);
		}

		/* Check that exactly the same items are in the playlist */
		fail_if(mafw_playlist_get_size(g_playlist, NULL) != nitems);
		mafw_playlist_get_starting_index(g_playlist, &next_id, &item,
                                                 NULL);
		newpos = 0;
		do
		{
			gpointer oldpos;

			fail_if(item == NULL);

			fail_if(!g_hash_table_lookup_extended(before, item,
							      NULL, &oldpos));

			g_hash_table_insert(after, item,
					    GINT_TO_POINTER(newpos));
			g_hash_table_remove(before, item);
		newpos++;
		}
		while (mafw_playlist_get_next(g_playlist, &next_id, &item,
							NULL));

		/* Switch $before and $after, as the latter array
		 * contains the up-to-date information now. */
		tmp	= before;
		before	= after;
		after	= tmp;
	}

	/* We've survived all the tortures.  Clean up. */
	g_hash_table_unref(before);
	g_hash_table_unref(after);
}

/* Test MafwPlaylist::shuffle(), is_shuffled() and unshuffle(). */
START_TEST(test_shuffle)
{
	guint nshuffles;
	GArray *contents_changed;
	guint nchanges_expected, nshuffles_expected;

	Loop = g_main_loop_new(NULL, TRUE);
	
	/*
	 * Check the receipt of property_changed(is-shuffled)
	 * and the non-recepipt of playlist_changed at once.
	 * The same communication pattern is utilized as with
	 * test_contents_changed_signal().
	 */
	nshuffles = 0;
	contents_changed = g_array_new(FALSE, FALSE,
				       sizeof(PlaylistChangedInfo));
	nchanges_expected = nshuffles_expected = 0;
	g_signal_connect(G_OBJECT(g_playlist), "contents-changed",
			 G_CALLBACK(on_playlist_changed), contents_changed);
	g_signal_connect(G_OBJECT(g_playlist), "notify",
			 G_CALLBACK(on_shuffled), &nshuffles);

	/* First try to shuffle an empty list.
	 * (Presumably it's empty due to the setup fixture.) */
	fail_if(mafw_playlist_is_shuffled(g_playlist));
	g_main_loop_run(Loop);
	fail_unless(mafw_playlist_shuffle(g_playlist, NULL));
	g_timeout_add(100,(GSourceFunc)g_main_loop_quit,Loop);
	g_main_loop_run(Loop);
	fail_if(contents_changed->len>1 || nshuffles != 1);
	fail_if(!mafw_playlist_is_shuffled(g_playlist));
	fail_unless(mafw_playlist_unshuffle(g_playlist, NULL));
	g_main_loop_run(Loop);

	/* Try with a single-item playlist.  Expect a playlist_changed
	 * (of insert_item()), and a is-shuffled (of shuffle()). */
	mafw_playlist_insert_item(g_playlist, 0, "item 0", NULL);
	fail_if(mafw_playlist_is_shuffled(g_playlist));
	fail_unless(mafw_playlist_shuffle(g_playlist, NULL));
	fail_unless(mafw_playlist_unshuffle(g_playlist, NULL));
	g_main_loop_run(Loop);
	g_main_loop_run(Loop);
	g_main_loop_run(Loop);
	fail_if(contents_changed->len != 2 || nshuffles != 4);
	fail_if(mafw_playlist_is_shuffled(g_playlist));
	nchanges_expected = nshuffles_expected = 4;

	/* Run the meat of the test. */
	test_shuffle_stress(25, 100);
	nchanges_expected   = 100;
	nshuffles_expected += 25;

	/* test_shuffle_stress() tried shuffle() to death,
	 * so see that unshuffle() restores playing indices. */
	fail_unless(mafw_playlist_unshuffle(g_playlist, NULL));
	g_main_loop_run(Loop);
	nshuffles_expected++;
	fail_if(mafw_playlist_is_shuffled(g_playlist));
	/* Finally collect, and check the number of signals received. */
	g_timeout_add_seconds(5,(GSourceFunc)g_main_loop_quit,Loop);
	dont_quit = TRUE;
	g_main_loop_run(Loop);
	fail_if(contents_changed->len -1!= nchanges_expected,"%d %d",
                contents_changed->len,nchanges_expected);

	fail_if(nshuffles != nshuffles_expected);

	/* The same as in test_contents_changed_signal(). */
	g_signal_handlers_disconnect_by_func(G_OBJECT(g_playlist),
					     G_CALLBACK(on_playlist_changed),
					     contents_changed);
	g_array_free(contents_changed, TRUE);
}
END_TEST

START_TEST(test_proxy_playlist_bind_lists)
{
	MafwProxyPlaylist* another = NULL;
	gchar* item = NULL;
	gchar* returned_item = NULL;
	guint id1, id2;

	/* Insert a test item with the global playlist object */
	item = g_strdup_printf("%s %u",
			       G_GNUC_FUNCTION,
			       g_random_int());
	mafw_playlist_insert_item(g_playlist, 0, item, NULL);

	/* Bind another playlist object to our global playlist */
	another = mafw_playlist_manager_get_playlist(
		mafw_playlist_manager_get(),
		mafw_proxy_playlist_get_id(
				      MAFW_PROXY_PLAYLIST(g_playlist)), NULL);

	/* Check whether the playlist was created successfully */
	fail_unless(MAFW_IS_PROXY_PLAYLIST(another),
		    "List creation failed.");

	/* Check that the lists are bound to the same playlist ID */
	id1 = mafw_proxy_playlist_get_id(
					 MAFW_PROXY_PLAYLIST(another));
	id2 = mafw_proxy_playlist_get_id(
				      MAFW_PROXY_PLAYLIST(g_playlist));
	fail_unless(id1 == id2,
		    "Unable to bind two playlist objects to the same playlist");

	/* Check that the item returned thru "the other" playlist object is
	   the same that was inserted thru the global playlist object */
	returned_item = mafw_playlist_get_item(MAFW_PLAYLIST(another), 0, NULL);
	fail_if(returned_item == NULL,
		"Unable to get an inserted item thru another playlist object");
	fail_if(strcmp(returned_item, item),
		"Got the wrong item thru another playlist object");
	g_free(returned_item);
	g_free(item);

	g_object_unref(another);
}
END_TEST


/* mafw_playlist_get_items_md() tests. */

#define URI(oid) (&oid[sizeof(MAFW_URI_SOURCE_UUID "::") - 1])

/* URI source hook. */
static void (*Old_md)(MafwSource *self,
			  const gchar *object_id,
			  const gchar *const *mdkeys,
			  MafwSourceMetadataResultCb cb,
			  gpointer user_data);
static guint Old_md_called;

static void mymd(MafwSource *self,
		     const gchar *object_id,
		     const gchar *const *mdkeys,
		     MafwSourceMetadataResultCb cb,
		     gpointer user_data)
{
	Old_md_called++;
	Old_md(self, object_id, mdkeys, cb, user_data);
}

/* The MIWMD request. */
static gpointer Req;
static guint Itemcb_called;
static gboolean Destructed;

static void destruct(gpointer called)
{
	Destructed = TRUE;
	checkmore_stop_loop();
}

static void itemcb_valid(MafwPlaylist *pls,
			 guint idx,
			 const gchar *oid,
			 GHashTable *md,
			 gpointer _)
{
	GValue *vuri;

	/* We've requested the URI in this case. */
	Itemcb_called++;
	fail_unless(md != NULL);
	vuri = mafw_metadata_first(md, MAFW_METADATA_KEY_URI);
	fail_unless(vuri != NULL);
	fail_if(strcmp(g_value_get_string(vuri), URI(Contents[idx])));
}

START_TEST(test_valid)
{
	/* Ordinary call, [0..2], valid metadata key. */
	Req = mafw_playlist_get_items_md(g_playlist, 0, 2,
					 MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
					 itemcb_valid, &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Itemcb_called == 3);
	fail_unless(Old_md_called == 3);
	fail_unless(Destructed);
	/* [0..0], valid metadata key. */
	Old_md_called = 0;
	Itemcb_called = 0;
	Destructed = FALSE;
	Req = mafw_playlist_get_items_md(g_playlist, 0, 0,
					 MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
					 itemcb_valid, &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Itemcb_called == 1);
	fail_unless(Old_md_called == 1);
	fail_unless(Destructed);
	/* [0..inf), valid metadata key. */
	Old_md_called = 0;
	Itemcb_called = 0;
	Destructed = FALSE;
	Req = mafw_playlist_get_items_md(g_playlist, 0, -1,
					 MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
					 itemcb_valid, &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Itemcb_called == PLS_SIZE);
	fail_unless(Old_md_called == PLS_SIZE);
	fail_unless(Destructed);
	/* [2..inf), valid metadata key. */
	Old_md_called = 0;
	Itemcb_called = 0;
	Destructed = FALSE;
	Req = mafw_playlist_get_items_md(g_playlist, 2, -1,
					 MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
					 itemcb_valid, &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Itemcb_called == PLS_SIZE - 2);
	fail_unless(Old_md_called == PLS_SIZE - 2);
	fail_unless(Destructed);
}
END_TEST

static void itemcb_nomd(MafwPlaylist *pls,
			   guint idx,
			   const gchar *oid,
			   GHashTable *md,
			   gpointer testcase)
{
	/* In this case we have requested NO metadata */
	Itemcb_called++;
	fail_unless(md == NULL);
}


START_TEST(test_valid_empty)
{
	mafw_playlist_clear(g_playlist, NULL);
	/* [0..-1], no keys. */
	Req = mafw_playlist_get_items_md(g_playlist, 0, -1, MAFW_SOURCE_NO_KEYS,
					  itemcb_nomd, &Destructed, destruct);
	checkmore_spin_loop(500);
	fail_unless(Itemcb_called == 0);
	fail_unless(Old_md_called == 0);
	fail_unless(Destructed);
}
END_TEST



START_TEST(test_invalid)
{
	/* Invalid range of items [0..999]. */
	Req = mafw_playlist_get_items_md(g_playlist, 0, 999, NULL, itemcb_nomd,
					 &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Old_md_called == 0);
	fail_unless(Itemcb_called == PLS_SIZE);
	fail_unless(Destructed);
}
END_TEST

START_TEST(test_invalid_2)
{
	/* Invalid range of items [10..20]. */
	Req = mafw_playlist_get_items_md(g_playlist, 10, 20, NULL, itemcb_nomd,
					 &Destructed, destruct);
	checkmore_spin_loop(500);
	fail_unless(Old_md_called == 0);
	fail_unless(Itemcb_called == 0);
}
END_TEST

START_TEST(test_no_md)
{
	/* Request no metadata. */
	Req = mafw_playlist_get_items_md(g_playlist, 0, 2, NULL, itemcb_nomd,
					 &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Itemcb_called == 3);
	fail_unless(Old_md_called == 0);
	fail_unless(Destructed);
}
END_TEST

static void itemcb_cancel(MafwPlaylist *pls,
			  guint idx,
			  const gchar *oid,
			  GHashTable *md,
			  gpointer _)
{
	Itemcb_called++;
	if (idx == 0)
		mafw_playlist_cancel_get_items_md(Req);
	/* It's a failure if we get called after canceling. */
	fail_if(idx > 0);
}

START_TEST(test_cancel_1)
{
	/* Cancelling after the first item. */
	Req = mafw_playlist_get_items_md(g_playlist, 0, 2, NULL, itemcb_cancel,
					 &Destructed, destruct);
	checkmore_spin_loop(-1);
	fail_unless(Destructed);
	fail_unless(Old_md_called == 0);
}
END_TEST

START_TEST(test_cancel_2)
{
	/* Cancelling before even starting the mainloop. */
	Req = mafw_playlist_get_items_md(g_playlist, 0, 2, NULL, itemcb_nomd,
					 &Destructed, destruct);
	mafw_playlist_cancel_get_items_md(Req);
	checkmore_spin_loop(-1);
	fail_unless(Destructed);
	fail_unless(Itemcb_called == 0);
	fail_unless(Old_md_called == 0);
}
END_TEST

static void multi_dest(gpointer called)
{
	if (++*(guint *)called == 2)
		checkmore_stop_loop();
}

START_TEST(test_multi_1)
{
	guint n_dest;

	/* Multiple requests. */
	n_dest = 0;
	mafw_playlist_get_items_md(g_playlist, 0, 1,
				   MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
				   itemcb_valid, &n_dest, multi_dest);
	mafw_playlist_get_items_md(g_playlist, 4, 7,
				   MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
				   itemcb_valid, &n_dest, multi_dest);
	checkmore_spin_loop(-1);
	fail_unless(n_dest == 2);
	fail_unless(Itemcb_called == 6);
	fail_unless(Old_md_called == 6);
}
END_TEST

static void itemcb_cancel_multi(MafwPlaylist *pls,
				guint idx,
				const gchar *oid,
				GHashTable *md,
				gpointer _)
{
	/* Cancel ourselves after the second item. */
	Itemcb_called++;
	if (idx == 1)
		mafw_playlist_cancel_get_items_md(Req);
	fail_if(idx > 1);
}

START_TEST(test_multi_2)
{
	guint n_dest;

	/* Canceling one of multiple requests. */
	n_dest = 0;
	Req = mafw_playlist_get_items_md(g_playlist, 0, 5, NULL,
					 itemcb_cancel_multi,
					 &n_dest, multi_dest);
	mafw_playlist_get_items_md(g_playlist, 0, 5,
				   MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
				   itemcb_valid, &n_dest, multi_dest);
	checkmore_spin_loop(-1);
	fail_unless(n_dest == 2);
	fail_unless(Itemcb_called == 2 + 6);
	fail_unless(Old_md_called == 0 + 6);
}
END_TEST


int main(void)
{
	TCase *tc;
	Suite *suite;

	g_type_init();
	/* Hook into the URI source's get_metadata(). */
	Old_md = MAFW_SOURCE_GET_CLASS(mafw_get_uri_source())->get_metadata;
	MAFW_SOURCE_GET_CLASS(mafw_get_uri_source())->get_metadata = mymd;

	checkmore_wants_dbus();
	g_setenv("MAFW_PLAYLIST_DIR", "testproxyplaylist", TRUE);

	suite = suite_create("MafwProxyPlaylist");

	tc = tcase_create("ProxyPlaylist");
	tcase_add_checked_fixture(tc, fx_shared_setup, fx_shared_teardown);
	tcase_add_unchecked_fixture(tc, fx_start_daemon, checkmore_stop);
	tcase_add_test(tc, test_insert_and_get_item);
	tcase_add_test(tc, test_append);
	tcase_add_test(tc, test_clear);
	tcase_add_test(tc, test_set_get_name);
	tcase_add_test(tc, test_get_size);
	tcase_add_test(tc, test_remove_items);
	tcase_add_test(tc, test_move_item);
	tcase_add_test(tc, test_set_get_repeat);
	tcase_add_test(tc, test_contents_changed_signal);
	tcase_add_test(tc, test_property_changed_signal);
	tcase_add_test(tc, test_shuffle);
	tcase_add_test(tc, test_iterator);
	tcase_add_test(tc, test_proxy_playlist_bind_lists);
	tcase_set_timeout(tc, 0);
	suite_add_tcase(suite, tc);

	tc = tcase_create("ProxyPlaylist-MIWMD");
	tcase_add_checked_fixture(tc, fx_miwmd_setup, fx_shared_teardown);
	tcase_add_unchecked_fixture(tc, fx_start_daemon, checkmore_stop);
	tcase_add_test(tc, test_valid);
	tcase_add_test(tc, test_valid_empty);
	tcase_add_test(tc, test_invalid);
	tcase_add_test(tc, test_invalid_2);
	tcase_add_test(tc, test_no_md);
	tcase_add_test(tc, test_cancel_1);
	tcase_add_test(tc, test_cancel_2);
	tcase_add_test(tc, test_multi_1);
	tcase_add_test(tc, test_multi_2);
	tcase_set_timeout(tc, 0);
	suite_add_tcase(suite, tc);

	return checkmore_run(srunner_create(suite),FALSE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
