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

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dbus/dbus.h>
#include <glib-object.h>

#include <libmafw/mafw.h>

#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "mafw-dbus-wrapper/wrapper.h"

#include <check.h>
#include <checkmore.h>
#include "mockbus.h"
#include "mocksource.h"
#include "errorsource.h"
#include <libmafw/mafw-metadata-serializer.h>

#define MAFW_DBUS_PROXY_PATH "/com/nokia/mafw/source/mocksource"
#define MAFW_DBUS_PATH "/com/nokia/mafw/source/mocksource"
#define MAFW_DBUS_INTERFACE MAFW_SOURCE_INTERFACE
#define FAKE_SOURCE_NAME "mocksource"
#define FAKE_SOURCE_SERVICE MAFW_SOURCE_SERVICE ".mockland." FAKE_SOURCE_NAME

extern const char *msg_sender_id;

static GMainLoop *Loop = NULL;

static gboolean quit_mainloop_on_tout(gpointer user_data)
{
       g_main_loop_quit(Loop);
       return FALSE;
}

START_TEST(test_export_unexport)
{
	MafwExtension *extension;

	extension = mocked_source_new("mocksource", "mocksource", Loop);
	mockbus_reset();
	wrapper_init();

	mock_appearing_extension(FAKE_SOURCE_SERVICE, FALSE);
	mafw_registry_add_extension(mafw_registry_get_instance(),
				     extension);

	mockbus_expect(mafw_dbus_method_full(
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS,
		"ReleaseName",
		MAFW_DBUS_STRING("com.nokia.mafw.source.mockland.mocksource")
		));
	mockbus_reply(MAFW_DBUS_UINT32(4));
	mafw_registry_remove_extension(mafw_registry_get_instance(),
					extension);

	mockbus_finish();
}
END_TEST

static void
add_metadatas_object(gpointer key, gpointer value, gpointer user_data)
{
	gchar ***objs = user_data;
	**objs = (gchar *)key;
	(*objs)++;
}

