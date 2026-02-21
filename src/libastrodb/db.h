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
 *  Copyright (C) 2005 - 2014 Liam Girdwood
 */

#ifndef __LIBADB_DB_H
#define __LIBADB_DB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct adb_db;

/********************** Library and debug *************************************/

/*! \typedef enum adb_msg_level
 * \ingroup Library
 *
 * AstroDB message level.
 */
enum adb_msg_level {
  ADB_MSG_NONE = 0,
  ADB_MSG_INFO = 1,
  ADB_MSG_WARN = 2,
  ADB_MSG_DEBUG = 3,
  ADB_MSG_VDEBUG = 4,
};

#define ADB_LOG_HTM_CORE (1 << 0)
#define ADB_LOG_HTM_GET (1 << 1)
#define ADB_LOG_HTM_FILE (1 << 2)
#define ADB_LOG_HTM_INSERT (1 << 3)
#define ADB_LOG_HTM_ALL                                                        \
  (ADB_LOG_HTM_CORE | ADB_LOG_HTM_GET | ADB_LOG_HTM_FILE | ADB_LOG_HTM_INSERT)

#define ADB_LOG_CDS_FTP (1 << 4)
#define ADB_LOG_CDS_PARSER (1 << 5)
#define ADB_LOG_CDS_SCHEMA (1 << 6)
#define ADB_LOG_CDS_TABLE (1 << 7)
#define ADB_LOG_CDS_DB (1 << 8)
#define ADB_LOG_CDS_IMPORT (1 << 9)
#define ADB_LOG_CDS_KDTREE (1 << 10)
#define ADB_LOG_CDS_ALL                                                        \
  (ADB_LOG_CDS_FTP | ADB_LOG_CDS_PARSER | ADB_LOG_CDS_SCHEMA |                 \
   ADB_LOG_CDS_TABLE | ADB_LOG_CDS_DB | ADB_LOG_CDS_IMPORT |                   \
   ADB_LOG_CDS_KDTREE)

#define ADB_LOG_SEARCH (1 << 11)
#define ADB_LOG_SOLVE (1 << 12)

#define ADB_LOG_ALL                                                            \
  (ADB_LOG_HTM_ALL | ADB_LOG_CDS_ALL | ADB_LOG_SEARCH | ADB_LOG_SOLVE)

/**
 * \brief Set messaging reporting level.
 * \ingroup library
 */
void adb_set_msg_level(struct adb_db *db, enum adb_msg_level level);

/**
 * \brief Set log level visibility flags.
 * \ingroup library
 */
void adb_set_log_level(struct adb_db *db, unsigned int log);

/**
 * \brief Create a library wrapper context.
 * \ingroup library
 */
struct adb_library *adb_open_library(const char *host, const char *remote,
                                     const char *local);

/**
 * \brief Free the library resources
 * \ingroup library
 */
void adb_close_library(struct adb_library *lib);

/**
 * \brief Get the libastrodb version number.
 * \ingroup misc
 */
const char *adb_get_version(void);

/********************* DB creation ********************************************/

/**
 * \brief Create a new catalog database context.
 * \ingroup catalog
 */
struct adb_db *adb_create_db(struct adb_library *lib, int depth, int tables);

/**
 * \brief Destroys catalog and frees resources
 * \ingroup catalog
 */
void adb_db_free(struct adb_db *db);

/********************* Table Management ***************************************/

/**
 * \brief Initialise new dataset.
 * \ingroup dataset
 */
int adb_table_open(struct adb_db *db, const char *cat_class, const char *cat_id,
                   const char *table_name);

/**
 * \brief Free's dataset and it's resources
 * \ingroup dataset
 */
int adb_table_close(struct adb_db *db, int table_id);

/**
 * \brief Add a custom field to the dataset for importing
 * \ingroup dataset
 */
int adb_table_hash_key(struct adb_db *db, int table_id, const char *key);

/**
 * \brief Get dataset cache size.
 * \return size in bytes
 * \ingroup dataset
 */
int adb_table_get_size(struct adb_db *db, int table_id);

int adb_table_get_count(struct adb_db *db, int table_id);

/**
 * \brief Get dataset object size.
 * \return size in bytes
 * \ingroup dataset
 */
int adb_table_get_object_size(struct adb_db *db, int table_id);

#ifdef __cplusplus
}
#endif

#endif
