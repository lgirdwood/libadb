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

/* points to array of objects that match hash number */
struct hash_object {
	int count; /* we can store > 1 object to cope with hash collisions */
	const struct adb_object *object[];
};

struct hash_map {
	struct hash_object **index;   /*!< Hash table pointing to objects */
	int offset;					/*!< offset in object for hashed ID */
	adb_ctype type;				/*!< C type  string or int */
	int size;						/*!< size in bytes */
	const char *key;
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

int hash_string(const char *data, int len, int mod);
int hash_int(int val, int mod);
void hash_free_maps(struct adb_table *table);
int hash_build_table(struct adb_table *table, int map);
int hash_build_set(struct adb_object_set *set, int map);

#endif

#endif
