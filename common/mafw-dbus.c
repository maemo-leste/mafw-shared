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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#define DBUS_API_SUBJECT_TO_CHANGE

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libmafw/mafw-errors.h>

#include "mafw-dbus.h"
#include <libmafw/mafw-metadata-serializer.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mafw-dbus"

/* Default values if one doesn't #define these as macros.
 * TODO see if gcc's hidden visibility helps about these a bit. */
char const * const MAFW_DBUS_INTERFACE = 0;
char const * const MAFW_DBUS_DESTINATION = 0;
char const * const MAFW_DBUS_PATH = 0;

/* Static prototypes. */
static gboolean clever_append_more_valist(DBusMessageIter *iter,
					  int first_arg_t, va_list *args);
#if MAFW_DEBUG
static char const *msg_info(DBusMessage *msg);
#endif

/* Aborts the program with some explanation. */
static __attribute__((noreturn)) void _die(char const *cry, DBusError *err)
{
	if (err && dbus_error_is_set(err))
		g_error("%s\n%s: %s)", cry, err->name, err->message);
	else
		g_error("%s", cry);
	/* g_error should abort... */
	g_assert_not_reached();
}

/* Stolen from D-Bus internals. */
typedef union
{
	dbus_int16_t  i16;
	dbus_uint16_t u16;
	dbus_int32_t  i32;
	dbus_uint32_t u32;
#ifdef DBUS_HAVE_INT64
	dbus_int64_t  i64;
	dbus_uint64_t u64;
#else
	struct {
		char eightbytes[8];
	};
#endif
	double dbl;
	unsigned char byt;
	char *str;
} DBusBasicValue;

/*
 * Appends an array (@values) of length @nelem consisting of @eltype
 * typed elements to @iter.  Handles arrays of basic types and and
 * arrays of strings (just like libdbus).
 */
gboolean mafw_dbus_message_append_array(DBusMessageIter *iter,
				    int eltype, int nelem,
				    void *values)
{
	DBusMessageIter sub;
	char esig[2];

	esig[0] = (char)eltype;
	esig[1] = '\0';
	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					      esig, &sub))
		return FALSE;
	if (dbus_type_is_fixed(eltype)) {
		if (!dbus_message_iter_append_fixed_array(&sub, eltype,
							  &values, nelem))
			return FALSE;
	} else if (eltype == DBUS_TYPE_STRING ||
		   eltype == DBUS_TYPE_SIGNATURE ||
		   eltype == DBUS_TYPE_OBJECT_PATH)
	{
		char **aval;
		int i;

		aval = values;
		for (i = 0; i < nelem; ++i)
			if (!dbus_message_iter_append_basic(&sub, eltype,
							    &aval[i]))
				return FALSE;
	}
	return dbus_message_iter_close_container(iter, &sub);
}

/* Appends a DBUS_TYPE_ARRAY of DBUS_TYPE_STRUCT whose signature is
 * the first of $args to $imsg.  $args is consumed until the closing
 * DBUS_TYPE_INVALID. */
static gboolean mafw_dbus_message_append_array_struct(DBusMessageIter *imsg,
					   va_list *args)
{
	int first;
	DBusMessageIter iary, istr;

	/* ``Lord, these people haven't heard of longjmp()!''
	 *				---Hackleberry Finn */
	if (!dbus_message_iter_open_container(imsg, DBUS_TYPE_ARRAY,
					      va_arg(*args, const gchar *),
					      &iary))
		return FALSE;
	while ((first = va_arg(*args, int)) != DBUS_TYPE_INVALID) {
		if (!dbus_message_iter_open_container(&iary, DBUS_TYPE_STRUCT,
						      NULL, &istr))
			return FALSE;
		if (!clever_append_more_valist(&istr, first, args))
			return FALSE;
		if (!dbus_message_iter_close_container(&iary, &istr))
			return FALSE;
	}
	if (!dbus_message_iter_close_container(imsg, &iary))
		return FALSE;

	return TRUE;
}

