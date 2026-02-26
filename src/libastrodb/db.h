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

/*! \struct adb_db
 * \brief Opaque context representing an open AstroDB database
 * \ingroup catalog
 */
struct adb_db;

/********************** Library and debug *************************************/

/**
 * \defgroup library Library Base
 * \brief Core initialization, configuration, and logging functionality
 */

/*! \enum adb_msg_level
 * \brief AstroDB logging and message verbosity levels
 * \ingroup library
 */
enum adb_msg_level {
	ADB_MSG_NONE = 0, /*!< No messages logged */
	ADB_MSG_INFO = 1, /*!< Informational messages only */
	ADB_MSG_WARN = 2, /*!< Warnings that do not halt operation */
	ADB_MSG_DEBUG = 3, /*!< Standard debug information */
	ADB_MSG_VDEBUG = 4, /*!< Verbose debug information */
};

/* Logging bitmasks indicating which subsystems emit log messages */

/** \brief Log HTM Core operations \ingroup library */
#define ADB_LOG_HTM_CORE (1 << 0)
/** \brief Log HTM Get/fetch operations \ingroup library */
#define ADB_LOG_HTM_GET (1 << 1)
/** \brief Log HTM File parsing operations \ingroup library */
#define ADB_LOG_HTM_FILE (1 << 2)
/** \brief Log HTM Insertion operations \ingroup library */
#define ADB_LOG_HTM_INSERT (1 << 3)
/** \brief Enable all HTM-related logging \ingroup library */
#define ADB_LOG_HTM_ALL \
	(ADB_LOG_HTM_CORE | ADB_LOG_HTM_GET | ADB_LOG_HTM_FILE | ADB_LOG_HTM_INSERT)

/** \brief Log CDS FTP download operations \ingroup library */
#define ADB_LOG_CDS_FTP (1 << 4)
/** \brief Log CDS catalog format parsing \ingroup library */
#define ADB_LOG_CDS_PARSER (1 << 5)
/** \brief Log CDS schema creation/loading \ingroup library */
#define ADB_LOG_CDS_SCHEMA (1 << 6)
/** \brief Log CDS table management \ingroup library */
#define ADB_LOG_CDS_TABLE (1 << 7)
/** \brief Log generic CDS DB internals \ingroup library */
#define ADB_LOG_CDS_DB (1 << 8)
/** \brief Log bulk record importing \ingroup library */
#define ADB_LOG_CDS_IMPORT (1 << 9)
/** \brief Log internal KD-Tree generation \ingroup library */
#define ADB_LOG_CDS_KDTREE (1 << 10)
/** \brief Enable all CDS-related logging \ingroup library */
#define ADB_LOG_CDS_ALL                                          \
	(ADB_LOG_CDS_FTP | ADB_LOG_CDS_PARSER | ADB_LOG_CDS_SCHEMA | \
	 ADB_LOG_CDS_TABLE | ADB_LOG_CDS_DB | ADB_LOG_CDS_IMPORT |   \
	 ADB_LOG_CDS_KDTREE)

/** \brief Log search subsystem query execution \ingroup library */
#define ADB_LOG_SEARCH (1 << 11)
/** \brief Log astrometric plate solving steps \ingroup library */
#define ADB_LOG_SOLVE (1 << 12)

/** \brief Enable purely all subsystems tracing/logging \ingroup library */
#define ADB_LOG_ALL \
	(ADB_LOG_HTM_ALL | ADB_LOG_CDS_ALL | ADB_LOG_SEARCH | ADB_LOG_SOLVE)

/**
 * \brief Set messaging reporting level on a specific database instance
 * \ingroup library
 * \param db The target database context
 * \param level The threshold verbosity level to apply (from adb_msg_level)
 */
void adb_set_msg_level(struct adb_db *db, enum adb_msg_level level);

/**
 * \brief Set specific log system masks visibility flags
 * \ingroup library
 * \param db The target database context
 * \param log Bitmask combining log domains (e.g. ADB_LOG_SOLVE | ADB_LOG_SEARCH)
 */
