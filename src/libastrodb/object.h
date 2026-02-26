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

#ifndef __LIBADB_OBJECT_H
#define __LIBADB_OBJECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup dataset Database Objects and Datasets
 * \brief Core structures for catalog objects and dataset handling
 */

/**
 * \brief Maximum size of an object name/designation string representation
 * \ingroup dataset
 */
#define ADB_OBJECT_NAME_SIZE 16

/**
 * \brief Macro to calculate the byte offset of a member within a struct type
 * \ingroup dataset
 */
#define adb_offset(x, y) (long)(&((x *)0)->y)

/**
 * \brief Macro to calculate the byte size of a member within a struct type
 * \ingroup dataset
 */
#define adb_sizeof(x, y) sizeof(((x *)0)->y)

/**
 * \brief Macro to calculate the number of elements in a static array
 * \ingroup dataset
 */
#define adb_size(x) (sizeof(x) / sizeof(x[0]))

struct adb_db;

/*! \struct adb_kd_tree
 * \brief KD tree node representation for spatial indexing within sets
 * \ingroup dataset
 */
struct adb_kd_tree {
	/* KD tree - indexs */
	int32_t child[2]; /*!< indices of the child tree nodes (left and right) */
	int32_t parent; /*!< index of the parent tree node */
	int32_t index; /*!< underlying dataset reference index for this node */
};

/*! \struct adb_import
 * \brief Import node structure used to construct tree relations during data loading
 * \ingroup dataset
 */
struct adb_import {
	/* next object (in Vmag or size) or NULL */
	struct adb_object *next; /*!< pointer to next object in list format */
	struct adb_kd_tree *kd; /*!< pointer to the associated KD tree node */
};

/*! \struct adb_object
 * \brief Core database astronomical object structure
 * \ingroup dataset
 *
 * Represents an individual entity (star, galaxy, etc.) loaded from a catalog.
 */
struct adb_object {
	/* primary keys for object hash based access */
	union {
		unsigned long id; /*!< unique numeric object ID */
		char designation
			[ADB_OBJECT_NAME_SIZE]; /*!< text-based object designation string */
	};

	/* primary keys for object attribute based access */
	double ra; /*!< Right Ascension coordinate */
	double dec; /*!< Declination coordinate */
	float mag; /*!< measured magnitude */
	float size; /*!< angular size of the object */

	union {
		struct adb_import
			import; /*!< only used internally during table importing */
		struct adb_kd_tree
			kd; /*!< only used for spatial relations on imported data */
	};

	/* remainder of object here - variable size and type */
};

/****************** Table Clipping ********************************************/

/*! \struct adb_object_set
 * \brief Opaque structure representing an active and constrained working set of database objects
 * \ingroup dataset
 */
struct adb_object_set;

/**
 * \brief Creates a new dataset collection linked to a specific database table
 * \ingroup dataset
 * \param db Pointer to the database context
 * \param table_id The identifier of the table to associate with the set
 * \return Pointer to the new dataset structure, or NULL on error
 */
struct adb_object_set *adb_table_set_new(struct adb_db *db, int table_id);

/**
 * \brief Apply constraint limits to a specific object dataset
 * \ingroup dataset
 * \param set The targeted dataset to constrain
 * \param ra Right Ascension coordinate representing the region center
 * \param dec Declination coordinate representing the region center
 * \param fov Field of View defining the radius around the coordinates
 * \param min_Z Minimum Z or magnitude limit for filtering
 * \param max_Z Maximum Z or magnitude limit for filtering
 * \return 0 on success, or an error code
 */
int adb_table_set_constraints(struct adb_object_set *set, double ra, double dec,
							  double fov, double min_Z, double max_Z);

/**
 * \brief Frees an active dataset and any associated memory
 * \ingroup dataset
 * \param set The target dataset to wipe and free
 */
void adb_table_set_free(struct adb_object_set *set);

/******************* Table Object Get *****************************************/

