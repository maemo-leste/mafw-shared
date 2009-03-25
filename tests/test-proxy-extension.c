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
#include <glib.h>
#include <check.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libmafw/mafw.h>

#include "libmafw-shared/mafw-proxy-source.h"
#include "common/mafw-util.h"
#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"
#include <libmafw/mafw-metadata-serializer.h>

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

#define UUID "EXTENSION"
/* For mafw_dbus_*() */
#define MAFW_DBUS_PATH MAFW_SOURCE_OBJECT "/" UUID
#define MAFW_DBUS_DESTINATION MAFW_SOURCE_SERVICE ".fake." UUID
#define MAFW_DBUS_INTERFACE MAFW_EXTENSION_INTERFACE

static GType fsrc_get_type(void);
typedef struct { MafwSourceClass parent; } FsrcClass;
typedef struct { MafwSource parent; } Fsrc;
G_DEFINE_TYPE(Fsrc, fsrc, MAFW_TYPE_SOURCE);

static void fsrc_class_init(FsrcClass *x)
{
}

static void fsrc_init(Fsrc *y)
{
	mafw_extension_add_property(MAFW_EXTENSION(y), "testp", G_TYPE_DOUBLE);
}


/* Proxy does a list_properties at _connect().  Expect that. */
static void mock_empty_props(void)
{
	mockbus_expect(mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					MAFW_DBUS_PATH,
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING("TestName"));
	mockbus_expect(mafw_dbus_method(MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_reply(MAFW_DBUS_STRVZ(NULL), MAFW_DBUS_C_ARRAY(UINT32, guint));
}

START_TEST(test_extension_properties_list)
{
	MafwExtension *extension;
	const GPtrArray *props;

	mockbus_reset();
	mockbus_expect(mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					MAFW_DBUS_PATH,
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING("TestName"));
	/* Beware: proxy-extension tries to be clever and asks the property list only
	 * once.  That means we don't mock_emtpy_props() here. */
	mockbus_expect(
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_reply(MAFW_DBUS_C_STRVZ("zidane", "cigany"),
		      MAFW_DBUS_C_ARRAY(UINT32, guint32,
					G_TYPE_INT, G_TYPE_DOUBLE));

	extension = MAFW_EXTENSION(mafw_proxy_source_new(UUID, "fake", mafw_registry_get_instance()));
	props = mafw_extension_list_properties(extension);
	fail_unless(props->len == 2);
	fail_if(strcmp(((MafwExtensionProperty *)(props->pdata[0]))->name, "zidane"));
	fail_unless(((MafwExtensionProperty *)(props->pdata[0]))->type == G_TYPE_INT);
	fail_if(strcmp(((MafwExtensionProperty *)(props->pdata[1]))->name, "cigany"));
	fail_unless(((MafwExtensionProperty *)(props->pdata[1]))->type == G_TYPE_DOUBLE);
	mockbus_finish();
}
END_TEST

static void got_extension_prop(MafwExtension *extension, const gchar *prop,
			  GValue *val, gpointer udata, const GError *err)
{
	fail_if(strcmp(prop, "zidane"));
	fail_unless(G_VALUE_TYPE(val) == G_TYPE_INT);
	fail_unless(g_value_get_int(val) == 12345);
	fail_unless(err == NULL);
	g_value_unset(val);
	g_free(val);
}

START_TEST(test_extension_properties_get_set)
{
	MafwExtension *extension;
	GValue v = {0};

	mockbus_reset();
	mockbus_expect(mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					MAFW_DBUS_PATH,
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING("TestName"));
	mockbus_expect(
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_reply(MAFW_DBUS_C_STRVZ("zidane", "cigany"),
		      MAFW_DBUS_C_ARRAY(UINT32, guint32,
					G_TYPE_INT, G_TYPE_DOUBLE));

	extension = MAFW_EXTENSION(mafw_proxy_source_new(UUID, "fake", mafw_registry_get_instance()));
	g_value_init(&v, G_TYPE_INT);
	g_value_set_int(&v, 12345);
	mockbus_expect(
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING("zidane"),
				      MAFW_DBUS_GVALUE(&v)));
	mafw_extension_set_property_int(extension, "zidane", 12345);
	mockbus_expect(mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
					     MAFW_EXTENSION_INTERFACE,
					     MAFW_EXTENSION_METHOD_GET_PROPERTY,
					     MAFW_DBUS_STRING("zidane")));
	mockbus_reply(MAFW_DBUS_STRING("zidane"),
		      MAFW_DBUS_GVALUE(&v));
	mafw_extension_get_property(extension, "zidane",
				     got_extension_prop, NULL);
	mockbus_finish();
}
END_TEST

START_TEST(test_gee_properties)
{
	GValue v = {0};
	gpointer src;
	

	mockbus_reset();
	mock_empty_props();
	src = mafw_proxy_source_new(UUID, "fake", mafw_registry_get_instance());

	g_value_init(&v, G_TYPE_STRING);
	g_value_set_static_string(&v, "moso masa");
	mockbus_expect(mafw_dbus_method(MAFW_EXTENSION_METHOD_SET_NAME,
					MAFW_DBUS_STRING("moso masa")));
	g_object_set(src, "name", "moso masa", NULL);
	
	mockbus_finish();
}
END_TEST

START_TEST(test_errors)
{
	gpointer src;

	mockbus_reset();
	mockbus_expect(mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					MAFW_DBUS_PATH,
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_error(MAFW_EXTENSION_ERROR, 1, "testproblem");
	mockbus_expect(
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_error(MAFW_EXTENSION_ERROR, 2, "testproblem");
	src = mafw_proxy_source_new(UUID, "fake", mafw_registry_get_instance());
	mockbus_expect(
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_error(MAFW_EXTENSION_ERROR, 3, "testproblem");
	
	fail_if(mafw_extension_list_properties(src));
	
	mockbus_expect(
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_reply(MAFW_DBUS_C_STRVZ("zidane", "cigany"),
		      MAFW_DBUS_C_ARRAY(UINT32, guint32,
					G_TYPE_INT, G_TYPE_DOUBLE));
	
	fail_if(!mafw_extension_list_properties(src));

	mockbus_finish();
}
END_TEST

static void name_changed_cb(MafwExtension *extension, GParamSpec *pspec,
				gboolean *cb_called)
{
	fail_if(*cb_called);
	*cb_called = TRUE;
}

static void error_cb(MafwExtension *extension,
	   GQuark domain, gint code, const gchar *message,
	   gboolean *cb_called)
{
	fail_if(*cb_called);
	*cb_called = TRUE;
	fail_if(strcmp(message, "message"));
	fail_if(code != 3);
}

static void prop_changed_cb(MafwExtension *extension,
			 const gchar *prop, const GValue *val,
			 gboolean *cb_called)
{
	fail_if(*cb_called);
	*cb_called = TRUE;
	fail_if(strcmp(prop, "testprop"));
	fail_if(g_value_get_int(val) != 2);
}

START_TEST(test_signals)
{
	gpointer src;
	GValue val = {0, };
	gboolean cb_called = FALSE;
	
	g_value_init(&val, G_TYPE_INT);
	g_value_set_int(&val, 2);

	mockbus_reset();
	mock_empty_props();
	src = mafw_proxy_source_new(UUID, "fake", mafw_registry_get_instance());
	
	g_signal_connect(src, "notify::name", (GCallback)name_changed_cb, &cb_called);
	g_signal_connect(src, "error", (GCallback)error_cb, &cb_called);
	g_signal_connect(src, "property-changed", (GCallback)prop_changed_cb, &cb_called);

	mockbus_incoming(mafw_dbus_signal(MAFW_EXTENSION_SIGNAL_PROPERTY_CHANGED,
					  MAFW_DBUS_STRING("testprop"),
					MAFW_DBUS_GVALUE(&val)));
	mockbus_deliver(mafw_dbus_session(NULL));
	mockbus_deliver(mafw_dbus_session(NULL));
	fail_if(!cb_called);
	cb_called = FALSE;
	
	mockbus_incoming(mafw_dbus_signal(MAFW_EXTENSION_SIGNAL_ERROR,
					MAFW_DBUS_STRING("domain_str"),
					MAFW_DBUS_INT32(3),
					MAFW_DBUS_STRING("message")));
	mockbus_deliver(mafw_dbus_session(NULL));
	fail_if(!cb_called);
	cb_called = FALSE;

	mockbus_incoming(mafw_dbus_signal(MAFW_EXTENSION_SIGNAL_NAME_CHANGED,
					MAFW_DBUS_STRING("TestName")));
	mockbus_deliver(mafw_dbus_session(NULL));
	fail_if(!cb_called);
	cb_called = FALSE;

	mockbus_finish();
}
END_TEST

static gboolean report_props(gpointer data)
{
	mockbus_reply(MAFW_DBUS_STRVZ(NULL), MAFW_DBUS_C_ARRAY(UINT32, guint));
	checkmore_stop_loop();
	mockbus_send_stored_reply();
	return FALSE;
}

START_TEST(test_exists)
{
	gpointer src;
	gpointer src2;
	
	src = g_object_new(fsrc_get_type(),
			    "uuid", UUID,
			    NULL);
	mafw_registry_add_extension(mafw_registry_get_instance(), src);
	mockbus_expect(mafw_dbus_method_full(
					MAFW_DBUS_DESTINATION,
					MAFW_DBUS_PATH,
					MAFW_EXTENSION_INTERFACE,
					MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING("TestName"));
	mockbus_expect(mafw_dbus_method(MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	src2 = mafw_proxy_source_new(UUID, "fake", mafw_registry_get_instance());
	g_object_ref(src2);
	g_idle_add(report_props, NULL);
	checkmore_spin_loop(100);
	fail_if(G_OBJECT(src2)->ref_count != 1);
	
	g_object_unref(src2);
}
END_TEST

int main(void)
{
	Suite *suite;

	suite = suite_create("MafwExtensionProxy");
if (1)	checkmore_add_tcase(suite, "Extension properties: list",
			    test_extension_properties_list);
if (1)	checkmore_add_tcase(suite, "Extension properties: get/set",
			    test_extension_properties_get_set);
if (1)	checkmore_add_tcase(suite, "Gee properties",
			    test_gee_properties);
if (1)	checkmore_add_tcase(suite, "Error cases",
			    test_errors);
if (1)	checkmore_add_tcase(suite, "Signals",
			    test_signals);
if (1)	checkmore_add_tcase(suite, "Source exists",
			    test_exists);

	return checkmore_run(srunner_create(suite), FALSE);
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