/* Appends #GValue (@gval) to the message (@iter). */
static gboolean clever_append_gvalue(DBusMessageIter *iter,
				     GValue *gval)
{
	DBusMessageIter sub;
	int etype;
	char esig[2];
	DBusBasicValue val;

	/* NOTE We have to have an 1-to-1 mapping of GTypes and D-Bus types,
	 * otherwise you may put in something and don't know what to get
	 * out... */
	switch (G_VALUE_TYPE(gval)) {
	case G_TYPE_CHAR:
		etype = DBUS_TYPE_BYTE;
		val.byt = g_value_get_char(gval);
		break;
	case G_TYPE_BOOLEAN:
		etype = DBUS_TYPE_BOOLEAN;
		val.u32 = g_value_get_boolean(gval);
		break;
	case G_TYPE_INT:
		etype = DBUS_TYPE_INT32;
		val.i32 = g_value_get_int(gval);
		break;
	case G_TYPE_UINT:
		etype = DBUS_TYPE_UINT32;
		val.u32 = g_value_get_uint(gval);
		break;
	case G_TYPE_INT64:
		etype = DBUS_TYPE_INT64;
		val.i64 = g_value_get_int64(gval);
		break;
	case G_TYPE_UINT64:
		etype = DBUS_TYPE_UINT64;
		val.i64 = g_value_get_uint64(gval);
		break;
	case G_TYPE_DOUBLE:
		etype = DBUS_TYPE_DOUBLE;
		val.dbl = g_value_get_double(gval);
		break;
	case G_TYPE_STRING:
		etype = DBUS_TYPE_STRING;
		val.str = (char *)g_value_get_string(gval);
		break;
	default:
		g_warning("Unsupported GValue of type: %d",
			  (int)G_VALUE_TYPE(gval));
		return FALSE;
		break;
	}
	esig[0] = (char)etype;
	esig[1] = '\0';
	if (!dbus_message_iter_open_container(iter,
					      DBUS_TYPE_VARIANT, esig, &sub))
		return FALSE;
	dbus_message_iter_append_basic(&sub, etype, &val);
	if (!dbus_message_iter_close_container(iter, &sub))
		return FALSE;
	return TRUE;
}

/* Consumes $args until a closing DBUS_TYPE_INVALID is found and appends
 * the appropriate arguments to $iter.  $args is updated. */
static gboolean clever_append_more_valist(DBusMessageIter *iter,
					  int first_arg_t, va_list *args)
{
	int arg_t;

	arg_t = first_arg_t;
	while (arg_t != DBUS_TYPE_INVALID) {
		if (arg_t == MAFW_DBUS_TYPE_STRVZ) {
			/* MAFW_DBUS_TYPE_STRVZ, (char **) */
			char **v;
			int nlen;

			v = va_arg(*args, char **);
			nlen = 0;
			if (v)
				for (nlen = 0; v[nlen]; ++nlen);
			if (!mafw_dbus_message_append_array(iter,
                                                            DBUS_TYPE_STRING,
                                                            nlen, v))
				goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_GBYTEARRAY) {
			/* MAFW_DBUS_TYPE_GBYTEARRAY, (GByteArray *) */
			GByteArray *ba;

			ba = va_arg(*args, GByteArray *);
			if (!mafw_dbus_message_append_array(iter, DBUS_TYPE_BYTE,
						ba->len, ba->data))
				goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_METADATA) {
			/* MAFW_DBUS_TYPE_METADATA, (GHashTable *) */
			gboolean isok;
			GHashTable *ht;
			GByteArray *ba;

			ht = va_arg(*args, GHashTable *);
			ba = mafw_metadata_freeze_bary(ht);
			isok = mafw_dbus_message_append_array(iter,
                                                              DBUS_TYPE_BYTE,
                                                              ba->len,
                                                              ba->data);
			g_byte_array_free(ba, TRUE);
			if (!isok) goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_GVALUE) {
			/* MAFW_DBUS_TYPE_GVALUE, (GValue *) */
			GValue *gval;

			gval = va_arg(*args, GValue *);
			if (!clever_append_gvalue(iter, gval))
				goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_GVALUEARRAY) {
			/* MAFW_DBUS_TYPE_GVALUEARRAY (GValueArray *) */
			GValueArray *varr;
			guint i;

			varr = va_arg(*args, GValueArray *);
			if (!dbus_message_iter_append_basic(iter,
							    DBUS_TYPE_UINT32,
							    &varr->n_values))
				goto fail;
			for (i = 0; i < varr->n_values; ++i) {
				GValue *gval;

				gval = g_value_array_get_nth(varr, i);
				if (!clever_append_gvalue(iter, gval))
					goto fail;
			}
		} else if (dbus_type_is_basic(arg_t)) {
			DBusBasicValue val;

			switch (arg_t) {
			case DBUS_TYPE_INT16:
				val.i16 = va_arg(*args, dbus_int32_t);
				break;
			case DBUS_TYPE_UINT16:
				val.u16 = va_arg(*args, dbus_uint32_t);
				break;
			case DBUS_TYPE_INT32:
				val.i32 = va_arg(*args, dbus_int32_t);
				break;
			case DBUS_TYPE_BOOLEAN:
			case DBUS_TYPE_UINT32:
				val.u32 = va_arg(*args, dbus_uint32_t);
				break;
			case DBUS_TYPE_INT64:
				val.i64 = va_arg(*args, dbus_int64_t);
				break;
			case DBUS_TYPE_UINT64:
				val.u64 = va_arg(*args, dbus_uint64_t);
				break;
			case DBUS_TYPE_BYTE:
				val.byt = va_arg(*args, int);
				break;
			case DBUS_TYPE_OBJECT_PATH:
			case DBUS_TYPE_SIGNATURE:
			case DBUS_TYPE_STRING:
				val.str = va_arg(*args, char *);
				break;
			case DBUS_TYPE_DOUBLE:
				val.dbl = va_arg(*args, double);
				break;
			default:
				g_warning("Unhandled basic type: %d", arg_t);
				goto fail;
				break;
			}
			if (!dbus_message_iter_append_basic(iter, arg_t, &val))
				goto fail;
		} else if (arg_t == DBUS_TYPE_ARRAY) {
			int etype, len;
			void *aval;

			etype = va_arg(*args, int);
			if (etype != DBUS_TYPE_STRUCT) {
				aval = va_arg(*args, void *);
				len = va_arg(*args, int);
				if (!mafw_dbus_message_append_array(iter, etype,
							 len, aval))
					goto fail;
			} else if (!mafw_dbus_message_append_array_struct(iter,
                                                                          args))
				goto fail;
		} else {
			g_warning("Unknown type: %d", arg_t);
			goto fail;
		}
		arg_t = va_arg(*args, int);
	}
	return TRUE;
fail:
	return FALSE;
}

