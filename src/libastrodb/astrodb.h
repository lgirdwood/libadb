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
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Liam Girdwood
 */

/*! \mainpage libastrodb
* <A>General Purpose Astronomical Database</A>
*
* \section intro Introduction
* Libncat is a general purpose astronomical database designed to give very fast
* access to <A href="http://cdsweb.u-strasbg.fr/">CDS</A> catalog data. It is designed to be independent of any underlying
* catalog data formatting and will import most data records providing the
* catalog ships with a "ReadMe" file that follows the CDS <A href="http://vizier.u-strasbg.fr/doc/catstd.htx">formatting specifications</A>.
* Libncat provides a simple database backend and exposes a C API for catalog access.
*
* The intended audience of libastrodb is C / C++ programmers, astronomers and anyone working with large astronomical catalogs.
*
* Libncat will be the database backend used by the <A href="http://nova.sf.net">Nova</A>
* project and most importantly, is free software.
*
* \section features Features
* The current release of libastrodb supports:-
*
* - Parsing of important fields in CDS ReadMe files
* - Downloading catalog data from CDS to a local library
* - Importing selected catalog object data and object fields into machine formats.
* - Fast access to catalog data based on :-
* 		- position
*		- position and magnitude
*		- hashed object ID's
*		- distance (near sky catalogs)
* - Progress feedback.
* - Powerfull catalog searching on any combinations of fields.
*
* \section docs Documentation
* API documentation for libastrodb is included in the source. It can also be found in this website and an offline tarball is available <A href="http://libnovacat.sf.net/libastrodbdocs.tar.gz">here</A>.
*
* \section download Download
* The latest release is 0.1 and is available <A href="http://sourceforge.net/project/showfiles.php?group_id=133878">here.</A>
*
* \section cvs CVS
* The latest CVS version of libastrodb is available via CVS <A href="http://sf.net/cvs/?group_id=133878">here.</A>
*
* \section licence Licence
* libastrodb is released under the <A href="http://www.gnu.org">GNU</A> LGPL.
*
* \section authors Authors
* libastrodb is maintained by <A href="mailto:liam@gnova.org">Liam Girdwood</A>
*
*/

#ifndef __LIBNCAT_H
#define __LIBNCAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define astrodb_offset(x,y) (long)(&((x*)0)->y) 		/* offset in struct */
#define astrodb_sizeof(x,y) sizeof(((x*)0)->y) 		/* size in struct */
#define astrodb_size(x) (sizeof(x)/sizeof(x[0]))		/* array size */

struct astrodb_library;
struct astrodb_db;
struct astrodb_table;
struct astrodb_search;

/*
 * libastrodb interface version
 */
#define ADB_IDX_VERSION 3

struct astrodb_posn_mag {
	double ra;
	double dec;
	float Vmag;
};

struct astrodb_posn_size {
	double ra;
	double dec;
	float size;
};

struct astrodb_posn_type {
	double ra;
	double dec;
	uint32_t type;
};

struct astrodb_size_type {
	double minor_size;
	double major_size;
	uint32_t type;
};

struct astrodb_object;

#define ADB_OBJECT_NAME_SIZE	16

struct adb_kd_tree {
	/* KD tree - indexs */
	int32_t child[2], parent, index;
};

struct adb_import {
	/* next object (in Vmag or size) or NULL */
	struct astrodb_object *next;

	struct adb_kd_tree *kd;
};

struct astrodb_object {
	/* primary keys for object hash based access */
	union {
		unsigned long id;
		char designation[ADB_OBJECT_NAME_SIZE];
	};
	/* primary keys for object attribute based access */
	union {
		struct astrodb_posn_mag posn_mag;
		struct astrodb_posn_size posn_size;
		struct astrodb_posn_type posn_type;
		struct astrodb_size_type size_type;
	};

	union {
		struct adb_import import; /* only used for importing */
		struct adb_kd_tree kd;	/* only used for imported data */
	};

	/* remainder of object here - variable size and type */
};


/********************** Library and debug *************************************/

/*! \typedef enum astrodb_msg_level
 * \ingroup Library
 *
 * AstroDB message level.
 */
enum astrodb_msg_level {
	ADB_MSG_NONE	= 0,
	ADB_MSG_INFO 	= 1,
	ADB_MSG_WARN	= 2,
	ADB_MSG_DEBUG	= 3,
	ADB_MSG_VDEBUG	= 4,
};

