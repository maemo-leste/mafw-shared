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
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/file.h>

#include <check.h>
#include <glib.h>

#include "libmafw-shared/mafw-playlist-manager.h"

#include <checkmore.h>

/* Standard definitions */
/* Path to the playlist daemon. */
#define MAFW_PLAYLIST_DAEMON	"../mafw-playlist-daemon/mafw-playlist-daemon"

/* Configuration {{{ */
#if 0
# define info			g_debug
# define LOG_CREATOR		"creator.log"
# define LOG_DESTROYER		"destroyer.log"
#else
# define info(...)		/* */
# define LOG_CREATOR		NULL
# define LOG_DESTROYER		NULL
#endif
/* }}} */

/* Macros {{{ */
/*
 * Override the function of `check' so at most one of the processes can
 * execute it.  This is necessary because _fail_unless() communicates
 * with the grandparent test runner through a file descriptor and if
 * more than one process writes something to it messages may be confused,
 * which `chech' reports as a fatal error.  Under stressing conditions
 * it is easy to hit this.
 */
#define _fail_unless(...)		\
do {					\
	lock(LOCK_EX);			\
	_fail_unless(__VA_ARGS__);	\
	lock(LOCK_UN);			\
} while (0)

/* Kept from the old test-playlist-manager.c just in case. */
#define flush_dbus()		/* NOP */
/* }}} */

/* Type definitions {{{ */
/*
 * This structure describes one round of the stress test.
 * @name:	the name of the playlist to be created
 * @ncreated:	how many times a playlist with @name has been created
 * @ndestroyed:	s/created/destroyed/
 *
 * In a successful test all playlists specified by the table must
 * have been created, and must have been destroyed the same times.
 * Playlists with the same name may be created multiple times.
 */
struct table_st {
	gchar *name;
	guint ncreated, ndestroyed;
	struct table_st *next;
};
/* }}} */

/* Private variables {{{ */
/*
 * Variables used by test_like_a_little_angel():
 * $Destroyer_died: set when the destroyer exits
 * $NRounds:	Configuration parameter; how many playlists to create...
 * $NCompleted:	...and how many of them have been created.  if reaches
 *		$NRounds the test is completed.
 * $Names:	Playlist name => table_st associations containing all the
 *		playlist names to be created by the creator.  Used in signal
 *		handlers to validate that a given playlist may have been
 *		created by the creator.
 * $Ids:	Playlist ID => table_st associations of playlists created
 *		by the creator and currently existing.  The destroyer picks
 *		the next playlist to destroy from this hash.
 * $Objects:	Set of known playlist objects used to ensure a playlist ID
 *		 corresponds to exactly one playlist object.
 */
static GMainLoop *Loop;
static gint Destroyer_died = -1;
static guint NRounds, NCompleted;
static GTree *Names, *Ids, *Objects;
/* }}} */

/* Program code */
/* Utilities {{{ */
/* Callback of g_tree_search() used to pick a random item from the tree
 * returning which direction to progresss the traversal.  $h tell how
 * far we are from the bottom of the tree, 1 designating a leaf. */
static gint choose_direction(gpointer unused, guint *h)
{
	/* From the bottom we cannot go anywhere.
	 * Otherwise return a random direction. */
	return (*h)-- > 1 ? g_random_int_range(-1, 2) : 0;
}

/* Returns a random value from $tree, or %NULL if the tree is empty.
 * The distribution of the returned values is not uniform. */
static gpointer rnd_tree_value(GTree *tree)
{
	guint h;
	gpointer val;

	/* Empty tree? */
	if (!(h = g_tree_height(tree)))
		return NULL;

	/* Traverse $tree in a random path.  May need to be repeated
	 * because choose_direction() can choose a direction leading
	 * nowhere (eg. in a 2-element tree). */
	do
		h = g_random_int_range(1, h+1);
	while (!(val = g_tree_search(tree, (GCompareFunc)choose_direction,
				     &h)));

	return val;
}

/* #GCompareDataFunc comparing pointer addresses. */
static gint cmpptrs(gconstpointer lhs, gconstpointer rhs, gpointer unused)
{
	if (lhs < rhs)
		return -1;
	else if (lhs > rhs)
		return  1;
	else
		return  0;
}

/* Locks or unlocks a file.  $op is the operation clode for flock().
 * Used from multiple processes to gain exclusive access to resources
 * in critical sections. */
