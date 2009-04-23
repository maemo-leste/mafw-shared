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
#include <checkmore.h>
#include "mafw-playlist-daemon/mpd-internal.h"

gboolean initialize = FALSE;

struct item {
	guint pidx;
	gchar *oid;
};

#define APLS(...)				\
	((struct item[]) {			\
		__VA_ARGS__,			\
		{0, NULL},			\
			})

#define EPLS APLS({0, NULL})

/* Creates a playlist with a known order from $items (setting playing indexes
 * accordingly), to allow testing correctness with shuffled playlists.
 * NOTE: this may be misleading a bit, as
 *   mkpls(APLS({1, "xxx"},
 *              {0, "yyy"},
 *              {2, "zzz"}));
 * results in a playlist with elements visually {xxx, yyy, zzz} and playing
 * order {yyy, xxx, zzz}.  That is, the numbers don't say which sequence that
 * particular item will be played in.  It was just convenient to reuse struct
 * items[] for this.
 */
static Pls *mkpls(struct item items[])
{
	Pls *p;
	guint i;

	p = pls_new(88, "playlist by mkpls");
	for (i = 0; items[i].oid; ++i)
		pls_append(p, items[i].oid);
	for (i = 0; items[i].oid; ++i)
		p->pidx[i] = items[i].pidx;
	return p;
}

/* Assert that $pls has $items. */
static void assert_pls(Pls *pls, struct item items[])
{
	guint i, len;

	for (len = 0; items[len].oid; ++len);
	for (i = 0; items[i].oid; ++i) {
		if (i >= pls->len)
			break;
		fail_if(strcmp(pls->vidx[i], items[i].oid),
			"oid mismatch at %u. '%s' != '%s'",
			i, items[i].oid, pls->vidx[i]);
		/* -1 means, that should not be checked, as it is random */
		if (items[i].pidx != -1)
			fail_unless(items[i].pidx == pls->pidx[i],
			    "pidx mismatch at %u: actual %u expected %u.",
			    i, pls->pidx[i], items[i].pidx);
	}
	if (i < len) {
		for (; items[i].oid; ++i)
			fprintf(stderr, "%u %u '%s'\n",
				i, items[i].pidx, items[i].oid);
		fail("expected more elements");
	}
	if (pls->len > len) {
		g_assert(i == len);
		for (; i < pls->len; ++i)
			fprintf(stderr, "%u %u '%s'\n",
				i, pls->pidx[i], pls->vidx[i]);
		fail("expected less elements");
	}
}

START_TEST(test_create)
{
	Pls *p;

	p = pls_new(10, "one");
	fail_unless(p->id == 10);
	fail_if(strcmp(p->name, "one"));
 	assert_pls(p, EPLS);
	pls_insert(p, 0, "alma");
	pls_insert(p, 1, "korte");
	assert_pls(p, APLS({0, "alma"},
			   {1, "korte"}));
	pls_clear(p);
 	assert_pls(p, EPLS);
	pls_clear(p);
 	assert_pls(p, EPLS);
	pls_set_name(p, "two");
	fail_if(strcmp(p->name, "two"));
	pls_free(p);
}
END_TEST

/* Fixture for the following operation tests. */
static Pls *Playlist;

static void setup_pls(void)
{
	Playlist = pls_new(99, "playlist by fixture");
}

static void teardown_pls(void)
{
	fail_unless(pls_check(Playlist));
	pls_free(Playlist);
}

START_TEST(test_append)
{
	Pls *p = Playlist;
	const gchar *oidl[] = {"ab", "cd", "ef", NULL};

	assert_pls(p, EPLS);
	fail_unless(pls_append(p, "alpha"));
	assert_pls(p, APLS({0, "alpha"}));
	fail_unless(pls_append(p, "beta"));
	assert_pls(p, APLS({0, "alpha"},
			   {1, "beta"}));

	pls_free(p);
	Playlist = p = mkpls(APLS({0, "eek"},
				  {2, "a"},
				  {1, "mouse"}));
	fail_unless(pls_append(p, "blackbeard"));
	assert_pls(p, APLS({0, "eek"},
			   {2, "a"},
			   {1, "mouse"},
			   {3, "blackbeard"}));
	pls_free(p);
	
	Playlist = p = mkpls(APLS({0, "eek"},
				  {2, "a"},
				  {1, "mouse"}));
	fail_unless(pls_appends(p, oidl, 3));
	assert_pls(p, APLS({0, "eek"},
			   {2, "a"},
			   {1, "mouse"},
			   {3, "ab"},
			   {4, "cd"},
			   {5, "ef"}));
}
END_TEST

