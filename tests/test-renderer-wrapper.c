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
#include <sys/types.h>
#include <dbus/dbus.h>
#include <glib-object.h>

#include <libmafw/mafw.h>

#include "common/dbus-interface.h"
#include "common/mafw-dbus.h"
#include "mafw-dbus-wrapper/wrapper.h"
#include "libmafw-shared/mafw-proxy-playlist.h"

#include <check.h>
#include <checkmore.h>
#include "mockbus.h"
#include "mockrenderer.h"
#include "errorrenderer.h"

#define MAFW_DBUS_PATH "/com/nokia/mafw/renderer/uuid"
#define MAFW_DBUS_INTERFACE MAFW_RENDERER_INTERFACE
#define FAKE_RENDERER_NAME "uuid"
#define FAKE_RENDERER_SERVICE MAFW_RENDERER_SERVICE ".mockland." \
        FAKE_RENDERER_NAME

static GMainLoop *Loop = NULL;

START_TEST(test_export_unexport)
{
	MafwExtension *extension;

	extension = mocked_renderer_new("name", "uuid", Loop);
	mockbus_reset();
	wrapper_init();
	mock_appearing_extension(FAKE_RENDERER_SERVICE, FALSE);
	mock_services(NULL);
	mafw_registry_add_extension(mafw_registry_get_instance(),
				     extension);

	mock_disappearing_extension(FAKE_RENDERER_SERVICE, FALSE);
	mockbus_reply(MAFW_DBUS_UINT32(4));
	mafw_registry_remove_extension(mafw_registry_get_instance(),
					extension);

	mockbus_finish();
}
END_TEST


START_TEST(test_rendererwrapper)
{
	MockedRenderer *renderer;
	MafwRegistry *reg;
	DBusMessage *c, *listpl;
	GValueArray *value;
	GValue v = {0};
	gpointer pl = mafw_proxy_playlist_new(3);

	value = g_value_array_new(2);
	g_value_init(&v, G_TYPE_UINT);
	g_value_set_uint(&v, 2008);
	g_value_array_append(value, &v);
	g_value_set_uint(&v, 05);
	g_value_array_append(value, &v);

	reg = MAFW_REGISTRY(mafw_registry_get_instance());
	mockbus_reset();
	wrapper_init();

	renderer = mocked_renderer_new("mock-snk", "uuid", Loop);

	mock_appearing_extension(FAKE_RENDERER_SERVICE, FALSE);
	mock_services(NULL);
	mafw_registry_add_extension(reg, MAFW_EXTENSION(renderer));

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY));
	mockbus_expect(mafw_dbus_reply(c));
	g_main_loop_run(Loop);
	ck_assert(renderer->play_called == 1);

	mockbus_expect(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_STATE_CHANGED,
				MAFW_DBUS_INT32(2)));
	g_signal_emit_by_name(G_OBJECT(renderer), "state-changed", 2);

	mockbus_expect(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_ITEM_CHANGED,
				MAFW_DBUS_INT32(2), MAFW_DBUS_STRING("oid")));
	g_signal_emit_by_name(G_OBJECT(renderer), "media-changed", 2, "oid");

	mockbus_expect(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_PLAYLIST_CHANGED,
				MAFW_DBUS_UINT32(3)));
	g_signal_emit_by_name(G_OBJECT(renderer), "playlist-changed", pl);

	mockbus_expect(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_BUFFERING_INFO,
				MAFW_DBUS_DOUBLE(2.2f)));
	g_signal_emit_by_name(G_OBJECT(renderer), "buffering-info", 2.2f);

	mockbus_expect(mafw_dbus_signal(MAFW_RENDERER_SIGNAL_METADATA_CHANGED,
					  MAFW_DBUS_STRING("date"),
					  MAFW_DBUS_GVALUEARRAY(value)));
	g_signal_emit_by_name(G_OBJECT(renderer), "metadata_changed", "date",
				value);


	renderer->get_stat_pl = pl;
	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_GET_STATUS));
	mockbus_expect(mafw_dbus_reply(c,MAFW_DBUS_UINT32(3),
					       MAFW_DBUS_UINT32(42),
					       MAFW_DBUS_INT32(Paused),
					       MAFW_DBUS_STRING("bar")));
	mockbus_deliver(NULL);
	ck_assert(renderer->get_status_called == 1);

	renderer->get_stat_pl = NULL;
	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_GET_STATUS));
	mockbus_expect(mafw_dbus_reply(
                               c,
                               MAFW_DBUS_UINT32(MAFW_PROXY_PLAYLIST_INVALID_ID),
                               MAFW_DBUS_UINT32(42),
                               MAFW_DBUS_INT32(Paused),
                               MAFW_DBUS_STRING("bar")));
	mockbus_deliver(NULL);
	ck_assert(renderer->get_status_called == 2);

	mockbus_incoming(c = mafw_dbus_method(
                                 MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
                                 MAFW_DBUS_UINT32(0)));
	mockbus_expect(mafw_dbus_reply(c));
	mockbus_deliver(NULL);
	ck_assert(renderer->assign_playlist_called == 1);

	mockbus_incoming(c = mafw_dbus_method(
                                 MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
                                 MAFW_DBUS_UINT32(3)));
	mockbus_expect(mafw_dbus_method_full(
                               DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                               DBUS_INTERFACE_DBUS,
                               "StartServiceByName",
                               MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                               MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(DBUS_START_REPLY_SUCCESS));
	mockbus_expect(mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
					MAFW_PLAYLIST_PATH,
					MAFW_PLAYLIST_INTERFACE,
					MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  3)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(3),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	mockbus_expect(mafw_dbus_reply(c));
	mockbus_deliver(NULL);
	ck_assert(renderer->assign_playlist_called == 2);

	mockbus_incoming(c =
                         mafw_dbus_method(MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
                                          MAFW_DBUS_UINT32(4)));
	mockbus_expect(listpl = mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
					MAFW_PLAYLIST_PATH,
					MAFW_PLAYLIST_INTERFACE,
					MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  4)));

	mockbus_reply_msg(mafw_dbus_error(listpl, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND,
					"Hihi"));
	mockbus_expect(mafw_dbus_error(c, MAFW_PLAYLIST_ERROR,
					MAFW_PLAYLIST_ERROR_PLAYLIST_NOT_FOUND,
					"Hihi"));
	mockbus_deliver(NULL);
	ck_assert(renderer->assign_playlist_called == 2);
	/* TODO mockbus_expect(UNAVAILABLE) */