static void lock(int op)
{
	static int hlock = -1;

	/*
	 * It is important not to open the file until the parent
	 * has fork()ed, because otherwise Linux will regard the
	 * file descriptors in the parent and the child equivalent,
	 * making LOCK_EX ineffective.
	 */
	if (hlock < 0)
		hlock = open("/dev/null", O_RDWR);
	flock(hlock, op);
}
/* }}} */

/* Fixtures {{{ */
/* Start the playlist daemon. */
static void start_daemon(void)
{
	/* The playlist daemon exit()s with 11 if the service
	 * is being provided by another instance of the daemon. */
	checkmore_start(MAFW_PLAYLIST_DAEMON, 11, NULL);

	/* Give dbusd enough time to notice the daemon. */
	g_usleep(200000);
}
/* }}} */

/* Tests */
/* Test mafw_playlist_manager_get(). {{{ */
START_TEST(test_get_manager)
{
	MafwPlaylistManager *manager;

	/* Check that mafw_playlist_manager_get()
	 * always returns the same object. */
	manager = mafw_playlist_manager_get();
	fail_if(manager == NULL,
		"Didn't get the playlist manager");
	fail_if(manager != mafw_playlist_manager_get(),
		"Getting differing playlist manager instances");
	fail_if(G_OBJECT(manager)->ref_count != 1,
		"Manager should not be referred by anyone");

	/* See if the destruction brings down the test. */
	checkmore_ignore("PlaylistManager is shutting down");
	g_object_unref(manager);
}
END_TEST /* }}} */

/* Test *_list_playlists(), *_get_playlist() and *_get_playlists(). {{{ */
START_TEST(test_get_playlists)
{
	guint i;
	gchar *name;
	GError *err;
	guint id;
	GTree *all;
	GArray *list;
	GPtrArray *mine, *plst;
	MafwPlaylistManager *manager;
	MafwPlaylistManagerItem *item;

	/*
	 * Strategy: create a bunch of playlists and try to retrieve them
	 * by multiple means with cold cache.
	 *
	 * $all stores ID => name associations of all shared playlists,
	 * whether created by us or not.  $mine contains pointers to
	 * the playlists we created.
	 */
	err	= NULL;
	manager = mafw_playlist_manager_get();
	all	= g_tree_new_full((GCompareDataFunc)cmpptrs,
				  NULL, NULL, g_free);
	mine	= g_ptr_array_new();

	/* $all <- all already existing playlists.
	 * Don't use mafw_playlist_manager_free_list_of_playlists()
	 * because we need to keep ->name. */
	fail_if(!(list = mafw_playlist_manager_list_playlists(manager,
								     &err)),
		err ? err->message : NULL);
	for (item = (MafwPlaylistManagerItem *)list->data;
	     item->name; item++)
		g_tree_insert(all, GUINT_TO_POINTER(item->id), item->name);
	g_array_free(list, TRUE);

	/* Add lots of playlists with random names. */
	for (i = 0; i < 100; i++) {
		MafwProxyPlaylist *playlist;

		name = g_strdup_printf("%.8X", g_random_int());
		playlist = mafw_playlist_manager_create_playlist(manager,
								  name,
								  NULL);
		fail_if(!playlist);
		id = mafw_proxy_playlist_get_id(playlist);

		g_ptr_array_add(mine, playlist);
		g_tree_insert(all, GUINT_TO_POINTER(id), name);
	}

	/* Kill $manager to start another one with clear store to force
	 * it asking the daemon. */
	checkmore_ignore("PlaylistManager is shutting down");
	g_object_unref(manager);
	manager = mafw_playlist_manager_get();

	/* Verify that mafw_playlist_manager_list_playlists()
	 * returns information consistent with $all. */
	list = mafw_playlist_manager_list_playlists(manager, NULL);
	for (item = (MafwPlaylistManagerItem *)list->data;
	     item->name; item++) {
		fail_if(!(name = g_tree_lookup(all, GUINT_TO_POINTER(item->id))));
		fail_unless(!strcmp(item->name, name));
	}
	mafw_playlist_manager_free_list_of_playlists(list);

	/* Do the same to mafw_chared_playlist_manager_get_playlists(). */
	plst = mafw_playlist_manager_get_playlists(manager, NULL);
	for (i = 0; i < plst->len; i++) {
		id = mafw_proxy_playlist_get_id(plst->pdata[i]);
		fail_if(!g_tree_lookup(all, GUINT_TO_POINTER(id)));
	}
	g_ptr_array_free(plst, TRUE);

	/* Destroy the playlists we created. */
	for (i = 0; i < mine->len; i++) {
		id = mafw_proxy_playlist_get_id(mine->pdata[i]);
		g_tree_remove(all, GUINT_TO_POINTER(id));
		mafw_playlist_manager_destroy_playlist(manager,
							mine->pdata[i],
							NULL);
	}

	/* Get the list of available playlists and verify there are
	 * exactly as many as it was before we started testing. */
	list = mafw_playlist_manager_list_playlists(manager, NULL);
	fail_if(list->len != g_tree_nnodes(all));
	mafw_playlist_manager_free_list_of_playlists(list);

	/* Clean up */
	g_tree_destroy(all);
	g_ptr_array_free(mine, TRUE);
}
END_TEST /* }}} */

