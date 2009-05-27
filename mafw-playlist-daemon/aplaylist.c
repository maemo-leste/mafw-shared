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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "mpd-internal.h"

#define APLAYLIST_VERSION "2"

extern gboolean initialize;

/* Globals */
/* Time to wait (in seconds) for edit operations to settle on a playlist,
 * before triggering saving.  The value is chosen assuming that programmatic
 * mass operations are quick, and user initiated edits shall be preserved
 * maximally. */
guint Settle_time = 1;

/* Forward declarations */
static gboolean ops_settled(Pls *pls);

/* Check pls is well-formed. That is, both pidx and iidx must contain all
 * indexes in the playlist, exactly once. */
gboolean pls_check(Pls *pls)
{
	gboolean isok;
	guint i;
	guint *hist_iidx;
        guint *hist_pidx;

	isok = TRUE;

        if (pls->shuffled) {
                hist_pidx = g_new0(guint, pls->len);
                hist_iidx = g_new0(guint, pls->len);
                for (i = 0; i < pls->len; ++i) {
                        hist_pidx[pls->pidx[i]]++;
                        hist_iidx[pls->iidx[i]]++;
                }
                for (i = 0; i < pls->len; ++i) {
                        if (hist_pidx[i] == 0) {
                                g_critical("%u is missing from pidx", i);
                                isok = FALSE;
                        } else if (hist_iidx[i] == 0) {
                                g_critical("%u is missing from iidx", i);
                                isok = FALSE;
                        } else if (hist_pidx[i] > 1) {
                                g_critical("%u is present in pidx more than one time",
                                           i);
                                isok = FALSE;
                        } else if (hist_iidx[i] > 1) {
                                g_critical("%u is present in iidx more than one time",
                                           i);
                                isok = FALSE;
                        }
                }
                g_free(hist_pidx);
                g_free(hist_iidx);
        }
	return isok;
}

/* Prints $pls stats and optionally items. */
void pls_dump(Pls *pls, gboolean items)
{
	guint i;

	g_print("-- id   : %u\n"
		"-- name : %s\n"
		"-- alloc: %u\n"
		"-- len  : %u\n"
		"-- waste: %u bytes\n", pls->id, pls->name,
		pls->alloc, pls->len,
		(sizeof(*pls->vidx) + sizeof(*pls->pidx)) *
		(pls->alloc - pls->len));

	if (!items) {
		return;
        }

	g_print("VI PL OID\n");
        if (pls->shuffled) {
                for (i = 0; i < pls->len; ++i) {
                        g_print("%2u %2u %s\n", i, pls->pidx[i], pls->vidx[i]);
                }
        } else {
                for (i = 0; i < pls->len; ++i) {
                        g_print("%2u %2u %s\n", i, i, pls->vidx[i]);
                }
        }

	pls_check(pls);
}

/* Called to lengthen the dirty timer at each edit operation, anticipating that
 * more edits will happen in the near future. */
static void i_am_dirty(Pls *pls)
{
	if (pls->dirty_timer)
		g_assert(g_source_remove(pls->dirty_timer));
	pls->dirty = TRUE;
	pls->dirty_timer = g_timeout_add_seconds(Settle_time,
                                                 (GSourceFunc)ops_settled, pls);
}

/* Timer callback called when edit operations have settled.  Calls save_me(),
 * which should try to save the playlist, and clear pls->dirty if successful.
 * If it doesn't, the timer will be restarted in the hope maybe it was a
 * temporary failure. */
static gboolean ops_settled(Pls *pls)
{
	g_assert(pls->dirty);
	/* Clear the timer in any case. */
	if (pls->dirty_timer) {
		g_assert(g_source_remove(pls->dirty_timer));
		pls->dirty_timer = 0;
	}
	save_me(pls);
	/* If save_me() succeeded, it should have cleared the dirty flag.  If
	 * it's still set, we reinstate the timer. */
	if (pls->dirty)
		i_am_dirty(pls);
	return FALSE;
}

/* Swap two elements by updating pidx and iidx accordingly */
static void swap_elements(Pls *pls, gint index1, gint index2)
{
        gint tmp;

        tmp = pls->pidx[index1];
        pls->pidx[index1] = pls->pidx[index2];
        pls->pidx[index2] = tmp;
        pls->iidx[pls->pidx[index1]] = index1;
        pls->iidx[pls->pidx[index2]] = index2;
}

