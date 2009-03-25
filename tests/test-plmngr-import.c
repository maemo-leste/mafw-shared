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

#include <checkmore.h>
#include <string.h>
#include "mockbus.h"
#include "mocksource.h"
#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "libmafw-shared/mafw-shared.h"
#include "../mafw-playlist-daemon/mpd-internal.h"
#include <libmafw/mafw-metadata-serializer.h>
#include <totem-pl-parser.h>

#define FAKE_NAME "TESTName"

#define FAKE_SOURCE_NAME "DEADBEEF"

#define FAKE_SOURCE_SERVICE MAFW_SOURCE_SERVICE ".fake." FAKE_SOURCE_NAME
#define FAKE_SOURCE_OBJECT MAFW_SOURCE_OBJECT "/" FAKE_SOURCE_NAME

#define MAFW_DBUS_PROXY_PATH "/com/nokia/mafw/proxy_source/mocksource"
#define MAFW_DBUS_PATH "/com/nokia/mafw/source/mocksource"
#define MAFW_DBUS_INTERFACE MAFW_SOURCE_INTERFACE

/* Playlists will be stored in PLS_DIR. */
#define PLS_DIR			"testplimport"

/* Mocks the messages happening at the construction of a extension:
 * - querying its very very friendly name (and returning FAKE_NAME)
 * - returning none for the list of runtime properties
 *
 * Pass the desired service name and object path as argument.
 */
static void mock_empty_props(const gchar *service, const gchar *object)
{
	mockbus_expect(mafw_dbus_method_full(service, object,
					     MAFW_EXTENSION_INTERFACE,
					     MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_reply(MAFW_DBUS_STRING(FAKE_NAME));
	mockbus_expect(mafw_dbus_method_full(service, object,
					     MAFW_EXTENSION_INTERFACE,
					     MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	mockbus_reply(MAFW_DBUS_STRVZ(NULL),
		      MAFW_DBUS_C_ARRAY(UINT32, guint));
}



/* When called before instantiating a registry, causes the registry to
 * `see' the given @active services. */
static void mock_services(const gchar *const *active)
{
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
					     DBUS_PATH_DBUS,
					     DBUS_INTERFACE_DBUS,
					     "ListNames"));
	mockbus_reply(MAFW_DBUS_STRVZ(active));
}

static void mock_appearing_extension(const gchar *service)
{
	mockbus_incoming(
		mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
				      DBUS_INTERFACE_DBUS,
				      "NameOwnerChanged",
				      MAFW_DBUS_STRING(service),
				      MAFW_DBUS_STRING(""),
				      MAFW_DBUS_STRING(service)));
}

static DBusMessage *append_browse_res(DBusMessage *replmsg,
				DBusMessageIter *iter_msg,
				DBusMessageIter *iter_array,
				guint browse_id,
				gint remaining_count, guint index,
				const gchar *object_id,
				GHashTable *metadata,
				const gchar *domain_str,
				guint errcode,
				const gchar *err_msg)
{
	DBusMessageIter istr;
	GByteArray *ba = NULL;

	if (!replmsg)
	{
		replmsg = dbus_message_new_method_call(MAFW_DBUS_DESTINATION,
			MAFW_SOURCE_OBJECT "/" FAKE_SOURCE_NAME,
			MAFW_SOURCE_INTERFACE,
			MAFW_PROXY_SOURCE_METHOD_BROWSE_RESULT);
		dbus_message_iter_init_append(replmsg,
						iter_msg);
		dbus_message_iter_append_basic(iter_msg,  DBUS_TYPE_UINT32,
						&browse_id);
		dbus_message_iter_open_container(iter_msg, DBUS_TYPE_ARRAY,
						 "(iusaysus)", iter_array);
	}
	dbus_message_iter_open_container(iter_array, DBUS_TYPE_STRUCT, NULL,
					&istr);
	
	ba = mafw_metadata_freeze_bary(metadata);
	
	
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_INT32, &remaining_count);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_UINT32, &index);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &object_id);
	mafw_dbus_message_append_array(&istr, DBUS_TYPE_BYTE, ba->len, ba->data);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &domain_str);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_UINT32, &errcode);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &err_msg);
	g_byte_array_free(ba, TRUE);
	dbus_message_iter_close_container(iter_array, &istr);
	return replmsg;
}