//	g_object_unref(reg);
	g_value_array_free(value);
	g_object_unref(pl);
	mockbus_finish();
}
END_TEST

START_TEST(test_renderer_errors)
{
	ErrorRenderer *renderer;
	MafwRegistry *reg;
	DBusMessage *c;
	GError *eerr = NULL;

	reg = mafw_registry_get_instance();
	mockbus_reset();
	wrapper_init();

	renderer = error_renderer_new("error-snk", "uuid", Loop);

	mock_appearing_extension(FAKE_RENDERER_SERVICE, FALSE);
	mock_services(NULL);
	mafw_registry_add_extension(MAFW_REGISTRY(reg),
                                    MAFW_EXTENSION(renderer));

	g_set_error(&eerr, MAFW_EXTENSION_ERROR,
		    MAFW_EXTENSION_ERROR_FAILED,
		    "Error renderer fails in everything it does.");
	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->play_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY_OBJECT,
				MAFW_DBUS_STRING("BLA")));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->play_object_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_PLAY_URI,
				MAFW_DBUS_STRING("BLA")));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->play_uri_called == 1);

	mockbus_incoming(mafw_dbus_method(MAFW_RENDERER_METHOD_STOP));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->stop_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_PAUSE));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->pause_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_RESUME));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->resume_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_NEXT));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->next_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_PREVIOUS));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->previous_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_GOTO_INDEX,
					      MAFW_DBUS_UINT32(1)));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->goto_index_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_SET_POSITION,
					       MAFW_DBUS_INT32(SeekAbsolute),
					       MAFW_DBUS_INT32(1337)));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->set_position_called == 1);

	mockbus_incoming(c =
                         mafw_dbus_method(MAFW_RENDERER_METHOD_GET_POSITION));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->get_position_called == 1);

	mockbus_incoming(c = mafw_dbus_method(MAFW_RENDERER_METHOD_GET_STATUS));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	g_main_loop_run(Loop);
	ck_assert(renderer->get_status_called == 1);


	mockbus_incoming(c =
                         mafw_dbus_method(MAFW_RENDERER_METHOD_ASSIGN_PLAYLIST,
                                          MAFW_DBUS_UINT32(3)));
	mockbus_expect(mafw_dbus_method_full(
                               DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                               DBUS_INTERFACE_DBUS,
                               "StartServiceByName",
                               MAFW_DBUS_STRING(MAFW_PLAYLIST_SERVICE),
                               MAFW_DBUS_UINT32(0)));
	mockbus_reply(MAFW_DBUS_UINT32(DBUS_START_REPLY_SUCCESS));
	mockbus_expect(mafw_dbus_method_full(MAFW_PLAYLIST_SERVICE,
					MAFW_PLAYLIST_PATH,
					MAFW_PLAYLIST_INTERFACE,
					MAFW_PLAYLIST_METHOD_LIST_PLAYLISTS,
					MAFW_DBUS_C_ARRAY(UINT32,
							  dbus_uint32_t,
							  3)));
	mockbus_reply(MAFW_DBUS_AST("us",
		MAFW_DBUS_STRUCT(
			MAFW_DBUS_UINT32(3),
			MAFW_DBUS_STRING("Entyem-pentyem"))));
	mockbus_expect(mafw_dbus_gerror(c, eerr));
	mockbus_deliver(NULL);
	ck_assert(renderer->assign_playlist_called == 1);
	mockbus_finish();
	g_error_free(eerr);
}
END_TEST

