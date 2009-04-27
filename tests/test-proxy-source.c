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

#include <assert.h>
#include <string.h>
#include <signal.h>

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
#include "mocksource.h"

/* Set to 1 to get extra messages. */
#if 0
# define VERBOSE	1
# define info		g_debug
#else
# define VERBOSE	0
# define info(...)	/* */
#endif


#define SOURCE_UUID "mocksource"

/* For mafw_dbus_*() */
#define MAFW_DBUS_PATH MAFW_SOURCE_OBJECT "/" SOURCE_UUID
#define MAFW_DBUS_DESTINATION MAFW_SOURCE_SERVICE ".fake." SOURCE_UUID
#define MAFW_DBUS_INTERFACE MAFW_SOURCE_INTERFACE

static gboolean browse_result_ok = FALSE;
static GMainLoop *mainloop_test = NULL;

static void browse_result(MafwSource * self, guint browse_id,
			  gint remaining_count, guint index,
			  const gchar *object_id, GHashTable *metadatas,
			  gpointer user_data, const GError *error)
{
	info("Browse result signal: browse_id: %d, remaining_count: %d, "
	     "index: %d, obj_id: %s\n", browse_id, remaining_count,
	     index, object_id);

	fail_if(browse_id != 4444);
	fail_if(remaining_count != -1);
	fail_if(index != 0);
	fail_if (strcmp(object_id, "testobject"));
	fail_if (!metadatas);
	fail_if (error);
	browse_result_ok = TRUE;

	g_main_loop_quit(mainloop_test);
}

static void browse_error_result(MafwSource * self, guint browse_id,
			  gint remaining_count, guint index,
			  const gchar *object_id, GHashTable *metadatas,
			  gpointer user_data, const GError *error)
{
	info("Browse result signal: browse_id: %d, remaining_count: %d, "
	     "index: %d, obj_id: %s\n", browse_id, remaining_count,
	     index, object_id);

	fail_if(browse_id != MAFW_SOURCE_INVALID_BROWSE_ID);
	fail_if(remaining_count != 0);
	fail_if(index != 0);
	fail_if (object_id != NULL);
	fail_if (metadatas);
	fail_if (!error);
	browse_result_ok = TRUE;
}

static gboolean quit_main_gugufoo(gpointer unused)
{
	g_main_loop_quit(mainloop_test);
	return FALSE;
}

static void browse_result2(MafwSource * self, guint browse_id,
			  gint remaining_count, guint index,
			  const gchar *object_id, GHashTable *metadatas,
			  gpointer user_data, const GError *error)
{
	GPtrArray *results;

	results = user_data;
	g_ptr_array_add(results, g_strdup(object_id));
	if (!strcmp(object_id, "testobject::item2")) {
		mafw_source_cancel_browse(self, browse_id, NULL);
		g_timeout_add_seconds(1, quit_main_gugufoo, NULL);
	}
}

static void browse_result2_invalid(MafwSource * self, guint browse_id,
				   gint remaining_count, guint index,
				   const gchar *object_id,
				   GHashTable *metadata,
				   gpointer user_data,
				   const GError *error)
{
	if (!strcmp(object_id, "testobject::item0")) {
		g_timeout_add_seconds(1, quit_main_gugufoo, NULL);
	}
}

/* Unit tests:
 *
 * x browse
 * X metadata
 * x all the API functions
 */

static void metadata_result(MafwSource *self, const gchar *object_id,
			    GHashTable *md, gpointer user_data,
			    const GError *error)
{
	GValue *v;

	fail_if(strcmp(object_id, SOURCE_UUID "::wherever"));
	fail_unless(md != NULL);
	v = mafw_metadata_first(md, "title");
	fail_if(strcmp(g_value_get_string(v), "Less than you"));
	v = mafw_metadata_first(md, "album");
	fail_if(strcmp(g_value_get_string(v), "Loudry service"));
	*(gboolean *)user_data = TRUE;
}

