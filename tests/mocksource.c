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

#include "mocksource.h"

/* Mocked renderer
 *
 * has looots of variables which are set when the appropriate method
 * is invoked, and can be checked later
 * parameter validation?
 */

static void quit_main_loop(MafwSource *self, gchar const *function_name)
{
	MockedSource* ms = (MockedSource*) self;

	if (ms->dont_quit == FALSE) {
		g_debug("g_main_loop_quit in '%s'", function_name);
		g_main_loop_quit(ms->mainloop);
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
	MockedSource* ms = MOCKED_SOURCE(self);

	if (callback != NULL)
	{
		GHashTable* md = mafw_metadata_new();
		mafw_metadata_add_str(md, "title", "Easy");
		do
		{
			callback(self, 1408, -1, 0, "testobject", md, user_data, NULL);
			if (ms->repeat_browse)
				ms->repeat_browse--;
		} while (ms->repeat_browse);
		if (!ms->dont_send_last)
			callback(self, 1408, 0, 0, NULL, md, user_data, NULL);
		g_hash_table_unref(md);
	}

	ms->browse_called++;
	quit_main_loop(self, G_STRFUNC);

	return 1408;
}

static gboolean cancel_browse(MafwSource *self, guint browse_id, GError **error)
{
	MockedSource* ms = MOCKED_SOURCE(self);

	ms->cancel_browse_called++;
	quit_main_loop(self, G_STRFUNC);

	return TRUE;
}

static void get_metadata(MafwSource *self,
			     const gchar *object_id,
			     const gchar *const *mdkeys,
			     MafwSourceMetadataResultCb callback,
			     gpointer user_data)
{
	MockedSource* ms = MOCKED_SOURCE(self);

	if (callback != NULL)
	{
		GHashTable* md = mafw_metadata_new();
		mafw_metadata_add_str(md, "title", "Easy");
		callback(self, object_id, md, user_data, NULL);
		g_hash_table_unref(md);
	}

	ms->get_metadata_called++;
	quit_main_loop(self, G_STRFUNC);

}

static void set_metadata(MafwSource *self, const gchar *object_id,
			     GHashTable *metadata,
			     MafwSourceMetadataSetCb callback,
			     gpointer user_data)
{
	MockedSource* ms = MOCKED_SOURCE(self);

	if (callback != NULL)
	{
		const gchar** failed_keys = (const gchar**) 
			MAFW_SOURCE_LIST("pertti", "pasanen");
		callback(self, object_id, failed_keys, user_data, NULL);
	}

	ms->set_metadata_called++;
	quit_main_loop(self, G_STRFUNC);

}

static void create_object(MafwSource *self, const gchar *parent,
			      GHashTable *metadata,
			      MafwSourceObjectCreatedCb callback,
			      gpointer user_data)
{
	MockedSource* ms = MOCKED_SOURCE(self);

	if (callback != NULL)
		callback(self, "testobject", user_data, NULL);

	ms->create_object_called++;
	quit_main_loop(self, G_STRFUNC);
}

static void destroy_object(MafwSource *self, const gchar *object_id,
			       MafwSourceObjectDestroyedCb callback,
			       gpointer user_data)
{
	MockedSource* ms = MOCKED_SOURCE(self);

	if (callback != NULL)
		callback(self, object_id, user_data, NULL);

	ms->destroy_object_called++;
	quit_main_loop(self, G_STRFUNC);
}

/*----------------------------------------------------------------------------
  Mocked source construction
  ----------------------------------------------------------------------------*/

G_DEFINE_TYPE(MockedSource, mocked_source, MAFW_TYPE_SOURCE);

static void mocked_source_class_init(MockedSourceClass *klass)
{
	MafwSourceClass *sclass = MAFW_SOURCE_CLASS(klass);

	sclass->browse = browse;
	sclass->cancel_browse = cancel_browse;

	sclass->get_metadata = get_metadata;
	sclass->set_metadata = set_metadata;

	sclass->create_object = create_object;
	sclass->destroy_object = destroy_object;
}

static void mocked_source_init(MockedSource *source)
{
	/* NOP */
}

gpointer mocked_source_new(const gchar *name, const gchar *uuid,
			   GMainLoop *mainloop)
{
	MockedSource *ms;
	
	ms = g_object_new(mocked_source_get_type(),
			  "plugin", "mockland",
			  "uuid", uuid,
			  "name", name,
			  NULL);
	ms->mainloop = mainloop;
	return ms;
}