/* Randomize amount elements from pool */
static void shuffle_elements(Pls *pls, gint amount)
{
        gint sidx, i;

        /* Adjust amount to not overflow */
        if (amount > (pls->len - pls->poolst)) {
                amount = pls->len - pls->poolst;
        }
        for (i = 0; i < amount; i++) {
                sidx = g_random_int_range(pls->poolst, pls->len);
                swap_elements(pls,
                              pls->poolst,g_random_int_range(pls->poolst,
                                                             pls->len));
                pls->poolst++;
        }
}

/* Create a new playlist with id and name */
Pls *pls_new(guint id, const gchar *name)
{
	Pls *p;

	if (!name || !*name) {
		return NULL;
        }

	p = g_new0(Pls, 1);
	p->id = id;
	p->dirty = TRUE;
	p->use_count = 0;
	p->dirty_timer = 0;
	pls_set_name(p, name);
	return p;
}

/* Change playlist name */
gboolean pls_set_name(Pls *pls, const gchar *name)
{
	if (!name || !*name) {
		return FALSE;
        }

	if (pls->name) {
		g_free(pls->name);
        }
	pls->name = g_strdup(name);

	if (!initialize) {
		i_am_dirty(pls);
        }

	return TRUE;
}

/* Empties playlist */
void pls_clear(Pls *pls)
{
	guint i;

	for (i = 0; i < pls->len; ++i) {
		g_free(pls->vidx[i]);
        }

	g_free(pls->vidx);
	g_free(pls->pidx);
        g_free(pls->iidx);
	pls->vidx = NULL;
	pls->pidx = NULL;
        pls->iidx = NULL;
	pls->len = pls->poolst = pls->alloc = 0;
	i_am_dirty(pls);
}

/* Remove completely playlist */
void pls_free(Pls *pls)
{
	pls_clear(pls);

	if (pls->dirty_timer) {
		g_source_remove(pls->dirty_timer);
        }

	if (pls->name) {
		g_free(pls->name);
        }

	g_free(pls);
}

static void maybe_realloc(Pls *pls, guint want_to_add)
{
	guint wantsize, s;

	g_assert(pls->alloc >= pls->len);
	wantsize = pls->len + want_to_add;
	/* TODO: this could also compact the array if len << alloc. */
	if (wantsize <= pls->alloc)
		return;
	/* min 16 items, otherwise nearest power of 2 */
	for (s = 16; s < wantsize; s <<= 1);
	pls->alloc = s;
	pls->vidx = g_realloc(pls->vidx, pls->alloc * sizeof(*pls->vidx));
        if (pls->shuffled) {
                pls->pidx = g_realloc(pls->pidx,
                                      pls->alloc * sizeof(*pls->pidx));
                pls->iidx = g_realloc(pls->iidx,
                                      pls->alloc * sizeof(*pls->iidx));
        }
}

/* Insert oids array (len sized) in playlist, at idx-th position. Already
 * existent elements are displaced. Returns @TRUE if elements have been
 * inserted */
gboolean pls_inserts(Pls *pls, guint idx, const gchar **oids, guint len)
{
	guint i;
	/* The inserted item `steals' the playing index from the element whose
	 * place it takes. */

        if (!oids || !len) {
                return FALSE;
        }

	/* This could either append or croak */
	if (idx > pls->len) {
		return FALSE;
	}

	maybe_realloc(pls, len);

	/* Push vidx up to alloc the new elements */
	g_memmove(&pls->vidx[idx + len], &pls->vidx[idx],
		  (pls->len - idx) * sizeof(pls->vidx[0]));

        /* Insert the new elements */
        for (i = 0; i < len; i++) {
                pls->vidx[idx+i] = g_strdup(oids[i]);
        }

        if (pls->shuffled) {
                /* Readjust old references in pidx and iidx */
                for (i = 0; i < pls->len; i++) {
                        if (pls->pidx[i] >= idx) {
                                pls->pidx[i] += len;
                                pls->iidx[pls->pidx[i]] = i;
                        }
                }

                /* Insert the new references in pidx and iidx */
                for (i = 0; i < len; i++) {
                        pls->pidx[pls->len+i] = idx+i;
                        pls->iidx[idx+i] = pls->len+i;
                }
        }

        pls->len += len;

	i_am_dirty(pls);

	return TRUE;
}