START_TEST(test_clear)
{
	Pls *p = Playlist;

	assert_pls(p, EPLS);
	pls_append(p, "xxx");
	pls_append(p, "xxx");
	pls_append(p, "xxx");
	assert_pls(p, APLS({0, "xxx"},
			   {1, "xxx"},
			   {2, "xxx"}));
	pls_clear(p);
	assert_pls(p, EPLS);
}
END_TEST

START_TEST(test_insert)
{
	Pls *p = Playlist;
	const gchar *oblist[] = { "ab", "cd", "ef"};

	fail_unless(pls_insert(p, 0, "alma"));
	assert_pls(p, APLS({0, "alma"}));
	fail_unless(pls_insert(p, 1, "dinnye"));
	assert_pls(p, APLS({0, "alma"},
			   {1, "dinnye"}));
	fail_if(pls_insert(p, 3, "no no"));
	assert_pls(p, APLS({0, "alma"},
			   {1, "dinnye"}));

	pls_clear(p);
	fail_if(pls_insert(p, 1, "should fail"));

	fail_unless(pls_insert(p, 0, "prepending"));
	assert_pls(p, APLS({0, "prepending"}));
	fail_unless(pls_insert(p, 0, "just"));
	assert_pls(p, APLS({0, "just"},
			   {1, "prepending"}));
	fail_unless(pls_insert(p, 0, "always"));
	assert_pls(p, APLS({0, "always"},
			   {1, "just"},
			   {2, "prepending"}));
	pls_free(p);

	/* Insert items */
	Playlist = p = mkpls(APLS({0, "insert"},
				  {1, "versus"},
				  {2, "shuffle"}));
	fail_unless(pls_inserts(p, 1, oblist, 3));
	assert_pls(p, APLS({0, "insert"},
			   {1, "ab"},
			   {2, "cd"},
			   {3, "ef"},
			   {4, "versus"},
			   {5, "shuffle"}));
	pls_free(p);
	Playlist = p = mkpls(APLS({2, "insert"},
				  {1, "versus"},
				  {0, "shuffle"}));
	p->shuffled = TRUE;
	fail_unless(pls_insert(p, 0, "will break"));
	assert_pls(p, APLS({-1, "will break"},
			   {-1, "insert"},
			   {-1, "versus"},
			   {-1, "shuffle"}));
	/* Array, what stores the original order */
	guint index_table[4] = {3, 2, 1, 0};
	gint i, j = 0;

	for (i=0; i<4; i++)
	{
		/* if it points to the first item, it should not be checked */
		if (p->pidx[i] != 0)
		{
			fail_if(p->pidx[i] != index_table[j]);
			j++;
		}
	}

	/* Get the new order */
	for (i=0; i<4; i++)
	{
		index_table[i] = p->pidx[i];
	}

	fail_unless(pls_insert(p, 4, "the last"));
	assert_pls(p, APLS({-1, "will break"},
			   {-1, "insert"},
			   {-1, "versus"},
			   {-1, "shuffle"},
			   {-1, "the last"}));
	
	j = 0;
	for (i=0; i<5; i++)
	{
		if (p->pidx[i] != 4)
		{
			fail_if(p->pidx[i] != index_table[j]);
			j++;
		}
	}

}
END_TEST

