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

#ifndef __ADB_TABLE_H
#define __ADB_TABLE_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <math.h>
#include <stdlib.h>

#include "cds.h"
#include "hash.h"
#include "htm.h"
#include "import.h"
#include "private.h"
#include "schema.h"

#define ADB_TABLE_MAX_FIELDS 128    /* max number of indexes */
#define ADB_TABLE_MAX_ALT_FIELDS 16 /* max number of alternate indexes */
#define ADB_TABLE_HISTOGRAM_DIVS 100

struct adb_db;
struct adb_table;

struct depth_map {
  float min_value; /*!< minimum object primary key value at this depth */
  float max_value; /*!< maximum object primary key value at this depth */
};

struct adb_object_set {
  struct adb_db *db;
  struct adb_table *table;
  struct adb_object_head *object_heads; /*!< clipped objects */
  struct htm_trixel *centre, **trixels;

  float fov;        /*!< Clipping radius in degrees */
  float centre_ra;  /*!< Clipping centre RA (circular) */
  float centre_dec; /*!< Clipping centre DEC (circular) */

  int fov_depth;
  int max_depth;
  int min_depth;
  int table_id;

  int valid_trixels;

  int count;
  int head_count;

  /* hashed object searching */
  struct table_hash hash;
};

/*! \struct struct adb_table
 * \brief Database Table.
 * \ingroup table
 *
 * Describes table in database.
 */
struct adb_table {
  int id;

  /* KD Tree Root */
  int kd_root;

  /* CDS identifiers */
  struct table_cds cds;

  /* table paths */
  struct table_path path;

  /* table object attributes */
  struct table_object object;

  /* depth by depth table HTM mappings */
  struct depth_map depth_map[HTM_MAX_DEPTH];
  int max_depth; /*!< deepest HTM depth used by this table */
  int depth_count[HTM_MAX_DEPTH];

  /* hashed object searching */
  struct table_hash hash;

  /* table import info */
  struct cds_importer import;

  struct adb_db *db;

  /* schema file index */
  struct table_file_index file_index;

  /* all objects in array */
  struct adb_object *objects;
};

/**
 * \brief Get the internal C type corresponding to a column type string.
 * \param type the type string
 * \return the corresponding adb_ctype
 */
adb_ctype table_get_column_ctype(char *type);

/**
 * \brief Get the size in bytes of a column type string.
 * \param type the type string
 * \return the size in bytes
 */
int table_get_column_csize(char *type);

/**
 * \brief Read trixels for a given table from the database.
 * \param db pointer to the database
 * \param table pointer to the table
 * \return 0 on success, negative error code on failure
 */
int table_read_trixels(struct adb_db *db, struct adb_table *table);

/**
 * \brief Insert an object into a table.
 * \param db pointer to the database
 * \param table_id the ID of the table
 * \param object pointer to the object to insert
 * \return 0 on success, negative error code on failure
 */
int table_insert_object(struct adb_db *db, int table_id,
                        struct adb_object *object);

/**
 * \brief Get the maximum depth mapped for a given value in a table.
 * \param table pointer to the table
 * \param value the value to search for
 * \return the max depth
 */
int table_get_object_depth_max(struct adb_table *table, float value);

/**
 * \brief Get the minimum depth mapped for a given value in a table.
 * \param table pointer to the table
 * \param value the value to search for
 * \return the min depth
 */
int table_get_object_depth_min(struct adb_table *table, float value);

/**
 * \brief Get the hashmap index of a key in a given table.
 * \param db pointer to the database
 * \param table_id the table ID
 * \param key the hashed key string
 * \return the hashmap index or negative error code on failure
 */
int table_get_hashmap(struct adb_db *db, int table_id, const char *key);

/**
 * \brief Return a table ID to the pool of available table IDs.
 * \param db pointer to the database
 * \param id the table ID to return
 */
void table_put_id(struct adb_db *db, int id);

/**
 * \brief Get a free table ID from the database pool.
 * \param db pointer to the database
 * \return the table ID or negative error code if none available
 */
int table_get_id(struct adb_db *db);

#endif

#endif