#define ADB_LOG_HTM_CORE	(1 << 0)
#define ADB_LOG_HTM_GET		(1 << 1)
#define ADB_LOG_HTM_FILE	(1 << 2)
#define ADB_LOG_HTM_INSERT	(1 << 3)
#define ADB_LOG_HTM_ALL		(ADB_LOG_HTM_CORE | ADB_LOG_HTM_GET |\
				 ADB_LOG_HTM_FILE | ADB_LOG_HTM_INSERT)

#define ADB_LOG_CDS_FTP		(1 << 4)
#define ADB_LOG_CDS_PARSER	(1 << 5)
#define ADB_LOG_CDS_SCHEMA	(1 << 6)
#define ADB_LOG_CDS_TABLE	(1 << 7)
#define ADB_LOG_CDS_DB		(1 << 8)
#define ADB_LOG_CDS_IMPORT	(1 << 9)
#define ADB_LOG_CDS_KDTREE	(1 << 10)
#define ADB_LOG_CDS_ALL		(ADB_LOG_CDS_FTP | ADB_LOG_CDS_PARSER |\
				 ADB_LOG_CDS_SCHEMA | ADB_LOG_CDS_TABLE |\
				 ADB_LOG_CDS_DB | ADB_LOG_CDS_IMPORT |\
				 ADB_LOG_CDS_KDTREE)

#define ADB_LOG_SEARCH		(1 << 11)
#define ADB_LOG_SOLVE		(1 << 12)

#define ADB_LOG_ALL		(ADB_LOG_HTM_ALL | ADB_LOG_CDS_ALL | ADB_LOG_SEARCH | ADB_LOG_SOLVE)

/*! \fn astrodb_library* astrodb_open_library(char *remote, char* local);
 * \brief Create a library
 * \ingroup library
 */
void astrodb_set_msg_level(struct astrodb_db *db, enum astrodb_msg_level level);

/*! \fn astrodb_library* astrodb_open_library(char *remote, char* local);
 * \brief Create a library
 * \ingroup library
 */
void astrodb_set_log_level(struct astrodb_db *db, unsigned int log);

/*! \fn astrodb_library* astrodb_open_library(char *remote, char* local);
 * \brief Create a library
 * \ingroup library
 */
struct astrodb_library *astrodb_open_library(const char *host,
	const char *remote, const char *local);

/*! \fn void astrodb_close_library(astrodb_library* lib);
 * \brief Free the library resources
 * \ingroup library
 */
void astrodb_close_library(struct astrodb_library *lib);

/*! \fn const char* astrodb_get_version(void);
 * \brief Get the libastrodb version number.
 * \ingroup misc
 */
const char *astrodb_get_version(void);



/********************* DB creation ********************************************/

/*! \fn astrodb_db* astrodb_create_db(astrodb_library* lib, char* cclass, char* cnum,
	double ra_min, double ra_max,double dec_min, double dec_max,
	double mag_faint, double mag_bright, int flags);
 * \brief Create a new catalog
 * \ingroup catalog
 */
struct astrodb_db *astrodb_create_db(struct astrodb_library *lib,
		int depth, int tables);

/*! \fn void astrodb_db_free (astrodb_db* cat);
 * \brief Destroys catalog and frees resources
 * \ingroup catalog
 */
void astrodb_db_free(struct astrodb_db *db);


/********************** Table import ******************************************/

/* convenience schema constructors */
#define astrodb_member(lname, lsymbol, lagg, latom, ltype, lunits, lgposn, limport) \
	{.name = lname, .symbol = lsymbol, .struct_offset = astrodb_offset(lagg, latom), \
	 .struct_bytes = astrodb_sizeof(lagg, latom), .units = lunits, .type = ltype, \
	 .units = lunits, .group_posn = lgposn, .import = limport,}
#define astrodb_gmember(lname, lsymbol, lagg, latom, ltype, lunits, lgposn, limport) \
	{.name = lname, .symbol = lsymbol, .struct_offset = astrodb_offset(lagg, latom), \
	 .struct_bytes = astrodb_sizeof(lagg, latom), .units = lunits, .type = ltype, \
	 .units = lunits, .import = limport, .group_posn = lgposn, \
	 .group_offset = astrodb_offset(lagg, latom),}

#define ADB_SCHEMA_NAME_SIZE		32
#define ADB_SCHEMA_SYMBOL_SIZE		8
#define ADB_SCHEMA_UNITS_SIZE		8