static gboolean set_ext_called;
static void dummy_set_ext_prop(MafwExtension *self, const gchar *name,
				  const GValue *value)
{
	ck_assert(!set_ext_called);
	set_ext_called = TRUE;
}

static gboolean get_ext_called;
static void dummy_get_ext_prop(MafwExtension *self, const gchar *name,
                               MafwExtensionPropertyCallback cb,
                               gpointer udata)
{
	GValue *value = NULL;

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_INT);
	g_value_set_int(value, 12345);

	cb(self, name, value, udata, NULL);

	ck_assert(!get_ext_called);
	get_ext_called = TRUE;
}

static gboolean name_chd_called;
static void name_changed_cb(MafwExtension *extension, GParamSpec *pspec,
				gpointer udata)
{
	gchar *name;

	g_object_get(extension, "name", &name, NULL);
	ck_assert(!strcmp(name, "fakename"));
	g_free(name);
	ck_assert(!name_chd_called);
	name_chd_called = TRUE;
}

START_TEST(test_extension)
{
	MockedRenderer *renderer;
	MafwRegistry *reg;
	DBusMessage *c;
	GValue v = {0};
	gchar *names[] = {"fakeprop", NULL};
	GType types[] = {G_TYPE_INT};

	g_value_init(&v, G_TYPE_INT);
	g_value_set_int(&v, 12345);

	reg = MAFW_REGISTRY(mafw_registry_get_instance());
	mockbus_reset();
	wrapper_init();

	renderer = mocked_renderer_new("mock-snk", "uuid", Loop);
	g_signal_connect(renderer, "notify::name", (GCallback)name_changed_cb,
				NULL);

	mock_appearing_extension(FAKE_RENDERER_SERVICE, FALSE);
	mock_services(NULL);
	mafw_registry_add_extension(reg, MAFW_EXTENSION(renderer));

	MAFW_EXTENSION_CLASS(G_OBJECT_GET_CLASS(renderer))->
				set_extension_property =
						dummy_set_ext_prop;
	MAFW_EXTENSION_CLASS(G_OBJECT_GET_CLASS(renderer))->
				get_extension_property =
						dummy_get_ext_prop;
	mafw_extension_add_property(MAFW_EXTENSION(renderer), "fakeprop",
					G_TYPE_INT);
	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_PROPERTY,
				      MAFW_DBUS_STRING("fakeprop"),
				      MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(set_ext_called);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_GET_PROPERTY,
				      MAFW_DBUS_STRING("fakeprop")));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRING("fakeprop"),
                                       MAFW_DBUS_GVALUE(&v)));
	mockbus_deliver(NULL);
	ck_assert(get_ext_called);
	get_ext_called = FALSE;

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_GET_PROPERTY,
				      MAFW_DBUS_STRING("fakepr")));
	mockbus_expect(mafw_dbus_error(c, MAFW_EXTENSION_ERROR,
				  MAFW_EXTENSION_ERROR_INVALID_PROPERTY,
				  "Unknown property: fakepr"));
	mockbus_deliver(NULL);
	ck_assert(!get_ext_called);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_LIST_PROPERTIES));
	if (sizeof(GType) == sizeof(guint32))
	{
		mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRVZ(names),
					       DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
					       types, 1));
	}
	else
	{
		mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRVZ(names),
					       DBUS_TYPE_ARRAY, DBUS_TYPE_UINT64,
					       types, 1));
	}

	mockbus_deliver(NULL);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_SET_NAME,
				      MAFW_DBUS_STRING("fakename")));
	mockbus_expect(mafw_dbus_signal_full(NULL,
				MAFW_DBUS_PATH,
				MAFW_EXTENSION_INTERFACE,
				MAFW_EXTENSION_SIGNAL_NAME_CHANGED,
				MAFW_DBUS_STRING("fakename")));
	mockbus_deliver(NULL);
	ck_assert(name_chd_called);

	mockbus_incoming(c =
		mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,
				      MAFW_EXTENSION_INTERFACE,
				      MAFW_EXTENSION_METHOD_GET_NAME));
	mockbus_expect(mafw_dbus_reply(c, MAFW_DBUS_STRING("fakename")));
	mockbus_deliver(NULL);

	mockbus_expect(
                mafw_dbus_signal_full(
                        NULL,
                        MAFW_DBUS_PATH,
                        MAFW_EXTENSION_INTERFACE,
                        MAFW_EXTENSION_SIGNAL_ERROR,
                        MAFW_DBUS_STRING("com.nokia.mafw.error.extension"),
                        MAFW_DBUS_INT32(3),
                        MAFW_DBUS_STRING("message")));
	g_signal_emit_by_name((gpointer)renderer, "error",
				MAFW_EXTENSION_ERROR,
				3,
				"message");

	mockbus_expect(mafw_dbus_signal_full(NULL,
				MAFW_DBUS_PATH,
				MAFW_EXTENSION_INTERFACE,
				MAFW_EXTENSION_SIGNAL_PROPERTY_CHANGED,
				MAFW_DBUS_STRING("fakeprop"),
				MAFW_DBUS_GVALUE(&v)));
	mafw_extension_emit_property_changed(MAFW_EXTENSION(renderer),
				"fakeprop",
				&v);

	mockbus_finish();
}
END_TEST

