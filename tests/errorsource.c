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

#include "errorsource.h"
#include "mockbus.h"

static void quit_main_loop(MafwSource *self, gchar const *function_name)
{
	ErrorSource* es = ERROR_SOURCE(self);

	if (es->dont_quit == FALSE) {
		g_debug("g_main_loop_quit in '%s'", function_name);
		g_main_loop_quit(es->mainloop);
	}
}

static guint browse(MafwSource* self,
		    const gchar *object_id,
		    gboolean recursive,
		    const MafwFilter *filter,
		    const gchar *sort_criteria,
		    const gchar *const *metadata,
		    guint skip_count,
		    guint item_count,
		    MafwSourceBrowseResultCb callback,
		    gpointer user_data)
{
	ErrorSource* es = ERROR_SOURCE(self);

	if (callback != NULL)
	{
		GError* error = NULL;
		GHashTable *md = mockbus_mkmeta(NULL);
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error source fails in everything it does.");
		callback(self, MAFW_SOURCE_INVALID_BROWSE_ID, 0, 0,
			 "testobject", md, user_data, error);
		mafw_metadata_release(md);
		g_error_free(error);
	}

	es->browse_called++;
	quit_main_loop(self, G_STRFUNC);

	return MAFW_SOURCE_INVALID_BROWSE_ID;
}

static gboolean cancel_browse(MafwSource *self, guint browse_id, GError **error)
{
	ErrorSource* es = ERROR_SOURCE(self);

	g_set_error(error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
		    "Error source fails in everything it does.");

	es->cancel_browse_called++;
	quit_main_loop(self, G_STRFUNC);
	
	return FALSE;
}

static void get_metadata(MafwSource *self,
			     const gchar *object_id,
			     const gchar *const *metadata,
			     MafwSourceMetadataResultCb callback,
			     gpointer user_data)
{
	ErrorSource* es = ERROR_SOURCE(self);

	if (callback != NULL)
	{
		GError* error = NULL;
		GHashTable *md = mockbus_mkmeta(NULL);
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error source fails in everything it does.");
		callback(self, "testobject", md, user_data, error);
		mafw_metadata_release(md);
		g_error_free(error);
	}

	es->get_metadata_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void set_metadata(MafwSource *self, const gchar *object_id,
			     GHashTable *metadata,
			     MafwSourceMetadataSetCb callback,
			     gpointer user_data)
{
	ErrorSource* ms = ERROR_SOURCE(self);

	if (callback != NULL)
	{
		const gchar** failed_keys = (const gchar**) 
			MAFW_SOURCE_LIST("pertti", "pasanen");
		GError* error = NULL;

		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error source fails in everything it does.");
		callback(self, object_id, failed_keys, user_data, error);
		g_error_free(error);
	}

	ms->set_metadata_called++;
	quit_main_loop(self, G_STRFUNC);

}

static void create_object(MafwSource *self, const gchar *parent,
			      GHashTable *metadata,
			      MafwSourceObjectCreatedCb callback,
			      gpointer user_data)
{
	ErrorSource* ms = ERROR_SOURCE(self);

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error source fails in everything it does.");
		callback(self, "testobject", user_data, error);
		g_error_free(error);
	}

	ms->create_object_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void destroy_object(MafwSource *self, const gchar *object_id,
			       MafwSourceObjectDestroyedCb callback,
			       gpointer user_data)
{
	ErrorSource* ms = ERROR_SOURCE(self);

	if (callback != NULL)
	{
		GError* error = NULL;
		g_set_error(&error, MAFW_EXTENSION_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Error source fails in everything it does.");
		callback(self, object_id, user_data, error);
		g_error_free(error);
	}

	ms->destroy_object_called++;
	quit_main_loop(self, G_STRFUNC);
}

/*----------------------------------------------------------------------------
  Error source construction
  ----------------------------------------------------------------------------*/

G_DEFINE_TYPE(ErrorSource, error_source, MAFW_TYPE_SOURCE);

static void error_source_class_init(ErrorSourceClass *klass)
{
	MafwSourceClass *sclass = MAFW_SOURCE_CLASS(klass);

	sclass->browse = browse;
	sclass->cancel_browse = cancel_browse;

	sclass->get_metadata = get_metadata;
	sclass->set_metadata = set_metadata;

	sclass->create_object = create_object;
	sclass->destroy_object = destroy_object;
}

static void error_source_init(ErrorSource *source)
{
	/* NOP */
}

GObject* error_source_new(const gchar *name, const gchar *uuid,
			  GMainLoop *mainloop)
{
	GObject* object;
	object = g_object_new(error_source_get_type(),
			      "plugin", "mockland",
			      "uuid", uuid,
			      "name", name,
			      NULL);
	ERROR_SOURCE(object)->mainloop = mainloop;
	return object;
}