START_TEST(test_source_wrapper)
{
	MockedSource *source;
	MafwRegistry *registry;
	GHashTable *metadata;
	DBusMessage *c;
	DBusMessage *replmsg;
	DBusMessageIter iter_array, iter_msg;
	gint i;
	const gchar *objlist[] = {"testobject", "testobject1", NULL};

	metadata = mockbus_mkmeta("title", "Easy", NULL);

	registry = mafw_registry_get_instance();
	mockbus_reset();
	wrapper_init();

	source = mocked_source_new("mocksource", "mocksource", Loop);

	mock_appearing_extension(FAKE_SOURCE_SERVICE, FALSE);
	mafw_registry_add_extension(MAFW_REGISTRY(registry),
                                    MAFW_EXTENSION(source));

	/* Browse */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_BOOLEAN(FALSE),
				      MAFW_DBUS_STRING("!(rating=sucks)"),
				      MAFW_DBUS_STRING("-year"),
				      MAFW_DBUS_C_STRVZ("title", "artist"),
				      MAFW_DBUS_UINT32(0),
				      MAFW_DBUS_UINT32(11)));
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 1408, -1, 0,
                                    "testobject", metadata, "", 0, "");
	replmsg = append_browse_res(replmsg, &iter_msg, &iter_array, 1408, 0, 0,
                                    "", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(1408)));

	mockbus_deliver(NULL);
	ck_assert(source->browse_called == 1);

	source->repeat_browse = 25;
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_BOOLEAN(FALSE),
				      MAFW_DBUS_STRING("!(rating=sucks)"),
				      MAFW_DBUS_STRING("-year"),
				      MAFW_DBUS_C_STRVZ("title", "artist"),
				      MAFW_DBUS_UINT32(0),
				      MAFW_DBUS_UINT32(11)));
	replmsg = NULL;
	for (i=0; i<25; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	replmsg = append_browse_res(NULL, &iter_msg, &iter_array, 1408, 0, 0,
                                    "", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(1408)));

	mockbus_deliver(NULL);
	ck_assert(source->browse_called == 2);

	source->repeat_browse = 830;
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_BOOLEAN(FALSE),
				      MAFW_DBUS_STRING("!(rating=sucks)"),
				      MAFW_DBUS_STRING("-year"),
				      MAFW_DBUS_C_STRVZ("title", "artist"),
				      MAFW_DBUS_UINT32(0),
				      MAFW_DBUS_UINT32(11)));
	replmsg = NULL;
	for (i=0; i<25; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	replmsg = NULL;
	for (i=0; i<75; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	replmsg = NULL;
	for (i=0; i<225; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	replmsg = NULL;
	for (i=0; i<500; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	replmsg = NULL;
	for (i=0; i<5; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	replmsg = append_browse_res(replmsg, &iter_msg, &iter_array, 1408, 0, 0,
				"", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);

	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(1408)));

	mockbus_deliver(NULL);
	ck_assert(source->browse_called == 3);

	/* Cancel browse */
	source->repeat_browse = 25;
	source->dont_send_last = TRUE;
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_BOOLEAN(FALSE),
				      MAFW_DBUS_STRING(""),
				      MAFW_DBUS_STRING(""),
				      MAFW_DBUS_C_STRVZ("title", "artist"),
				      MAFW_DBUS_UINT32(0),
				      MAFW_DBUS_UINT32(11)));
	replmsg = NULL;
	for (i=0; i<25; i++)
		replmsg = append_browse_res(replmsg, &iter_msg, &iter_array,
                                            1408, -1, 0,
                                            "testobject", metadata, "", 0, "");
	dbus_message_iter_close_container(&iter_msg, &iter_array);
	mockbus_expect(replmsg);
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_UINT32(1408)));
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_CANCEL_BROWSE,
					      MAFW_DBUS_UINT32(1408)));

	mockbus_expect(mafw_dbus_reply(c));
	mockbus_deliver(NULL);
	mockbus_deliver(NULL);

	g_timeout_add(100, quit_mainloop_on_tout, NULL);
	g_main_loop_run(Loop);

	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_CANCEL_BROWSE,
					      MAFW_DBUS_UINT32(31337)));

	mockbus_expect(mafw_dbus_reply(c));

	mockbus_deliver(NULL);
	ck_assert(source->cancel_browse_called == 2);
	mockbus_finish();

	/* Get metadata */
	mockbus_incoming(c =
                         mafw_dbus_method(
                                 MAFW_SOURCE_METHOD_GET_METADATA,
                                 MAFW_DBUS_STRING("testobject"),
                                 MAFW_DBUS_C_STRVZ("title", "artist")));

	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_METADATA(metadata)));

	mockbus_deliver(NULL);
	ck_assert(source->get_metadata_called == 1);

	/* Get metadatas */
	GHashTable *objshash = g_hash_table_new_full(g_str_hash, g_str_equal,
						     NULL, NULL);
	i = 0;
	while (objlist[i])
		g_hash_table_insert(objshash, (gpointer)objlist[i++], NULL);

	const gchar **objs = g_new0(const gchar *, i);
	const gchar **tmp = objs;

	g_hash_table_foreach (objshash, add_metadatas_object, &tmp);
	g_hash_table_destroy(objshash);

	mockbus_incoming(c =
                         mafw_dbus_method(
                                 MAFW_SOURCE_METHOD_GET_METADATAS,
                                 MAFW_DBUS_C_STRVZ("testobject", "testobject2"),
                                 MAFW_DBUS_C_STRVZ("title", "artist")));

	mockbus_expect(mdatas_repl(c, objs, metadata, FALSE));

	mockbus_deliver(NULL);
	ck_assert(source->get_metadatas_called == 1);
	g_free(objs);

	/* Set metadata */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_SET_METADATA,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_METADATA(metadata)));

	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRING("testobject"),
				       MAFW_DBUS_C_STRVZ("pertti", "pasanen")));

	mockbus_deliver(NULL);
	ck_assert(source->set_metadata_called == 1);

	/* Create object */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_CREATE_OBJECT,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_METADATA(metadata)));

	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRING("testobject")));

	mockbus_deliver(NULL);
	ck_assert(source->create_object_called == 1);

	/* Destroy object */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_DESTROY_OBJECT,
					      MAFW_DBUS_STRING("testobject")));

	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRING("testobject")));

	mockbus_deliver(NULL);
	ck_assert(source->destroy_object_called == 1);

	/* Signals */
	mockbus_expect(mafw_dbus_signal(MAFW_SOURCE_SIGNAL_METADATA_CHANGED,
					MAFW_DBUS_STRING("testobj")));
	g_signal_emit_by_name(source, "metadata-changed", "testobj");

	mockbus_expect(mafw_dbus_signal(MAFW_SOURCE_SIGNAL_CONTAINER_CHANGED,
				MAFW_DBUS_STRING("testobj")));
	g_signal_emit_by_name(source, "container-changed", "testobj");

        mockbus_expect(mafw_dbus_signal(MAFW_SOURCE_SIGNAL_UPDATING,
                                        MAFW_DBUS_INT32(25),
                                        MAFW_DBUS_INT32(4),
                                        MAFW_DBUS_INT32(6),
                                        MAFW_DBUS_INT32(12)));
        g_signal_emit_by_name(source, "updating", 25, 4, 6, 12);

	mockbus_finish();
	mafw_metadata_release(metadata);
}
END_TEST

