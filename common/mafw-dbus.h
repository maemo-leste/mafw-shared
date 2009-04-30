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

#ifndef __MAFW_DBUS_H__
#define __MAFW_DBUS_H__

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

/* MAFW specific message construction. */

/**
 * Additional types known by mafw_dbus_* functions.
 *
 * Since D-Bus type constants are < 256, we shift by 9 (2^9 is a nice
 * number).
 */
/**
 * MAFW_DBUS_TYPE_STRVZ:
 *
 * Type constant denoting a %NULL-terminated string array.  If it
 * passed to mafw_dbus_parse() then the returned value should be freed
 * using g_strfreev()!
 */
#define MAFW_DBUS_TYPE_STRVZ      ((1<<9)+82)
/**
 * MAFW_DBUS_TYPE_GBYTEARRAY:
 *
 * Type constant denoting a #GByteArray.
 */
#define MAFW_DBUS_TYPE_GBYTEARRAY ((1<<9)+83)
/**
 * MAFW_DBUS_TYPE_METADATA:
 *
 * Type constant denoting a #GHashTable containing MAFW metadata.
 */
#define MAFW_DBUS_TYPE_METADATA   ((1<<9)+84)
/**
 * MAFW_DBUS_TYPE_GVALUE:
 *
 * Type constant denoting a #GValue.
 */
#define MAFW_DBUS_TYPE_GVALUE     ((1<<9)+85)
/**
 * MAFW_DBUS_TYPE_SAVEPOINT:
 *
 * Type constant denoting a point where you can restart parsing.
 */
#define MAFW_DBUS_TYPE_SAVEPOINT  ((1<<9)+86)
/**
 * MAFW_DBUS_TYPE_IGNORE:
 *
 * Type constant denoting a message argument to be ignored completely.
 */
#define MAFW_DBUS_TYPE_IGNORE     ((1<<9)+87)
/**
 * MAFW_DBUS_TYPE_GVALUEARRAY:
 *
 * Type constant denoting a #GValueArray.
 */
#define MAFW_DBUS_TYPE_GVALUEARRAY ((1<<9)+88)


/**
 * MAFW_DBUS_STRVZ:
 * @v: zero-terminated string array.
 */
#define MAFW_DBUS_STRVZ(v) MAFW_DBUS_TYPE_STRVZ, (v)
/**
 * MAFW_DBUS_GBYTEARRAY:
 * @v: pointer to a #GByteArray.
 */
#define MAFW_DBUS_GBYTEARRAY(v) MAFW_DBUS_TYPE_GBYTEARRAY, (v)
/**
 * MAFW_DBUS_GVALUE:
 * @v: pointer to a #GValue.
 */
#define MAFW_DBUS_GVALUE(v) MAFW_DBUS_TYPE_GVALUE, (v)
/**
 * MAFW_DBUS_METADATA:
 * @v: pointer to a #GHashTable, containing MAFW specific metadata.
 */
#define MAFW_DBUS_METADATA(v) MAFW_DBUS_TYPE_METADATA, (v)
/**
 * MAFW_DBUS_GVALUEARRAY:
 * @v: pointer to a #GValueArray.
 */
#define MAFW_DBUS_GVALUEARRAY(v) MAFW_DBUS_TYPE_GVALUEARRAY, (v)

/**
 * MAFW_DBUS_SAVEPOINT:
 * @v: pointer to a #DBusMessageIter where to recored the current position
 *     of the parser.  The parser is NOT advanced.  To ignore a message
 *     argument for the time being add a subsequent %MAFW_DBUS_TYPE_IGNORE.
 *     You can add this macro at the end of #mafw_dbus_parse() to detect
 *     optional arguments.
 */
#define MAFW_DBUS_SAVEPOINT(v) MAFW_DBUS_TYPE_SAVEPOINT, (v)