/*
 * Similar to dbus_message_append_args_valist() but knows more types
 * specific to MAFW (GByteArray, metadata hashtable, zero terminated
 * string array), and also does not require to pass pointers.
 * Example:
 *
 * mafw_dbus_method("kungfu",
 * 	MAFW_DBUS_STRVZ(lol),
 * 	MAFW_DBUS_INT(remaining_count));
 */
static gboolean clever_append_valist(DBusMessage *msg,
				     int first_arg_t, va_list args)
{
	DBusMessageIter iter;

	dbus_message_iter_init_append(msg, &iter);
	return clever_append_more_valist(&iter, first_arg_t, &args);
}

static gboolean clever_parse_strvz(DBusMessageIter *iter, char ***values)
{
	DBusMessageIter sub;
	int eltype;
	GPtrArray *pa;

	g_assert(values);
	eltype = dbus_message_iter_get_element_type(iter);
	if (eltype == DBUS_TYPE_INVALID) {
		/* Empty */
		*values = NULL;
		return TRUE;
	}
	if (eltype != DBUS_TYPE_STRING &&
	    eltype != DBUS_TYPE_SIGNATURE &&
	    eltype != DBUS_TYPE_OBJECT_PATH) {
		g_warning("A string array was expected");
		return FALSE;
	}
	dbus_message_iter_recurse(iter, &sub);
	pa = g_ptr_array_new();
	while (dbus_message_iter_get_arg_type(&sub) !=
	       DBUS_TYPE_INVALID) {
		char *s;

		dbus_message_iter_get_basic(&sub, &s);
		g_ptr_array_add(pa, g_strdup(s));
		dbus_message_iter_next(&sub);
	}
	g_ptr_array_add(pa, NULL);
	*values = (char **)g_ptr_array_free(pa, FALSE);
	return TRUE;
}

static gboolean clever_parse_gbytearray(DBusMessageIter *iter, GByteArray **bap)
{
	DBusMessageIter sub;
	GByteArray *ba;
	guint8 *data;
	gint len, eltype;

	g_assert(bap);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return FALSE;
	eltype = dbus_message_iter_get_element_type(iter);
	if (eltype == DBUS_TYPE_INVALID) {
		/* Empty */
		*bap = NULL;
		return TRUE;
	}
	dbus_message_iter_recurse(iter, &sub);
	if (eltype != DBUS_TYPE_BYTE) return FALSE;
	dbus_message_iter_get_fixed_array(&sub, &data, &len);
	ba = g_byte_array_new();
	g_byte_array_append(ba, data, len);
	*bap = ba;
	return TRUE;
}

gboolean mafw_dbus_message_parse_metadata(DBusMessageIter *iter,
                                          GHashTable **htp)
{
	DBusMessageIter sub;
	GHashTable *ht;
	guint8 *data;
	gint len, eltype;

	g_assert(htp);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return FALSE;
	eltype = dbus_message_iter_get_element_type(iter);
	if (eltype == DBUS_TYPE_INVALID) {
		/* Empty */
		*htp = NULL;
		return TRUE;
	}
	if (eltype != DBUS_TYPE_BYTE) return FALSE;
	dbus_message_iter_recurse(iter, &sub);
	dbus_message_iter_get_fixed_array(&sub, &data, &len);
	ht = mafw_metadata_thaw((gchar const *)data, len);
	*htp = ht;
	return TRUE;
}

