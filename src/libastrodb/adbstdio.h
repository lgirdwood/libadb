/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Copyright (C) 2008 - 2014 Liam Girdwood
 */

#ifndef __ADB_ERROR_H
#define __ADB_ERROR_H

#include <stdio.h>
#include <stdarg.h>
#include <execinfo.h>
#include "db.h"

#define DEBUG_BUFFER	1024

static inline void adbout_(struct adb_db *db, const char *level,
	const char *file, const char *func, int line, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stdout, "%s:%s:%s:%d:  ", level, file, func, line);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

static inline void adberr_(struct adb_db *db, const char *level,
	const char *file, const char *func, int line, const char *fmt, ...)
{
	int j, nptrs;
	va_list va;
	void *buffer[DEBUG_BUFFER];
	char **str;

	va_start(va, fmt);
	fprintf(stderr, "%s:%s:%s:%d:  ", level, file, func, line);
	vfprintf(stderr, fmt, va);
	va_end(va);

	nptrs = backtrace(buffer, DEBUG_BUFFER);
	str = backtrace_symbols(buffer, nptrs);

	for (j = 0; j < nptrs; j++)
		fprintf(stderr, "  %s\n", str[j]);

	free(str);
}

#define adb_error(db, format, arg...) \
	adberr_(db, "E", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_info(db, type, format, arg...) \
	if (db->msg_level >= ADB_MSG_INFO  && type & db->msg_flags) \
		adbout_(db, "  I", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_warn(db, type, format, arg...) \
	if (db->msg_level >= ADB_MSG_WARN && type & db->msg_flags) \
		adbout_(db, " W", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_debug(db, type, format, arg...) \
	if (unlikely(db->msg_level >= ADB_MSG_DEBUG) && type & db->msg_flags) \
		adbout_(db, "   D", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_vdebug(db, type, format, arg...) \
	if (unlikely(db->msg_level >= ADB_MSG_VDEBUG) && type & db->msg_flags) \
		adbout_(db, "   D", __FILE__, __func__, __LINE__, format, ## arg)

static inline void htmout_(struct htm *htm, const char *level,
	const char *file, const char *func, int line, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stdout, "%s:%s:%s:%d:  ", level, file, func, line);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

static inline void htmerr_(struct htm *htm, const char *level,
	const char *file, const char *func, int line, const char *fmt, ...)
{
	int j, nptrs;
	va_list va;
	void *buffer[DEBUG_BUFFER];
	char **str;

	va_start(va, fmt);
	fprintf(stderr, "%s:%s:%s:%d:  ", level, file, func, line);
	vfprintf(stderr, fmt, va);
	va_end(va);

	nptrs = backtrace(buffer, DEBUG_BUFFER);
	str = backtrace_symbols(buffer, nptrs);

	for (j = 0; j < nptrs; j++)
		fprintf(stderr, "  %s\n", str[j]);

	free(str);
}

#define adb_htm_error(htm, format, arg...) \
	htmerr_(htm, "E", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_htm_info(htm, type, format, arg...) \
	if (htm->msg_level >= ADB_MSG_INFO  && type & htm->msg_flags) \
		htmout_(htm, "  I", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_htm_warn(htm, type, format, arg...) \
	if (htm->msg_level >= ADB_MSG_WARN && type & htm->msg_flags) \
		htmout_(htm, " W", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_htm_debug(htm, type, format, arg...) \
	if (unlikely(htm->msg_level >= ADB_MSG_DEBUG) && type & htm->msg_flags) \
		htmout_(htm, "   D", __FILE__, __func__, __LINE__, format, ## arg)

#define adb_htm_vdebug(htm, type, format, arg...) \
	if (unlikely(htm->msg_level >= ADB_MSG_VDEBUG) && type & htm->msg_flags) \
		htmout_(htm, "   D", __FILE__, __func__, __LINE__, format, ## arg)

static inline void adboutl_(struct adb_library *lib, const char *level,
	const char *file, const char *func, int line, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stdout, "%s:%s:%s:%d:  ", level, file, func, line);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

static inline void adberrl_(struct adb_library *lib, const char *level,
	const char *file, const char *func, int line, const char *fmt, ...)
{
	int j, nptrs;
	va_list va;
	void *buffer[DEBUG_BUFFER];
	char **str;

	va_start(va, fmt);
	fprintf(stderr, "%s:%s:%s:%d:  ", level, file, func, line);
	vfprintf(stderr, fmt, va);
	va_end(va);

	nptrs = backtrace(buffer, DEBUG_BUFFER);
	str = backtrace_symbols(buffer, nptrs);

	for (j = 0; j < nptrs; j++)
		fprintf(stderr, "  %s\n", str[j]);

	free(str);
}

#define astrolib_error(lib, format, arg...) \
	adberrl_(lib, "E", __FILE__, __func__, __LINE__, format, ## arg)

#define astrolib_info(lib, type, format, arg...) \
		adboutl_(lib, "  I", __FILE__, __func__, __LINE__, format, ## arg)

#endif
