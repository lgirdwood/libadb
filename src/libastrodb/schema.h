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

#ifndef __ADB_SCHEMA_H
#define __ADB_SCHEMA_H

#define ADB_TABLE_HISTOGRAM_DIVS	100
#define ADB_TABLE_DEPTH_DIVS	100

struct astrodb_db;
struct astrodb_table;
struct astrodb_schema_field;

struct table_depth {
	float min;
	float max;
};

/*! \typedef astrodb_table_info
 * \brief Dataset binary db index.
 * \ingroup dataset
 *
 * Index data for binary catalog datasets.
 */
struct table_file_index {
	int32_t catalog_magic;		/*!< magic number */
	int32_t endian_magic;		/*!< magic number to determine endianess */
	int32_t max_depth;			/*!< max depth of table */
	int32_t object_bytes;			/*!< Object size (bytes) */
	int32_t object_count;			/*!< Number of objects in catalog */
	int32_t field_count;		/*!< number of custom fields */
	int32_t insert_id;			/*!< object import func */
	uint32_t kd_root;
	uint32_t histo[ADB_TABLE_HISTOGRAM_DIVS];
	struct table_depth depth[ADB_TABLE_DEPTH_DIVS];
};

/*
 * Importer API.
 */

int schema_add_alternative_field(struct astrodb_db *db,
	struct astrodb_table *table, const char *field, int pri_idx);

int schema_order_import_index(struct astrodb_db *db,
	struct astrodb_table *table);

int schema_get_field(struct astrodb_db *db, struct astrodb_table *table,
	const char *field);
int schema_get_alt_field(struct astrodb_db *db, struct astrodb_table *table,
	const char *field);

int schema_add_field(struct astrodb_db *db, struct astrodb_table *table,
	struct astrodb_schema_field *new_schema_object);

int schema_add_alternative_field(struct astrodb_db *db,
	struct astrodb_table *table, const char *field, int pri_idx);

int schema_write(struct astrodb_db *db, struct astrodb_table *table);


/*
 * Data Access API
 */

int schema_read(struct astrodb_db *db, struct astrodb_table *table);

#endif