/*! \typedef enum astrodb_ctype
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
} astrodb_ctype;

typedef enum {
	ADB_OTYPE_STAR,
	ADB_OTYPE_GALAXY,
} astrodb_otype;

typedef int (*adb_field_import1)(struct astrodb_object *, int, char *);
typedef int (*adb_field_import2)(struct astrodb_object *, int, char *, char *);

/*! \struct astrodb_schema_object
 * \ingroup dataset
 *
 * Represents a field within a dfataset structure.
 */
struct astrodb_schema_field {
	char name[ADB_SCHEMA_NAME_SIZE];		/*!< field name */
	char symbol[ADB_SCHEMA_SYMBOL_SIZE];		/*!< field symbol */
	int32_t struct_offset;					/*!< struct offset */
	int32_t struct_bytes;					/*!< struct size */
	int32_t group_offset;					/*!< group offset, -1 if atomic */
	int32_t group_posn;					/*!< group posn, lowest is first */
	int32_t text_size;					/*!< line size */
	int32_t text_offset;					/*! line offset */
	astrodb_ctype type;				/*!< field type */
	char units[ADB_SCHEMA_UNITS_SIZE];		/*!< field units */
	adb_field_import1 import;			/* custom insert method */
};

/*! \fn int astrodb_table_create(struct astrodb_db *db, char *cat_class,
		char *cat_id, char *table_name, enum astrodb_table_type type,
		float min_limit, float max_limit, float max_resolution);
 * \brief Create a new dataset
 * \ingroup dataset
 */
int astrodb_table_import_new(struct astrodb_db *db,
		const char *cat_class, const char *cat_id, const char *table_name,
		const char *depth_field, float min_limit, float max_limit,
		astrodb_otype otype);

/*! \fn int astrodb_table_register_schema(struct astrodb_db *db,
					struct astrodb_schema_object *schema,
					int schema_size, int object_size);
 * \brief Register a new table schema
 * \ingroup table
 */
int astrodb_table_import_schema(struct astrodb_db *db, int table_id,
					struct astrodb_schema_field *schema,
					int num_schema_fields, int object_size);

/*! \fn int astrodb_table_alt_field(astrodb_table *table, char* field, char* alt,
					int flags);
* \brief Set an alternative field if another field is blank
* \ingroup dataset
*/
int astrodb_table_import_field(struct astrodb_db *db, int table_id,
				const char *field, const char *alt, int flags);


int astrodb_table_import(struct astrodb_db *db, int table_id);


/********************* Table Management ***************************************/

/*! \fn astrodb_table_open(astrodb_table *table, astrodb_progress progress, int ra,
			int dec, int mag)
 * \brief Initialise new dataset.
 * \ingroup dataset
 */
int astrodb_table_open(struct astrodb_db *db, const char *cat_class,
		const char *cat_id, const char *table_name);

/*! \fn void astrodb_table_close(astrodb_table *table);
 * \brief Free's dataset and it's resources
 * \ingroup dataset
 */
int astrodb_table_close(struct astrodb_db *db, int table_id);

/*! \fn int astrodb_table_hash_key(astrodb_table *table, char* field);
* \brief Add a custom field to the dataset for importing
* \ingroup dataset
*/
int astrodb_table_hash_key(struct astrodb_db *db, int table_id, const char *key);

/*! \fn int astrodb_table_get_size(astrodb_table *table);
 * \brief Get dataset cache size.
 * \return size in bytes
 * \ingroup dataset
 */
int astrodb_table_get_size(struct astrodb_db *db, int table_id);

int astrodb_table_get_count(struct astrodb_db *db, int table_id);

/*! \fn int astrodb_table_get_object_size(astrodb_table *table);
 * \brief Get dataset object size.
 * \return size in bytes
 * \ingroup dataset
 */
int astrodb_table_get_object_size(struct astrodb_db *db, int table_id);

/*! \fn astrodb_ctype astrodb_table_get_field_type(astrodb_table *table, char* field);
 * \brief Get field type in struct
 * \ingroup dataset
 */
astrodb_ctype astrodb_table_get_field_type(struct astrodb_db *db,
	int table_id, const char* field);

/*! \fn int astrodb_table_get_field_offset(astrodb_table *table, char* field);
 * \brief Get field offset in struct
 * \ingroup dataset
 */
int astrodb_table_get_field_offset(struct astrodb_db *db,
	int table_id, const char* field);


/****************** Table Clipping ********************************************/