static gboolean clever_parse_gvalue(DBusMessageIter *iter, GValue *gvp)
{
	DBusMessageIter sub;
	int eltype;
	DBusBasicValue v;

	g_assert(gvp);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_VARIANT)
		return FALSE;
	dbus_message_iter_recurse(iter, &sub);
	eltype = dbus_message_iter_get_arg_type(&sub);
	if (eltype == DBUS_TYPE_INVALID) {
		g_warning("Got an empty variant.");
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &v);
	switch (eltype) {
	case DBUS_TYPE_BYTE:
		g_value_init(gvp, G_TYPE_CHAR);
		g_value_set_char(gvp, v.byt);
		break;
	case DBUS_TYPE_BOOLEAN:
		g_value_init(gvp, G_TYPE_BOOLEAN);
		g_value_set_boolean(gvp, v.u32);
		break;
	case DBUS_TYPE_INT32:
		g_value_init(gvp, G_TYPE_INT);
		g_value_set_int(gvp, v.i32);
		break;
	case DBUS_TYPE_UINT32:
		g_value_init(gvp, G_TYPE_UINT);
		g_value_set_uint(gvp, v.u32);
		break;
	case DBUS_TYPE_INT64:
		g_value_init(gvp, G_TYPE_INT64);
		g_value_set_int64(gvp, v.i64);
		break;
	case DBUS_TYPE_UINT64:
		g_value_init(gvp, G_TYPE_UINT64);
		g_value_set_uint64(gvp, v.u64);
		break;
	case DBUS_TYPE_DOUBLE:
		g_value_init(gvp, G_TYPE_DOUBLE);
		g_value_set_double(gvp, v.dbl);
		break;
	case DBUS_TYPE_STRING:
		g_value_init(gvp, G_TYPE_STRING);
		g_value_set_string(gvp, v.str);
		break;
	default:
		g_warning("Unsupported D-Bus type '%c'", eltype);
		return FALSE;
	}
	return TRUE;
}

static gboolean clever_parse_basic(DBusMessageIter *iter, int arg_t, void *lval)
{
	int itype;

	g_assert(lval);
	itype = dbus_message_iter_get_arg_type(iter);
	if (itype != arg_t) {
		g_warning("Actual '%c' and expected '%c' "
			  "argument types mismatch",
			  itype, arg_t);
		return FALSE;
	}
	dbus_message_iter_get_basic(iter, lval);
	return TRUE;
}

static gboolean clever_parse_array(DBusMessageIter *iter, va_list *args)
{
	DBusMessageIter sub;
	int eltype, extype;
	int *nelem;
	void *values;
	GPtrArray *pa;

	extype = va_arg(*args, int);
	values = va_arg(*args, void *);
	g_assert(values);
	nelem = va_arg(*args, int *);
	g_assert(nelem);
	if (dbus_message_iter_get_arg_type(iter) !=
	    DBUS_TYPE_ARRAY) {
		g_warning("Array expected");
		return FALSE;
	}
	eltype = dbus_message_iter_get_element_type(iter);
	if (eltype == DBUS_TYPE_INVALID) {
		/* Empty array. */
		*(DBusBasicValue **)values = NULL;
		*nelem = 0;
		return TRUE;
	}
	if (eltype != extype) {
		g_warning("Actual '%c' and expected '%c' "
			  "argument types mismatch",
			  eltype, extype);
		return FALSE;
	}
	dbus_message_iter_recurse(iter, &sub);
	if (dbus_type_is_fixed(eltype)) {
		dbus_message_iter_get_fixed_array(&sub, values, nelem);
		return TRUE;
	}
	if (eltype != DBUS_TYPE_STRING &&
	    eltype != DBUS_TYPE_SIGNATURE &&
	    eltype != DBUS_TYPE_OBJECT_PATH) {
		g_warning("Arrays of complex types '%c' "
			  "are not supported", eltype);
		return FALSE;
	}
	pa = g_ptr_array_new();
	while (dbus_message_iter_get_arg_type(&sub) !=
	       DBUS_TYPE_INVALID) {
		char *s;

		dbus_message_iter_get_basic(&sub, &s);
		g_ptr_array_add(pa, g_strdup(s));
		dbus_message_iter_next(&sub);
	}
	g_ptr_array_add(pa, NULL);
	*nelem = pa->len - 1;
	*(char ***)values = (char **)g_ptr_array_free(pa, FALSE);
	return TRUE;
}

