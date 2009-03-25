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
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <checkmore.h>
#include "libmafw-shared/mafw-playlist-manager.h"

/* Definitions */
/* Playlists will be stored in PLS_DIR. */
#define PLS_DIR			"testpld"
/* Path to the playlist daemon. */
#define MAFW_PLAYLIST_DAEMON	"../mafw-playlist-daemon/mafw-playlist-daemon"

/* Start the playlist daemon. */
static void start_daemon(void)
{
	/* The playlist daemon exit()s with 11 if the service
	 * is being provided by another instance of the daemon. */
	checkmore_start(MAFW_PLAYLIST_DAEMON, 11, NULL);
	/* Give dbusd enough time to notice the daemon. */
	g_usleep(500000);
}

static void assert_item(gpointer pls, guint idx, const gchar *expected)
{
	gchar *oid;

	oid = mafw_playlist_get_item(pls, idx, NULL);
	fail_if(oid == NULL);
	fail_if(strcmp(oid, expected));
	g_free(oid);
}

static gboolean destr_failed_called;
static void pls_destr_failed(MafwPlaylistManager *playlist,
				GObject *arg1, gpointer user_data)
{
	destr_failed_called = TRUE;
	checkmore_stop_loop();
}

static gboolean pl_dest_called;
static void pls_destroyed(MafwPlaylistManager *plm,
			  MafwPlaylist *pls, gpointer ourpls)
{
	/* Assure that the correct playlist was destroyed. */
	fail_unless(pls == ourpls);
	pl_dest_called = TRUE;
}

START_TEST(test_basic_persistence)
{
	gpointer plm, pls, pls2;

	/* Start MPD, create a playlist, stop, restart and see that it's saved. */
	system("test -d '" PLS_DIR "' && rm -rf '" PLS_DIR "'");
	start_daemon();
	plm = mafw_playlist_manager_get();
	fail_unless(plm != NULL);
	pls = mafw_playlist_manager_create_playlist(plm, "lofasz", NULL);
	fail_unless(pls != NULL);
	g_signal_connect(plm, "playlist-destruction-failed",
			 G_CALLBACK(pls_destr_failed), pls);
	g_signal_connect(plm, "playlist-destroyed",
			 G_CALLBACK(pls_destroyed), pls);
	mafw_playlist_insert_item(pls, 0, "alfa", NULL);
	mafw_playlist_insert_item(pls, 1, "bravo", NULL);
	mafw_playlist_insert_item(pls, 2, "charlie", NULL);
	mafw_playlist_insert_item(pls, 3, "delta", NULL);
	mafw_playlist_insert_item(pls, 4, "echo", NULL);
	checkmore_stop();
	

	start_daemon();
	pls2 = mafw_playlist_manager_create_playlist(plm, "lofasz", NULL);
	fail_unless(mafw_playlist_get_size(pls2, NULL) == 5);
	fail_unless(mafw_playlist_get_repeat(pls2) == FALSE);
	assert_item(pls2, 0, "alfa");
	assert_item(pls2, 1, "bravo");
	assert_item(pls2, 2, "charlie");
	assert_item(pls2, 3, "delta");
	assert_item(pls2, 4, "echo");
	mafw_playlist_increment_use_count(pls2, NULL);
	g_object_ref(pls2);
	mafw_playlist_manager_destroy_playlist(plm, pls2, NULL);
	g_object_ref(pls2);
	checkmore_spin_loop(500);
	fail_if(!destr_failed_called);
	mafw_playlist_decrement_use_count(pls2, NULL);
	mafw_playlist_manager_destroy_playlist(plm, pls2, NULL);
	checkmore_spin_loop(500);
	fail_if(!pl_dest_called);
	pl_dest_called = FALSE;
	checkmore_stop();
}
END_TEST

START_TEST(test_diskfull)
{
	gpointer plm, pls;
	guint id;
	mode_t oldmask;
	
	pl_dest_called = FALSE;

	system("test -d '" PLS_DIR "' && rm -rf '" PLS_DIR "'");
	/* Full disk is simulated by a read-only playlist directory.
	 * This may break under fakeroot (as everyhing is writable for root), 
	 * but why would one run tests under fakeroot? */
	oldmask = umask(0222);
	mkdir(PLS_DIR, 0555);
	/* Umask must be reset, otherwise other files (.gcda for example) will
	 * be also read-only, which messes up the rest of the tests when
	 * profiling. */
	umask(oldmask);
	start_daemon();
	plm = mafw_playlist_manager_get();
	fail_unless(plm != NULL);
	pls = mafw_playlist_manager_create_playlist(plm, "lofasz", NULL);
	fail_unless(pls != NULL);
	id = mafw_proxy_playlist_get_id(pls);
	mafw_playlist_insert_item(pls, 0, "alfa", NULL);
	mafw_playlist_insert_item(pls, 1, "bravo", NULL);
	mafw_playlist_insert_item(pls, 2, "charlie", NULL);
	mafw_playlist_insert_item(pls, 3, "delta", NULL);
	mafw_playlist_insert_item(pls, 4, "echo", NULL);
	/* Wait till mpd has a chance to save. */
	g_usleep((3+2) * G_USEC_PER_SEC);
	checkmore_stop();
	/* Let the messages drain. */
	checkmore_spin_loop(100);

	g_signal_connect(plm, "playlist-destroyed",
			 G_CALLBACK(pls_destroyed), pls);
	start_daemon();
	checkmore_spin_loop(100);
	/* We expect the playlist to be nonexistent.  And also that a
	 * playlist-destroyed signal is emitted. */
	pls = mafw_playlist_manager_get_playlist(plm, id, NULL);
	fail_unless(pls == NULL);
	checkmore_stop();
	fail_if(!pl_dest_called);
}
END_TEST


int main(void)
{
	TCase *tc;
	Suite *suite;

	checkmore_wants_dbus();
	/* Put playlists in a separate. */
	g_setenv("MAFW_PLAYLIST_DIR", PLS_DIR, TRUE);

	suite = suite_create("Playlist daemon");

	tc = tcase_create("Playlist persistence");
	tcase_set_timeout(tc, 0);
if (1)	tcase_add_test(tc, test_basic_persistence);
if (1)	tcase_add_test(tc, test_diskfull);
	suite_add_tcase(suite, tc);

	return checkmore_run(srunner_create(suite), FALSE);
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0 foldmethod=marker: */