/* Test *_dup_playlists(), *_get_playlists(). {{{ */
START_TEST(test_dup_playlists)
{
        gchar *name, *plst_name, *new_name;
        GPtrArray *plst;
        MafwPlaylistManager *manager;
        MafwProxyPlaylist *playlist, *new_playlist;
        gboolean found = FALSE;
        int j = 0;

        /*
         * Strategy: create a playlist, dup the playlist.
         * Verify the same by getting the playlists. then destroy the created/duped playlist.
         *
         */
        manager = mafw_playlist_manager_get();


        /*create playlist with random name*/
        name = g_strdup_printf("%.8X", g_random_int());
        playlist = mafw_playlist_manager_create_playlist(manager, name, NULL);
        fail_if(!playlist);

        /*set some random properties of playlist*/
         mafw_playlist_set_repeat(MAFW_PLAYLIST(playlist),TRUE);
         mafw_playlist_is_shuffled(MAFW_PLAYLIST(playlist));
         mafw_playlist_insert_item(MAFW_PLAYLIST(playlist), 0, "t item 1", NULL);
         mafw_playlist_insert_item(MAFW_PLAYLIST(playlist), 1, "t item 2", NULL);

        /*Dup playlist with a diff name*/
         new_name = g_strdup_printf("%s_dup", name);
         fail_if((new_playlist = mafw_playlist_manager_dup_playlist(manager, playlist,
	 							new_name,
                                                                NULL)) == NULL);
	plst_name = mafw_playlist_get_name(MAFW_PLAYLIST(new_playlist));
	fail_if(strcmp(plst_name, new_name) != 0);
	g_free(plst_name);
	fail_if(mafw_proxy_playlist_get_id(playlist) ==
			mafw_proxy_playlist_get_id(new_playlist));
        /* Verify that mafw_playlist_manager_list_playlists()
         * returns the correct information */
         plst = mafw_playlist_manager_get_playlists(manager, NULL);
         for (j = 0; j < plst->len; j++) {
        	 plst_name = mafw_playlist_get_name(plst->pdata[j]);
                 if(!strcmp(plst_name, new_name)){
                	found = TRUE;
                        g_free(plst_name);
                        fail_if(!mafw_playlist_get_repeat(plst->pdata[j]));
                        fail_if(mafw_playlist_is_shuffled(plst->pdata[j]) !=
			 		mafw_playlist_is_shuffled(
			 			MAFW_PLAYLIST(playlist)));
                        mafw_playlist_manager_destroy_playlist(manager,
			 	MAFW_PROXY_PLAYLIST(g_object_ref(plst->pdata[j])),
			 			NULL);
                        break;
                }
                g_free(plst_name);
        }

        mafw_playlist_manager_destroy_playlist(manager, playlist, NULL);
        g_ptr_array_free(plst, TRUE);
        g_free(new_name);
        g_free(name);
        fail_if(!found);
}
END_TEST /* }}} */

/* Test *_create_playlist() and *_destroy_playlist() and the signals. {{{ */
/* Misc {{{ */
/* Prints some information about $playlist; used to monitor the activities
 * of the creator and destroyer processes. */
