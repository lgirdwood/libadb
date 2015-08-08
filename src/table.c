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
 *  Copyright (C) 2008 - 2012 Liam Girdwood
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
#include "table.h"
#include "debug.h"
#include "private.h"
#include "readme.h"

static int table_open_local(struct adb_db *db, int table_id)
{
	struct adb_table *table = &db->table[table_id];
	struct stat stat_info;
	char file[ADB_PATH_SIZE];
	int ret;

	/* check for local binary schema  */
	sprintf(file, "%s%s%s", table->path.local, table->path.file, ".schema");
	ret = stat(file, &stat_info);
	if (ret < 0) {
		adb_info(db, ADB_LOG_CDS_TABLE, "Did not find table schema %s %d\n",
			file, -errno);
		return -errno;
	}

	adb_info(db, ADB_LOG_CDS_TABLE,
		"Found table schema %s\n", table->path.file);
	ret = schema_read(db, table);
	if (ret < 0) {
		adb_error(db, "Failed to load table schema %s %d\n",
			file, ret);
		return ret;
	}

	/* now read in objects */
	ret = table_read_trixels(db, table);
	if (ret < 0) {
		adb_error(db, "Failed to read table objects from file %d\n",
			ret);
		return ret;
	}
	return 0;
}

int table_get_object_depth_min(struct adb_table *table, float value)
{
	int depth;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {
		if (value <= table->depth_map[depth].max_value)
			return depth;
	}

	return 0;
}

int table_get_object_depth_max(struct adb_table *table, float value)
{
	int depth;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {

		if (value <= table->depth_map[depth].min_value)
			return depth;
	}

	return table->db->htm->depth;
}

int table_get_id(struct adb_db *db)
{
	int i;

	for (i = 0; i < ADB_MAX_TABLES; i++) {
		if (!db->table_in_use[i]) {
			db->table_in_use[i] = 1;
			return i;
		}
	}

	return -EINVAL;
}

void table_put_id(struct adb_db *db, int id)
{
	db->table_in_use[id] = 0;
}

/*! \fn int adb_table_open(adb_table * table, adb_progress progress, int ra, int dec, int mag)
 * \param table dataset
 * \param progress Progress callback
 * \param ra RA stride
 * \param dec Dec stride
 * \param mag Magnitude stride
 *
 * Initialise a dataset. This will download and import the raw data if necessary.
 */
int adb_table_open(struct adb_db *db, const char *cat_class,
		const char *cat_id, const char *table_name)
{
	struct adb_table *table;
	int table_id, ret = -EINVAL;
	char local[ADB_PATH_SIZE];

	table_id = table_get_id(db);
	if (table_id < 0)
		return table_id;

	table = &db->table[table_id];
	memset(table, 0, sizeof(*table));
	table->db = db;
	table->id = table_id;

	adb_info(db, ADB_LOG_CDS_TABLE,
		"Opening table: %s (%d) from Catalog %s:%s\n",
		table_name, table_id, cat_class, cat_id);

	/* setup the paths */
	table->cds.class = strdup(cat_class);
	if (table->cds.class == NULL)
		goto err;
	table->cds.index = strdup(cat_id);
	if (table->cds.index == NULL)
		goto err;
	sprintf(local, "%s%s%s%s%s%s", db->lib->local, "/",
		cat_class, "/", cat_id, "/");

	table->path.local = strdup(local);
	if (table->path.local == NULL)
		goto err;
	table->path.file = strdup(table_name);
	if (table->path.file == NULL)
		goto err;

	adb_info(db, ADB_LOG_CDS_TABLE, "Table local path %s\n",
		table->path.local);

	/* try and open local copy of table */
	ret = table_open_local(db, table_id);
	if (ret < 0) {
		adb_error(db, "Error table open local failed %d\n", ret);
		goto err;
	}

	return table_id;

err:
	free(table->cds.class);
	free(table->cds.index);
	free(table->path.local);
	table_put_id(db, table_id);
	return ret;
}

/*! \fn void adb_table_close(adb_table* table)
 * \param table dataset
 *
 * Free dataset resources
 */
int adb_table_close(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	adb_info(db, ADB_LOG_CDS_TABLE, "Closing table %d %s\n",
		table_id, table->path.file);

	hash_free_maps(table);
	free(table->objects);
	free(table->cds.class);
	free(table->cds.index);
	free(table->path.local);
	free(table->path.file);
	table_put_id(db, table_id);
	return 0;
}