static void no_metadata_result(MafwSource *self, const gchar *object_id,
			       GHashTable *md, gpointer user_data,
			       const GError *error)
{
	fail_unless(md == NULL);
	fail_unless(error != NULL);
	fail_unless(error->domain == MAFW_SOURCE_ERROR);
	fail_unless(error->code == MAFW_SOURCE_ERROR_INVALID_OBJECT_ID);
	fail_if(strcmp(error->message, "shekira"));
	*(gboolean *)user_data = TRUE;
}

START_TEST(test_metadata)
{
	GHashTable *metadata;
	MafwProxySource *sp;
	gboolean called;

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);

	sp = MAFW_PROXY_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
				mafw_registry_get_instance()));

	/* Valid request. */
	metadata = mockbus_mkmeta("title", "Less than you",
				  "album", "Loudry service",
				  NULL);
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATA,
			       MAFW_DBUS_STRING(SOURCE_UUID "::wherever"),
			       MAFW_DBUS_C_STRVZ("album", "title")));
	mockbus_reply(MAFW_DBUS_METADATA(metadata));
	mafw_metadata_release(metadata);
	called = FALSE;
	mafw_source_get_metadata(MAFW_SOURCE(sp),
				 SOURCE_UUID "::wherever",
				 MAFW_SOURCE_LIST("album", "title"),
				 metadata_result, &called);
	fail_unless(called);

	/* Invalid request. */
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATA,
			       MAFW_DBUS_STRING("invaliduuid::whatever"),
			       MAFW_DBUS_C_STRVZ("pancake")));
	mockbus_error(MAFW_SOURCE_ERROR, MAFW_SOURCE_ERROR_INVALID_OBJECT_ID,
		      "shekira");
	called = FALSE;
	mafw_source_get_metadata(MAFW_SOURCE(sp),
				 "invaliduuid::whatever",
				 MAFW_SOURCE_LIST("pancake"),
				 no_metadata_result, &called);
	fail_unless(called);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

static void _metadatas_cb(MafwSource *self, GHashTable *metadatas,
				gboolean error_state, const GError *error)
{
	GHashTable *cur_md;
	GValue *val;
	fail_if(g_hash_table_size(metadatas) != 2);
	cur_md = g_hash_table_lookup(metadatas, "testobject");
	fail_if(!cur_md);
	fail_if(g_hash_table_size(cur_md) != 1);
	val = mafw_metadata_first(cur_md, "title");
	fail_if(val == NULL);
	cur_md = g_hash_table_lookup(metadatas, "testobject1");
	fail_if(!cur_md);
	fail_if(g_hash_table_size(cur_md) != 1);
	val = mafw_metadata_first(cur_md, "title");
	fail_if(val == NULL);
	if (!error_state)
	{
		fail_if(error != NULL);
	}
	else
	{
		fail_if(error == NULL);
		fail_if(error->code != 10);
		fail_if(strcmp(error->message, METADATAS_ERROR_MSG) != 0);
	}
}

static void _metadatas_err_cb(MafwSource *self, GHashTable *metadatas,
				gpointer udat, const GError *error)
{
	fail_if(error == NULL);
	fail_if(error->code != 10);
	fail_if(strcmp(error->message, METADATAS_ERROR_MSG) != 0);
	fail_if(metadatas != NULL);
}


