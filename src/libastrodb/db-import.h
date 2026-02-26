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

/**
 * \defgroup import Database Import
 * \brief Structures, macros, and API for defining schema and importing text catalogs
 */

/**
 * \brief Convenience constructor macro for defining an atomic schema member (e.g. integer, float)
 * \ingroup import
 * \param lname String name labeling the field
 * \param lsymbol Short symbol identifying the field
 * \param lagg The aggregate parent struct type holding the field
 * \param latom The specific member variable name within the struct
 * \param ltype The adb_ctype data type parsing format to use
 * \param lunits String denoting measurement units
 * \param lgposn Positional grouping index for multi-part values like HMS (-1 if none)
 * \param limport Optional specific import callback handler for the field
 */
#define adb_member(lname, lsymbol, lagg, latom, ltype, lunits, lgposn, \
				   limport)                                            \
	{                                                                  \
		.name = lname,                                                 \
		.symbol = lsymbol,                                             \
		.struct_offset = adb_offset(lagg, latom),                      \
		.struct_bytes = adb_sizeof(lagg, latom),                       \
		.units = lunits,                                               \
		.type = ltype,                                                 \
		.group_posn = lgposn,                                          \
		.import = limport,                                             \
	}

/**
 * \brief Convenience constructor macro for defining a grouped schema member (e.g. part of HMS coordinates)
 * \ingroup import
 * \param lname String name labeling the field
 * \param lsymbol Short symbol identifying the field
 * \param lagg The aggregate parent struct type holding the field
 * \param latom The specific member variable name within the struct
 * \param ltype The adb_ctype format
 * \param lunits String denoting measurement units
 * \param lgposn Ordinal position combining the group elements logically
 * \param limport Optional specific import callback handler
 */
#define adb_gmember(lname, lsymbol, lagg, latom, ltype, lunits, lgposn, \
					limport)                                            \
	{                                                                   \
		.name = lname,                                                  \
		.symbol = lsymbol,                                              \
		.struct_offset = adb_offset(lagg, latom),                       \
		.struct_bytes = adb_sizeof(lagg, latom),                        \
		.units = lunits,                                                \
		.type = ltype,                                                  \
		.import = limport,                                              \
		.group_posn = lgposn,                                           \
		.group_offset = adb_offset(lagg, latom),                        \
	}

/** \brief Maximum length for a schema field name string \ingroup import */
#define ADB_SCHEMA_NAME_SIZE 32
/** \brief Maximum length for a schema field symbol string \ingroup import */
#define ADB_SCHEMA_SYMBOL_SIZE 8
/** \brief Maximum length for a schema field units string \ingroup import */
#define ADB_SCHEMA_UNITS_SIZE 8

/*! \enum adb_ctype
 * \brief C data type enumeration mapping ASCII catalog string formatting formats to memory
 * \ingroup import
 */
typedef enum {
	ADB_CTYPE_INT, /*!< Standard 32-bit Integer */
	ADB_CTYPE_SHORT, /*!< 16-bit Short Integer */
	ADB_CTYPE_DOUBLE, /*!< 64-bit Double Precision Float */
	ADB_CTYPE_DEGREES, /*!< Double precision value treated natively as angular degrees */
	ADB_CTYPE_FLOAT, /*!< 32-bit Single Precision Float */
	ADB_CTYPE_STRING, /*!< Standard fixed-length character array or string */
	ADB_CTYPE_SIGN, /*!< Character representing explicitly a mathematical '+' or '-' */
	ADB_CTYPE_DOUBLE_HMS_HRS, /*!< The Hours integer segment of an HMS coordinate converted to a degree double */
	ADB_CTYPE_DOUBLE_HMS_MINS, /*!< The Minutes segment of an HMS coordinate converted to a degree double */
	ADB_CTYPE_DOUBLE_HMS_SECS, /*!< The Seconds float segment of an HMS coordinate converted to a degree double */
	ADB_CTYPE_DOUBLE_DMS_DEGS, /*!< The Degrees segment of a DMS coordinate converted to double */
	ADB_CTYPE_DOUBLE_DMS_MINS, /*!< The arc-minutes segment of a DMS coordinate converted to double */
	ADB_CTYPE_DOUBLE_DMS_SECS, /*!< The arc-seconds sequence of a DMS coordinate converted to a double */
	ADB_CTYPE_DOUBLE_MPC, /*!< Minor planet center specific double date format */
	ADB_CTYPE_NULL, /*!< Missing or purposefully ignored column token */
} adb_ctype;

/*! \enum adb_import_type
 * \brief Sorting configuration specifying object orientation on structural limits
 * \ingroup import
 */
typedef enum {
	ADB_IMPORT_INC, /*!< Imported limit property sorted via increasing values */
	ADB_IMPORT_DEC, /*!< Imported limit property sorted via decreasing values */
} adb_import_type;

/*! \typedef adb_field_import1
 * \brief Custom import function parsing a single token into a target object
 * \ingroup import
 */
typedef int (*adb_field_import1)(struct adb_object *, int, char *);

/*! \typedef adb_field_import2
 * \brief Custom import function parsing two raw tokens into an initialized target object
 * \ingroup import
 */