/*! \fn void astrodb_table_clip_on_fov (astrodb_table *table, double ra, double dec,
					double clip_fov, double faint_mag,
					double bright_mag);
 * \brief Set dataset clipping area based on field of view
 * \ingroup dataset
 */
struct astrodb_object_set *astrodb_table_set_new(struct astrodb_db *db,
	int table_id);

int astrodb_table_set_constraints(struct astrodb_object_set *set,
				double ra, double dec,
				double fov, double min_Z,
				double max_Z);

/*! \fn void astrodb_table_unclip (astrodb_table *table);
 * \brief Unclip dataset clipping area to full boundaries
 * \ingroup dataset
 */
void astrodb_table_set_free(struct astrodb_object_set *set);



/******************* Table Object Get *****************************************/

#define adb_object_ra(object) object->posn_mag.ra
#define adb_object_dec(object) object->posn_mag.dec
#define adb_object_keyval(object) object->posn_mag.Vmag
#define adb_object_vmag(object) object->posn_mag.Vmag
#define adb_object_size(object) object->posn_size.size
#define adb_object_type(object) object->posn_type.type
#define adb_object_min_size(object) object->size_type.minor_size
#define adb_object_maj_size(object) object->size_type.major_size

struct astrodb_object_head {
	const void *objects;
	unsigned int count;
};

int astrodb_table_set_get_objects(struct astrodb_object_set *set);

/*! \fn void* astrodb_table_get_object (astrodb_table *table, char *id, char *field);
 * \brief Get object from catalog based on ID
 * \return pointer to object or NULL
 * \ingroup dataset
 */
int astrodb_table_set_get_object(struct astrodb_object_set *set,
	const void *id, const char *field, const struct astrodb_object **object);

const struct astrodb_object *astrodb_table_set_get_nearest_on_object(
	struct astrodb_object_set *set, const struct astrodb_object *object);

const struct astrodb_object *astrodb_table_set_get_nearest_on_pos(
	struct astrodb_object_set *set, double ra, double dec);

struct astrodb_object_head *adb_set_get_head(struct astrodb_object_set *set);
int adb_set_get_count(struct astrodb_object_set *set);

/******************** Table Search ********************************************/

/*! \typedef enum astrodb_operator
 * \ingroup search
 *
 * Search operators
 */
enum astrodb_operator {
	ADB_OP_AND,				/*!< AND */
	ADB_OP_OR				/*!< OR */
};

/*! \typedef enum astrodb_comparator
 * \ingroup search
 *
 * Search field comparators
 */
enum astrodb_comparator {
	ADB_COMP_LT,			/*!< less than */
	ADB_COMP_GT, 			/*!< greater than */
	ADB_COMP_EQ,			/*!< equal to */
	ADB_COMP_NE			/*!< not equal to */
};

/*! \typedef int (*astrodb_custom_comparator)(void* object);
 *
 * A customised object search comarator.
 */
typedef int (*astrodb_custom_comparator)(void *object);

struct astrodb_search;

 /*! \fn astrodb_search* astrodb_search_new(astrodb_table *table);
 * \brief Creates an new search object
 * \ingroup search
 */
struct astrodb_search *astrodb_search_new(struct astrodb_db *db, int table_id);

/*! \fn void astrodb_search_free(astrodb_search* search);
 * \brief Free's a search and it resources
 * \ingroup search
 */
void astrodb_search_free(struct astrodb_search *search);

/*! \fn int astrodb_search_add_operator(astrodb_search* search, astrodb_operator op);
 * \brief Add an operation in RPN to the search
 * \ingroup search
 */
int astrodb_search_add_operator(struct astrodb_search *search,
				enum astrodb_operator op);

/*! \fn int astrodb_search_add_comparator(astrodb_search* search, char* field,
					astrodb_comparator compare, char* value);
 * \brief Add a comparator_t in RPN to the search
 * \ingroup search
 */
int astrodb_search_add_comparator(struct astrodb_search *search,
		const char* field, enum astrodb_comparator comp, const char* value);

/*! \fn int astrodb_search_add_custom_comparator(astrodb_search* search,
						astrodb_custom_comparator compare);
 * \brief Add a comparator_t in RPN to the search
 * \ingroup search
 */
int astrodb_search_add_custom_comparator(struct astrodb_search *search,
					astrodb_custom_comparator comp);

/*! \fn int astrodb_search_get_results(astrodb_search* search, astrodb_progress progress,
					astrodb_slist **result, unsigned int src);
 * \brief Execute a search
 * \ingroup search
 */