/**
 * \brief Quick macro to get the Right Ascension of an object pointer
 * \ingroup dataset
 */
#define adb_object_ra(object) object->ra

/**
 * \brief Quick macro to get the Declination of an object pointer
 * \ingroup dataset
 */
#define adb_object_dec(object) object->dec

/**
 * \brief Quick macro to get the magnitude of an object pointer
 * \ingroup dataset
 */
#define adb_object_mag(object) object->mag

/**
 * \brief Quick macro to get the size of an object pointer
 * \ingroup dataset
 */
#define adb_object_size(object) object->size

/**
 * \brief Universal table field name representing object designations
 * \ingroup dataset
 */
#define ADB_FIELD_DESIGNATION "_DESIGNATION"

/*! \struct adb_object_head
 * \brief A generic array container wrapper holding retrieved sequential objects
 * \ingroup dataset
 */
struct adb_object_head {
	const void *objects; /*!< array pointer containing matching objects */
	unsigned int count; /*!< count indicating the size of the objects array */
};

/**
 * \brief Evaluate and populate the underlying objects in a constrained dataset
 * \ingroup dataset
 * \param set The target dataset representation
 * \return 0 on success, or an error code
 */
int adb_set_get_objects(struct adb_object_set *set);

/**
 * \brief Use a target dataset to look up and cache properties of a hash key string
 * \ingroup dataset
 * \param set The target dataset context
 * \param key The character string representing the hash query key
 * \return 0 on success, or an error code
 */
int adb_set_hash_key(struct adb_object_set *set, const char *key);

/**
 * \brief Retrieve a specific object from a set using an identifier matching a field
 * \ingroup dataset
 * \param set The target dataset collection
 * \param id Pointer to the reference value for evaluation
 * \param field String name of the field to execute the query against
 * \param object Double pointer populated with queried object reference on success
 * \return 0 on success, or an error code
 */
int adb_set_get_object(struct adb_object_set *set, const void *id,
					   const char *field, const struct adb_object **object);

/**
 * \brief Fetch an object directly from an unconstrained database table using an ID
 * \ingroup dataset
 * \param db Pointer to the overall database context
 * \param table_id Identifier for the root table to query from
 * \param id Pointer to the reference value for evaluation
 * \param field String name of the field to execute the query against
 * \param object Double pointer populated with queried object reference on success
 * \return 0 on success, or an error code
 */
int adb_table_get_object(struct adb_db *db, int table_id, const void *id,
						 const char *field, const struct adb_object **object);

/**
 * \brief Find the nearest spatial neighbor corresponding to a target object within a subset
 * \ingroup dataset
 * \param set Constrained initialized dataset of surrounding objects
 * \param object The starting query object to originate spatial search from
 * \return Pointer to the nearest neighboring object, or NULL if empty
 */
const struct adb_object *
adb_table_set_get_nearest_on_object(struct adb_object_set *set,
									const struct adb_object *object);

/**
 * \brief Find the nearest spatial neighbor around specific coordinates inside a subset
 * \ingroup dataset
 * \param set Constrained initialized dataset of surrounding objects
 * \param ra Right Ascension coordinate query (radians)
 * \param dec Declination coordinate query (radians)
 * \return Pointer to the nearest neighboring object, or NULL if empty
 */
const struct adb_object *
adb_table_set_get_nearest_on_pos(struct adb_object_set *set, double ra,
								 double dec);

/**
 * \brief Fetch the internal linear object head context for a loaded object set
 * \ingroup dataset
 * \param set The target internal dataset currently populated
 * \return The mapped head structure, or NULL if the dataset is unpopulated
 */
struct adb_object_head *adb_set_get_head(struct adb_object_set *set);

/**
 * \brief Obtain the integer element count tracked inside a loaded bounds set
 * \ingroup dataset
 * \param set The target active data pool bounds to check
 * \return Total internal matching records within range bounds
 */
int adb_set_get_count(struct adb_object_set *set);

#ifdef __cplusplus
};
#endif

#endif