static gchar *test_uri_list[] = { "file://test1/test1.pls", 
				  "file://test2/test2.pls",
				  "file://test3/test3.pls",
				  NULL
				};
static gchar **uril;
				
static gboolean return_parser_error;

TotemPlParserResult
totem_pl_parser_parse_with_base (TotemPlParser *parser, const char *url,
				 const char *base, gboolean fallback)
{
	gint i = 0;

	if (return_parser_error)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	if (uril)
	{
		while (uril[i])
		{
			g_signal_emit_by_name(parser, "entry-parsed", uril[i],
						NULL);
			i++;
		}
	}
	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

START_TEST(test_import_source)
{
	DBusMessage *c, *mdata, *browse;
	GHashTable *metadata;
	extern GTree *Playlists;
	gchar *oid;
	Pls *pls;
	DBusMessage *replmsg;
	DBusMessageIter iter_array, iter_msg;

		
	metadata = mockbus_mkmeta(MAFW_METADATA_KEY_URI, "http://test.test",
			MAFW_METADATA_KEY_MIME, MAFW_METADATA_VALUE_MIME_CONTAINER,
			NULL);
	
	mockbus_reset();
	mockbus_expect(mafw_dbus_method_full(
			       DBUS_SERVICE_DBUS,
			       DBUS_PATH_DBUS,
			       DBUS_INTERFACE_DBUS,
			       "RequestName",
			       MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
			       MAFW_DBUS_UINT32(4)
			       ));
	mockbus_reply(MAFW_DBUS_UINT32(1));
	mock_services(NULL);
	mafw_shared_deinit();
	init_playlist_wrapper(dbus_bus_get(0, NULL), TRUE, FALSE);
	
	/* Source ID, but no registered source */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mafw_dbus_error(c, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_IMPORT_FAILED,
					"Source not found"));
	mockbus_deliver(NULL);

	/* Lets register a fake-source */
	mock_appearing_extension(FAKE_SOURCE_SERVICE);
	mock_empty_props(FAKE_SOURCE_SERVICE, FAKE_SOURCE_OBJECT);

	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mdata = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
					FAKE_SOURCE_OBJECT,
					MAFW_SOURCE_INTERFACE,
					MAFW_SOURCE_METHOD_GET_METADATA,
					MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
					MAFW_DBUS_STRVZ(MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME))));
	mockbus_expect(browse = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
				 FAKE_SOURCE_OBJECT,
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRVZ(MAFW_SOURCE_NO_KEYS),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply_msg(mafw_dbus_reply(mdata, MAFW_DBUS_METADATA(metadata)));
	mockbus_reply_msg(mafw_dbus_reply(browse, MAFW_DBUS_UINT32(3)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(2)));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);

	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 3, 1, 0,
				"test::oid1", NULL, "", 0, "");
	replmsg = append_browse_res(replmsg, &iter_msg, &iter_array, 3, 0, 1,
				"test::oid2", NULL, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_incoming(replmsg);
	
	mockbus_expect(c = mafw_dbus_method_full(
				"dummy.service.name",
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(2),
				MAFW_DBUS_UINT32(1)));
	mockbus_expect(mafw_dbus_signal_full(NULL, MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
						MAFW_DBUS_UINT32(1)));
	
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);

	/* Now check whether the new pl is correct */
	pls = g_tree_lookup(Playlists, GUINT_TO_POINTER(1));
	fail_if(pls == NULL);
	fail_if(pls->len != 2);
	oid = pls_get_item(pls, 0);
	fail_if(oid == NULL);
	fail_if(strcmp(oid, "test::oid1") != 0);
	g_free(oid);
	oid = pls_get_item(pls, 1);
	fail_if(oid == NULL);
	fail_if(strcmp(oid, "test::oid2") != 0);
	g_free(oid);
	mockbus_finish();

	/* Error during browse test*/
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mdata = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
					      FAKE_SOURCE_OBJECT,
					      MAFW_SOURCE_INTERFACE,
					      MAFW_SOURCE_METHOD_GET_METADATA,
					      MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
					      MAFW_DBUS_STRVZ(MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME))));
	mockbus_expect(browse = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
				 FAKE_SOURCE_OBJECT,
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRVZ(MAFW_SOURCE_NO_KEYS),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply_msg(mafw_dbus_reply(mdata, MAFW_DBUS_METADATA(metadata)));
	mockbus_reply_msg(mafw_dbus_reply(browse, MAFW_DBUS_UINT32(4)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(3)));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);

	/* First browse OK */
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4, 1, 0,
				"test::oid1", NULL, "", 0, "");
	/* Second has error */
	replmsg = append_browse_res(replmsg, &iter_msg, &iter_array, 4, 0, 1,
				"test::oid2", NULL, "domain_str", 10, "error->message");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_incoming(replmsg);

	mockbus_expect(c = mafw_dbus_method_full(
				"dummy.service.name",
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(3),
				MAFW_DBUS_STRING("domain_str"),
				MAFW_DBUS_INT32(10),
				MAFW_DBUS_STRING("error->message")));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mafw_metadata_release(metadata);
	mockbus_finish();
	

	/* Error at get_metadata */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mdata = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
					      FAKE_SOURCE_OBJECT,
					      MAFW_SOURCE_INTERFACE,
					      MAFW_SOURCE_METHOD_GET_METADATA,
					      MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
					      MAFW_DBUS_STRVZ(MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME))));
	mockbus_reply_msg(mafw_dbus_error(mdata, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_IMPORT_FAILED,
					"Source not found"));
	
	mockbus_expect(c = mafw_dbus_method_full(
				"dummy.service.name",
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(4),
				MAFW_DBUS_STRING("com.nokia.mafw.error.playlist"),
				MAFW_DBUS_INT32(MAFW_PLAYLIST_ERROR_IMPORT_FAILED),
				MAFW_DBUS_STRING("Source not found")));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(4)));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	

	/* Test, when importing from a file... plfile empty */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("file://test/test.pls"),
				  MAFW_DBUS_STRING("")));
	mockbus_expect(c = mafw_dbus_method_full(
				"dummy.service.name",
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(5),
				MAFW_DBUS_UINT32(2)));
	mockbus_expect(mafw_dbus_signal_full(NULL, MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
						MAFW_DBUS_UINT32(2)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(5)));
	
	mockbus_deliver(NULL);
	mockbus_finish();

	/* Now check whether the new pl is correct */
	pls = g_tree_lookup(Playlists, GUINT_TO_POINTER(2));
	fail_if(pls == NULL);
	fail_if(pls->len != 0);
	
	
	
	
	/* Test, when importing from a file... plfile not empty */
	uril = test_uri_list;
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("file://test/test.pls"),
				  MAFW_DBUS_STRING("")));
	mockbus_expect(c = mafw_dbus_method_full(
				"dummy.service.name",
				MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,
				MAFW_PLAYLIST_METHOD_PLAYLIST_IMPORTED,
				MAFW_DBUS_UINT32(6),
				MAFW_DBUS_UINT32(3)));
	mockbus_expect(mafw_dbus_signal_full(NULL, MAFW_PLAYLIST_PATH,
				MAFW_PLAYLIST_INTERFACE,MAFW_PLAYLIST_SIGNAL_PLAYLIST_CREATED,
						MAFW_DBUS_UINT32(3)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(6)));
	
	mockbus_deliver(NULL);
	mockbus_finish();
	
	/* Now check whether the new pl is correct */
	pls = g_tree_lookup(Playlists, GUINT_TO_POINTER(3));
	fail_if(pls == NULL);
	fail_if(pls->len != 3);
	oid = pls_get_item(pls, 0);
	fail_if(oid == NULL);
	fail_if(strcmp(oid, "urisource::file://test1/test1.pls") != 0);
	g_free(oid);
	oid = pls_get_item(pls, 1);
	fail_if(oid == NULL);
	fail_if(strcmp(oid, "urisource::file://test2/test2.pls") != 0);
	g_free(oid);
	oid = pls_get_item(pls, 2);
	fail_if(oid == NULL);
	fail_if(strcmp(oid, "urisource::file://test3/test3.pls") != 0);
	g_free(oid);

	/* Cancel a non-existing request */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
				  MAFW_DBUS_UINT32(6)));
	mockbus_expect(mafw_dbus_error(c, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_INVALID_IMPORT_ID,
					"ImportID not found"));
	
	mockbus_deliver(NULL);
	mockbus_finish();
	/* Parse failed*/
	return_parser_error = TRUE;
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING("file://test/test.pls"),
				  MAFW_DBUS_STRING("")));
	mockbus_expect(mafw_dbus_error(c, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_IMPORT_FAILED,
					"Playlist parsing failed."));
	mockbus_deliver(NULL);
	mockbus_finish();

	return;
}
END_TEST

