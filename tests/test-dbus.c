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

#include <check.h>

#include <checkmore.h>
#include "common/mafw-dbus.h"

/* Check that mafw-dbus accepts zero-element ASTs. */
START_TEST(test_ast_zero)
{
	DBusMessage *msg;
	DBusMessageIter imsg, iary;

	msg = mafw_dbus_signal_full("a.b", "/a/b", "a.b", "ab",
		MAFW_DBUS_AST("us"));
	fail_unless(!strcmp(dbus_message_get_signature(msg), "a(us)"));

	dbus_message_iter_init(msg, &imsg);
	fail_if(dbus_message_iter_get_arg_type(&imsg) != DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);
	fail_if(dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_INVALID);
	fail_if(dbus_message_iter_has_next(&imsg));
	dbus_message_unref(msg);
}
END_TEST

/* Check the creation of ordinary multiple-element ASTs. */
START_TEST(test_ast)
{
	guint i;
	const gchar *str;
	DBusMessage *msg;
	DBusMessageIter imsg, iary, istr;

	msg = mafw_dbus_signal_full("a.b", "/a/b", "a.b", "ab",
		MAFW_DBUS_AST("us",
			MAFW_DBUS_STRUCT(MAFW_DBUS_UINT32(10),
					 MAFW_DBUS_STRING("alpha")),
			MAFW_DBUS_STRUCT(MAFW_DBUS_UINT32(30),
					 MAFW_DBUS_STRING("gamma")),
			MAFW_DBUS_STRUCT(MAFW_DBUS_UINT32(40),
					 MAFW_DBUS_STRING("delta"))));
	fail_unless(!strcmp(dbus_message_get_signature(msg), "a(us)"));

	dbus_message_iter_init(msg, &imsg);
	fail_if(dbus_message_iter_get_arg_type(&imsg) != DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);

	/* First elements */
	fail_if(dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_STRUCT);
	dbus_message_iter_recurse(&iary, &istr);
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_UINT32);
	dbus_message_iter_get_basic(&istr, &i);
	fail_if(i != 10);
	fail_if(!dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_STRING);
	dbus_message_iter_get_basic(&istr, &str);
	fail_unless(!strcmp(str, "alpha"));
	fail_if(dbus_message_iter_next(&istr));
	fail_if(!dbus_message_iter_next(&iary));

	/* Seconds element */
	fail_if(dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_STRUCT);
	dbus_message_iter_recurse(&iary, &istr);
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_UINT32);
	dbus_message_iter_get_basic(&istr, &i);
	fail_if(i != 30);
	fail_if(!dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_STRING);
	dbus_message_iter_get_basic(&istr, &str);
	fail_unless(!strcmp(str, "gamma"));
	fail_if(dbus_message_iter_next(&istr));
	fail_if(!dbus_message_iter_next(&iary));

	/* Third element */
	fail_if(dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_STRUCT);
	dbus_message_iter_recurse(&iary, &istr);
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_UINT32);
	dbus_message_iter_get_basic(&istr, &i);
	fail_if(i != 40);
	fail_if(!dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_STRING);
	dbus_message_iter_get_basic(&istr, &str);
	fail_unless(!strcmp(str, "delta"));
	fail_if(dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_next(&iary));
	fail_if(dbus_message_iter_next(&imsg));

	dbus_message_unref(msg);
}
END_TEST

/* Check that ordinary arguments around ASTs are preserved.
 * Also tests MAFW_DBUS_SAVEPOINT for basic functionality. */
START_TEST(test_ast_more)
{
	double d;
	gboolean is;
	DBusMessage *msg;
	DBusMessageIter imsg, iary;

	msg = mafw_dbus_signal_full("a.b", "/a/b", "a.b", "ab",
		MAFW_DBUS_BOOLEAN(FALSE),
		MAFW_DBUS_AST("us",
			MAFW_DBUS_STRUCT(MAFW_DBUS_UINT32(10),
					 MAFW_DBUS_STRING("alpha"))),
		MAFW_DBUS_DOUBLE(3.42));
	fail_unless(!strcmp(dbus_message_get_signature(msg), "ba(us)d"));

	mafw_dbus_parse(msg,
		       	DBUS_TYPE_BOOLEAN,	&is,
			MAFW_DBUS_SAVEPOINT(&imsg), MAFW_DBUS_TYPE_IGNORE,
		       	DBUS_TYPE_DOUBLE,	&d);
	fail_if(is != FALSE);
	fail_if(d != 3.42);

	/* Verify that $imsg points to the AST and is fully usable. */
	fail_if(dbus_message_iter_get_arg_type(&imsg) != DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);
	fail_if(dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_STRUCT);
	fail_if(!dbus_message_iter_next(&imsg));
	fail_if(dbus_message_iter_get_arg_type(&imsg) != DBUS_TYPE_DOUBLE);

	dbus_message_unref(msg);
}
END_TEST