static void plprint(const gchar *tag, MafwProxyPlaylist *playlist)
{
#if 0
	gchar *name;

	/* $name can be NULL, espectiall if if $playlist is destroyed. */
	g_object_get(playlist, "name", &name, NULL);
	g_warning("%s %u. %s (%p)", tag,
		mafw_proxy_playlist_get_id(playlist),
	       	name, playlist);
	g_free(name);
#endif
}

/* SIGCHLD handler of the creator. */
static void destroyer_died(int unused)
{
	int status;

	if (Destroyer_died >= 0)
		/* We already got a SIGCHLD and now
		 * being called from the test function. */
		return;

	/* The dead child can only be the destroyer.
	 * We cannot fail() from a signal handler, `check' doesn't like it. */
	if (wait(&status) <= 0)
		g_assert_not_reached();
	Destroyer_died = checkmore_child_died(status, "destroyer", 0);
}
/* }}} */

/* Signal handling {{{ */
/* playlist_created() {{{
 * Executed on MafwPlaylistManager::playlist_created. */
static void playlist_created(MafwPlaylistManager *manager,
			     MafwProxyPlaylist *playlist)
{
	gchar *name;
	struct table_st *table;
	guint id;

	/* Based $table verify that $playlist could have been created
	 * by the creator, then add it to $Ids and $Objects. */
	fail_if(playlist == NULL);

	/* Is $playlist valid?  If $name is NULL the destroyer
	 * was quicker to destroy than us to notice. */
	g_object_get(playlist, "name", &name, NULL);
	if (!name)
		return;
	fail_if(!(table = g_tree_lookup(Names, name)),
		"Unexpected playlist `%s'", name);
	g_free(name);

	plprint("CREATED", playlist);
	if (!table->ncreated++)
		NCompleted++;

	/* Register it in $Ids.  Since playlist ids are very unique now
	 * no two playlist can exist with the same id. */
	id = mafw_proxy_playlist_get_id(playlist);
	fail_if(g_tree_lookup(Ids, GUINT_TO_POINTER(id)) != NULL,
		"Playlist ID %u already seen", id);
	g_tree_insert(Ids, GUINT_TO_POINTER(id), table);

	/* Register it in $Objects. */
	fail_if(g_tree_lookup(Objects, playlist) != NULL,
		"Playlist %p already seen", playlist);
	g_tree_insert(Objects, g_object_ref(playlist), playlist);
} /* }}} */

/* playlist_destroyed() {{{
 * Executed on MafwPlaylistManager::playlist_destroyed. */
static void playlist_destroyed(MafwPlaylistManager *manager,
			       MafwProxyPlaylist *playlist)
{
	struct table_st *table;
	guint id;

	/*
	 * Verify that $playlist is valid and deregisters it from $Ids
	 * and $Objects.  If all playlists in the test $table has been
	 * created and destroyed quit the main loop.
	 */
	plprint("DESTROYED", playlist);

	/* Validate $playlist. */
	id = mafw_proxy_playlist_get_id(playlist);
	if (!(table = g_tree_lookup(Ids, GUINT_TO_POINTER(id))))
		fail("Unexpected playlist %u.", id);
	else
		table->ndestroyed++;

	/* Deregister it. */
	fail_if(!g_tree_remove(Ids, GUINT_TO_POINTER(id)),
		"Playlist ID %u not found", id);
	fail_if(!g_tree_remove(Objects, playlist),
		"Playlist %p not found", playlist);

	/* Are we finished? */
	if (NCompleted >= NRounds && g_tree_nnodes(Ids) == 0) {
		flush_dbus();
		g_main_loop_quit(Loop);
	}
} /* }}} */

/* attach_sighands() {{{
 * Connects the playlist_created() and playlist_destroyed() callbacks
 * to the appropriate signals of the manager. */
static void attach_sighands(void)
{
	MafwPlaylistManager *manager;

	manager = mafw_playlist_manager_get();
	g_signal_connect(manager, "playlist_created",
			 G_CALLBACK(playlist_created),   NULL);
	g_signal_connect(manager, "playlist_destroyed",
			 G_CALLBACK(playlist_destroyed), NULL);
}
/* }}} */
/* }}} */

/* do_create() {{{ */
/*
 * Picks the next playlist name from the pregenerated list to create.
 * It may return the entry of a name for which a playlist already
 * exists.  Returns NULL if the creator seems to have finished.
 */
static struct table_st *pick_to_create(void)
{
	static guint rounds = 0;
	gboolean may_recreate;
	struct table_st *current;

