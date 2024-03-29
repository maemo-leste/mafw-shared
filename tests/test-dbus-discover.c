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
#include <signal.h>
#include <libmafw/mafw-registry.h>

#include <glib.h>

#include <check.h>
#include "libmafw-shared/mafw-proxy-renderer.h"
#include "libmafw-shared/mafw-shared.h"
#include "common/dbus-interface.h"
#include "common/mafw-util.h"
#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

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

#define FAKE_SOURCE_NAME "DEADBEEF"
#define FAKE_RENDERER_NAME "CAFEBABE"

#define FAKE_RENDERER_SERVICE MAFW_RENDERER_SERVICE ".fake." FAKE_RENDERER_NAME
#define FAKE_SOURCE_SERVICE MAFW_SOURCE_SERVICE ".fake." FAKE_SOURCE_NAME
#define FAKE_RENDERER_OBJECT MAFW_RENDERER_OBJECT "/" FAKE_RENDERER_NAME
#define FAKE_SOURCE_OBJECT MAFW_SOURCE_OBJECT "/" FAKE_SOURCE_NAME

#define FAKE_PLUGIN_NAME "test_plugin"
#define FAKE_PLUGIN_SERVICE MAFW_PLUGIN_SERVICE "." FAKE_PLUGIN_NAME

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "test-dbus-discover"

static GMainLoop *Mainloop;

START_TEST(test_construct_nonempty)
{
	MafwRegistry *reg;
	gpointer extension;
	const gchar *extensions[] = {FAKE_RENDERER_SERVICE,
				FAKE_SOURCE_SERVICE,
				NULL};

	/* We don't use the singleton registry instance, because that does not
	 * work with NOFORK mode.  mockbus_reset() clears associated handlers,
	 * and it will not be recreated at later occasions. */
	mock_services(extensions);
	mock_empty_props(FAKE_RENDERER_SERVICE, FAKE_RENDERER_OBJECT);
	mock_empty_props(FAKE_SOURCE_SERVICE, FAKE_SOURCE_OBJECT);

	mafw_shared_deinit();
	reg = g_object_new(MAFW_TYPE_REGISTRY, NULL);
	ck_assert(mafw_shared_init(reg, NULL));

	ck_assert_int_eq(g_list_length(mafw_registry_get_renderers(reg)), 1);
	ck_assert_int_eq(g_list_length(mafw_registry_get_sources(reg)), 1);

	extension = mafw_registry_get_extension_by_uuid(reg,
                                                        FAKE_RENDERER_NAME);
	ck_assert(extension != NULL);
	ck_assert(!strcmp("fake", mafw_extension_get_plugin(extension)));
	ck_assert(!strcmp(FAKE_RENDERER_NAME, mafw_extension_get_uuid(extension)));
	ck_assert(!strcmp(FAKE_NAME, mafw_extension_get_name(extension)));

	extension = mafw_registry_get_extension_by_uuid(reg, FAKE_SOURCE_NAME);
	ck_assert(extension != NULL);
	ck_assert(!strcmp("fake", mafw_extension_get_plugin(extension)));
	ck_assert(!strcmp(FAKE_SOURCE_NAME, mafw_extension_get_uuid(extension)));
	ck_assert(!strcmp(FAKE_NAME, mafw_extension_get_name(extension)));
	mafw_shared_deinit();
	g_object_unref(reg);
	mockbus_finish();
}
END_TEST

static void source_cb(MafwRegistry *reg, MafwSource *src, gint *ncalled)
{
	ck_assert(MAFW_IS_SOURCE(src));
	ck_assert(!strcmp(mafw_extension_get_uuid(MAFW_EXTENSION(src)),
                       FAKE_SOURCE_NAME));
	(*ncalled)++;
	if (*ncalled == 2)
		g_main_loop_quit(Mainloop);
}

static void renderer_cb(MafwRegistry *reg, MafwRenderer *renderer,
                        gint *ncalled)
{
	ck_assert(MAFW_IS_RENDERER(renderer));
	ck_assert(!strcmp(mafw_extension_get_uuid(MAFW_EXTENSION(renderer)),
                       FAKE_RENDERER_NAME));
	(*ncalled)++;
	if (*ncalled == 2)
		g_main_loop_quit(Mainloop);
}