void adb_set_log_level(struct adb_db *db, unsigned int log);

/*! \struct adb_library
 * \brief Internal context wrapping paths or state for the library instance
 * \ingroup library
 */
struct adb_library;

/**
 * \brief Create a library wrapper context pointing to remote and local domains
 * \ingroup library
 * \param host Network host or address used to contact CDS/Simbad
 * \param remote Path to directories containing records on the network host
 * \param local Path to local data sync directories
 * \return Pointer to the allocated library context, or NULL if unavailable
 */
struct adb_library *adb_open_library(const char *host, const char *remote,
									 const char *local);

/**
 * \brief Close, disconnect, and free all resources within the library wrapper
 * \ingroup library
 * \param lib The targeted library context to dispose
 */
void adb_close_library(struct adb_library *lib);

/**
 * \brief Get the libastrodb version number as a static string
 * \ingroup library
 * \return String representing library version (e.g., "1.0.0")
 */
const char *adb_get_version(void);

/********************* DB creation ********************************************/

/**
 * \defgroup catalog Catalogs and DB Management
 * \brief Functions creating/tearing down top level catalog databases (CDS formats etc.)
 */

/**
 * \brief Allocate and initialize a new main catalog database context
 * \ingroup catalog
 * \param lib Base library wrapper previously established
 * \param depth Specifies HTM detail level constraints or tree depths
 * \param tables Total number of maximum open tables anticipated
 * \return A pointer to the database layout, or NULL on error
 */
struct adb_db *adb_create_db(struct adb_library *lib, int depth, int tables);

/**
 * \brief Gracefully destroys a catalog context, flushing data and freeing handles
 * \ingroup catalog
 * \param db The database descriptor reference to free
 */
void adb_db_free(struct adb_db *db);

/********************* Table Management ***************************************/

/**
 * \brief Opens or initializes a specific catalog dataset as a logical table
 * \ingroup dataset
 * \param db Connected Database wrapper Context
 * \param cat_class String categorizing the catalog's base designation (e.g. "I")
 * \param cat_id Identifier tracking subset or version within the category (e.g. "239")
 * \param table_name Direct specific label resolving to physical data boundaries
 * \return integer handle denoting the unique ID to refer to this table context, or error 
 */
int adb_table_open(struct adb_db *db, const char *cat_class, const char *cat_id,
				   const char *table_name);

/**
 * \brief Flush writes, clear cache limits, and release a managed dataset mapping
 * \ingroup dataset
 * \param db Reference Database Context where the table was initialized
 * \param table_id Logical integer identifier indicating the targeted table
 * \return 0 upon successful close execution, or negative on failures
 */
int adb_table_close(struct adb_db *db, int table_id);

/**
 * \brief Prepare dataset architecture adding custom string identifiers as hashed queries
 * \ingroup dataset
 * \param db Reference Database context wrapper
 * \param table_id Registered internal table scope reference
 * \param key The identifier to explicitly hash format layout bounds against
 * \return 0 acknowledging creation/readiness for key logic
 */
int adb_table_hash_key(struct adb_db *db, int table_id, const char *key);

/**
 * \brief Get total cache file bounds dynamically in bytes
 * \ingroup dataset
 * \param db Reference Database context wrapper
 * \param table_id Registered internal table scope reference
 * \return size footprint measured directly in bytes mapped
 */
int adb_table_get_size(struct adb_db *db, int table_id);

/**
 * \brief Get internal physical entry quantity within a whole loaded table structure
 * \ingroup dataset
 * \param db Connected Database context
 * \param table_id Table to count elements inside
 * \return The raw object item count across memory maps and physical layers
 */
int adb_table_get_count(struct adb_db *db, int table_id);

/**
 * \brief Evaluate specific bytes representing individual row entries/objects globally
 * \ingroup dataset
 * \param db Reference to Database context
 * \param table_id The identifier target structure
 * \return size required to encompass strictly one item mapping
 */
int adb_table_get_object_size(struct adb_db *db, int table_id);

#ifdef __cplusplus
}
#endif

#endif