/* Insert oid at idx-th position in playlist*/
gboolean pls_insert(Pls *pls, guint idx, const gchar *oid)
{
	const gchar *oids[] = {oid};
	return pls_inserts(pls, idx, oids, 1);
}

/* Append oid:s (len sized) in playlist */
gboolean pls_appends(Pls *pls, const gchar **oid, guint len)
{
	return pls_inserts(pls, pls->len, oid, len);
}

/* Append oid in playlist */
gboolean pls_append(Pls *pls, const gchar *oid)
{
        const gchar *oids[] = {oid};
	return pls_inserts(pls, pls->len, oids, 1);
}

/* Remove idx-th element from playlist. Returns @TRUE if removing was
 * successful */
gboolean pls_remove(Pls *pls, guint idx)
{
	guint opx, i;

	if (idx >= pls->len) {
		return FALSE;
        }

	g_free(pls->vidx[idx]);

	/* Push the rest downwards */
	g_memmove(&pls->vidx[idx], &pls->vidx[idx + 1],
		  (pls->len - idx - 1) * sizeof(pls->vidx[0]));

        if (pls->shuffled) {
                opx = pls->iidx[idx];

                /* Renumber pidxes to reflect removal */
                for (i = 0; i < pls->len; i++) {
                        if (pls->pidx[i] > idx) {
                                pls->pidx[i]--;
                        }
                }

                /* If element was shuffled, push remaining downwards */
                if (opx < pls->poolst) {
                        g_memmove(&pls->pidx[opx], &pls->pidx[opx+1],
                                  (pls->poolst - opx - 1)*sizeof(pls->pidx[0]));

                        /* Adjust pool index */
                        pls->poolst--;
                } else {
                        /* Overwrite removed element with latest */
                        pls->pidx[opx] = pls->pidx[pls->len-1];
                }

                /* Rebuild iidx */
                for (i = 0; i < pls->len-1; i++) {
                        pls->iidx[pls->pidx[i]] = i;
                }
        }

        pls->len--;

	i_am_dirty(pls);

	return TRUE;
}

/* Shuffle playlist */
void pls_shuffle(Pls *pls)
{
        gint i;

        /* If playlist wasn't shuffled, assign room to store pidx and iidx */
        if (!pls->shuffled) {
                pls->pidx = g_new(guint, pls->alloc);
                pls->iidx = g_new(gint, pls->alloc);

                /* Initializes pidx and iidx */
                for (i = 0; i < pls->len; i++) {
                        pls->pidx[i] = pls->iidx[i] = i;
                }
        }

        pls->shuffled = TRUE;
        pls->poolst = 0;

        i_am_dirty(pls);
}

/* Unshuffle playlist */
void pls_unshuffle(Pls *pls)
{
        if (pls->shuffled) {
                pls->shuffled = FALSE;

                /* Free pidx and iidx */
                g_free(pls->pidx);
                g_free(pls->iidx);

                pls->pidx = NULL;
                pls->iidx = NULL;
                i_am_dirty(pls);
        }
}

/* Returns the idx:th clip of the playlist. Also, shuffle it if it is
 * unshuffled */
gchar *pls_get_item(Pls *pls, guint idx)
{
	if (idx >= pls->len) {
		return NULL;
        }

        if (pls->shuffled) {
                /* If element is unshuffled, shuffle it */
                if (pls->iidx[idx] >= pls->poolst) {
                        swap_elements(pls, pls->poolst, pls->iidx[idx]);
                        pls->poolst++;
                }
        }

	return g_strdup(pls->vidx[idx]);
}

/* Returns a chunk of elements from playlist, starting in fidx and ending in
 * lidx (at most) */