START_TEST(test_remove)
{
	Pls *p = Playlist;

	fail_if(pls_remove(p, 0));
	fail_if(pls_remove(p, 10));
	fail_if(pls_remove(p, -2));

	pls_append(p, "xyzzy");
	pls_append(p, "is");
	pls_append(p, "magic");
	fail_if(pls_remove(p, 3));
	assert_pls(p, APLS({0, "xyzzy"},
			   {1, "is"},
			   {2, "magic"}));
	fail_unless(pls_remove(p, 1));
	assert_pls(p, APLS({0, "xyzzy"},
			   {1, "magic"}));
	fail_unless(pls_remove(p, 1));
	assert_pls(p, APLS({0, "xyzzy"}));
	fail_if(pls_remove(p, 1));
	fail_unless(pls_remove(p, 0));
	assert_pls(p, EPLS);

	pls_free(p);
	Playlist = p = mkpls(APLS({3, "xyzzy"},
				  {1, "is"},
				  {0, "true"},
				  {2, "magic"}));
	fail_unless(pls_remove(p, 2));
	assert_pls(p, APLS({2, "xyzzy"},
			   {0, "is"},
			   {1, "magic"}));
	fail_unless(pls_remove(p, 2));
	assert_pls(p, APLS({1, "xyzzy"},
			   {0, "is"}));
}
END_TEST

START_TEST(test_move)
{
	Pls *p = Playlist;

	pls_append(p, "a");
	pls_append(p, "b");
	pls_append(p, "c");
	pls_append(p, "d");
	pls_move(p, 0, 0);
	fail_unless(pls_move(p, 0, 0));
	assert_pls(p, APLS({0, "a"},
			   {1, "b"},
			   {2, "c"},
			   {3, "d"}));
	fail_unless(pls_move(p, 0, 1));
	assert_pls(p, APLS({0, "b"},
			   {1, "a"},
			   {2, "c"},
			   {3, "d"}));
	fail_unless(pls_move(p, 3, 0));
	assert_pls(p, APLS({0, "d"},
			   {1, "b"},
			   {2, "a"},
			   {3, "c"}));

	/* See with a shuffled playlist. */
	pls_free(p);
	Playlist = p = mkpls(APLS({1, "a"},
				  {3, "b"},
				  {0, "c"},
				  {2, "d"}));
	assert_pls(p, APLS({1, "a"},
			   {3, "b"},
			   {0, "c"},
			   {2, "d"}));

	fail_unless(pls_move(p, 0, 1));
	assert_pls(p, APLS({1, "b"},
			   {3, "a"},
			   {0, "c"},
			   {2, "d"}));
	fail_unless(pls_move(p, 2, 0));
	assert_pls(p, APLS({1, "c"},
			   {3, "b"},
			   {0, "a"},
			   {2, "d"}));
}
END_TEST

START_TEST(test_shuffle_empty)
{
	Pls *p = Playlist;

	fail_if(pls_is_shuffled(p));
	pls_shuffle(p);
	/* So, what's the definition of is-shuffled for an empty playlist, if
	 * shuffle is an operation and not a state? :) */
	fail_unless(pls_is_shuffled(p) || !pls_is_shuffled(p));
}
END_TEST

START_TEST(test_shuffle)
{
	Pls *p = Playlist;
	guint i, nonrandom;

	pls_append(p, "AA");
	pls_append(p, "BB");
	pls_append(p, "CC");
	pls_append(p, "DD");
	pls_append(p, "EE");
	pls_append(p, "FF");
	fail_if(pls_is_shuffled(p));
	pls_shuffle(p);
	fail_unless(pls_is_shuffled(p));
	pls_unshuffle(p);
	fail_if(pls_is_shuffled(p));

	nonrandom = 0;
	for (i = 0; i < 50; ++i) {
		pls_shuffle(p);
		nonrandom += !pls_is_shuffled(p);
		fail_unless(pls_check(p));
	}
	fail_if(nonrandom > 4);
}
END_TEST

