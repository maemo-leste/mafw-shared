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
#include "config.h"
#endif

#include <glib.h>

#include "mafw-util.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-util"

/**
 * mafw_util_array_to_glist:
 * @array: a %NULL terminated array of pointers.
 *
 * Converts a %NULL-terminated pointer array to a #GList.
 *
 * Returns: the #GList, owned by the caller.
 */
GList *mafw_util_array_to_glist(void **array)
{
	GList *r;

	r = NULL;
	while (*array)
		r = g_list_append(r, *array++);
	return r;
}

/**
 * mafw_util_array_to_glist_n:
 * @array: a pointer array.
 * @length: the length of the array.
 *
 * Converts a pointer array to a #GList.
 *
 * Returns: the #GList, owned by the caller.
 */
GList *mafw_util_array_to_glist_n(void **array, guint length)
{
	GList *r;
	int i;

	for (r = NULL, i = 0; i < length; ++i)
		r = g_list_append(r, array[i]);
	return r;
}

/**
 * mafw_util_array_to_glist_v:
 * @first:
 * @...: pointer arguments, terminated by %NULL.
 *
 * Converts a variable length list of pointers to a #GList.
 *
 * Returns: the #GList, owned by the caller.
 */
GList *mafw_util_array_to_glist_v(void *first, ...)
{
	GList *r;
	va_list args;
	void *p;

	r = NULL;
	va_start(args, first);
	while ((p = va_arg(args, void *)))
		r = g_list_append(r, p);
	va_end(args);
	return r;
}

/**
 * mafw_util_glist_to_array:
 * @list: a #GList.
 * @length: a location to return length of resulting array.
 *
 * Converts a #GList to a pointer array.
 *
 * Returns: a pointer array, owned by the caller, or %NULL
 *          if list length is 0.
 */
void **mafw_util_glist_to_array(GList *list, guint *length)
{
 	void **arr, **t;

	*length = g_list_length(list);
	if (!*length) return NULL;
	t = arr = g_new(void *, *length);
	for (; list; list = list->next)
		*t++ = list->data;
	return arr;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