gchar **pls_get_items(Pls *pls, guint fidx, guint lidx)
{
	GPtrArray *oidarray = NULL;
	gchar **oids;
	guint i;

        /* Check range */
	if (fidx >= pls->len || lidx < fidx) {
		return NULL;
        }

        /* Adjust upper limit */
	if (lidx > pls->len-1) {
		lidx = pls->len-1;
        }

	oidarray = g_ptr_array_sized_new(lidx - fidx + 2);

        /* Copy chunk playlist */
	for (i=fidx; i <= lidx; i++) {
		g_ptr_array_add(oidarray, pls->vidx[i]);
	}

	g_ptr_array_add(oidarray, NULL);

	oids = (gchar**)oidarray->pdata;
	g_ptr_array_free(oidarray, FALSE);

	return oids;
}

/* Sets the first playable item's visual index, and object id if the list is not
   empty */
void pls_get_starting(Pls *pls, guint *index, gchar **oid)
{
	if (pls->len) {
                if (!pls->shuffled) {
                        *index = 0;
                        *oid = g_strdup(pls->vidx[0]);
                } else {
                        /* If there are no shuffled elements, shuffle one */
                        if (pls->poolst == 0) {
                                shuffle_elements(pls, 1);
                        }
                        *index = pls->pidx[0];
                        *oid = g_strdup(pls->vidx[*index]);
                }
        }
}

/* Sets the last playable item's visual index, and object id if the list is not
   empty */
void pls_get_last(Pls *pls, guint *index, gchar **oid)
{
	if (pls->len) {
                if (!pls->shuffled) {
                        *index = pls->len-1;
                        *oid = g_strdup(pls->vidx[pls->len-1]);
                } else {
                        /* Need to shuffle all elements */
                        shuffle_elements(pls, pls->len);
                        *index = pls->pidx[pls->len-1];
                        *oid = g_strdup(pls->vidx[*index]);
                }
	}
}


/* Sets the next playable item's visual index, and object id if there is any,
   according to the repeat setting. Returns @TRUE if clip found. */
gboolean pls_get_next(Pls *pls, guint *index, gchar **oid)
{
        /* Check range */
        if (*index >= pls->len) {
                return FALSE;
        }

        if (!pls->shuffled) {
                /* If current clip is the last, but repeat is on, returns the
                 * first */
                if (*index < pls->len-1) {
                        (*index)++;
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                } else if (pls->repeat) {
                        *index = 0;
                        *oid = g_strdup(pls->vidx[0]);
                        return TRUE;
                } else {
                        /* Out of range */
                        return FALSE;
                }
        } else {
                /* Is the next element still shuffled? */
                if ((pls->iidx[*index]+1) < pls->poolst) {
                        *index = pls->pidx[pls->iidx[*index]+1];
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                }

                /* Is the element unshuffled? If so, shuffle it, and continue */
                if (pls->iidx[*index] >= pls->poolst) {
                        swap_elements(pls, pls->poolst, pls->iidx[*index]);
                        pls->poolst++;
                }

                /* Shuffle a new element, if available. Else, if repeat is on
                 * then use the first one */
                if (pls->poolst < pls->len) {
                        shuffle_elements(pls, 1);
                        *index = pls->pidx[pls->poolst-1];
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                } else if (pls->repeat) {
                        *index = pls->pidx[0];
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                } else {
                        /* No more elements */
                        return FALSE;
                }
        }
}

/* Sets the previous playable item's visual index, and object id if there is
   any, according to the repeat setting. Returns @TRUE if clip is found */
gboolean pls_get_prev(Pls *pls, guint *index, gchar **oid)
{
        /* Check range */
        if (*index >= pls->len) {
                return FALSE;
        }

        if (!pls->shuffled) {
                /* if current clip is the first, but repeat is on, returns the
                 * last */
                if (*index > 0) {
                        (*index)--;
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                } else if (pls->repeat) {
                        *index = pls->len-1;
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                } else {
                        /* No prev */
                        return FALSE;
                }
        } else {
                /* Is the element unshuffled? If so, shuffle it and continue */
                if (pls->iidx[*index] >= pls->poolst) {
                        swap_elements(pls, pls->poolst, pls->iidx[*index]);
                        pls->poolst++;
                }

                /* Is there a previous element? */
                if (pls->iidx[*index] > 0) {
                        *index=pls->pidx[pls->iidx[*index]-1];
                        *oid = g_strdup(pls->vidx[*index]);
                        return TRUE;
                }

                /* Current element is the first playable; there is no prev
                   unless repeat is on. NOTE: pls_get_last shuffle all elements
                   in pool */
                if (pls->repeat) {
                        pls_get_last(pls, index, oid);
                        return TRUE;
                } else {
                        return FALSE;
                }
        }
}