/* Check that ASTs can be embedded recursively. */
START_TEST(test_ast_recursive)
{
	double d;
	gboolean is;
	DBusMessage *msg;
	DBusMessageIter imsg, iary, istr, iary2, istr2;

	msg = mafw_dbus_signal_full("a.b", "/a/b", "a.b", "ab",
		DBUS_TYPE_ARRAY, DBUS_TYPE_STRUCT, "(ua(bd)s)",
			MAFW_DBUS_STRUCT(
				MAFW_DBUS_UINT32(10),
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRUCT, "(bd)",
					MAFW_DBUS_STRUCT(
						MAFW_DBUS_BOOLEAN(FALSE),
						MAFW_DBUS_DOUBLE(3.42)),
					DBUS_TYPE_INVALID,
				MAFW_DBUS_STRING("alpha")),
			DBUS_TYPE_INVALID);
	fail_unless(!strcmp(dbus_message_get_signature(msg), "a(ua(bd)s)"));

	dbus_message_iter_init(msg, &imsg);
	fail_if(dbus_message_iter_get_arg_type(&imsg) != DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&imsg, &iary);

	/* First field */
	fail_if(dbus_message_iter_get_arg_type(&iary) != DBUS_TYPE_STRUCT);
	dbus_message_iter_recurse(&iary, &istr);
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_UINT32);

	/* Second field -- sub-AST */
	fail_if(!dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_ARRAY);
	dbus_message_iter_recurse(&istr, &iary2);
	fail_if(dbus_message_iter_get_arg_type(&iary2) != DBUS_TYPE_STRUCT);
	dbus_message_iter_recurse(&iary2, &istr2);
	fail_if(dbus_message_iter_get_arg_type(&istr2) != DBUS_TYPE_BOOLEAN);
	dbus_message_iter_get_basic(&istr2, &is);
	fail_if(is != FALSE);
	fail_if(!dbus_message_iter_next(&istr2));
	fail_if(dbus_message_iter_get_arg_type(&istr2) != DBUS_TYPE_DOUBLE);
	dbus_message_iter_get_basic(&istr2, &d);
	fail_if(d != 3.42);
	fail_if(dbus_message_iter_next(&istr2));
	fail_if(dbus_message_iter_next(&iary2));

	/* Third field */
	fail_if(!dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_STRING);
	fail_if(dbus_message_iter_next(&istr));
	fail_if(dbus_message_iter_next(&iary));
	fail_if(dbus_message_iter_next(&imsg));
	dbus_message_unref(msg);
}
END_TEST

/* Test MAFW_DBUS_SAVEPOINT() at the end of args.
 * Also tests MAFW_DBUS_TYPE_IGNORE. */
START_TEST(test_savepoint)
{
	DBusMessage *msg;
	DBusMessageIter imsg;

	msg = mafw_dbus_signal_full("a.b", "/a/b", "a.b", "ab",
		MAFW_DBUS_UINT32(10),
		MAFW_DBUS_STRING("alpha"));
	mafw_dbus_parse(msg,
			MAFW_DBUS_TYPE_IGNORE,
			MAFW_DBUS_TYPE_IGNORE,
		       	MAFW_DBUS_SAVEPOINT(&imsg));

	/* Verify that $imsg points to the AST and is fully usable. */
	fail_unless(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_INVALID);
	dbus_message_unref(msg);
}
END_TEST

START_TEST(test_gvaluearray)
{
	DBusMessage *msg;
	GValueArray *varr1, *varr2;
	GValue v = {0};
	DBusMessageIter imsg, ivar;
	guint nelem;
	gint val;

	varr1 = g_value_array_new(2);
	g_value_init(&v, G_TYPE_INT);
	g_value_set_int(&v, 43);
	g_value_array_append(varr1, &v);
	g_value_set_int(&v, 88);
	g_value_array_append(varr1, &v);
	g_value_unset(&v);

	/* I. Marshalling */
	msg = mafw_dbus_signal_full("a.b", "/a/b", "a.b", "ab",
				    MAFW_DBUS_GVALUEARRAY(varr1));

	/* Number of elements (GValues) in the array. */
	dbus_message_iter_init(msg, &imsg);
	fail_unless(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_UINT32);
	dbus_message_iter_get_basic(&imsg, &nelem);
	fail_unless(nelem == 2);

	/* $nelem times GValue. */
	fail_unless(dbus_message_iter_next(&imsg));
	fail_unless(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_VARIANT);
	dbus_message_iter_recurse(&imsg, &ivar);
	fail_unless(dbus_message_iter_get_arg_type(&ivar) == DBUS_TYPE_INT32);
	dbus_message_iter_get_basic(&ivar, &val);
	fail_unless(val == 43);
	fail_if(dbus_message_iter_has_next(&ivar));

	fail_unless(dbus_message_iter_next(&imsg));
	fail_unless(dbus_message_iter_get_arg_type(&imsg) == DBUS_TYPE_VARIANT);
	dbus_message_iter_recurse(&imsg, &ivar);
	fail_unless(dbus_message_iter_get_arg_type(&ivar) == DBUS_TYPE_INT32);
	dbus_message_iter_get_basic(&ivar, &val);
	fail_unless(val == 88);
	fail_if(dbus_message_iter_has_next(&ivar));

	fail_if(dbus_message_iter_has_next(&imsg));

	/* II. Parsing */
	mafw_dbus_parse(msg, MAFW_DBUS_TYPE_GVALUEARRAY, &varr2);
	fail_unless(varr2->n_values == 2);
	fail_unless(g_value_get_int(&varr2->values[0]) == 43);
	fail_unless(g_value_get_int(&varr2->values[1]) == 88);

	g_value_array_free(varr1);
	g_value_array_free(varr2);
	dbus_message_unref(msg);
}
END_TEST

int main(void)
{
	TCase *tcase;
	Suite *suite;

	suite = suite_create("mafw-dbus");
	tcase = tcase_create("Array of structs");
	suite_add_tcase(suite, tcase);
	tcase_add_test(tcase, test_ast_zero);
	tcase_add_test(tcase, test_ast);
	tcase_add_test(tcase, test_ast_more);
	tcase_add_test(tcase, test_ast_recursive);

	tcase = tcase_create("mafw-dbus types");
	suite_add_tcase(suite, tcase);
	tcase_add_test(tcase, test_gvaluearray);

	tcase = tcase_create("Misc");
	suite_add_tcase(suite, tcase);
	tcase_add_test(tcase, test_savepoint);

	return checkmore_run(srunner_create(suite), FALSE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