/**
 * These macros make message construction code cleaner.
 * Example:
 *
 * mafw_dbus_method("get_metaxa",
 * 	MAFW_DBUS_STRING(oid),
 * 	MFAW_DBUS_BOOLEAN(with_icecubes),
 * 	MAFW_DBUS_STRVZ(keys));
 */
#define MAFW_DBUS_BYTE(v)       DBUS_TYPE_BYTE, (v)
#define MAFW_DBUS_BOOLEAN(v)    DBUS_TYPE_BOOLEAN, (v)
#define MAFW_DBUS_INT16(v)      DBUS_TYPE_INT16, (v)
#define MAFW_DBUS_UINT16(v)     DBUS_TYPE_UINT16, (v)
#define MAFW_DBUS_INT32(v)      DBUS_TYPE_INT32, (v)
#define MAFW_DBUS_UINT32(v)     DBUS_TYPE_UINT32, (v)
#define MAFW_DBUS_INT64(v)      DBUS_TYPE_INT64, (v)
#define MAFW_DBUS_UINT64(v)     DBUS_TYPE_UINT64, (v)
#define MAFW_DBUS_DOUBLE(v)     DBUS_TYPE_DOUBLE, (v)
#define MAFW_DBUS_STRING(v)     DBUS_TYPE_STRING, (v)

/**
 * MAFW_DBUS_C_STRVZ:
 *
 * Constructs a %NULL-terminated string array from a literal.
 */
#define MAFW_DBUS_C_STRVZ(...) MAFW_DBUS_TYPE_STRVZ,	\
		(char **){(char *[]){ __VA_ARGS__, NULL }}

/**
 * MAFW_DBUS_C_ARRAY:
 * @dt: D-Bus type of elements.
 * @ct: C type of elements.
 *
 * Constructs an array of @ct (with D-Bus type @dt) from the
 * arguments.  Warning: evaluates its arguments twice.
 */
#define MAFW_DBUS_C_ARRAY(dt, ct, ...)				\
	DBUS_TYPE_ARRAY, DBUS_TYPE_##dt,			\
		(ct[]){ __VA_ARGS__ },			\
		(sizeof((ct[]){ __VA_ARGS__ }) / sizeof(ct))

/**
 * MAFW_DBUS_AST:
 * @sig: signature of the elements of the array, omitting the structure
 *       markers; must be a string literal
 *
 * Append an array of structures with types inside specified by @sig
 * to the message.  The rest of the arguments are the elements of the
 * array embraced by MAFW_DBUS_STRUCT().  Example:
 *
 * mafw_dbus_msg(..., MAFW_DBUS_AST("us",
 *	MAFW_DBUS_STRUCT(MAFW_DBUS_UINT32(10), DBUS_TYPE_STRING("alpha")),
 *	MAFW_DBUS_STRUCT(MAFW_DBUS_UINT32(30), DBUS_TYPE_STRING("gamma"))));
 *
 * If you feel crazy enough you can embed ASTs recursively, but you cannot
 * use the MAFW_DBUS_AST() macro inside (TODO CPP bug?).  Example:
 *
 * mafw_dbus_msg(...,
 *	DBUS_TYPE_ARRAY, DBUS_TYPE_STRUCT, "(ua(bd)s)",
 *		MAFW_DBUS_STRUCT(
 *			MAFW_DBUS_UINT32(10),
 *			DBUS_TYPE_ARRAY, DBUS_TYPE_STRUCT, "(bd)",
 *				MAFW_DBUS_STRUCT(
 *					MAFW_DBUS_BOOLEAN(FALSE),
 *					MAFW_DBUS_DOUBLE(3.42)),
 *				DBUS_TYPE_INVALID,
 *			MAFW_DBUS_STRING("alpha")),
 *		DBUS_TYPE_INVALID);
 */
#define MAFW_DBUS_STRUCT(...) __VA_ARGS__, DBUS_TYPE_INVALID
#define MAFW_DBUS_AST(sig, ...) \
	DBUS_TYPE_ARRAY, DBUS_TYPE_STRUCT, \
	"(" sig ")", ##__VA_ARGS__, DBUS_TYPE_INVALID