START_TEST(test_registration)
{
	MafwRegistry *reg;
	gint nadded, nremoved;

	/* 1. no services initially, then a renderer and source appear. */
	mock_services(NULL);
	mafw_shared_deinit();
	reg = g_object_new(MAFW_TYPE_REGISTRY, NULL);
	ck_assert(mafw_shared_init(reg, NULL));

	ck_assert_int_eq(g_list_length(mafw_registry_get_renderers(reg)), 0);
	ck_assert_int_eq(g_list_length(mafw_registry_get_sources(reg)), 0);

	nadded = nremoved = 0;
	g_signal_connect(reg, "source-added",
			 G_CALLBACK(source_cb), &nadded);
	g_signal_connect(reg, "source-removed",
			 G_CALLBACK(source_cb), &nremoved);
	g_signal_connect(reg, "renderer-added",
			 G_CALLBACK(renderer_cb), &nadded);
	g_signal_connect(reg, "renderer-removed",
			 G_CALLBACK(renderer_cb), &nremoved);

	mock_appearing_extension(FAKE_RENDERER_SERVICE, TRUE);
	mock_empty_props(FAKE_RENDERER_SERVICE, FAKE_RENDERER_OBJECT);
	mock_appearing_extension(FAKE_SOURCE_SERVICE, TRUE);
	mock_empty_props(FAKE_SOURCE_SERVICE, FAKE_SOURCE_OBJECT);
	g_main_loop_run(Mainloop);

	ck_assert(nadded == 2 && nremoved == 0);
	ck_assert_int_eq(g_list_length(mafw_registry_get_renderers(reg)), 1);
	ck_assert_int_eq(g_list_length(mafw_registry_get_sources(reg)), 1);

	/* 2. the previous two extensions go away */
	nadded = nremoved = 0;
	mock_disappearing_extension(FAKE_RENDERER_SERVICE, TRUE);
	mock_disappearing_extension(FAKE_SOURCE_SERVICE, TRUE);
	g_main_loop_run(Mainloop);

	ck_assert(nadded == 0 && nremoved == 2);
	ck_assert(!g_list_length(mafw_registry_get_renderers(reg)) != 0);
	ck_assert(!g_list_length(mafw_registry_get_sources(reg)) != 0);

	/* 3. cope with invalid messages */
	nadded = nremoved = 0;
	checkmore_ignore("extension with invalid service name*");
	mock_appearing_extension(MAFW_RENDERER_SERVICE, TRUE);
	mock_appearing_extension(MAFW_RENDERER_SERVICE "..", TRUE);
	mock_appearing_extension(MAFW_RENDERER_SERVICE "...", TRUE);
	mock_appearing_extension(MAFW_RENDERER_SERVICE "..alpha.beta", TRUE);
	mock_appearing_extension(MAFW_RENDERER_SERVICE ".parara", TRUE);
	mock_appearing_extension(MAFW_SOURCE_SERVICE ".parara.", TRUE);
	g_timeout_add(500, (GSourceFunc)g_main_loop_quit, Mainloop);
	g_main_loop_run(Mainloop);
	ck_assert(nadded == 0 && nremoved == 0);
	mafw_shared_deinit();
	mockbus_finish();
}
END_TEST

/* Fixtures. */
static void setup(void)
{
	Mainloop = g_main_loop_new(NULL, FALSE);
	mockbus_reset();
}

static void teardown(void)
{
	mockbus_finish();
	g_main_loop_unref(Mainloop);
}

int main(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("Mafw dbus discover");
	tc = tcase_create("tests");
	suite_add_tcase(suite, tc);
if (1)	tcase_add_test(tc, test_construct_nonempty);
if (1)	tcase_add_test(tc, test_registration);
	tcase_add_checked_fixture(tc, setup, teardown);

	return checkmore_run(srunner_create(suite), FALSE);
}
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