typedef int (*adb_field_import2)(struct adb_object *, int, char *, char *);

/*! \struct adb_schema_field
 * \ingroup import
 * \brief Logical schema definition representing a single data column map from ASCII
 *
 * Dictates byte alignments, parsing formats (adb_ctype), and relationships 
 * to load plain-text structured catalog files into structured binary memory.
 */
struct adb_schema_field {
	char name[ADB_SCHEMA_NAME_SIZE]; /*!< Explicit field name identifier */
	char symbol
		[ADB_SCHEMA_SYMBOL_SIZE]; /*!< Shorthand symbol referring to the field */
	int32_t
		struct_offset; /*!< Memory byte offset where the value begins in struct adb_object */
	int32_t
		struct_bytes; /*!< Storage byte magnitude the value structurally requires */
	int32_t
		group_offset; /*!< Offset pointing to a grouped data structure (-1 if atomic/independent) */
	int32_t
		group_posn; /*!< The ordinal parsing order position within a group (lowest integer is first) */
	int32_t
		text_size; /*!< Character length footprint representing value in ASCII line string */
	int32_t
		text_offset; /*!< Byte offset locating value start in an ASCII chunk line */
	adb_ctype
		type; /*!< Defined parser enumeration dictating translation of ASCII text */
	char units
		[ADB_SCHEMA_UNITS_SIZE]; /*!< Displayable unit measurements string map */
	adb_field_import1
		import; /*!< Optional custom insert/conversion parsing callback procedure */
};

/**
 * \brief Configure a new table architecture mapped for importing
 * \ingroup import
 * \param db Active database context
 * \param cat_class General class label of the catalog text (e.g. "II")
 * \param cat_id ID of the parent catalog (e.g. "246")
 * \param table_name Specific filename or reference binding
 * \param depth_field Schema label indicating the principal value driving tree generation
 * \param min_limit Floor clamp boundary applied universally across importing entries
 * \param max_limit Ceiling clamp boundary condition applied across importing
 * \param otype Indicated Sorting structure orientation (Incrementing or Decreasing)
 * \return Result table ID for newly allocated setup, or negative on failure
 */
int adb_table_import_new(struct adb_db *db, const char *cat_class,
						 const char *cat_id, const char *table_name,
						 const char *depth_field, float min_limit,
						 float max_limit, adb_import_type otype);

/**
 * \brief Register the completed schema format definitions against a new table
 * \ingroup import
 * \param db Initialized parent Database context
 * \param table_id Pending target table ID
 * \param schema C Array outlining sequential schema logic mappings
 * \param num_schema_fields Count denoting the amount of elements present inside schema array
 * \param object_size Explicit binary byte layout density mapping to one instance
 * \return 0 marking complete success tying the schema config, or negative on failures
 */
int adb_table_import_schema(struct adb_db *db, int table_id,
							struct adb_schema_field *schema,
							int num_schema_fields, int object_size);

/**
 * \brief Designate an alternative fallback field resolving for instances where standard field reads empty
 * \ingroup import
 * \param db Parent Database connection context
 * \param table_id Reference to the target allocated mapping table
 * \param field String name of the primary parsing field
 * \param alt String name of alternative field schema
 * \param flags Options or behavior control bits applying to this failover evaluation
 * \return 0 denoting successfully establishing link mapping, or negative resolving error
 */
int adb_table_import_field(struct adb_db *db, int table_id, const char *field,
						   const char *alt, int flags);

/**
 * \brief Trigger sequence to digest ASCII file resolving logic limits and translating to cache
 * \ingroup import
 * \param db Context mapping holding root constraints
 * \param table_id Logical integer binding the ready setup scheme to process text from
 * \return Evaluated sequence code; 0 on success, < 0 corresponding to system failures
 */
int adb_table_import(struct adb_db *db, int table_id);

/**
 * \brief Peek dynamically evaluating the schema type configured representing a struct field
 * \ingroup import
 * \param db Active mapping bounds instance
 * \param table_id Handle tied to subset scheme checking
 * \param field Identified schema map item resolving
 * \return Specific recognized enumeration format type
 */
adb_ctype adb_table_get_field_type(struct adb_db *db, int table_id,
								   const char *field);

/**
 * \brief Access configured struct binary block offset bytes matching field name
 * \ingroup import
 * \param db Master wrapper binding context limits
 * \param table_id The identifier target structure
 * \param field Name lookup reference resolving against schema definitions
 * \return Byte footprint depth offset from object root
 */
int adb_table_get_field_offset(struct adb_db *db, int table_id,
							   const char *field);

/**
 * \brief Forcibly assign an explicit unique alias and record scale footprint independent of file mapping
 * \ingroup import
 * \param db Parent state controller context structure
 * \param table_id Target integer ID marking specific architecture
 * \param dataset Literal label replacement enforcing overrides
 * \param num_objects Count setting fixed limits rather than parsing boundaries
 * \return Explicit 0 evaluating to success overriding structures
 */
int adb_table_import_alt_dataset(struct adb_db *db, int table_id,
								 const char *dataset, int num_objects);

#ifdef __cplusplus
};
#endif

#endif
