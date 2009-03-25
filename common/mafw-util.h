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

#ifndef __MAFW_UTIL_H__
#define __MAFW_UTIL_H__

#include <glib.h>

/* void * <-> GList convenience functions */
extern GList *mafw_util_array_to_glist(void **array);
extern GList *mafw_util_array_to_glist_n(void **array, guint length);
extern GList *mafw_util_array_to_glist_v(void *first, ...);
extern void **mafw_util_glist_to_array(GList *list, guint *length);

#endif
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