#define MATCH_STR "type='signal',interface='org.freedesktop.DBus'," \
			"member='NameOwnerChanged',arg0='%s',arg2=''"

START_TEST(test_activate_settings)
{
	MockedSource *source;
	MafwRegistry *registry;
	DBusMessage *c;
	GValue v = {0};
	gchar *matchstr;

	registry = mafw_registry_get_instance();
	mockbus_reset();
	wrapper_init();

	source = mocked_source_new("mocksource", "mocksource", Loop);

	mock_appearing_extension(FAKE_SOURCE_SERVICE, FALSE);
	mafw_registry_add_extension(MAFW_REGISTRY(registry),
                                    MAFW_EXTENSION(source));
	g_value_init(&v, G_TYPE_BOOLEAN);

	// asking deactivity	
	g_value_set_boolean(&v, FALSE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);

	// asking activity twice, and then deactivate it
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	g_value_set_boolean(&v, FALSE);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);

	// asking activity, then activating by other one, deactivate by first one, deact by second one
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	msg_sender_id = ":1.104";
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	g_value_set_boolean(&v, FALSE);
	msg_sender_id = ":1.103";
	mockbus_incoming(c =
	mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	msg_sender_id = ":1.104";
	mockbus_incoming(c =
	mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);

	// asking activity, then activating by other one, disconnect by first one, deact by second one
	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	msg_sender_id = ":1.104";
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	g_value_set_boolean(&v, FALSE);
	msg_sender_id = ":1.103";
	mockbus_incoming(c =
	mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
					      DBUS_INTERFACE_DBUS,
					      "NameOwnerChanged",
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING("")));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	msg_sender_id = ":1.104";
	mockbus_incoming(c =
	mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);
	
	// asking activity, then deactivating by other one, disconnect by first one
	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	g_value_set_boolean(&v, FALSE);
	msg_sender_id = ":1.104";
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	msg_sender_id = ":1.103";
	mockbus_incoming(c =
	mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
					      DBUS_INTERFACE_DBUS,
					      "NameOwnerChanged",
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING("")));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);

	/* Creating another source. Reffing both of them, and reffing one of
	them by another UIs, closing the first UI*/
	MockedSource *source2;
	source2 = mocked_source_new("mocksource2", "mocksource2", Loop);

	mock_appearing_extension(MAFW_SOURCE_SERVICE ".mockland." FAKE_SOURCE_NAME "2", FALSE);
	mafw_registry_add_extension(MAFW_REGISTRY(registry),
                                    MAFW_EXTENSION(source2));

	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH "2",
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);

	msg_sender_id = ":1.104";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	msg_sender_id = ":1.103";
	mockbus_incoming(c =
	mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
					      DBUS_INTERFACE_DBUS,
					      "NameOwnerChanged",
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING("")));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != TRUE);

	msg_sender_id = ":1.104";
	g_value_set_boolean(&v, FALSE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);

	/* Reffing both sources, and reffing one of
	them by another UIs, closing the last UI. Unreffing the first source,
	closing UI*/
	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != TRUE);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH "2",
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	msg_sender_id = ":1.104";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);

	msg_sender_id = ":1.104";
	mockbus_incoming(c =
	mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
					      DBUS_INTERFACE_DBUS,
					      "NameOwnerChanged",
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING("")));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);

	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, FALSE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);

	mockbus_incoming(c =
	mafw_dbus_signal_full(NULL, DBUS_PATH_DBUS,
					      DBUS_INTERFACE_DBUS,
					      "NameOwnerChanged",
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING(msg_sender_id),
					      MAFW_DBUS_STRING("")));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != TRUE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != TRUE);


	/* Reffing both sources by a UI, reffing one of them by another UI,
	destroying this source */
	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != TRUE);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH "2",
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);

	msg_sender_id = ":1.104";
	g_value_set_boolean(&v, TRUE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source)->activate_state != FALSE);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);

	mockbus_expect(mafw_dbus_method_full(
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS,
		"ReleaseName",
		MAFW_DBUS_STRING("com.nokia.mafw.source.mockland.mocksource")
		));
	mockbus_reply(MAFW_DBUS_UINT32(4));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mafw_registry_remove_extension(mafw_registry_get_instance(),
					MAFW_EXTENSION(source));

	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != FALSE);
	
	msg_sender_id = ":1.103";
	g_value_set_boolean(&v, FALSE);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH "2",
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING(MAFW_PROPERTY_EXTENSION_ACTIVATE),
				      MAFW_DBUS_GVALUE(&v)));
	matchstr = g_strdup_printf(MATCH_STR, msg_sender_id);
	mockbus_expect(mafw_dbus_method_full(DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS,
						     "RemoveMatch",
						     MAFW_DBUS_STRING(matchstr)));
	g_free(matchstr);
	mockbus_deliver(NULL);
	ck_assert(MOCKED_SOURCE(source2)->activate_state != TRUE);


	mockbus_finish();
}
END_TEST

