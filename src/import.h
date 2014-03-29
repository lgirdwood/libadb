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

#ifndef __ADB_IMPORT_H
#define __ADB_IMPORT_H

#include <stdlib.h>
#include <math.h>

#include "private.h"
#include "htm.h"

#define ADB_TABLE_MAX_FIELDS		128	/* max number of indexes */
#define ADB_TABLE_MAX_ALT_FIELDS		16	/* max number of alternate indexes */

struct adb_db;

struct alt_field {
	struct adb_schema_field key_field;     /* primary field index */
	struct adb_schema_field alt_field;     /* alternate field index */
	adb_field_import2 import; /*!< alt field importer */
};

/*! \struct struct cds_importer
 * \brief Database Table Importer.
 * \ingroup table
 *
 * Describes import table config.
 */
struct cds_importer {
	struct readme *readme;	/*!< Catalog ReadMe data */
	/* object index description */
	struct cds_byte_desc *byte_desc;
	struct cds_file_info *file_info;

	struct adb_schema_field field[ADB_TABLE_MAX_FIELDS];	/*!< key and custom field descriptors */
	struct alt_field alt_field[ADB_TABLE_MAX_ALT_FIELDS];    /*!< alt src data */
	int text_length;                /*!< length in chars of each record */
	int text_buffer_bytes;		/*!< largest import record text size */

	/* Histogram field and alt field */
	struct adb_schema_field *histogram_key;
	struct alt_field *histogram_alt_key;
	const char *depth_field;

	/* KD Tree data */
	int kd_root;
};

typedef int (*object_import) (struct adb_db *, struct adb_object *,
	struct adb_table *);

struct table_object {
	int bytes;				/*!< Object size (bytes) */
	int count;      		/*!< Number of objects in set */
	int field_count;		/*!< number of key and custom fields */
	int num_alt_fields;                 /*!< number of alternative object fields */
	object_import import;
	int new;
	float min_value;
	float max_value;
	adb_import_type otype;
};

/* init row insertion function */
void table_init_object_import(struct adb_db *db, int table_id);

adb_field_import2 table_get_alt_key_import(struct adb_db *db,
	adb_ctype type);

adb_field_import1 table_get_column_import(struct adb_db *db,
	adb_ctype type);

int table_write_trixels(struct adb_db *db, struct adb_table *table,
	int table_id);

int import_build_kdtree(struct adb_db *db, struct adb_table *table,
	int table_id);

int import_get_object_depth_max(struct adb_table *table, float value);
int import_get_object_depth_min(struct adb_table *table, float value);

#endif