START_TEST(test_iterator)
{
	Pls *p = Playlist;
	gchar *oid = NULL;
	guint new_idx = 0;

	pls_free(p);
	
	/* Check with empty playlist */
	p = pls_new(66, "test-pl");
	pls_get_starting(p, &new_idx, &oid);
	fail_if(oid);
	pls_get_next(p, &new_idx, &oid);
	fail_if(oid);
	new_idx = 1;
	pls_get_next(p, &new_idx, &oid);
	fail_if(oid);
	pls_free(p);

	Playlist = p = mkpls(APLS({0, "a"},
				  {1, "b"},
				  {2, "c"},
				  {3, "d"}));
	pls_get_last(p, &new_idx, &oid);
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "d"));
	g_free(oid);
	oid = NULL;

	pls_get_starting(p, &new_idx, &oid);
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "a"));
	g_free(oid);
	oid = NULL;

	pls_get_next(p, &new_idx, &oid);
	fail_if(new_idx != 1);
	fail_if(strcmp(oid, "b"));
	g_free(oid);
	oid = NULL;

	pls_get_prev(p, &new_idx, &oid);
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "a"));
	g_free(oid);
	oid = NULL;
	
	pls_get_prev(p, &new_idx, &oid);
	fail_if(oid);
	
	new_idx = 3;
	pls_get_next(p, &new_idx, &oid);
	fail_if(oid);
	
	/*repeat on */
	p->repeat = TRUE;
	pls_get_next(p, &new_idx, &oid);
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "a"));
	g_free(oid);
	oid = NULL;
	
	pls_get_prev(p, &new_idx, &oid);	
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "d"));
	g_free(oid);
	oid = NULL;

	pls_get_last(p, &new_idx, &oid);
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "d"));
	g_free(oid);
	oid = NULL;

	pls_free(p);

	/* Shuffle on */
	Playlist = p = mkpls(APLS({2, "a"},
				  {3, "b"},
				  {1, "c"},
				  {0, "d"}));
	p->shuffled = TRUE;

	pls_get_last(p, &new_idx, &oid);
	fail_if(new_idx != 0);
	fail_if(strcmp(oid, "a"));
	g_free(oid);
	oid = NULL;

	pls_get_starting(p, &new_idx, &oid);
	fail_if(new_idx != 2);
	fail_if(strcmp(oid, "c"));
	g_free(oid);
	oid = NULL;

	pls_get_next(p, &new_idx, &oid);
	fail_if(new_idx != 3);
	fail_if(strcmp(oid, "d"));
	g_free(oid);
	oid = NULL;

	pls_get_prev(p, &new_idx, &oid);
	fail_if(new_idx != 2);
	fail_if(strcmp(oid, "c"));
	g_free(oid);
	oid = NULL;
}
END_TEST

/* All modifying operations should set the dirty state. */
START_TEST(test_dirty)
{
	Pls *p;

	p = pls_new(55, "pls");
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_append(p, "alma");
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_insert(p, 0, "zero");
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_remove(p, 1);
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_shuffle(p);
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_unshuffle(p);
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_set_repeat(p, TRUE);
	fail_unless(p->dirty);

	p->dirty = FALSE;
	pls_append(p, "a few");
	pls_append(p, "more items");
	pls_move(p, 0, 1);
	fail_unless(p->dirty);

	pls_free(p);
}
END_TEST

/* See if loading a saved playlist results in the same. */
START_TEST(test_save)
{
	Pls *p1, *p2;
	gint i;
	gchar name[16];

	unlink("tale.mp");
	p1 = pls_new(44, "tale");
	for (i = 0; i < 24; ++i) {
		sprintf(name, "item_%02u", i);
		pls_append(p1, name);
	}
	fail_unless(p1->dirty);
	fail_unless(pls_save(p1, "tale.mp"));
	p2 = pls_load("tale.mp");
	fail_if(p2 == NULL);
	fail_unless(p2->id == p1->id);
	fail_if(strcmp(p2->name, p1->name));
	fail_unless(p2->repeat == p1->repeat);
	fail_unless(p2->shuffled == p1->shuffled);
	fail_unless(p2->len == p1->len);
	fail_unless(p2->dirty);
	pls_free(p1);
	pls_free(p2);
}
END_TEST

START_TEST(stress_persist)
{
#ifndef __ARMEL__
	GTimeVal t0, t1;
	gchar name[64];
	Pls *p1;
	guint i;
	gulong usec;

	unlink("p1.mp");
	p1 = pls_new(666, "firstborn");
	for (i = 0; i < 20000; ++i) {
		sprintf(name, "alonguuid::some/long/item_%02u", i);
		pls_append(p1, name);
	}
	g_get_current_time(&t0);
	for (i = 0; i < 10; ++i)
		fail_unless(pls_save(p1, "p1.mp"));
	g_get_current_time(&t1);
	/* Let's say that saving 20k elements under 150ms is good. */
	usec =  (t1.tv_sec * G_USEC_PER_SEC + t1.tv_usec) -
		(t0.tv_sec * G_USEC_PER_SEC + t0.tv_usec);
	fail_unless(usec < (20*150*1000));
	pls_free(p1);
#endif
}
END_TEST

