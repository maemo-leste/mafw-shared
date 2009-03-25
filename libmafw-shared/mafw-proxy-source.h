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

#ifndef __MAFW_PROXY_SOURCE_H__
#define __MAFW_PROXY_SOURCE_H__

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <glib.h>
#include <glib-object.h>

#include <libmafw/mafw-source.h>

G_BEGIN_DECLS

#define MAFW_TYPE_PROXY_SOURCE \
        (mafw_proxy_source_get_type())
#define MAFW_PROXY_SOURCE(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), MAFW_TYPE_PROXY_SOURCE, MafwProxySource))
#define MAFW_IS_PROXY_SOURCE(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), MAFW_TYPE_PROXY_SOURCE))
#define MAFW_PROXY_SOURCE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), MAFW_TYPE_PROXY_SOURCE, MafwProxySourceClass))
#define MAFW_PROXY_SOURCE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), MAFW_TYPE_PROXY_SOURCE, MafwProxySourceClass))
#define MAFW_IS_PROXY_SOURCE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), MAFW_TYPE_PROXY_SOURCE))

typedef struct _MafwProxySource MafwProxySource;
typedef struct _MafwProxySourcePrivate MafwProxySourcePrivate;

struct _MafwProxySource {
	MafwSource parent;
	MafwProxySourcePrivate *priv;
};

typedef struct _MafwProxySourceClass MafwProxySourceClass;
struct _MafwProxySourceClass {
	MafwSourceClass parent;
};

GType mafw_proxy_source_get_type(void);

/**
 * Create a new MafwProxySource object
 */
GObject *mafw_proxy_source_new(const gchar *uuid, const gchar *plugin,
				MafwRegistry *registry);


G_END_DECLS
#endif				/* __MAFW_PROXY_SOURCE_H__ */
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
