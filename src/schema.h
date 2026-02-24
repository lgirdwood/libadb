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

#ifndef __ADB_SCHEMA_H
#define __ADB_SCHEMA_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define ADB_TABLE_HISTOGRAM_DIVS	100
#define ADB_TABLE_DEPTH_DIVS	100

/*! \defgroup schema Schema
 *
 * Dataset schema definition and loader routines.
 */

struct adb_db;
struct adb_table;
struct adb_schema_field;

/*! \struct table_depth
 * \brief Table depth limits.
 * \ingroup table
 *
 * Defines the minimum and maximum property bounds within a specific depth array.
 */
struct table_depth {
	float min; /*!< Minimum bounding value */
	float max; /*!< Maximum bounding value */
};

/*! \struct table_file_index
 * \brief Dataset binary db index.
 * \ingroup schema
 *
 * Index data for binary catalog datasets.
 */
struct table_file_index {
	int32_t catalog_magic;		/*!< Magic number identifying catalog format */
	int32_t endian_magic;		/*!< Magic number to determine endianess */
	int32_t max_depth;			/*!< Maximum KD-tree or HTM depth of table */
	int32_t object_bytes;		/*!< Object size in bytes */
	int32_t object_count;		/*!< Total number of objects in catalog */
	int32_t field_count;		/*!< Number of custom indexed fields */
	int32_t insert_id;			/*!< Object import function identifier */
	uint32_t kd_root;           /*!< HTM root node offset */
	uint32_t histo[ADB_TABLE_HISTOGRAM_DIVS];       /*!< Values density histogram */
	struct table_depth depth[ADB_TABLE_DEPTH_DIVS]; /*!< Bounds per depth tier */
} __attribute__((packed));

/*
 * Importer API.
 */

/*!
 * \brief Find a primary field index.
 * \ingroup schema
 *
 * \param db Database instance
 * \param table Table containing the schema fields
 * \param field Name of the field to search for
 * \return The field index within the table's schema, or a negative error code
 */
int schema_get_field(struct adb_db *db, struct adb_table *table,
	const char *field);

/*!
 * \brief Find an alternate field index.
 * \ingroup schema
 *
 * \param db Database instance
 * \param table Table containing the schema fields
 * \param field Name of the alternate field to search for
 * \return The alternate field index within the table's schema, or a negative error code
 */
int schema_get_alt_field(struct adb_db *db, struct adb_table *table,
	const char *field);

/*!
 * \brief Add a primary schema field.
 * \ingroup schema
 *
 * \param db Database instance
 * \param table Table to bind the new field column
 * \param new_schema_object The pre-defined schema field configurations
 * \return Native field offset ID, or negative error code
 */
int schema_add_field(struct adb_db *db, struct adb_table *table,
	struct adb_schema_field *new_schema_object);

/*!
 * \brief Add an alternative schema field logic mapping.
 * \ingroup schema
 *
 * \param db Database instance
 * \param table Target database table
 * \param field String name of the alternate schema property
 * \param pri_idx Target index of the existing primary field mapping
 * \return Alternate field ID, or negative error code
 */
int schema_add_alternative_field(struct adb_db *db,
	struct adb_table *table, const char *field, int pri_idx);

/*!
 * \brief Sort the table fields array.
 * \ingroup schema
 *
 * Orders the field indexes in the table schema block for consistent serialization.
 *
 * \param db Database instance
 * \param table Table whose indexes will be sorted
 * \return 0 on success, negative error code on failure
 */
int schema_order_import_index(struct adb_db *db,
	struct adb_table *table);

/*!
 * \brief Write the binary schema mappings.
 * \ingroup schema
 *
 * Serializes the schema structures into the output catalog disk file.
 *
 * \param db Database instance
 * \param table Database table to write
 * \return 0 on success, negative error code on failure
 */
int schema_write(struct adb_db *db, struct adb_table *table);

/*!
 * \brief Read and load the table schema configurations.
 * \ingroup schema
 *
 * Reads a native datastore binary representation and translates it 
 * into memory mappings on the library instance.
 *
 * \param db Database instance
 * \param table Database table bound to disk reader
 * \return 0 on success, negative error code on failure
 */
int schema_read(struct adb_db *db, struct adb_table *table);

#endif

#endif