static gboolean clever_parse_gvaluearray(DBusMessageIter *iter,
					 GValueArray **pvarr)
{
	guint nelem;

	g_assert(pvarr);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_UINT32) {
		g_warning("UINT32 (n_elem) expected");
		return FALSE;
	}
	dbus_message_iter_get_basic(iter, &nelem);
	if (!dbus_message_iter_next(iter))
		return FALSE;
	*pvarr = g_value_array_new(nelem);
	while (nelem-- > 0) {
		GValue v = {0};

		if (!clever_parse_gvalue(iter, &v))
			return FALSE;
		if (nelem > 0 && !dbus_message_iter_next(iter)) {
			g_warning("Expected more GValues");
			return FALSE;
		}
		g_value_array_append(*pvarr, &v);
		g_value_unset(&v);
	}
	return TRUE;
}

/*
 * Replacement for dbus_message_get_args_valist(), allowing extended
 * types, similar to clever_append_valist().  These functions can be
 * extended later with other MAFW-specific types.
 */
static gboolean clever_parse_valist(DBusMessage *msg,
				    int first_arg_t, va_list args)
{
	DBusMessageIter iter;
	int arg_t;

	dbus_message_iter_init(msg, &iter);
	for (arg_t = first_arg_t; arg_t != DBUS_TYPE_INVALID;
	     arg_t = va_arg(args, int)) {
		if (arg_t == MAFW_DBUS_TYPE_SAVEPOINT) {
			*va_arg(args, DBusMessageIter *) = iter;
			continue;
		}

		if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INVALID){
			g_warning("Message ends prematurely");
			goto fail;
		}

		if (arg_t == MAFW_DBUS_TYPE_STRVZ) {
			if (!clever_parse_strvz(&iter, va_arg(args, char ***)))
			       	goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_GBYTEARRAY) {
			if (!clever_parse_gbytearray(&iter,
                                                     va_arg(args,
                                                            GByteArray **)))
			       	goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_METADATA) {
			if (!mafw_dbus_message_parse_metadata(
                                    &iter, va_arg(args, GHashTable **)))
			       	goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_GVALUE) {
			if (!clever_parse_gvalue(&iter, va_arg(args, GValue *)))
			       	goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_GVALUEARRAY) {
			if (!clever_parse_gvaluearray(&iter,
                                                      va_arg(args,
                                                             GValueArray **)))
			       	goto fail;
		} else if (arg_t == MAFW_DBUS_TYPE_IGNORE) {
			/* NOP */
		} else if (dbus_type_is_basic(arg_t)) {
			if (!clever_parse_basic(&iter, arg_t, va_arg(args,
                                                                     void *)))
			       	goto fail;
		} else if (arg_t == DBUS_TYPE_ARRAY) {
			if (!clever_parse_array(&iter, &args))
			       	goto fail;
		} else {
			g_warning("Unhandled type: %d", arg_t);
			goto fail;
		}

		dbus_message_iter_next(&iter);
	}
	return TRUE;
fail:
	return FALSE;
}

/**
 * mafw_dbus_reply_v:
 * @call:           the call to reply to.
 * @first_arg_type: same as for dbus_message_append_args().
 * @...:            same as for dbus_message_append_args().
 *
 * Constructs a reply message for the passed @call, appending the
 * specified arguments.  The argument list must be terminated by
 * %DBUS_TYPE_INVALID.
 *
 * Returns: the newly allocated, filled-in message.
 */
DBusMessage *mafw_dbus_reply_v(DBusMessage *call,
			       int first_arg_type, ...)
{
	DBusMessage *msg;
	va_list args;
	int r;

	g_assert(call != NULL);

	msg = dbus_message_new_method_return(call);
	va_start(args, first_arg_type);
	r = clever_append_valist(msg, first_arg_type, args);
	va_end(args);
	if (!r) _die("error appending message arguments", NULL);
	return msg;
}

/**
 * mafw_dbus_ack_or_error:
 * @conn: a #DBusConnection.
 * @call: a method-call #DBusMessage.
 * @error: a #GError or %NULL.
 *
 * Sends a D-Bus error message, if @error is set, otherwise it sends a
 * void D-Bus reply.  Frees the error after sending.
 */
void mafw_dbus_ack_or_error(DBusConnection *conn,
			    DBusMessage *call, GError *error)
{
	if (error) {
		mafw_dbus_send(conn, mafw_dbus_gerror(call, error));
		g_error_free(error);
	} else {
		  mafw_dbus_send(conn, mafw_dbus_reply(call));
	}
}

