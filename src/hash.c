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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "hash.h"
#include "table.h"
#include "debug.h"
#include "private.h"
#include "readme.h"

/*! \fn static inline int calc_hash(char* data, int len, int mod)
* \brief Calculate hash based on string <data> and in range <mod>
*
* We don't hash ' ' or '-'.
*/
int hash_string(const char *data, int len, int mod)
{
	int val = 0;

	if (mod == 0) {
		//adb_error(db, "mod is 0\n");
		return 0;
	}

	while (*data != 0) {
		if (*data >= '0' && *data <= 'z')
			val = ((val << 5) ^ (val >> 27)) ^ *data;
		data++;
	}

	val = abs(val % mod);
	return val;
}

/*! \fn static inline int calc_hash(char* data, int len, int mod)
* \brief Calculate hash based on string <data> and in range <mod>
*
*/
int hash_int(int val, int mod)
{
	return abs(val % mod);
}

void hash_free_maps(struct adb_table *table)
{
	int i, j;

	for (i = 0; i < table->hash.num; i++) {

		for (j =0; j < table->object.count; j++) {
			if (table->hash.map[i].index[j])
				free(table->hash.map[i].index[j]);
		}

		free(table->hash.map[i].index);
	}
}

static int hash_insert_object(struct hash_map *hash_map,
		const struct adb_object *object, unsigned int index)
{
	int size, count;

	if (hash_map->index[index]) {

		/* hash exists, realloc and add to end*/
		count = hash_map->index[index]->count + 1;

		size = sizeof(struct hash_object *) +
			sizeof(struct adb_object *) * count;

		hash_map->index[index] = realloc(hash_map->index[index], size);
		if (!hash_map->index[index])
			return -ENOMEM;

		hash_map->index[index]->object[count - 1] = object;
		hash_map->index[index]->count++;
	} else {

		/* no hash,so create new one */
		hash_map->index[index] = calloc(1, sizeof(struct hash_object) +
			sizeof(struct adb_object *));
		if (!hash_map->index[index])
			return -ENOMEM;

		hash_map->index[index]->object[0] = object;
		hash_map->index[index]->count = 1;

	}

	return 0;
}

static void table_hash_string(struct adb_table *table, int map)
{
	const void *object = table->objects;
	const void *field;
	int i, index;

	/* hash object for this map */
	for (i = 0; i < table->object.count; i++) {
		field = object + table->hash.map[map].offset;

		index = hash_string(field, table->hash.map[map].size,
			table->object.count);

		hash_insert_object(&table->hash.map[map], object, index);
		object += table->object.bytes;
	}
}

static void table_hash_int(struct adb_table *table, int map)
{
	const void *object = table->objects;
	const void *field;
	int i, index;

	/* hash object for this map */
	for (i = 0; i < table->object.count; i++) {
		field = object + table->hash.map[map].offset;

		index = hash_int(*((int*)field), table->object.count);

		hash_insert_object(&table->hash.map[map], object, index);
		object += table->object.bytes;
	}
}

int hash_build_table(struct adb_table *table, int map)
{
	struct hash_object **hash_object;

	hash_object = calloc(sizeof(struct hash_object *), table->object.count);
	if (hash_object == NULL)
		return -ENOMEM;

	table->hash.map[map].index = hash_object;

	switch (table->hash.map[map].type) {
	case ADB_CTYPE_STRING:
		table_hash_string(table, map);
		break;
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_INT:
		table_hash_int(table, map);
		break;
	case ADB_CTYPE_DOUBLE_MPC:
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_FLOAT:
	case ADB_CTYPE_DOUBLE:
	case ADB_CTYPE_DEGREES:
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
		adb_error(table->db, "ctype %d not implemented\n",
			table->hash.map[map].type);
		break;
	}

	return 0;
}

static void set_hash_string(struct adb_object_set *set, int map)
{
	const struct adb_object_head *object_heads = set->object_heads;
	const void *object;
	const void *field;
	int i, j, index;

	for (i = 0; i < set->head_count; i++) {
		object = object_heads->objects;

		for (j = 0; j < object_heads->count; j++) {

			field = object + set->hash.map[map].offset;
			index = hash_string(field, set->hash.map[map].size, set->count);

			hash_insert_object(&set->hash.map[map], object, index);
			object += set->table->object.bytes;
		}
	}
}

static void set_hash_int(struct adb_object_set *set, int map)
{
	const struct adb_object_head *object_heads = set->object_heads;
	const void *object;
	const void *field;
	int i, j, index;

	for (i = 0; i < set->head_count; i++) {
		object = object_heads->objects;

		for (j = 0; j < object_heads->count; j++) {

			field = object + set->hash.map[map].offset;
			index = hash_int(*((int*)field), set->count);

			hash_insert_object(&set->hash.map[map], object, index);
			object += set->table->object.bytes;
		}
	}
}

int hash_build_set(struct adb_object_set *set, int map)
{
	struct hash_object **hash_object;
	struct adb_table *table = set->table;

	hash_object = calloc(sizeof(struct hash_object *), set->count);
	if (hash_object == NULL)
		return -ENOMEM;

	set->hash.map[map].index = hash_object;
	set->hash.map[map].offset = table->hash.map[map].offset;
	set->hash.map[map].type = table->hash.map[map].type;
	set->hash.map[map].size = table->hash.map[map].size;
	set->hash.map[map].key = table->hash.map[map].key;

	switch (set->hash.map[map].type) {
	case ADB_CTYPE_STRING:
		set_hash_string(set, map);
		break;
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_INT:
		set_hash_int(set, map);
		break;
	case ADB_CTYPE_DOUBLE_MPC:
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_FLOAT:
	case ADB_CTYPE_DOUBLE:
	case ADB_CTYPE_DEGREES:
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
		adb_error(set->db, "ctype %d not implemented\n",
			set->hash.map[map].type);
		break;
	}

	return 0;
}