/* Feed junk to pls_load(). */
START_TEST(fuzz_load)
{
	Pls *p;

	fail_if(pls_load("a_nonexistent_file") != NULL);
	g_file_set_contents("junk",
			    ""
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "lfszp is some random string"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V4\n"
			    "is not a version we know\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "-3451\n"
			    "invalid id\n"
			    "542312432143243\n"
			    "1\n"
			    "-10\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "-3451\n"
			    "invalid repeat setting and negative length\n"
			    "542312432143243\n"
			    "0\n"
			    "-10\n"
			    "1,asdf\n"
			    "2,fdsa\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "3451\n"
			    "invalid shuffle setting\n"
			    "1\n"
			    "2342\n"
			    "-10\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "123\n"
			    "missing items\n"
			    "1\n"
			    "1\n"
			    "10\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "123\n"
			    "wrong playing indexes\n"
			    "1\n"
			    "1\n"
			    "2\n"
			    "-1,one\n"
			    "4,two\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "123\n"
			    "empty object ids\n"
			    "1\n"
			    "1\n"
			    "2\n"
			    "1,\n"
			    "0,two\n"
			    , -1, NULL);
	fail_if(pls_load("junk") != NULL);
	g_file_set_contents("junk",
			    "V1\n"
			    "123\n"
			    "something\n"
			    "1\n"
			    "1\n"
			    "2\n"
			    "0,alma\n"
			    "1,korte\n"
			    "2,too much!!!!!!!\n"
			    , -1, NULL);
	/* This will succeed, we don't care if it actually has more items than
	 * $len says. */
	fail_unless((p = pls_load("junk")) != NULL);
	pls_free(p);
	unlink("junk");
}
END_TEST

/* aplaylist wants to call save_me(). */
static gboolean Save_me_noop = TRUE;
static GMainLoop *TheLoop;
/* Number of times save_me() was called. */
static guint Times_saved;
/* Playlist pointers passed to save_me() ORed together, functioning as a very
 * primitive set.  Used to verify that all expected playlists were saved. */
static guint Playlists_saved;

void save_me(Pls *pls)
{
	/* No-op unless said so. */
	if (Save_me_noop)
		return;

	fail_unless(pls->dirty);
	Times_saved++;
	Playlists_saved |= (guint)pls;
	pls->dirty = FALSE;
}

/* Timer/idle functions, invoked multiple times, dispatching manually (as I
 * don't want to split these into separate functions).  Intended to simulate
 * events.  WARNING: might contain macro abuse. */

struct edit {
	gint step;
	Pls *pls;
	gboolean (*fn)(struct edit *e);
};

#define START_EDIT(name) \
	static gboolean name(struct edit *e)	\
	{					\
	Pls *pls = e->pls;			\
	e->step += 1;				\
	switch (e->step)

#define STEP(n) case n

#define NEXT(when) do { g_timeout_add(when, (GSourceFunc)e->fn, e); return FALSE; } while (0)
#define DONE       return FALSE

#define END_EDIT				\
	return FALSE;				\
	}

/* Time taken: 2.5s */
START_EDIT(edit_a_bit)
{
STEP(0):
	pls_append(pls, "alma");
	NEXT(100);
STEP(1):
	pls_insert(pls, 0, "boo");
	NEXT(1000);
STEP(2):
	pls_append(pls, "out");
		NEXT(500);
STEP(3):
	pls_append(pls, "of");
	NEXT(800);
STEP(4):
	pls_append(pls, "cheese");
	NEXT(100);
STEP(5):
	pls_shuffle(pls);
	DONE;
}
END_EDIT

/* Time taken: 0.5s */
START_EDIT(free_before_saving)
{
	/* We destroy the playlist before it gets a saved. */
STEP(0):
	pls_append(pls, "alma");
	NEXT(500);
STEP(1):
	pls_free(pls);
	DONE;
}
END_EDIT