/**
 * mafw_dbus_error:
 * @call:           the call to reply to with an error.
 * @domain:         MAFW error domain GQuark.
 * @code:           MAFW error code.
 * @message:        Free form error message string;
 *		    not expected to be %NULL.
 *
 * Constructs an error return message to the passed @call.
 * It merges the MAFW error code to the dbus message in order
 * to make the mapping to GError possible in the receiving end.
 *
 * Returns: the newly allocated, filled-in message.
 */
DBusMessage *mafw_dbus_error(DBusMessage *call, GQuark domain,
			     gint code, const gchar *message)
{
	DBusMessage *msg;

	msg = dbus_message_new_error_printf(call, "com.nokia.mafw", "%s:%u:%s",
				g_quark_to_string(domain),code, message);
	return msg;
}

/**
 * mafw_dbus_gerror:
 *
 * Like #mafw_dbus_error() but the details are extracted from @error.
 */
DBusMessage *mafw_dbus_gerror(DBusMessage *call, const GError *error)
{
	return mafw_dbus_error(call,
			       error->domain, error->code, error->message);
}

/**
 * mafw_dbus_error_to_gerror:
 * @domain: the caller's error domain.
 * @glep:   where to place the new #GError or %NULL.
 * @dbe:    the D-BUS error to convert.
 *
 * Converts a #DBusError to a #GError.  The input error is expected to have
 * been raised either by the D-BUS library or a mafw communication party
 * extension using #mafw_dbus_error().  @dbe must contain an error and will be
 * #dbus_error_free()d.  @glep may be %NULL, in which case no new #GError
 * is created, ignoring the output of this function.
 */
void mafw_dbus_error_to_gerror(GQuark domain, GError **glep, DBusError *dbe)
{
	g_assert(dbus_error_is_set(dbe));

	if (g_str_has_prefix(dbe->name, "org.freedesktop.DBus.")) {
		/* If $dbe is from D-BUS we cannot communicate with the
		 * addressee (who is supposedly a extension---we don't talk
		 * to anyone else). */
		g_set_error(glep, domain,
                            MAFW_EXTENSION_ERROR_EXTENSION_NOT_AVAILABLE,
			    "%s", dbe->message ? dbe->message : dbe->name);
	} else {
		guint code;
		gchar *msg;
		gchar *codestr;

		codestr = strchr(dbe->message, ':');
		codestr[0] = '\0';
		codestr++;
		msg = strchr(codestr, ':');
		msg[0] = '\0';
		msg++;

		/* $dbe is created by mafw_dbus_error(), parse it. */
		if (sscanf(codestr, "%u", &code) != 1)
			g_assert_not_reached();
		g_set_error(glep, g_quark_from_string(dbe->message),
			    code, "%s", msg);
	}

	dbus_error_free(dbe);
}

/**
 * @msg:    which message to extract the error from.
 * @domain: the caller's error domain.
 *
 * Like #dbus_set_error_from_message(), but returns a newly allocated
 * #GError if @msg is a %DBUS_MESSAGE_TYPE_ERROR, and unrefs it.
 * Otherwise return %NULL.  You may want to prefer this function
 * over #mafw_dbus_error_to_gerror().
 */
GError *mafw_dbus_is_error(DBusMessage *msg, GQuark domain)
{
	GError *gle;
	DBusError dbe;

	dbus_error_init(&dbe);
	if (!dbus_set_error_from_message(&dbe, msg))
		return NULL;

	gle = NULL;
	mafw_dbus_error_to_gerror(domain, &gle, &dbe);

	return gle;
}

/**
 * mafw_dbus_msg:
 * @type:        the message type (%DBUS_MESSAGE_TYPE_*).
 * @noreply:     no_reply header field.
 * @destination: bus name.
 * @path:        object path.
 * @interface:   interface.
 * @member:      member.
 * @first_arg_t: type of the first argument (%DBUS_TYPE_*).
 * @...:         the rest of arguments (type, value), terminated by
 *               %DBUS_TYPE_INVALID.
 *
 * Constructs a message from the given arguments. Sets header fields
 * appropriately if they are not %NULL.  Appends the passed arguments,
 * wrapping dbus_message_append_args().  You should use the
 * convenience macros instead of this function.
 *
 * Returns: the newly constructed message.
 */
DBusMessage *mafw_dbus_msg(int type, int noreply, char const *destination,
			   char const *path, char const *interface,
			   char const *member, int first_arg_t, ...)
{
	DBusMessage *t;
	va_list args;
	int r;

	t = dbus_message_new(type);
	if (destination)
		dbus_message_set_destination(t, destination);
	if (path)
		dbus_message_set_path(t, path);
	if (interface)
		dbus_message_set_interface(t, interface);
	if (member)
		dbus_message_set_member(t, member);
	if (noreply)
		dbus_message_set_no_reply(t, TRUE);

	va_start(args, first_arg_t);
	r = clever_append_valist(t, first_arg_t, args);
	va_end(args);
	if (!r) _die("error appending message arguments", NULL);
	return t;
}