START_TEST(test_metadatas)
{
	MafwProxySource *sp = NULL;
	const gchar *const *metadata_keys = NULL;
	GHashTable *metadata;
	DBusMessage *req;
	const gchar *objlist[] = {"testobject", "testobject1", "testobject2",
					NULL};
	

	metadata_keys = MAFW_SOURCE_LIST("title");
	metadata = mockbus_mkmeta("title", "More than words", NULL);

	mockbus_reset();
	
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	
	mockbus_expect(req =
		mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATAS,
				 MAFW_DBUS_C_STRVZ("testobject", "testobject1", 
							"testobject2"),
				 MAFW_DBUS_C_STRVZ("title")));

	mockbus_reply_msg(mdatas_repl(req, objlist, metadata, FALSE));


	sp = MAFW_PROXY_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	mafw_source_get_metadatas(MAFW_SOURCE(sp), objlist,
				metadata_keys,
				(MafwSourceMetadataResultsCb)_metadatas_cb,
				(gpointer)FALSE);


	mockbus_expect(req =
		mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATAS,
				 MAFW_DBUS_C_STRVZ("testobject", "testobject1", 
							"testobject2"),
				 MAFW_DBUS_C_STRVZ("title")));

	mockbus_reply_msg(mdatas_repl(req, objlist, metadata, TRUE));

	mafw_source_get_metadatas(MAFW_SOURCE(sp), objlist,
				metadata_keys,
				(MafwSourceMetadataResultsCb)_metadatas_cb,
				(gpointer)TRUE);


	mafw_metadata_release(metadata);
	
	mockbus_expect(req =
		mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATAS,
				 MAFW_DBUS_C_STRVZ("testobject", "testobject1", 
							"testobject2"),
				 MAFW_DBUS_C_STRVZ("title")));

	mockbus_reply_msg(mdatas_repl(req, objlist, NULL, TRUE));
	mafw_source_get_metadatas(MAFW_SOURCE(sp), objlist,
				metadata_keys,
				(MafwSourceMetadataResultsCb)_metadatas_err_cb,
				NULL);

	/* Error */
	mockbus_expect(req =
		mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATAS,
				 MAFW_DBUS_C_STRVZ("testobject", "testobject1", 
							"testobject2"),
				 MAFW_DBUS_C_STRVZ("title")));

	mockbus_error(MAFW_SOURCE_ERROR, 10, METADATAS_ERROR_MSG);

	mafw_source_get_metadatas(MAFW_SOURCE(sp), objlist,
				metadata_keys,
				(MafwSourceMetadataResultsCb)_metadatas_err_cb,
				NULL);

	mockbus_finish();
}
END_TEST

START_TEST(test_browse)
{
	MafwProxySource *sp = NULL;
	guint browse_id = 0;
	const gchar *const *metadata_keys = NULL;
	GHashTable *metadata;
	DBusMessage *replmsg;
	DBusMessageIter iter_array, iter_msg;

	metadata_keys = MAFW_SOURCE_LIST("title");
	metadata = mockbus_mkmeta("title", "More than words", NULL);

	mockbus_reset();
	
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	
	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING("testobject"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_C_STRVZ("title"),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(10)));

	mockbus_reply(MAFW_DBUS_UINT32(4444));



	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4444, -1, 0,
				"testobject", metadata, "", 0, "");

	dbus_message_iter_close_container(&iter_msg, &iter_array);

	mockbus_incoming(replmsg);
	mafw_metadata_release(metadata);

	sp = MAFW_PROXY_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	browse_id = mafw_source_browse(MAFW_SOURCE(sp),
					     "testobject", FALSE, NULL, NULL,
					     metadata_keys, 0, 10,
					     browse_result, NULL);


	info("Browse ID: %d", browse_id);

	mainloop_test = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop_test);
	
	fail_unless(browse_result_ok == TRUE, "Browse result signal missing.");


	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING("testobject"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_C_STRVZ(NULL),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(10)));

	mockbus_error(MAFW_SOURCE_ERROR, 2, "testproblem");

	
	browse_id = mafw_source_browse(MAFW_SOURCE(sp),
					     "testobject", FALSE, NULL, NULL,
					     NULL, 0, 10,
					     browse_error_result, NULL);


	info("Browse ID: %d", browse_id);

	fail_unless(browse_result_ok == TRUE, "Browse result signal missing.");
	
	fail_if(browse_id != MAFW_SOURCE_INVALID_BROWSE_ID);
	
	
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	g_main_loop_unref(mainloop_test);
	mockbus_finish();
	
}
END_TEST