static Suite *rendererwrapper_suite(void)
{
	Suite *suite;
	TCase *tc_rendererwrapper, *tc_export_unexport, *tc_renderer_errors,
			*tc_extension;

	suite = suite_create("Renderer wrapper");
if (1) {
	tc_export_unexport = checkmore_add_tcase(suite,
                                                 "Exporting & unexporting",
                                                 test_export_unexport);
	tcase_set_timeout(tc_export_unexport, 60);
}
if (1) {
	tc_rendererwrapper = checkmore_add_tcase(suite, "Renderer dbus api",
			    test_rendererwrapper);
	tcase_set_timeout(tc_rendererwrapper, 60);
}
if (1) {
	tc_renderer_errors = checkmore_add_tcase(suite, "Renderer errors",
			    test_renderer_errors);
	tcase_set_timeout(tc_renderer_errors, 60);
}
if (1) {
	tc_extension = checkmore_add_tcase(suite, "Extension",
			    test_extension);
	tcase_set_timeout(tc_extension, 60);
}
	/*valgrind needs more time*/

	return suite;
}

/*****************************************************************************
 * Test case execution
 *****************************************************************************/

int main(void)
{
	Loop = g_main_loop_new(NULL, FALSE);
	return checkmore_run(srunner_create(rendererwrapper_suite()), FALSE);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