/**
 * mafw_dbus_send_async:
 * @connection: the #DBusConnection to use.
 * @pending_return: location for #DBusPendingCall, or %NULL.
 * @message: the #DBusMessage to send.
 *
 * Sends the specified message on @connection (and flushes it).  If
 * @pending_return is not %NULL, the call will be asynchronous, and a
 * #DBusPendingCall instance will be stored at @pending_return.  If
 * it's %NULL, dbus_connection_send() will be used.  Unrefs the
 * message after sending.  You might want to use the mafw_dbus_send()
 * macro for no-reply messages and signals.
 *
 * Returns: the message serial.
 */
dbus_uint32_t mafw_dbus_send_async(DBusConnection *connection,
				   DBusPendingCall **pending_return,
				   DBusMessage *message)
{
	dbus_uint32_t serial;

	g_assert(connection != NULL);
	g_assert(message != NULL);

#ifdef MAFW_DEBUG
	g_debug("send: %s", msg_info(message));
#endif
	/* Use different functions to send depending on pending_call. */
	if (pending_return) {
		if (!dbus_connection_send_with_reply(connection, message,
						     pending_return, -1))
			goto err;
		serial = dbus_message_get_serial(message);
	} else {
		/* No-reply message. */
		if (!dbus_connection_send(connection, message, &serial))
				goto err;
	}
	dbus_message_unref(message);
	dbus_connection_flush(connection);
	return serial;
err:
	_die("error sending message", NULL);
}

/**
 * mafw_dbus_call:
 * @connection: the #DBusConnection to use.
 * @message:    the method call message.
 *
 * Sends the given message and waits for its reply.
 * Unrefs the message after sending.
 *
 * Returns: the reply message.
 */
DBusMessage *mafw_dbus_call(DBusConnection *connection,
			    DBusMessage *message,
			    GQuark domain, GError **error)
{
	DBusMessage *reply;
	DBusError dbe;

	g_assert(connection != NULL);
	g_assert(message != NULL);

#ifdef MAFW_DEBUG
	g_debug("call: %s", msg_info(message));
#endif
	dbus_error_init(&dbe);
	reply = dbus_connection_send_with_reply_and_block(connection,
							  message,
							  -1,
							  &dbe);

	if (!reply)
		/* Either a D-BUS or a mafw error. */
		mafw_dbus_error_to_gerror(domain, error, &dbe);

	dbus_message_unref(message);
	return reply;
}

#if MAFW_DEBUG
/*
 * Returns a statically allocated string telling information about the
 * passed message.  It's format is:
 *
 * "[type] destination/path: interface.member(signature)"
 */