START_TEST(test_cancel_browse)
{
	GPtrArray *results;
	MafwProxySource *src;
	DBusMessage *replmsg;
	DBusMessageIter iter_array, iter_msg;

	/* I. call browse(), wait for 2 results, then cancel it ->
	 * proxy should sent the cancel message */

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING("bigcan"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_C_STRVZ("faszom"),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(4444));
	
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4444, 5, 0,
				"testobject::item0", NULL, "", 0, "");
	replmsg = append_browse_res(replmsg, &iter_msg, &iter_array, 4444, 4, 0,
				"testobject::item1", NULL, "", 0, "");
	replmsg = append_browse_res(replmsg, &iter_msg, &iter_array, 4444, 3, 0,
				"testobject::item2", NULL, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	
	mockbus_incoming(replmsg);
	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_CANCEL_BROWSE,
				 MAFW_DBUS_UINT32(4444)));
	mockbus_reply();
	src = MAFW_PROXY_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));
	
	results = g_ptr_array_new();
	mafw_source_browse(MAFW_SOURCE(src),
				 "bigcan", FALSE, NULL, NULL,
				 MAFW_SOURCE_LIST("faszom"), 0, 0,
				 browse_result2, results);
	g_main_loop_run(mainloop_test = g_main_loop_new(NULL, FALSE));

	fail_if(results->len != 3);
	fail_if(strcmp(results->pdata[0], "testobject::item0"));
	fail_if(strcmp(results->pdata[1], "testobject::item1"));
	fail_if(strcmp(results->pdata[2], "testobject::item2"));

	g_free(results->pdata[0]);
	g_free(results->pdata[1]);
	g_free(results->pdata[2]);
	g_ptr_array_free(results, TRUE);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)src);
	mockbus_finish();
}
END_TEST

START_TEST(test_cancel_browse_invalid)
{
	MafwProxySource *src;
	guint32 browse_id;
	DBusMessage *replmsg;
	DBusMessageIter iter_array, iter_msg;
	GError *error = NULL;

	/* II. call browse(), wait till it's finished, then try to
	 * cancel it -> proxy should NOT send anything. */

	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING("bigcan"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_C_STRVZ("faszom"),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(4444));
	
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4444, 0, 0,
				"testobject::item0", NULL, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_incoming(replmsg);

	src = MAFW_PROXY_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));

	fail_if(mafw_source_cancel_browse(MAFW_SOURCE(src), 10, &error));
	fail_if(!error);
	g_error_free(error);

	browse_id = mafw_source_browse(
		MAFW_SOURCE(src),
		"bigcan", FALSE, NULL, NULL,
		MAFW_SOURCE_LIST("faszom"), 0, 0,
	       	browse_result2_invalid, NULL);

	g_main_loop_run(mainloop_test = g_main_loop_new(NULL, FALSE));

	fail_if(mafw_source_cancel_browse(MAFW_SOURCE(src), browse_id, NULL));



	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING("abc"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_C_STRVZ("def"),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(4444));
	
	browse_id = mafw_source_browse(
		MAFW_SOURCE(src),
		"abc", FALSE, NULL, NULL,
		MAFW_SOURCE_LIST("def"), 0, 0,
	       	browse_result2_invalid, NULL);
	error = NULL;
	mockbus_expect(
		mafw_dbus_method(MAFW_SOURCE_METHOD_CANCEL_BROWSE,
				 MAFW_DBUS_UINT32(4444)));
	mockbus_error(MAFW_SOURCE_ERROR, 2, "testproblem");
	fail_if(mafw_source_cancel_browse(MAFW_SOURCE(src), browse_id, &error));
	fail_if(!error);
	g_error_free(error);

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)src);
	mockbus_finish();
}
END_TEST

static void object_created(MafwSource *src, const gchar *objectid,
			   gpointer *comm, const GError *error)
{
	fail_unless(comm[0] == src);
	fail_unless(GPOINTER_TO_INT(comm[1]) == FALSE);

	fail_if(error != NULL);
	fail_if(!objectid);
	fail_if(strcmp(objectid, "babba") != 0);
	comm[1] = GINT_TO_POINTER(TRUE);
}

static void object_not_created(MafwSource *src, const gchar *objectid,
			       gpointer *comm, const GError *error)
{
	fail_unless(comm[0] == src);
	fail_unless(GPOINTER_TO_INT(comm[1]) == FALSE);

	fail_if(objectid != NULL);
	fail_if(error    == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code   != MAFW_EXTENSION_ERROR_ACCESS_DENIED);
	fail_if(strcmp(error->message, "balfasz") != 0);

	comm[1] = GINT_TO_POINTER(TRUE);
}

