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
#include "common/mafw-dbus.h"
#include "common/dbus-interface.h"
#include <libmafw/mafw-metadata-serializer.h>

#define MAFW_DBUS_PATH MAFW_SOURCE_OBJECT "/mocksource"

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
			callback(self, 1408, -1, 0, "testobject", md, user_data,
                                 NULL);
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

static void get_metadatas(MafwSource *self,
			     const gchar **object_ids,
			     const gchar *const *mdkeys,
			     MafwSourceMetadataResultsCb callback,
			     gpointer user_data)
{
	MockedSource* ms = MOCKED_SOURCE(self);

	g_assert(object_ids && object_ids[0] && object_ids[1] &&
                 !object_ids[2]);
	if (callback != NULL)
	{
		GHashTable* md = mafw_metadata_new();
		GHashTable *mdatas = g_hash_table_new_full(
                        g_str_hash,
                        g_str_equal,
                        NULL,
                        (GDestroyNotify)mafw_metadata_release);
		mafw_metadata_add_str(md, "title", "Easy");
		g_hash_table_insert(mdatas, "testobject1", md);
		g_hash_table_insert(mdatas, "testobject", md);
		g_hash_table_ref(md);
		callback(self, mdatas, user_data, NULL);
		g_hash_table_unref(mdatas);
	}

	ms->get_metadatas_called++;
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
	sclass->get_metadatas = get_metadatas;
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

DBusMessage *append_browse_res(DBusMessage *replmsg,
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
			MAFW_DBUS_PATH,
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


	dbus_message_iter_append_basic(&istr, DBUS_TYPE_INT32,
						&remaining_count);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_UINT32, &index);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &object_id);
	mafw_dbus_message_append_array(&istr, DBUS_TYPE_BYTE, ba->len,
						ba->data);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &domain_str);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_UINT32, &errcode);
	dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING, &err_msg);
	g_byte_array_free(ba, TRUE);
	dbus_message_iter_close_container(iter_array, &istr);
	return replmsg;
}

DBusMessage *mdatas_repl(DBusMessage *req, const gchar **objlist,
					GHashTable *metadata,
					gboolean add_error)
{
	DBusMessage *replmsg;
	GByteArray *ba = NULL;
	const gchar *domain_str = "";
	guint errcode = 0;
	const gchar *err_msg = "";
	DBusMessageIter iter_array, iter_msg, istr;

	replmsg = dbus_message_new_method_return(req);
	dbus_message_iter_init_append(replmsg,
					&iter_msg);
	if (metadata)
	{

		dbus_message_iter_open_container(&iter_msg, DBUS_TYPE_ARRAY,
						 "(say)", &iter_array);

		dbus_message_iter_open_container(&iter_array, DBUS_TYPE_STRUCT,
					NULL, &istr);

		ba = mafw_metadata_freeze_bary(metadata);


		dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
						&objlist[0]);
		mafw_dbus_message_append_array(&istr, DBUS_TYPE_BYTE, ba->len,
						ba->data);
		dbus_message_iter_close_container(&iter_array, &istr);

		dbus_message_iter_open_container(&iter_array, DBUS_TYPE_STRUCT,
					NULL, &istr);
		dbus_message_iter_append_basic(&istr, DBUS_TYPE_STRING,
							&objlist[1]);
		mafw_dbus_message_append_array(&istr, DBUS_TYPE_BYTE, ba->len,
						ba->data);
		dbus_message_iter_close_container(&iter_array, &istr);
		g_byte_array_free(ba, TRUE);
		dbus_message_iter_close_container(&iter_msg, &iter_array);
	}

	if (add_error)
	{
		domain_str = "TESTdomain";
		errcode = 10;
		err_msg = METADATAS_ERROR_MSG;
	}
	dbus_message_iter_append_basic(&iter_msg, DBUS_TYPE_STRING,
                                       &domain_str);
	dbus_message_iter_append_basic(&iter_msg, DBUS_TYPE_UINT32, &errcode);
	dbus_message_iter_append_basic(&iter_msg, DBUS_TYPE_STRING, &err_msg);
	return replmsg;
}