#undef STEP
#undef NEXT
#undef DONE
#undef START_EDIT
#undef END_EDIT

/* Bundles the edit operation (fn), its `instruction counter' and the assigned
 * playlist together.  Use run_edit() to assign the playlist and set the wheel
 * rolling. */
static struct edit edits[] = {
	{-1, NULL, edit_a_bit},
	{-1, NULL, edit_a_bit},
	{-1, NULL, free_before_saving},
};

/* Reset the instruction counter of and start the editing operation in
 * edits[$idx]. */
static void run_edit(int idx, Pls *pls)
{
	edits[idx].step = -1;
	edits[idx].pls = pls;
	g_timeout_add(10, (GSourceFunc)edits[idx].fn, &edits[idx]);
}

/* Quits the main loop after $time seconds. */
static void quit_after(guint time)
{
	g_timeout_add_seconds(time, (GSourceFunc)g_main_loop_quit, TheLoop);
}

START_TEST(test_dirty_timer)
{
	Pls *p;

	Settle_time = 1;
	Save_me_noop = FALSE;
	TheLoop = g_main_loop_new(NULL, FALSE);
	/* Edit the playlist and ensure that save_me() is called. */
	Playlists_saved = Times_saved = 0;
	p = pls_new(44, "MELON MELON MELON");
	run_edit(0, p);
	quit_after(3 + Settle_time);
	g_main_loop_run(TheLoop);
	fail_unless(Times_saved >= 1);
	fail_unless(Playlists_saved == (0|(guint)p));

	/* See if destroying a playlist removes the dirty timer. */
	Playlists_saved = Times_saved = 0;
	run_edit(2, p);
	quit_after(Settle_time + 1);
	g_main_loop_run(TheLoop);
	fail_unless(Times_saved == 0);
	fail_unless(Playlists_saved == 0);
}
END_TEST

START_TEST(multi_dirty)
{
	Pls *a, *b, *c;

	Settle_time = 1;
	Save_me_noop = FALSE;
	TheLoop = g_main_loop_new(NULL, FALSE);
	Playlists_saved = Times_saved = 0;
	/* Edit two playlists and see if both are saved. */
	a = pls_new(555, "OUT OF CHEESE ERROR");
	b = pls_new(444, "Teh-a-Tee-may");
	c = pls_new(444, "they invented boredom.");
	run_edit(0, a);
	run_edit(1, b);
	run_edit(2, c);
	quit_after(3 + Settle_time);
	g_main_loop_run(TheLoop);
	fail_unless(Times_saved >= 2);
	fail_unless(Playlists_saved == (0|(guint)a|(guint)b));
	pls_free(a);
	pls_free(b);
}
END_TEST

int main(void)
{
	int rv;
	Suite *suite;
	TCase *tc;

	suite = suite_create("A playlist");

	tc = tcase_create("Various");
	tcase_set_timeout(tc, 0);
	if (1) tcase_add_test(tc, test_create);
	if (1) tcase_add_test(tc, test_dirty);
	if (1) tcase_add_test(tc, test_save);
	if (1) tcase_add_test(tc, stress_persist);
	if (1) tcase_add_test(tc, fuzz_load);
	/* The following two tests take longer time. */
	if (1) tcase_add_test(tc, test_dirty_timer);
	if (1) tcase_add_test(tc, multi_dirty);
	suite_add_tcase(suite, tc);
if (1){
	tc = tcase_create("Operations");
	tcase_add_test(tc, test_append);
if (1){	tcase_add_test(tc, test_clear);
	tcase_add_test(tc, test_insert);
	tcase_add_test(tc, test_remove);
	tcase_add_test(tc, test_move);
	tcase_add_test(tc, test_iterator);
	tcase_add_test(tc, test_shuffle_empty);
	tcase_add_test(tc, test_shuffle);}
	tcase_add_checked_fixture(tc, setup_pls, teardown_pls);
	suite_add_tcase(suite, tc);
}
	rv = checkmore_run(srunner_create(suite), TRUE);
	/* This way valgrind has a chance to cry out. */
	Playlist = NULL;
	return rv;
}
