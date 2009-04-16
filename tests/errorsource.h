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

#ifndef ERRORSOURCE_H
#define ERRORSOURCE_H

#include <glib-object.h>
#include <libmafw/mafw.h>
#include <libmafw/mafw-source.h>

#define ERROR_SOURCE(o)							\
	(G_TYPE_CHECK_INSTANCE_CAST((o),				\
				    error_source_get_type(),		\
				    ErrorSource))

typedef struct {
	MafwSourceClass parent;
} ErrorSourceClass;

typedef struct {
	MafwSource parent;

	int browse_called, cancel_browse_called, get_metadata_called,
		get_metadatas_called,
		set_metadata_called, create_object_called,
		destroy_object_called;

	GMainLoop *mainloop;
	gboolean dont_quit;
} ErrorSource;

GType error_source_get_type(void);
GObject* error_source_new(const gchar *name, const gchar *uuid,
			  GMainLoop *mainloop);
#endif