START_TEST(test_cancel_import)
{
	DBusMessage *c, *mdata, *browse, *cancel_browse;
	GHashTable *metadata;
	DBusMessage *replmsg;
	DBusMessageIter iter_array, iter_msg;
	
	metadata = mockbus_mkmeta(MAFW_METADATA_KEY_URI, "http://test.test",
			MAFW_METADATA_KEY_MIME, MAFW_METADATA_VALUE_MIME_CONTAINER,
			NULL);
	
	mockbus_reset();
	mockbus_expect(mafw_dbus_method_full(
			       DBUS_SERVICE_DBUS,
			       DBUS_PATH_DBUS,
			       DBUS_INTERFACE_DBUS,
			       "RequestName",
			       MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
			       MAFW_DBUS_UINT32(4)
			       ));
	mockbus_reply(MAFW_DBUS_UINT32(1));
	mock_services(NULL);
	mafw_shared_deinit();
	init_playlist_wrapper(dbus_bus_get(0, NULL), TRUE, FALSE);

	/* Cancel a non-existing request */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
				  MAFW_DBUS_UINT32(6)));
	mockbus_expect(mafw_dbus_error(c, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_INVALID_IMPORT_ID,
					"ImportID not found"));
	mockbus_deliver(NULL);
	mockbus_finish();

	/* Lets register a fake-source */
	mock_appearing_extension(FAKE_SOURCE_SERVICE);
	mock_empty_props(FAKE_SOURCE_SERVICE, FAKE_SOURCE_OBJECT);
	
	/* Cancel, before the get-metadata-res returns */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mdata = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
					FAKE_SOURCE_OBJECT,
					MAFW_SOURCE_INTERFACE,
					MAFW_SOURCE_METHOD_GET_METADATA,
					MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
					MAFW_DBUS_STRVZ(MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME))));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(1)));
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
				  MAFW_DBUS_UINT32(1)));
	mockbus_expect(mafw_dbus_reply(c));

	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mockbus_reply_msg(mafw_dbus_reply(mdata, MAFW_DBUS_METADATA(metadata)));
	mockbus_send_stored_reply();
	mockbus_finish();
	
	/* Cancel, before the first browse-res returns */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mdata = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
					      FAKE_SOURCE_OBJECT,
					      MAFW_SOURCE_INTERFACE,
					      MAFW_SOURCE_METHOD_GET_METADATA,
					      MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
					      MAFW_DBUS_STRVZ(MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME))));
	mockbus_expect(browse = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
				 FAKE_SOURCE_OBJECT,
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRVZ(MAFW_SOURCE_NO_KEYS),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply_msg(mafw_dbus_reply(mdata, MAFW_DBUS_METADATA(metadata)));
	mockbus_reply_msg(mafw_dbus_reply(browse, MAFW_DBUS_UINT32(4)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(2)));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);

	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
				  MAFW_DBUS_UINT32(2)));
	
	mockbus_expect(cancel_browse = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
				 FAKE_SOURCE_OBJECT,
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_CANCEL_BROWSE,
				 MAFW_DBUS_UINT32(4)));
	mockbus_reply_msg(mafw_dbus_reply(cancel_browse));
	mockbus_expect(mafw_dbus_reply(c));

	/* First browse OK */
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4, 1, 0,
				"test::oid1", NULL, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	
	mockbus_incoming(replmsg);
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mockbus_finish();
	
	/* Cancel, after the first browse-res returns */
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_IMPORT_PLAYLIST,
				  MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				  MAFW_DBUS_STRING("")));
	
	mockbus_expect(mdata = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
					      FAKE_SOURCE_OBJECT,
					      MAFW_SOURCE_INTERFACE,
					      MAFW_SOURCE_METHOD_GET_METADATA,
					      MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
					      MAFW_DBUS_STRVZ(MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					MAFW_METADATA_KEY_MIME))));
	mockbus_expect(browse = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
				 FAKE_SOURCE_OBJECT,
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_BROWSE,
				 MAFW_DBUS_STRING(FAKE_SOURCE_NAME "::"),
				 MAFW_DBUS_BOOLEAN(FALSE),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRING(""),
				 MAFW_DBUS_STRVZ(MAFW_SOURCE_NO_KEYS),
				 MAFW_DBUS_UINT32(0),
				 MAFW_DBUS_UINT32(0)));
	mockbus_reply_msg(mafw_dbus_reply(mdata, MAFW_DBUS_METADATA(metadata)));
	mockbus_reply_msg(mafw_dbus_reply(browse, MAFW_DBUS_UINT32(4)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(3)));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	
	/* First browse OK */
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4, 2, 0,
				"test::oid1", NULL, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_incoming(replmsg);
	
	mockbus_incoming(c = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
				  MAFW_PLAYLIST_PATH,
				  MAFW_PLAYLIST_INTERFACE,
				  MAFW_PLAYLIST_METHOD_CANCEL_IMPORT,
				  MAFW_DBUS_UINT32(3)));
	
	mockbus_expect(cancel_browse = mafw_dbus_method_full(FAKE_SOURCE_SERVICE,
				 FAKE_SOURCE_OBJECT,
				 MAFW_SOURCE_INTERFACE,
				 MAFW_SOURCE_METHOD_CANCEL_BROWSE,
				 MAFW_DBUS_UINT32(4)));
	mockbus_reply_msg(mafw_dbus_reply(cancel_browse));
	mockbus_expect(mafw_dbus_reply(c));
	
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 4, 2, 1,
				"test::oid2", NULL, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_incoming(replmsg);
	
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);
	mafw_metadata_release(metadata);
	mockbus_finish();
	
	return;
}
END_TEST

static Suite *pluginwrapper_suite(void)
{
	Suite *suite;
	TCase *tc_import_src, *tc_cancel_import ;

	suite = suite_create("Playlist-mngr-wrapper-import");
	if (1){ tc_import_src = checkmore_add_tcase(suite, "Import source",
			    test_import_source);
		tcase_set_timeout(tc_import_src, 60);
	}
	if (1){ tc_cancel_import = checkmore_add_tcase(suite, "Cancel import",
			    test_cancel_import);
		tcase_set_timeout(tc_cancel_import, 60);
	}
	/*valgrind needs more time to execute*/
	
	
	
	return suite;
}

/*****************************************************************************
 * Test case execution
 *****************************************************************************/

int main(void)
{
	g_setenv("MAFW_PLAYLIST_DIR", PLS_DIR, TRUE);
	return checkmore_run(srunner_create(pluginwrapper_suite()), FALSE);
}