START_TEST(test_object_creation)
{
	GHashTable *md;
	MafwSource *src;
	gpointer comm[2];

	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	
	src = MAFW_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));

	/* Without metadata */
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_CREATE_OBJECT,
				       	MAFW_DBUS_STRING("mamma"),
				       	DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
					NULL, 0));
	mockbus_reply(MAFW_DBUS_STRING("babba"));

	comm[0] = src;
	comm[1] = GINT_TO_POINTER(FALSE);
	mafw_source_create_object(src, "mamma", NULL,
				(MafwSourceObjectCreatedCb)object_created,
			       	comm);
	mockbus_finish();
	fail_if(GPOINTER_TO_INT(comm[1]) != TRUE);

	/* With metadata */
	md = mafw_metadata_new();
	mafw_metadata_add_int(md, "life",  10);
	mafw_metadata_add_int(md, "death", 100, 1000);
	mafw_metadata_add_str(md, "blood", "creek", "Mary's");
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_CREATE_OBJECT,
				       	MAFW_DBUS_STRING("mamma"),
					MAFW_DBUS_METADATA(md)));
	mockbus_reply(MAFW_DBUS_STRING("babba"));

	comm[1] = GINT_TO_POINTER(FALSE);
	mafw_source_create_object(src, "mamma", md,
				(MafwSourceObjectCreatedCb)object_created,
			       	comm);
	mockbus_finish();
	fail_if(GPOINTER_TO_INT(comm[1]) != TRUE);

	/* With error */
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_CREATE_OBJECT,
				       	MAFW_DBUS_STRING("pappa"),
					MAFW_DBUS_METADATA(md)));
	mockbus_error(MAFW_SOURCE_ERROR, MAFW_EXTENSION_ERROR_ACCESS_DENIED,
		      "balfasz");

	comm[1] = GINT_TO_POINTER(FALSE);
	mafw_source_create_object(src, "pappa", md,
				(MafwSourceObjectCreatedCb)object_not_created,
			       	comm);
	mockbus_finish();
	fail_if(GPOINTER_TO_INT(comm[1]) != TRUE);

	/* Clean up */
	mafw_metadata_release(md);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)src);
}
END_TEST

static gboolean mdat_set_cb_called, medat_set_failed_cb_called,
		mdat_set_error_cb_called;

static void metadata_set_cb(MafwSource *src, const gchar *objectid,
			    const gchar **failed_keys,
			    gpointer *user_data, const GError *error)
{
	fail_if(strcmp(objectid, "edited_object_id") != 0, "wrong object id");
	fail_if(error != NULL, "error was not supposed to be set");
	fail_if(*((gint*)user_data) != 42);
	fail_if(g_strv_length((gchar**)failed_keys) != 0);
	mdat_set_cb_called = TRUE;
}

static void metadata_set_failed_key_cb(MafwSource *src, const gchar *objectid,
				       const gchar **failed_keys,
				       gpointer *user_data, const GError *error)
{
	fail_if(strcmp(objectid, "edited_object_id") != 0, "wrong object id");
	fail_if(error == NULL, "error was supposed to be set");
	fail_if(*((gint*)user_data) != 42);
	fail_if(failed_keys == NULL);
	fail_if(strcmp(failed_keys[0], "wrong_key") != 0);
	fail_if(error->code != MAFW_SOURCE_ERROR_UNSUPPORTED_METADATA_KEY);
	medat_set_failed_cb_called = TRUE;
}

static void metadata_set_with_error_cb(MafwSource *src, const gchar *objectid,
				       const gchar **failed_keys,
				       gpointer *user_data, const GError *error)
{
	fail_if(objectid != NULL);
	fail_if(*((gint*)user_data) != 42);
	fail_if(error == NULL, "Error was not set");
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code != MAFW_EXTENSION_ERROR_ACCESS_DENIED);
	fail_if(strcmp(error->message, "ei pysty") != 0);
	mdat_set_error_cb_called = TRUE;
}

