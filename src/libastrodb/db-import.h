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

#ifndef __LIBADB_IMPORT_H
#define __LIBADB_IMPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct adb_db;
struct adb_object;

/********************** Table import ******************************************/

/* convenience schema constructors */
#define adb_member(lname, lsymbol, lagg, latom, ltype, lunits, lgposn, limport) \
	{.name = lname, .symbol = lsymbol, .struct_offset = adb_offset(lagg, latom), \
	.struct_bytes = adb_sizeof(lagg, latom), .units = lunits, .type = ltype, \
	.units = lunits, .group_posn = lgposn, .import = limport,}
#define adb_gmember(lname, lsymbol, lagg, latom, ltype, lunits, lgposn, limport) \
	{.name = lname, .symbol = lsymbol, .struct_offset = adb_offset(lagg, latom), \
	.struct_bytes = adb_sizeof(lagg, latom), .units = lunits, .type = ltype, \
	.units = lunits, .import = limport, .group_posn = lgposn, \
	.group_offset = adb_offset(lagg, latom),}

#define ADB_SCHEMA_NAME_SIZE		32
#define ADB_SCHEMA_SYMBOL_SIZE		8
#define ADB_SCHEMA_UNITS_SIZE		8

/*! \typedef enum adb_ctype
 * \ingroup dataset
 *
 * C type of ASCII dataset field
 */
typedef enum {
	ADB_CTYPE_INT,					/*!< int */
	ADB_CTYPE_SHORT,				/*!< short */
	ADB_CTYPE_DOUBLE, 				/*!< double */
	ADB_CTYPE_FLOAT,				/*!< float */
	ADB_CTYPE_STRING,				/*!< string */
	ADB_CTYPE_SIGN,					/*!< sign + or - */
	ADB_CTYPE_DOUBLE_HMS_HRS,		/*!< degrees Hours (HMS)*/
	ADB_CTYPE_DOUBLE_HMS_MINS, 	/*!< degrees Minutes  (HMS)*/
	ADB_CTYPE_DOUBLE_HMS_SECS, 	/*!< degrees Seconds (HMS)*/
	ADB_CTYPE_DOUBLE_DMS_DEGS,	/*!< degrees (DMS) */
	ADB_CTYPE_DOUBLE_DMS_MINS, 	/*!< degrees Minutes (DMS) */
	ADB_CTYPE_DOUBLE_DMS_SECS,		/*!< degrees Seconds (DMS) */
	ADB_CTYPE_DOUBLE_MPC,			/*!< Minor planet centre date format */
	ADB_CTYPE_NULL,					/*!< NULL */
} adb_ctype;

typedef enum {
	ADB_IMPORT_INC,
	ADB_IMPORT_DEC,
} adb_import_type;

typedef int (*adb_field_import1)(struct adb_object *, int, char *);
typedef int (*adb_field_import2)(struct adb_object *, int, char *, char *);

/*! \struct adb_schema_object
 * \ingroup dataset
 *
 * Represents a field within a dataset structure.
 */
struct adb_schema_field {
	char name[ADB_SCHEMA_NAME_SIZE];		/*!< field name */
	char symbol[ADB_SCHEMA_SYMBOL_SIZE];		/*!< field symbol */
	int32_t struct_offset;					/*!< struct offset */
	int32_t struct_bytes;					/*!< struct size */
	int32_t group_offset;					/*!< group offset, -1 if atomic */
	int32_t group_posn;					/*!< group posn, lowest is first */
	int32_t text_size;					/*!< line size */
	int32_t text_offset;					/*! line offset */
	adb_ctype type;				/*!< field type */
	char units[ADB_SCHEMA_UNITS_SIZE];		/*!< field units */
	adb_field_import1 import;			/* custom insert method */
};

/*! \fn int adb_table_create(struct adb_db *db, char *cat_class,
		char *cat_id, char *table_name, enum adb_table_type type,
		float min_limit, float max_limit, float max_resolution);
 * \brief Create a new dataset
 * \ingroup dataset
 */
int adb_table_import_new(struct adb_db *db,
		const char *cat_class, const char *cat_id, const char *table_name,
		const char *depth_field, float min_limit, float max_limit,
		adb_import_type otype);

/*! \fn int adb_table_register_schema(struct adb_db *db,
					struct adb_schema_object *schema,
					int schema_size, int object_size);
 * \brief Register a new table schema
 * \ingroup table
 */
int adb_table_import_schema(struct adb_db *db, int table_id,
					struct adb_schema_field *schema,
					int num_schema_fields, int object_size);

/*! \fn int adb_table_alt_field(adb_table *table, char* field, char* alt,
					int flags);
* \brief Set an alternative field if another field is blank
* \ingroup dataset
*/
int adb_table_import_field(struct adb_db *db, int table_id, const char *field,
	const char *alt, int flags);


int adb_table_import(struct adb_db *db, int table_id);

/*! \fn adb_ctype adb_table_get_field_type(adb_table *table, char* field);
 * \brief Get field type in struct
 * \ingroup dataset
 */
adb_ctype adb_table_get_field_type(struct adb_db *db, int table_id,
	const char* field);

/*! \fn int adb_table_get_field_offset(adb_table *table, char* field);
 * \brief Get field offset in struct
 * \ingroup dataset
 */
int adb_table_get_field_offset(struct adb_db *db, int table_id,
	const char* field);

#ifdef __cplusplus
};
#endif

#endif