	/* Any names with which we haven't created a playlist? */
	if (NCompleted >= NRounds)
		return NULL;

	/* Give a 1 to 5 chance for recreating a playlist. */
	may_recreate = !(g_random_int() % 5);
	current = rnd_tree_value(Names);
	for (;;) {
		if (!current->ncreated)
			/* Hasn't been created. */
			break;
		else if (!current->ndestroyed && may_recreate)
			/* Currently exists, try to recreate. */
			break;
		current = current->next;
	}

	/* Flush the session D-BUS periodically so we don't bury
	 * the daemon with messages. */
	if (++rounds % 100 == 0) {
		info("CREATOR: %u", rounds);
		flush_dbus();
	}

	return current;
}

/* Pick a name and create a playlist with it. */
static gboolean do_create(void)
{
	GError *err;
	struct table_st *current;
	MafwProxyPlaylist *playlist;
	MafwPlaylistManager *manager;

	if (!(current = pick_to_create())) {
		info("FINISHED");
		if (g_tree_nnodes(Ids) == 0) {
			flush_dbus();
			g_main_loop_quit(Loop);
		}
		return FALSE;
	}

	err	 = NULL;
	manager  = mafw_playlist_manager_get();
	playlist = mafw_playlist_manager_create_playlist(manager,
							       	current->name,
							       	&err);
	fail_if(!playlist, err ? err->message : NULL);

	/* mafw_playlist_manager_create_playlist() may have failed
	 * because of run-time reasons. */
	if (playlist) {
		plprint("CREATEING", playlist);
		g_object_unref(playlist);
	}

	return TRUE;
}
/* }}} */

/* do_destroy() {{{ */
static gboolean add_destroy(void);

/* Returns a random playlist to destroy, or NULL if currently
 * we don't have any playlists due to the creator. */
static MafwProxyPlaylist *pick_to_destroy(void)
{
	static guint rounds = 0;
	MafwProxyPlaylist *playlist;

	/* Select a random playlist from $Objects. */
	playlist = rnd_tree_value(Objects);

	if (++rounds % 100 == 0) {
		info("DESTROYER: %u", rounds);
		flush_dbus();
	}

	return playlist;
}

/* Pick a random playlist and destroy it. */
static gboolean do_destroy(void)
{
	MafwProxyPlaylist *playlist;
	MafwPlaylistManager *manager;

	if (!(playlist = pick_to_destroy())) {
		if (NCompleted >= NRounds) {
			info("FINISHED");
			flush_dbus();
			g_main_loop_quit(Loop);
		} else
			/* Just run out of playlists, wait
			 * until the creator creates some. */
			g_timeout_add_seconds(2,
                                              (GSourceFunc)add_destroy,
                                              NULL);
		return FALSE;
	}

	plprint("DESTROYING", playlist);

	manager = mafw_playlist_manager_get();
	mafw_playlist_manager_destroy_playlist(manager,
						      g_object_ref(playlist),
						      NULL);

	return TRUE;
}

/* Timeout handler to add back do_destroy() as an idle function. */
static gboolean add_destroy(void)
{
	/* Wait 0.1 sec between do_destroy()s not to hog the daemon. */
	g_timeout_add(100, (GSourceFunc)do_destroy, NULL);
	return FALSE;
}
/* }}} */

