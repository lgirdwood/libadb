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
 *  Copyright (C) 2008, 2012 Liam Girdwood
 */

#ifndef __LNC_TABLE_H
#define __LNC_TABLE_H

#include <stdlib.h>
#include <math.h>
#include <libastrodb/private.h>
#include <libastrodb/htm.h>
#include <libastrodb/astrodb.h>
#include <libastrodb/hash.h>
#include <libastrodb/schema.h>
#include <libastrodb/import.h>

#define ADB_TABLE_MAX_FIELDS		128	/* max number of indexes */
#define ADB_TABLE_MAX_ALT_FIELDS		16	/* max number of alternate indexes */

#define ADB_TABLE_HISTOGRAM_DIVS	100

struct adb_db;
struct adb_table;

struct depth_map {
	int table_id;			/*!< our table ID at this depth */
	float min_value;		/*!< minimum object primary key value at this depth */
	float max_value;		/*!< maximum object primary key value at this depth */
};


/*! \struct struct adb_object_setper
 * \brief Database Table Clipping.
 * \ingroup table
 *
 * Describes clipping table in database.
 */
struct adb_object_set {
	struct adb_db *db;
	struct adb_table *table;
	struct adb_object_head *object_heads;	/*!< clipped objects */
	struct htm_trixel *centre, **trixels;
	
	float fov;						/*!< Clipping radius in degrees */
	float centre_ra;				/*!< Clipping centre RA (circular) */
	float centre_dec;			/*!< Clipping centre DEC (circular) */

	int fov_depth;
	int max_depth;
	int min_depth;
	int table_id;

	int valid_trixels;

	int count;
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
	struct depth_map depth_map[ADB_MAX_TABLES];
	int max_depth;		/*!< deepest HTM depth used by this table */

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

adb_ctype table_get_column_ctype(char *type);
int table_get_column_csize(char *type);

int table_read_trixels(struct adb_db *db, struct adb_table *table, 
	int table_id);

int table_insert_object(struct adb_db *db, int table_id,
		struct adb_object *object);

int table_get_object_depth_max(struct adb_table *table, float value);
int table_get_object_depth_min(struct adb_table *table, float value);

#endif