START_TEST(test_set_metadata)
{
	GHashTable *md;
	MafwSource *src;
	gint user_data;

	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	/* Connect */
	src = MAFW_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
				mafw_registry_get_instance()));

	/* All keys accepted, no errors */
	md = mafw_metadata_new();
	mafw_metadata_add_int(md, "life",  10);
	mafw_metadata_add_int(md, "death", 100, 1000);
	mafw_metadata_add_str(md, "blood", "creek", "Mary's");
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_SET_METADATA,
				       	MAFW_DBUS_STRING("edited_object_id"),
					MAFW_DBUS_METADATA(md)));
	mockbus_reply(MAFW_DBUS_STRING("edited_object_id"),
		      DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
		      NULL, 0);

	user_data = 42;
	mafw_source_set_metadata(
			    src, "edited_object_id", md,
			    (MafwSourceMetadataSetCb)metadata_set_cb,
			    &user_data);
	mockbus_finish();

	/* With invalid object id */
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_SET_METADATA,
				       	MAFW_DBUS_STRING("this_will_fail"),
					MAFW_DBUS_METADATA(md)));
	mockbus_error(MAFW_SOURCE_ERROR, MAFW_EXTENSION_ERROR_ACCESS_DENIED,
		      "ei pysty");

	mafw_source_set_metadata(
			    src, "this_will_fail", md,
			    (MafwSourceMetadataSetCb)metadata_set_with_error_cb,
			    &user_data);
	mockbus_finish();

	/* Clean up */
	mafw_metadata_release(md);

	/* With some failed keys */
	md = mafw_metadata_new();
	mafw_metadata_add_int(md, "ok_key",  10);
	mafw_metadata_add_int(md, "wrong_key", 100, 1000);
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_SET_METADATA,
				       	MAFW_DBUS_STRING("edited_object_id"),
					MAFW_DBUS_METADATA(md)));
	mockbus_reply(MAFW_DBUS_STRING("edited_object_id"),
		      MAFW_DBUS_C_STRVZ("wrong_key"),
		      MAFW_DBUS_STRING(g_quark_to_string(MAFW_SOURCE_ERROR)),
		      MAFW_DBUS_INT32(MAFW_SOURCE_ERROR_UNSUPPORTED_METADATA_KEY),
		      MAFW_DBUS_STRING("don't know what that key means")
		);

	user_data = 42;
	mafw_source_set_metadata(
			    src, "edited_object_id", md,
			    (MafwSourceMetadataSetCb)metadata_set_failed_key_cb,
			    &user_data);
	mockbus_finish();

	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)src);
	mafw_metadata_release(md);
	
	fail_unless(mdat_set_cb_called && medat_set_failed_cb_called &&
		mdat_set_error_cb_called && TRUE, "Some cb was not called");
}
END_TEST

static void object_destroyed(MafwSource *src, const gchar *objectid,
			     gpointer *comm, const GError *error)
{
	fail_unless(comm[0] == src);
	fail_unless(GPOINTER_TO_INT(comm[1]) == FALSE);

	fail_if(!objectid);
	fail_if(strcmp(objectid, "police") != 0);
	comm[1] = GINT_TO_POINTER(TRUE);
}

static void object_not_destroyed(MafwSource *src, const gchar *objectid,
				 gpointer *comm, const GError *error)
{
	fail_unless(comm[0] == src);
	fail_unless(GPOINTER_TO_INT(comm[1]) == FALSE);

	fail_if(!objectid);
	fail_if(strcmp(objectid, "whitehouse") != 0);

	fail_if(error == NULL);
	fail_if(error->domain != MAFW_SOURCE_ERROR);
	fail_if(error->code   != MAFW_EXTENSION_ERROR_ACCESS_DENIED);
	fail_if(strcmp(error->message, "loser") != 0);

	comm[1] = GINT_TO_POINTER(TRUE);
}