/* @TRUE if playlist is shuffled */
gboolean pls_is_shuffled(Pls *pls)
{
	return pls->shuffled;
}

/* Change repeat mode */
void pls_set_repeat(Pls *pls, gboolean repeat)
{
	pls->repeat = repeat;
	i_am_dirty(pls);
}

/* Change the refcount */
void pls_set_use_count(Pls *pls, guint use_count)
{
	pls->use_count = use_count;
	i_am_dirty(pls);
}

/* Moves a clip from "from" to "to" */
gboolean pls_move(Pls *pls, guint from, guint to)
{
        gchar *aoid;
        guint mdest, msrc, mlen;

        if (from == to)
                return TRUE;
        /* XXX: this could clamp at pls->len... */
        if (from >= pls->len || to >= pls->len) {
                return FALSE;
        }
/*
 * Move keeps (vidx, pidx) pairs, and only moves object ids around:
 *
 * V P O   V P O
 * 0 2 a   0 2 a
 * 1 3 b   1 3 c
 * 2 0 c   2 0 d
 * 3 1 d   3 1 b
 * 4 4 e   4 4 e
 *
 *    1 -> 3
 */
        if (from < to) {
                mdest = from;
                msrc = from + 1;
                mlen = to - from;
        } else {
                mdest = to + 1;
                msrc = to;
                mlen = from - to;
        }

        aoid = pls->vidx[from];
        g_memmove(&pls->vidx[mdest], &pls->vidx[msrc],
                  mlen * sizeof(pls->vidx[0]));
        pls->vidx[to] = aoid;

	i_am_dirty(pls);
	return TRUE;
}

/* Key compare function (GCompareDataFunc).  keys are ids. */
gint pls_cmpids(gconstpointer a, gconstpointer b, gpointer unused)
{
	if (a < b)
		return -1;
	else if (a == b)
		return 0;
	else
		return 1;
}

/* Playlists are saved in flat text files.  First there is a five line header,
 * consisting of:
 *
 * version: "V" + an integer (last version is 2)
 * id: integer > 0
 * name: string, everything until newline
 * repeat: integer, 0 or 1
 * shuffle: integer, 0 or 1
 * length: integer > 0
 * pool start: integer > 0 && <= length
 *
 * Then items follow, one per line:
 *
 * playing index,uuid
 *
 * where pidx is a non-negative integer, equivalent to pidx[n] of the in-core
 * Pls structure, and uuid is a string lasting till the end of the line.
 */
gboolean pls_save(Pls *pls, const gchar *fn)
{
	FILE *f;
	guint i;
	gchar *tmpf;
	gboolean isok, tmpok;

	/* First write the playlist into a temporary file, then move it over
	 * the requested filename. */
	tmpok = isok = FALSE;
	tmpf = g_strdup_printf("%s.tmp", fn);
	if (!(f = fopen(tmpf, "w+"))) {
		goto out1;
        }

	if (fprintf(f,
		    "V" APLAYLIST_VERSION "\n"
		    "%u\n"
		    "%s\n"
		    "%d\n"
                    "%d\n"
		    "%u\n"
                    "%u\n",
		    pls->id,
		    pls->name,
		    pls->repeat,
		    pls->shuffled,
		    pls->len,
                    pls->poolst) < 0) {
		goto out2;
        }

        if (pls->shuffled) {
                for (i = 0; i < pls->len; ++i) {
                        if (fprintf(f, "%u,%s\n",
                                    pls->pidx[i], pls->vidx[i]) < 0) {
                                goto out2;
                        }
                }
        } else {
                for (i = 0; i < pls->len; ++i) {
                        if (fprintf(f, "%u,%s\n", i, pls->vidx[i]) < 0) {
                                goto out2;
                        }
                }
        }
	/* Try to minimize data loss. */
	fflush(f);
	fsync(fileno(f));
	if (fclose(f) != 0) {
		goto out2;
        }

	/* tmpok == .tmp file written */
	tmpok = TRUE;
	if (rename(tmpf, fn) == -1) {
		g_critical("rename('%s','%s') failed: %s", tmpf, fn,
			   g_strerror(errno));
		goto out2;
	}

	/* XXX: we might need to fsync() the fd of the containing
	 * directory... See fsync(2). */
	isok = TRUE;

out2:	if (!tmpok) {
		if (unlink(tmpf) == -1) {
			g_warning("unlink: %s", g_strerror(errno));
                }
	}

out1:	g_free(tmpf);
	return isok;
}