/*! \fn int adb_table_get_size(adb_table* table)
 * \param table dataset
 * \return size in bytes
 *
 * Get dataset memory usage.
 */
int adb_table_get_size(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	adb_debug(db, ADB_LOG_CDS_TABLE,
		"table %d object size %d fields %d total %d\n",
		table_id, table->object.bytes, table->object.count,
		table->object.bytes * table->object.count);

	return table->object.bytes * table->object.count;
}

int adb_table_get_count(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	return table->object.count;
}

/*! \fn adb_ctype adb_table_get_field_type(adb_table* table, char* field)
 * \param table Dataset
 * \param field Dataset field name
 * \return C type
 *
 * Get the C type for a field within a dataset.
 */
adb_ctype adb_table_get_field_type(struct adb_db *db,
	int table_id, const char *field)
{
	struct adb_table *table;
	int i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;

	table = &db->table[table_id];

	/* check std field */
	if (!strcmp(field, ADB_FIELD_DESIGNATION))
		return ADB_CTYPE_STRING;

	/* check for default fields */
	for (i = 0; i < table->object.field_count; i++) {
		if (!strcmp(table->import.field[i].symbol, field))
			return table->import.field[i].type;
	}

	adb_error(db, "can't find field %s in table %d\n", field, table_id);
	return ADB_CTYPE_NULL;
}

/*! \fn int adb_table_get_field_offset(adb_table* table, char* field)
 * \param table Dataset
 * \param field Dataset field name
 * \return Field offset in bytes
 *
 * Gets the offset in bytes for a field within a dataset.
 */
int adb_table_get_field_offset(struct adb_db *db,
	int table_id, const char *field)
{
	struct adb_table *table;
	int i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;

	table = &db->table[table_id];

	/* check std field */
	if (!strcmp(field, ADB_FIELD_DESIGNATION))
		return 0;

	/* check custom fields */
	for (i = 0; i < table->object.field_count; i++) {
		if (!strcmp(table->import.field[i].symbol, field)) {
			return table->import.field[i].struct_offset;
		}
	}

	/* check key_field alt fields */
	for (i = 0; i < table->object.num_alt_fields; i++) {
		if (!strcmp(table->import.alt_field[i].key_field.symbol, field)) {
			return table->import.alt_field[i].key_field.struct_offset;
		}
	}

	adb_error(db, "failed to find field %s\n", field);
	return -EINVAL;
}

int adb_table_get_field_size(struct adb_db *db,
	int table_id, const char *field)
{
	struct adb_table *table;
	int i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;

	table = &db->table[table_id];

	/* check std field */
	if (!strcmp(field, ADB_FIELD_DESIGNATION))
		return ADB_OBJECT_NAME_SIZE;

	/* check custom fields */
	for (i = 0; i < table->object.field_count; i++) {
		if (!strcmp(table->import.field[i].symbol, field)) {
			return table->import.field[i].struct_bytes;
		}
	}

	/* check key_field alt fields */
	for (i = 0; i < table->object.num_alt_fields; i++) {
		if (!strcmp(table->import.alt_field[i].key_field.symbol, field)) {
			return table->import.alt_field[i].key_field.struct_bytes;
		}
	}

	adb_error(db, "failed to find field %s\n", field);
	return -EINVAL;
}

/*! \fn int adb_table_get_object_size(adb_table* table);
 */
int adb_table_get_object_size(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;

	table = &db->table[table_id];

	return table->object.bytes;
}


static int add_hash_key(struct adb_db *db, int table_id,
	struct table_hash *hash, const char *key)
{