START_TEST(test_object_destruction)
{
	MafwSource *src;
	gpointer comm[2];

	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);
	src = MAFW_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));

	/* Success */
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_DESTROY_OBJECT,
				       	MAFW_DBUS_STRING("police")));
	mockbus_reply(DBUS_TYPE_INVALID);

	comm[0] = src;
	comm[1] = GINT_TO_POINTER(FALSE);
	mafw_source_destroy_object(src, "police",
				(MafwSourceObjectDestroyedCb)object_destroyed,
			       	comm);
	mockbus_finish();
	fail_if(GPOINTER_TO_INT(comm[1]) != TRUE);

	/* Failure */
	mockbus_expect(mafw_dbus_method(MAFW_SOURCE_METHOD_DESTROY_OBJECT,
				       	MAFW_DBUS_STRING("whitehouse")));
	mockbus_error(MAFW_SOURCE_ERROR, MAFW_EXTENSION_ERROR_ACCESS_DENIED,
		      "loser");

	comm[1] = GINT_TO_POINTER(FALSE);
	mafw_source_destroy_object(src, "whitehouse",
				(MafwSourceObjectCreatedCb)object_not_destroyed,
			       	comm);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)src);
	mockbus_finish();
	fail_if(GPOINTER_TO_INT(comm[1]) != TRUE);
}
END_TEST

static gboolean mdata_chd, cont_chd;

static void check_signals(void)
{
	if (mdata_chd && cont_chd)
	{
		g_main_loop_quit(mainloop_test);
	}
}


static void sp_metadata_changed(MafwSource *self, const gchar *object_id)
{
	fail_if(strcmp(object_id, "str") != 0, "Wrong object_id");
	mdata_chd = TRUE;
	check_signals();
}

static void sp_container_changed(MafwSource *self, const gchar *object_id)
{
	fail_if(strcmp(object_id, "str_oid") != 0, "Wrong object_id");
	cont_chd = TRUE;
	check_signals();
}

START_TEST(test_source_signals)
{
	MafwProxySource *sp = NULL;
	GHashTable *metadata;

	mainloop_test = g_main_loop_new(NULL, FALSE);
	
	metadata = mockbus_mkmeta("title", "Less than you", NULL);
	
	
	mockbus_reset();
	mock_empty_props(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH);

	sp = MAFW_PROXY_SOURCE(mafw_proxy_source_new(SOURCE_UUID, "fake",
					mafw_registry_get_instance()));
	fail_unless(sp != NULL, "Object construction failed");

	g_signal_connect(sp, "metadata-changed",
			 G_CALLBACK(sp_metadata_changed), NULL);
	g_signal_connect(sp, "container-changed",
			 G_CALLBACK(sp_container_changed), NULL);
	
	mockbus_incoming(mafw_dbus_signal(MAFW_SOURCE_SIGNAL_METADATA_CHANGED,
					MAFW_DBUS_STRING("str")));
	mockbus_incoming(mafw_dbus_signal(MAFW_SOURCE_SIGNAL_CONTAINER_CHANGED,
				MAFW_DBUS_STRING("str_oid")));
	g_main_loop_run(mainloop_test);

	mafw_metadata_release(metadata);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
                                        (gpointer)sp);
	mockbus_finish();
}
END_TEST

/*****************************************************************************
 * Test case management
 *****************************************************************************/
static Suite *mafw_proxy_source_suite_new(void)
{
	Suite *suite;

	suite = suite_create("MafwProxySource");
	checkmore_add_tcase(suite, "Browse", test_browse);
	tcase_set_timeout(checkmore_add_tcase(suite, "Cancel browse",
					      test_cancel_browse), 5);
	tcase_set_timeout(checkmore_add_tcase(suite,
					      "Cancel invalid browse session",
					      test_cancel_browse_invalid), 5);
	checkmore_add_tcase(suite, "Metadata", test_metadata);
	checkmore_add_tcase(suite, "Metadatas", test_metadatas);
	checkmore_add_tcase(suite, "Create object",  test_object_creation);
	checkmore_add_tcase(suite, "Destroy object", test_object_destruction);
	checkmore_add_tcase(suite, "Set metadata", test_set_metadata);
	checkmore_add_tcase(suite, "Signal testing",
			    test_source_signals);
	return suite;
}

/*****************************************************************************
 * Test case execution
 *****************************************************************************/

int main(void)
{
	SRunner *r;

	r = srunner_create(mafw_proxy_source_suite_new());
	return checkmore_run(r, FALSE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