/* Similar to fgets(), but chops the optional trailing newline. */
static char *fgetsnl(char *buf, int size, FILE *f)
{
	char *b;
	int l;

	b = fgets(buf, size, f);
	if (!b)
		return NULL;
	l = strlen(b);
	if (b[l-1] == '\n')
		b[l-1] = '\0';
	return b;
}

Pls *pls_load(const gchar *fn)
{
	/* @buf makes this non-reentrant.  We don't allow more than 2k long
	 * lines.  buf[2048] is '\0', we don't  it up. */
	static char buf[2048+1];
	Pls *p;
	FILE *f;
	gint version, id, repeat, shuffled, len, poolst;
	gchar *name;
	guint i;

	p = NULL;
	name = NULL;
	if (!(f = fopen(fn, "r"))) {
		return NULL;
        }

	/* Need to use fgets() because '\n' in fscanf() matches arbitrary
	 * amount of whitespace. */

        /* Read version */
	if (!fgetsnl(buf, sizeof(buf), f) ||
	    sscanf(buf, "V%d", &version) != 1 ||
            version < 0) {
		goto out1;
        }

	/* Latest version is 2, though this function is able to manage v1 too.
	 * If format changes in the future, you'll need to change this code, and
	 * care about backward compatibility. */
	if (version != 1 && version != 2) {
		goto out1;
        }

        /* Read id */
	if (!fgetsnl(buf, sizeof(buf), f) ||
	    sscanf(buf, "%d", &id) != 1 ||
	    id < 0) {
		goto out1;
        }

        /* Read name */
	if (!fgetsnl(buf, sizeof(buf), f)) {
		goto out1;
        }
	name = strdup(buf);

        /* Read repeat mode */
	if (!fgetsnl(buf, sizeof(buf), f) ||
	    (sscanf(buf, "%d", &repeat) != 1 &&
	     (repeat != 0 || repeat != 1))) {
		goto out2;
        }

        /* Read shuffle mode */
	if (!fgetsnl(buf, sizeof(buf), f) ||
	    (sscanf(buf, "%d", &shuffled) != 1 &&
	     (shuffled != 0 || shuffled != 1))) {
		goto out2;
        }

        /* Read length */
	if (!fgetsnl(buf, sizeof(buf), f) ||
	    sscanf(buf, "%d", &len) != 1 ||
	    len < 0) {
		goto out2;
        }

        /* Read pool start; version >=2 */
        if (version == 2) {
                if (!fgetsnl(buf, sizeof(buf), f) ||
                    sscanf(buf, "%d", &poolst) != 1 ||
                    poolst < 0) {
                        goto out2;
                }
        } else {
                /* All elements are already shuffled */
                poolst = len;

        }

	p = pls_new(id, name);
	p->repeat = repeat;
	p->shuffled = shuffled;
        p->poolst = poolst;
	maybe_realloc(p, len);

        /* Read entries */
        for (i = 0; i < len; ++i) {
                guint pidx;
                gchar *oid;

                oid = NULL;
                /* We do sanity check on pidx. */
                if (!fgetsnl(buf, sizeof(buf), f) ||
                    sscanf(buf, "%u,%a[^\n]", &pidx, &oid) != 2 ||
                    pidx >= len) {
                        if (oid) {
                                free(oid);
                        }

                        pls_free(p);
                        p = NULL;
                        goto out2;
                }

                p->vidx[i] = oid;

                if (p->shuffled) {
                        p->pidx[i] = pidx;
                        p->iidx[pidx] = i;
                }
        }

	/* We don't really want to detect if the file has more items than
	 * $len... */
	p->len = i;

out2:   free(name);

out1:   fclose(f);

	return p;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
