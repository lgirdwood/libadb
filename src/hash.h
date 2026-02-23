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

#ifndef __ADB_HASH_H
#define __ADB_HASH_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <stdlib.h>
#include <math.h>

#include <libastrodb/db-import.h>
#include "private.h"
#include "htm.h"

#define ADB_MAX_HASH_MAPS		16	/* number of hash maps */

struct adb_table;

/*! \struct hash_object
 * \brief Object hash bucket.
 * \ingroup table
 *
 * Points to an array of objects that match a hash number. We can store > 1 object to cope with hash collisions.
 */
struct hash_object {
	int count; /*!< Number of objects in this block */
	const struct adb_object *object[]; /*!< Array of objects matching this hash */
};

/*! \struct hash_map
 * \brief Single hash map index.
 * \ingroup table
 *
 * Defines a hash index over a specific field.
 */
struct hash_map {
	struct hash_object **index;   /*!< Hash table pointing to objects */
	int offset;					/*!< Offset in object for hashed ID */
	adb_ctype type;				/*!< C type (string or int) */
	int size;						/*!< Size of the hashed field in bytes */
	const char *key;              /*!< String key (field name) */
};

/*! \struct struct table_hash
 * \brief Database Table Hash.
 * \ingroup table
 *
 * Describes import table config.
 */
struct table_hash {
	struct hash_map map[ADB_MAX_HASH_MAPS];
	int num;
};

/*!
 * \brief Hash a string value.
 *
 * Computes a hash index for a given string.
 *
 * \param data The string data to hash
 * \param len The length of the string
 * \param mod Modulo divisor (number of buckets)
 * \return The computed hash index
 */
int hash_string(const char *data, int len, int mod);

/*!
 * \brief Hash an integer value.
 *
 * Computes a hash index for a given integer.
 *
 * \param val The integer value to hash
 * \param mod Modulo divisor (number of buckets)
 * \return The computed hash index
 */
int hash_int(int val, int mod);

/*!
 * \brief Free hash maps in a table.
 *
 * Frees all dynamically allocated memory within a table's hash index array.
 *
 * \param table Database table pointer
 */
void hash_free_maps(struct adb_table *table);

/*!
 * \brief Build hash table maps.
 *
 * Populates a table's given hash map by hashing each object.
 *
 * \param table Database table to index
 * \param map Index of the map inside `table->hash` to populate
 * \return 0 on success, negative error code on failure
 */
int hash_build_table(struct adb_table *table, int map);

/*!
 * \brief Build hash map for an object set.
 *
 * Populates a subset's given hash map by hashing each object.
 *
 * \param set Target object set
 * \param map Index of the map to populate
 * \return 0 on success, negative error code on failure
 */
int hash_build_set(struct adb_object_set *set, int map);

#endif

#endif