START_TEST(test_like_a_little_angel)
{ /* {{{ */
	guint i;
	int comm[2];
	gboolean is_creator;
	struct table_st table[NRounds];

	/*
	 * Test that playlist creation and destruction works, and the
	 * events are properly communicated to all interested parties.
	 *
	 * Strategy: two processes, the creator and the destroyer
	 * are running simultanously.  The creator creates random
	 * playlists, and the destroyer is destroying them,  Both
	 * processes perform all kinds of sanity checks to see that
	 * exactly what is supposed to be created/destroyed is
	 * created and destroyed.
	 */

	/* Create the registries.  $Ids is a GTree because
	 * we've got helpers to use a playlist ID as a key. */
	Names	= g_tree_new_full((GCompareDataFunc)strcmp, NULL, g_free, NULL);
	Ids	= g_tree_new_full((GCompareDataFunc)cmpptrs, NULL, NULL, NULL);
	Objects	= g_tree_new_full(cmpptrs, NULL, g_object_unref, NULL);

	/* Generate a bunch of random names with which the creator
	 * will create playlists. */
	memset(table, 0, sizeof(table));
	for (i = 0; i < G_N_ELEMENTS(table); i++) {
		/* .name will be free()d by $Names. */
		table[i].name = g_strdup_printf("%.8X", g_random_int());
		table[i].next = i < G_N_ELEMENTS(table) - 1
		       	? &table[i+1] : &table[0];
		g_tree_insert(Names, table[i].name, &table[i]);
	}

	/* The pipe is used to synchronize the startup of the
	 * creator and the destroyer. */
	pipe(comm);
	Loop = g_main_loop_new(NULL, TRUE);

	/* Start and initialize the processes. */
	signal(SIGCHLD, destroyer_died);
	if ((is_creator = check_fork() != 0)) {
		/* This is the creator. */
		char unused;

		/* Normally LOG_CREATOR is NULL so stderr is not redirected. */
		checkmore_redirect(LOG_CREATOR);

		/* Signal handlers are connected to the manager after for
		 * because the first mafw_playlist_manager_get()
		 * opens a connection to dbusd and we need different
		 * connections in the two processes. */
		attach_sighands();

		/* All action is happening in idle time, because
		 * the manager needs to process D-BUS messages. */
		g_idle_add((GSourceFunc)do_create, NULL);

		/* Wait until the the destroyer is ready,
		 * so it won't miss any D-BUS messages. */
		read(comm[0], &unused, 1);
	} else {
		/* This is the destroyer. */
		checkmore_redirect(LOG_DESTROYER);
		attach_sighands();
		add_destroy();

		/* Tell the creator we can go.
		 * (Actually we're racing, but it's only theoretical.) */
		write(comm[1], ">", 1);
	}

	/* Creator do_create()s, destroyer do_destroy()s
	 * when they are not busy with processing each
	 * other's events. */
	g_main_loop_run(Loop);

	/* Test finished.  Verify that evey playlist created by the
	 * creator has been destroyed by the destroyer. */
	fail_if(g_tree_nnodes(Ids)	!= 0);
	fail_if(g_tree_nnodes(Objects)	!= 0);

	/* Check that every playlist names have been created and
	 * destroyed the same times. */
	for (i = 0; i < G_N_ELEMENTS(table); i++) {
		fail_if(table[i].ncreated == 0);
		fail_if(table[i].ncreated != table[i].ndestroyed);
	}

	/* Clean up. */
	g_tree_destroy(Names);
	g_tree_destroy(Ids);
	g_tree_destroy(Objects);

	/* Exit */
	if (is_creator) {
		signal(SIGCHLD, SIG_DFL);
		destroyer_died(0);
		fail_if(Destroyer_died);
	} else
		/* exit() the destroyer, otherwise `check' gets confused. */
		exit(0);
}
END_TEST /* }}} */
/* }}} */

/* The main function */
int main(int argc, char const *argv[])
{ /* {{{ */
	TCase *tc;
	Suite *suite;
	guint timeout;

	/* The number of rounds by the stress test
	 * is reduced ridicolously by popular demand.
	 * Don't timeout by default. */
	NRounds = 50;
	timeout =  0;

	/* Get $NRounds $timeout from the command line. */
	if (argv[1]) {
		NRounds = atoi(argv[1]);
		if (argv[2])
			/* A more reasonable default would be
			 * NRounds / 10) + 5. */
			timeout = atoi(argv[2]);
	}

	checkmore_wants_dbus();
	g_setenv("MAFW_PLAYLIST_DIR", "testplaylistmanager", TRUE);

	suite = suite_create("MafwPlaylistManager");

	tc = tcase_create("Basic");
	tcase_add_test(tc, test_get_manager);
	tcase_add_unchecked_fixture(tc, start_daemon, checkmore_stop);
	suite_add_tcase(suite, tc);

	tc = tcase_create("End to end");
	tcase_add_unchecked_fixture(tc, start_daemon, checkmore_stop);
	tcase_add_test(tc, test_get_playlists);
	tcase_add_test(tc, test_like_a_little_angel);
	tcase_add_test(tc, test_dup_playlists);
	tcase_set_timeout(tc, timeout);
	suite_add_tcase(suite, tc);

	return checkmore_run(srunner_create(suite), FALSE);
} /* }}} */

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0 foldmethod=marker: */