/* Communication area between D-BUS handlers and the various callbacks
 * set therein to be called when a extensions or registry completes. */
typedef struct {
	/* The connection to reply on and the message to reply to. */
	DBusConnection *con;
	DBusMessage *msg;
} MafwDBusOpCompletedInfo;

extern DBusMessage *mafw_dbus_reply_v(DBusMessage *call,
                                      int first_arg_type, ...);
extern void mafw_dbus_ack_or_error(DBusConnection *conn,
				   DBusMessage *call, GError *error);
extern DBusMessage *mafw_dbus_error(DBusMessage *call, GQuark domain,
				    gint code, const gchar *message);
extern DBusMessage *mafw_dbus_gerror(DBusMessage *call, const GError *error);
extern void mafw_dbus_error_to_gerror(GQuark domain, GError **glep,
				      DBusError *dbe);
extern GError *mafw_dbus_is_error(DBusMessage *msg, GQuark domain);
extern DBusMessage *mafw_dbus_msg(int type, int noreply,
				  char const *destination, char const *path,
				  char const *interface, char const *member,
				  int first_arg_t, ...);
extern gint mafw_dbus_count_args(DBusMessage *msg);

/* If the library user doesn't #define these as macros, they stay
 * this.  These variables make sensible defaults (== 0) for them. */
extern char const * const MAFW_DBUS_INTERFACE;
extern char const * const MAFW_DBUS_DESTINATION;
extern char const * const MAFW_DBUS_PATH;

/**
 * mafw_dbus_reply:
 * @call:           the call to reply to.
 * @first_arg_type: same as for dbus_message_append_args().
 * @...:            same as for dbus_message_append_args().
 *
 * Convenience wrapper around mafw_dbus_reply_v(), automatically
 * terminating its arguments with %DBUS_TYPE_INVALID.
 *
 * Returns: the newly allocated, filled-in message.
 */