char const *msg_info(DBusMessage *msg)
{
#define MLEN 512
#define append(fmt, ...)						\
	do {								\
		int w;							\
		w = snprintf(p, MLEN + 1 - l, fmt, ##__VA_ARGS__);	\
		l += w;							\
		p += w;							\
		if (l > MLEN) return info;				\
	} while (0)

	static char info[MLEN + 1];
	char *p;
	int l;

	l = 0;
	p = info;
	switch (dbus_message_get_type(msg)) {
	case DBUS_MESSAGE_TYPE_METHOD_CALL:   append("[method] "); break;
	case DBUS_MESSAGE_TYPE_METHOD_RETURN: append("[return] "); break;
	case DBUS_MESSAGE_TYPE_ERROR:         append("[error]  "); break;
	case DBUS_MESSAGE_TYPE_SIGNAL:        append("[signal] "); break;
	default: append("[%.3u]  ", dbus_message_get_type(msg)); break;
	}
	append("%s%s: %s.%s(%s)",
	       dbus_message_get_destination(msg),
	       dbus_message_get_path(msg),
	       dbus_message_get_interface(msg),
	       dbus_message_get_member(msg),
	       dbus_message_get_signature(msg));
	return info;
}
#undef MLEN
#undef append
#endif /* MAFW_DEBUG */

/**
 * mafw_dbus_parse_message_v:
 * @msg: the #DBusMessage to parse.
 * @first_argument_type: D-Bus type of the first argument.
 * @...: Variable length list of (type, value) pairs.
 *
 * Parses the given message according to the arbitrary argument list.
 * Wraps dbus_message_get_args().  Aborts on failure.
 */
void mafw_dbus_parse_message_v(DBusMessage *msg,
			       int first_arg_type, ...)
{
	DBusError err;
	va_list args;
	gboolean r;

#ifdef MAFW_DEBUG
	g_debug("parsing %s", msg_info(msg));
#endif
	dbus_error_init(&err);
	va_start(args, first_arg_type);
	r = clever_parse_valist(msg, first_arg_type, args);
	va_end(args);
	if (!r) _die("error while parsing message", &err);
}

/**
 * mafw_dbus_count_args:
 * @msg: a #DBusMessage.
 *
 * Returns: the number of arguments in @msg
 */
gint mafw_dbus_count_args(DBusMessage *msg)
{
	DBusMessageIter iter;
	gint count = 0;

	if (dbus_message_iter_init(msg, &iter) == FALSE) {
		/* No arguments. */
		return 0;
	}
	count = 1;
	while (dbus_message_iter_next(&iter))
		count++;
	return count;
}

/**
 * mafw_dbus_session:
 *
 * Acquires a connection to the session bus.  If fails prints diagnostics.
 * This function is a convenience wrapper around #dbus_bus_get().
 *
 * Returns: a connection to the session bus or %NULL if it couldn't be
 * established.
 *
 * TODO Should it abort rather than whining?
 */
DBusConnection *mafw_dbus_session(GError **errp)
{
	DBusError dbe;
	DBusConnection *dbus;

	dbus_error_init(&dbe);
	dbus = dbus_bus_get(DBUS_BUS_SESSION, &dbe);
	if (dbus_error_is_set(&dbe)) {
		g_critical("Couldn't connect to the session bus: %s",
			   dbe.name);
		g_set_error(errp, MAFW_ERROR, MAFW_EXTENSION_ERROR_FAILED,
			    "Couldn't connect to the session bus");
		if (dbus != NULL)
			/* The docs doesn't say clearly when
			 * does dbus_bus_get() return NULL. */
			dbus_connection_unref(dbus);
		return NULL;
	} else
		return dbus;
}

/**
 * mafw_dbus_open:
 * @address:      socket address of remote side.
 * @conn:         the location to put the acquired #DBusConnection.
 * @handler:      handler function to install.
 * @handler_data: additional data to pass to the handler.
 *
 * Tries to open the given @address and stores the acquired
 * #DBusConnection in the location specified (which must contain
 * %NULL, otherwise it returns %FALSE, to prevent double tries).  Then
 * it installs a filter function (@handler) to which @handler_data
 * will be passed at each invocation.  For a bonus, it also sets up
 * the connection with the default GLib main context.  If anything
 * fails, it complains loudly.
 *
 * Returns: %TRUE if all went OK, %FALSE otherwise.
 * TODO should it abort?
 */
gboolean mafw_dbus_open(char const *address, DBusConnection **conn,
			gpointer handler, gpointer handler_data)
{
	g_return_val_if_fail(address != NULL, FALSE);
	g_assert(conn != NULL);
	g_return_val_if_fail(*conn == NULL, FALSE);

	*conn = dbus_connection_open(address, NULL);
	if (*conn == NULL) {
		g_warning("Cannot open connection");
		return FALSE;
	}
	if (!dbus_connection_add_filter(*conn, handler, handler_data, NULL)) {
		g_warning("Failed to set up filter");
		dbus_connection_unref(*conn);
		*conn = NULL;
		return FALSE;
	}
	dbus_connection_setup_with_g_main(*conn, NULL);
	return TRUE;
}

/* Async operation helpers */
/* Returns a new MafwDBusOpCompletedInfo filled with $con and $msg. */
MafwDBusOpCompletedInfo *mafw_dbus_oci_new(DBusConnection *con,
					   DBusMessage *msg)
{
	MafwDBusOpCompletedInfo *oci;

	oci = g_new0(MafwDBusOpCompletedInfo, 1);
	oci->con = dbus_connection_ref(con);
	oci->msg = dbus_message_ref(msg);

	return oci;
}

/* Undoes mafw_dbus_oci_new(). */
void mafw_dbus_oci_free(MafwDBusOpCompletedInfo *oci)
{
	dbus_message_unref(oci->msg);
	dbus_connection_unref(oci->con);
	g_free(oci);
}

/* Returns $error to $info->con and releases $info. */
void mafw_dbus_oci_error(MafwDBusOpCompletedInfo *oci, GError *error)
{
	/* XXX Some call this function with NULL $error because some
	 * other functions return FALSE while NOT setting the supplied
	 * GError.  I consider it a bug and hence this is a workaround. */
	if (!error)
		error = g_error_new(MAFW_EXTENSION_ERROR,
				    MAFW_EXTENSION_ERROR_INVALID_PARAMS,
				    "Invalid params");

	mafw_dbus_send(oci->con, mafw_dbus_gerror(oci->msg, error));
	g_error_free(error);
	mafw_dbus_oci_free(oci);
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