START_TEST(test_source_errors)
{
	ErrorSource *source;
	MafwRegistry *registry;
	DBusMessage *c;
	GError *error = NULL;
	const gchar *domain_str;
	GHashTable *metadata;
	GHashTable *metadata_empty;

	registry = mafw_registry_get_instance();
	mockbus_reset();
	wrapper_init();

	source = ERROR_SOURCE(error_source_new("mocksource", "mocksource",
                                               Loop));

	mock_appearing_extension(FAKE_SOURCE_SERVICE, FALSE);
	mafw_registry_add_extension(MAFW_REGISTRY(registry),
                                    MAFW_EXTENSION(source));

	g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
		    "Error source fails in everything it does.");
	domain_str = g_quark_to_string(error->domain);
	metadata = mockbus_mkmeta("title", "Easy", NULL);
	metadata_empty = mockbus_mkmeta(NULL);

	/* Browse */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_BROWSE,
					  MAFW_DBUS_STRING("testobject"),
					  MAFW_DBUS_BOOLEAN(FALSE),
					  MAFW_DBUS_STRING("!(rating=sucks)"),
					  MAFW_DBUS_STRING("-year"),
					  MAFW_DBUS_C_STRVZ("title", "artist"),
					  MAFW_DBUS_UINT32(0),
					  MAFW_DBUS_UINT32(11)));
	mockbus_expect(mafw_dbus_gerror(c, error));

	mockbus_deliver(NULL);
	ck_assert(source->browse_called == 1);

	/* Cancel browse */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_CANCEL_BROWSE,
					      MAFW_DBUS_UINT32(31337)));

	mockbus_expect(mafw_dbus_gerror(c, error));

	mockbus_deliver(NULL);
	ck_assert(source->cancel_browse_called == 1);

	/* Get metadata */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_GET_METADATA,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_C_STRVZ("title", "artist")));

	mockbus_expect(mafw_dbus_gerror(c, error));
	mockbus_deliver(NULL);
	ck_assert(source->get_metadata_called == 1);

	/* Set metadata */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_SET_METADATA,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_METADATA(metadata)));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRING("testobject"),
				       MAFW_DBUS_C_STRVZ("pertti", "pasanen"),
				       MAFW_DBUS_STRING(domain_str),
				       MAFW_DBUS_INT32(error->code),
				       MAFW_DBUS_STRING(error->message)));
	mockbus_deliver(NULL);
	ck_assert(source->get_metadata_called == 1);

	/* Create object */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_CREATE_OBJECT,
				      MAFW_DBUS_STRING("testobject"),
				      MAFW_DBUS_METADATA(metadata)));
	mockbus_expect(mafw_dbus_gerror(c, error));

	mockbus_deliver(NULL);
	ck_assert(source->create_object_called == 1);

	/* Destroy object */
	mockbus_incoming(c = mafw_dbus_method(MAFW_SOURCE_METHOD_DESTROY_OBJECT,
					      MAFW_DBUS_STRING("testobject")));

	mockbus_expect(mafw_dbus_gerror(c, error));

	mockbus_deliver(NULL);
	ck_assert(source->destroy_object_called == 1);

	mockbus_finish();
	g_error_free(error);
	mafw_metadata_release(metadata);
	mafw_metadata_release(metadata_empty);
}
END_TEST

static Suite *source_wrapper_suite(void)
{
	Suite *suite;
	TCase *tc_export_unexport, *tc_source_wrapper, *tc_activate, *tc_source_errors;

	suite = suite_create("Source Wrapper");
if(1) {	tc_export_unexport = checkmore_add_tcase(suite,
                                                 "Exporting & unexporting",
                                                 test_export_unexport);
	tcase_set_timeout(tc_export_unexport, 60); }
if(1) {	tc_source_wrapper = checkmore_add_tcase(suite, "Source wrapper",
                                                test_source_wrapper);
	tcase_set_timeout(tc_source_wrapper, 60); }
if(1) {	tc_activate = checkmore_add_tcase(suite, "Extension activate settings",
                                                test_activate_settings);
	tcase_set_timeout(tc_activate, 60); }

if(1) {	tc_source_errors = checkmore_add_tcase(suite, "Source errors",
			    test_source_errors);
	tcase_set_timeout(tc_source_errors, 60); }
	return suite;
}

/*****************************************************************************
 * Test case execution
 *****************************************************************************/

int main(void)
{
	Loop = g_main_loop_new(NULL, FALSE);
	return checkmore_run(srunner_create(source_wrapper_suite()), FALSE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