#define mafw_dbus_reply(_m, ...) \
	mafw_dbus_reply_v(_m, ##__VA_ARGS__, DBUS_TYPE_INVALID)

/**
 * mafw_dbus_method_full:
 *
 * Wrapper for mafw_dbus_msg() that creates a method call.  You can
 * set every part of the message.
 */
#define mafw_dbus_method_full(_d, _p, _i, _m, ...)			\
	mafw_dbus_msg(DBUS_MESSAGE_TYPE_METHOD_CALL, 0, _d, _p, _i, _m, \
		      ##__VA_ARGS__, DBUS_TYPE_INVALID)

/**
 * mafw_dbus_method:
 *
 * Similar to mafw_dbus_method_full(), except that it uses defaults.
 * Define the following macros in your source file:
 *
 * %MAFW_DBUS_INETRFACE, %MAFW_DBUS_DESTINATION, %MAFW_DBUS_PATH
 */
#define mafw_dbus_method(_m, ...)					\
	mafw_dbus_method_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,	\
			      MAFW_DBUS_INTERFACE, _m, ##__VA_ARGS__)

/**
 * mafw_dbus_signal_full:
 *
 * Wrapper for mafw_dbus_msg() that creates a signal.  You can
 * set every part of the message.
 */
#define mafw_dbus_signal_full(_d, _p, _i, _m, ...)			\
	mafw_dbus_msg(DBUS_MESSAGE_TYPE_SIGNAL, 1, _d, _p, _i, _m,	\
		      ##__VA_ARGS__, DBUS_TYPE_INVALID)

/**
 * mafw_dbus_signal:
 *
 * Similar to mafw_dbus_signal_full(), except that it uses defaults.
 * Define the following macros in your source file:
 */
#define mafw_dbus_signal(_m, ...)					\
	mafw_dbus_signal_full(MAFW_DBUS_DESTINATION, MAFW_DBUS_PATH,	\
			      MAFW_DBUS_INTERFACE, _m, ##__VA_ARGS__)

/* Message sending. */
extern dbus_uint32_t mafw_dbus_send_async(DBusConnection *connection,
					  DBusPendingCall **pending_return,
					  DBusMessage *message);
/**
 * mafw_dbus_send:
 * @_conn: the #DBusConnection to use.
 * @_m:    a #DBusMessage.
 *
 * Sends a message without caring for results.  Use for signals, and
 * no-reply messages.  Unrefs the message after sending.
 *
 * Returns: the serial of the message sent.
 */
#define mafw_dbus_send(_conn, _m) \
	mafw_dbus_send_async(_conn, NULL, _m)

extern DBusMessage *mafw_dbus_call(DBusConnection *connection,
				   DBusMessage *message,
				   GQuark domain, GError **error);

/* Message dispatching and parsing. */

/**
 * mafw_dbus_is_ours:
 * @_msg: a #DBusMessage.
 *
 * Returns %TRUE if the given message has the interface defined with
 * %MAFW_DBUS_INTERFACE.
 */
#define mafw_dbus_is_ours(_msg) \
	(dbus_message_has_interface(_msg, MAFW_DBUS_INTERFACE))

/**
 * mafw_dbus_is_signal:
 *
 * Wrapper for dbus_message_is_signal() except it takes the
 * %MAFW_DBUS_INTERFACE into account.
 */
#define mafw_dbus_is_signal(_msg, _m) \
	dbus_message_is_signal(_msg, MAFW_DBUS_INTERFACE, _m)

/**
 * mafw_dbus_is_method:
 *
 * Wrapper for dbus_message_is_method_call() except it takes the
 * %MAFW_DBUS_INTERFACE into account.
 */
#define mafw_dbus_is_method(_msg, _m) \
	dbus_message_is_method_call(_msg, MAFW_DBUS_INTERFACE, _m)

extern void mafw_dbus_parse_message_v(DBusMessage *msg,
				      int first_arg_type, ...);

/**
 * mafw_dbus_parse:
 * @_msg: a #DBusMessage.
 * @...:
 *
 * Wraps mafw_dbus_parse_message_v(), only you don't need to terminate the
 * argument list with %DBUS_TYPE_INVALID.  Aborts if it fails.
 */
#define mafw_dbus_parse(_msg, ...) \
	mafw_dbus_parse_message_v(_msg, ##__VA_ARGS__, DBUS_TYPE_INVALID)

/**
 * mafw_dbus_message_append_array:
 * @iter: iterator from append the arrays-elements
 * @eltype: type of the elemts
 * @nelem: number of the elemets
 * @values: elements
 *
 * Appends an array (@values) of length @nelem consisting of @eltype
 * typed elements to @iter.  Handles arrays of basic types and and
 * arrays of strings (just like libdbus).
 */
extern gboolean mafw_dbus_message_append_array(DBusMessageIter *iter,
				    int eltype, int nelem,
				    void *values);
/**
 * mafw_dbus_message_parse_metadata:
 * @iter: iterator points to the metadata in a message
 * @htp: address of a has-table
 *
 * Parses a hash-table, received in a message
 */
extern gboolean mafw_dbus_message_parse_metadata(DBusMessageIter *iter,
					GHashTable **htp);

/* Connection setup ease. */
extern DBusConnection *mafw_dbus_session(GError **errp);
extern gboolean mafw_dbus_open(char const *address, DBusConnection **conn,
			       gpointer handler, gpointer handler_data);

/* Async operation helpers */
extern MafwDBusOpCompletedInfo *mafw_dbus_oci_new(DBusConnection *con,
						  DBusMessage *msg);
extern void mafw_dbus_oci_free(MafwDBusOpCompletedInfo *oci);
extern void mafw_dbus_oci_error(MafwDBusOpCompletedInfo *info,
			       	GError *error);

#endif
/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