int astrodb_search_get_results(struct astrodb_search *search,
				struct astrodb_object_set *set,
				const struct astrodb_object **objects[]);

/*! \fn int astrodb_search_get_hits(astrodb_search* search);
 * \brief Get the number of search hit_count.
 * \ingroup search
 */
int astrodb_search_get_hits(struct astrodb_search *search);

/*! \fn int astrodb_search_get_tests(astrodb_search* search);
 * \brief Get the number of search test_count
 * \ingroup search
 */
int astrodb_search_get_tests(struct astrodb_search *search);

/******************** Table Solver ********************************************/

#define ADB_NUM_TARGETS	16

/*! \struct astrodb_pobject
 * \brief Plate object
 * \ingroup solve
 */
struct astrodb_pobject {
	int x;
	int y;
	unsigned int adu;
};

enum astrodb_constraint {
	ADB_CONSTRAINT_MAG,
	ADB_CONSTRAINT_FOV,
	ADB_CONSTRAINT_RA,
	ADB_CONSTRAINT_DEC,
	/* TODO add others */
};

enum astrodb_find {
	ADB_FIND_ALL,
	ADB_FIND_FIRST,
};

struct astrodb_source_objects {
	const struct astrodb_object **objects;
	int num_objects;
};

/*! \struct astrodb_solve_objects
 * \brief Solved objects
 * \ingroup solve
 */
struct astrodb_solve_objects {
	/* in order of brightness */
	const struct astrodb_object *object[ADB_NUM_TARGETS];

	/* source object storage */
	struct astrodb_source_objects source;
	struct astrodb_object_set *set;

	double delta_pa;
	double delta_distance;
	double delta_magnitude;
	double divergance;
	double rad_per_pix;
};

struct astrodb_solve;

 /*! \fn astrodb_search* astrodb_search_new(astrodb_table *table);
 * \brief Creates an new search object
 * \ingroup search
 */
struct astrodb_solve *astrodb_solve_new(struct astrodb_db *db, int table_id);

/*! \fn void astrodb_search_free(astrodb_search* search);
 * \brief Free's a search and it resources
 * \ingroup search
 */
void astrodb_solve_free(struct astrodb_solve *solve);

int astrodb_solve_set_magnitude_delta(struct astrodb_solve *solve,
		double delta_mag);

int astrodb_solve_set_distance_delta(struct astrodb_solve *solve,
		double delta_pixels);

int astrodb_solve_set_pa_delta(struct astrodb_solve *solve,
		double delta_degrees);

/*! \fn int astrodb_search_add_operator(astrodb_search* search, astrodb_operator op);
 * \brief Add an operation in RPN to the search
 * \ingroup search
 */
int astrodb_solve_add_plate_object(struct astrodb_solve *solve,
				struct astrodb_pobject *pobject);

/*! \fn int astrodb_search_add_comparator(astrodb_search* search, char* field,
					astrodb_comparator compare, char* value);
 * \brief Add a comparator_t in RPN to the search
 * \ingroup search
 */
int astrodb_solve_constraint(struct astrodb_solve *solve,
		enum astrodb_constraint type, double min, double max);


/*! \fn int astrodb_solve(struct astrodb_solve *solve,
				struct astrodb_object_set *set,
				struct astrodb_solve_objects **solve_objects[],
				double dist_coeff, double mag_coeff,
				double pa_coeff);
 * \brief Execute a search
 * \ingroup search
 */
int astrodb_solve(struct astrodb_solve *solve,
				struct astrodb_object_set *set, enum astrodb_find find);

int astrodb_solve_get_solutions(struct astrodb_solve *solve,
	unsigned int solution, struct astrodb_solve_objects **solve_objects);

/*! \fn int astrodb_solve_get_object(struct astrodb_solve *solve,
		struct astrodb_solve_objects *solve_objects,
		struct astrodb_pobject *pobject, const struct astrodb_object **object);
 * \brief Execute a search
 * \ingroup search
 */
int astrodb_solve_get_object(struct astrodb_solve *solve,
	struct astrodb_solve_objects *solve_objects,
	struct astrodb_pobject *pobject, const struct astrodb_object **object,
	struct astrodb_object *o);

int astrodb_solve_prep_solution(struct astrodb_solve *solve,
		unsigned int solution, double fov, double mag_limit);

#ifdef __cplusplus
};
#endif

#endif