	if (!strcmp(key, ADB_FIELD_DESIGNATION)) {

		hash->map[hash->num].offset = 0;
		hash->map[hash->num].size = ADB_OBJECT_NAME_SIZE;
		hash->map[hash->num].type = ADB_CTYPE_STRING;

	} else {

		hash->map[hash->num].offset =
			adb_table_get_field_offset(db, table_id, key);
		if (hash->map[hash->num].offset < 0) {
			adb_error(db, "invalid field offset %s\n", key);
			return -EINVAL;
		}

		hash->map[hash->num].size =
			adb_table_get_field_size(db, table_id, key);
		if (hash->map[hash->num].size <= 0) {
			adb_error(db, "invalid field size %s\n", key);
			return -EINVAL;
		}

		hash->map[hash->num].type =
			adb_table_get_field_type(db, table_id, key);
		switch (hash->map[hash->num].type) {
		case ADB_CTYPE_INT:
		case ADB_CTYPE_SHORT:
		case ADB_CTYPE_STRING:
			break;
		case ADB_CTYPE_DEGREES:
		case ADB_CTYPE_DOUBLE:
		case ADB_CTYPE_FLOAT:
		case ADB_CTYPE_SIGN:
		case ADB_CTYPE_DOUBLE_HMS_HRS:
		case ADB_CTYPE_DOUBLE_HMS_MINS:
		case ADB_CTYPE_DOUBLE_HMS_SECS:
		case ADB_CTYPE_DOUBLE_DMS_DEGS:
		case ADB_CTYPE_DOUBLE_DMS_MINS:
		case ADB_CTYPE_DOUBLE_DMS_SECS:
		case ADB_CTYPE_DOUBLE_MPC:
		case ADB_CTYPE_NULL:
			adb_error(db, "field %s type not supported for hash\n", key);
			return -EINVAL;
		}
	}

	return 0;
}

/*! \fn int adb_table_hash_key(adb_table* table, char* field)
 * \param table dataset
 * \param field Field to be hashed.
 * \return 0 on success
 *
 * Add a field to be hashed for fast lookups.
 */
int adb_table_hash_key(struct adb_db *db, int table_id, const char *key)
{
	struct adb_table *table;
	int ret;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;

	table = &db->table[table_id];

	if (table->hash.num == ADB_MAX_HASH_MAPS) {
		adb_error(db, "too many hashed keys %s\n", key);
		return -EINVAL;
	}

	ret = add_hash_key(db, table_id, &table->hash, key);
	if (ret < 0)
		return ret;

	adb_info(db, ADB_LOG_CDS_TABLE,
		"added hash for key %s on table %d with length %d at "
		"offset %d type %d\n", key, table_id,
		table->hash.map[table->hash.num].size,
		table->hash.map[table->hash.num].offset,
		table->hash.map[table->hash.num].type);
	table->hash.map[table->hash.num].key = key;

	hash_build_table(table, table->hash.num);
	table->hash.num++;

	return ret;
}

int adb_set_hash_key(struct adb_object_set *set, const char *key)
{
	int ret;

	if (set->hash.num == ADB_MAX_HASH_MAPS) {
		adb_error(set->db, "too many hashed keys %s\n", key);
		return -EINVAL;
	}

	ret = add_hash_key(set->db, set->table_id, &set->hash, key);
	if (ret < 0)
		return ret;

	adb_info(set->db, ADB_LOG_CDS_TABLE,
		"added hash for key %s on table %d with length %d at "
		"offset %d type %d\n", key, set->table_id,
		set->hash.map[set->hash.num].size,
		set->hash.map[set->hash.num].offset,
		set->hash.map[set->hash.num].type);
	set->hash.map[set->hash.num].key = key;

	hash_build_set(set, set->hash.num);
	set->hash.num++;

	return ret;
}

int table_get_hashmap(struct adb_db *db, int table_id, const char *key)
{
	struct adb_table *table;
	int i;

	table = &db->table[table_id];

	for (i = 0; i < table->hash.num; i++) {
		if (!strcmp(key, table->hash.map[i].key))
			return i;
	}
	adb_error(db, "failed to get hash map index for %s\n", key);
	return -EINVAL;
}


#if 0
/*! \fn int adb_table_register_schema(adb_table* table, adb_schema_object* field, int idx_size);
 * \param table dataset
 * \param field Object field index
 * \param idx_size Number of fields in index
 * \return 0 on success
 *
 * Register a new custom object type
 */
int adb_table_insert_object(struct adb_db *db, int table_id,
		struct adb_object *object)
{
	struct adb_table *table;
	int insert;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return 0;

	table = &db->table[table_id];
	object->next = NULL;

	insert = table_insert_object(db, table_id, object);
	if (insert >= 0) {
		table->object.count ++;
		table->object.new = 1;
		return 1;
	} else {
		adb_error(db, "failed to insert object into table %s\n",
			table->cds.name);
		return 0;
	}
}
#endif
