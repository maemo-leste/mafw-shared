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

#include <glib.h>

#include <checkmore.h>
#include "common/mafw-util.h"

START_TEST(test_a2l)
{
	void *arr[] = { GINT_TO_POINTER(1), GINT_TO_POINTER(2),
			GINT_TO_POINTER(3), GINT_TO_POINTER(4), 0 };
	GList *l;

	l = mafw_util_array_to_glist((void **)arr);
	ck_assert(g_list_length(l) == 4);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 0)) == 1);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 1)) == 2);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 2)) == 3);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 3)) == 4);
	g_list_free(l);

	l = mafw_util_array_to_glist_n((void **)arr, 4);
	ck_assert(g_list_length(l) == 4);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 0)) == 1);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 1)) == 2);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 2)) == 3);
	ck_assert(GPOINTER_TO_INT(g_list_nth_data(l, 3)) == 4);
	g_list_free(l);

	/* Test with zero length. */
	l = mafw_util_array_to_glist_n((void **)arr, 0);
	ck_assert(g_list_length(l) == 0);
	ck_assert(l == NULL);
	g_list_free(l);
}
END_TEST

START_TEST(test_l2a)
{
	GList *l;
	void **arr;
	guint len;

	l = NULL;
	l = g_list_append(l, GINT_TO_POINTER(333));
	l = g_list_append(l, GINT_TO_POINTER(444));
	arr = mafw_util_glist_to_array(l, &len);
	ck_assert(len == 2);
	ck_assert(GPOINTER_TO_INT(arr[0]) == 333);
	ck_assert(GPOINTER_TO_INT(arr[1]) == 444);
	g_free(arr);
	g_list_free(l);
}
END_TEST

int main(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("mafw-util");
	tc = checkmore_add_tcase(suite, "List handling", test_a2l);
	tcase_add_test(tc, test_l2a);
	return checkmore_run(srunner_create(suite), TRUE);
}
