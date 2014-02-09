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
#include <libastrodb/table.h>
#include <libastrodb/adbstdio.h>
#include <libastrodb/private.h>
#include <libastrodb/readme.h>

static int table_open_local(struct astrodb_db *db, int table_id)
{
	struct astrodb_table *table = &db->table[table_id];
	struct stat stat_info;
	char file[ADB_PATH_SIZE];
	int ret;

	/* check for local binary schema  */
	sprintf(file, "%s%s%s", table->path.local, table->path.file, ".schema");

	ret = stat(file, &stat_info);
	if (ret == 0) {

		adb_info(db, ADB_LOG_CDS_TABLE, "Found table schema %s\n", table->path.file);
		ret = schema_read(db, table);
		if (ret < 0) {
			adb_error(db, "Failed to load table schema %s %d\n",
				file, ret);
			return ret;
		}

		/* now read in objects */
		ret = table_read_trixels(db, table, table_id);
		if (ret < 0) {
			adb_error(db, "Failed to read table objects from file %d\n",
				ret);
			return ret;
		}
		return 1;
	}

	adb_info(db, ADB_LOG_CDS_TABLE, "Did not find table schema %s\n", file);
	return 0;
}

int table_get_object_depth_min(struct astrodb_table *table, float value)
{
	int depth;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {
		if (value <= table->depth_map[depth].max_value)
			return depth;
	}

	return 0;
}

int table_get_object_depth_max(struct astrodb_table *table, float value)
{
	int depth;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {

		if (value <= table->depth_map[depth].min_value)
			return depth;
	}

	return table->db->htm->depth;
}

/*! \fn int astrodb_table_open(astrodb_table * table, astrodb_progress progress, int ra, int dec, int mag)
 * \param table dataset
 * \param progress Progress callback
 * \param ra RA stride
 * \param dec Dec stride
 * \param mag Magnitude stride
 *
 * Initialise a dataset. This will download and import the raw data if necessary.
 */
int astrodb_table_open(struct astrodb_db *db, const char *cat_class,
		const char *cat_id, const char *table_name)
{
	struct astrodb_table *table;
	int table_id, ret = -EINVAL;
	char local[ADB_PATH_SIZE];

	if (db->table_count == ADB_MAX_TABLES - 1)
		return -EINVAL;

	table_id = db->table_count;
	table = &db->table[table_id];
	table->db = db;

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

	db->table_count++;
	return 0;

err:
	free(table->cds.class);
	free(table->cds.index);
	free(table->path.local);
	return ret;
}

/*! \fn void astrodb_table_close(astrodb_table* table)
 * \param table dataset
 *
 * Free dataset resources
 */
int astrodb_table_close(struct astrodb_db *db, int table_id)
{
	struct astrodb_table *table;
//	int err;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;
	table = &db->table[table_id];

	adb_info(db, ADB_LOG_CDS_TABLE, "Closing table %d %s\n",
		table_id, table->path.file);

#if 0
	if (table->object.new) {
		err = schema_write(db, table);
		if (err < 0)
			adb_error(db, "failed to save table schema %s\n",
				table->cds.name);

		err = table_write_trixels(db, table, table_id);
		if (err < 0)
			adb_error(db, "failed to save table data %s\n",
				table->cds.name);
	}
#endif
	hash_free_maps(table);
	free(table->objects);
	return 0;
}


/*! \fn int astrodb_table_get_size(astrodb_table* table)
 * \param table dataset
 * \return size in bytes
 *
 * Get dataset memory usage.
 */
int astrodb_table_get_size(struct astrodb_db *db, int table_id)
{
	struct astrodb_table *table;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;
	table = &db->table[table_id];

	adb_debug(db, ADB_LOG_CDS_TABLE, 
		"table %d object size %d fields %d total %d\n",
		table_id, table->object.bytes, table->object.count,
		table->object.bytes * table->object.count);

	return table->object.bytes * table->object.count;
}

int astrodb_table_get_count(struct astrodb_db *db, int table_id)
{
	struct astrodb_table *table;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;
	table = &db->table[table_id];

	return table->object.count;
}

/*! \fn astrodb_ctype astrodb_table_get_field_type(astrodb_table* table, char* field)
 * \param table Dataset
 * \param field Dataset field name
 * \return C type
 *
 * Get the C type for a field within a dataset.
 */
astrodb_ctype astrodb_table_get_field_type(struct astrodb_db *db,
	int table_id, const char *field)
{
	struct astrodb_table *table;
	int i;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;

	table = &db->table[table_id];

	/* check for default fields */
	for (i = 0; i < table->object.field_count; i++) {
		if (!strcmp(table->import.field[i].symbol, field))
			return table->import.field[i].type;
	}

	adb_error(db, "can't find field %s in table %d\n", field, table_id);
	return ADB_CTYPE_NULL;
}

/*! \fn int astrodb_table_get_field_offset(astrodb_table* table, char* field)
 * \param table Dataset
 * \param field Dataset field name
 * \return Field offset in bytes
 *
 * Gets the offset in bytes for a field within a dataset.
 */
int astrodb_table_get_field_offset(struct astrodb_db *db,
	int table_id, const char *field)
{
	struct astrodb_table *table;
	int i;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;

	table = &db->table[table_id];

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

int astrodb_table_get_field_size(struct astrodb_db *db,
	int table_id, const char *field)
{
	struct astrodb_table *table;
	int i;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;

	table = &db->table[table_id];

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

/*! \fn int astrodb_table_get_object_size(astrodb_table* table);
 */
int astrodb_table_get_object_size(struct astrodb_db *db, int table_id)
{
	struct astrodb_table *table;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;

	table = &db->table[table_id];

	return table->object.bytes;
}

/*! \fn int astrodb_table_hash_key(astrodb_table* table, char* field)
 * \param table dataset
 * \param field Field to be hashed.
 * \return 0 on success
 *
 * Add a field to be hashed for fast lookups.
 */
int astrodb_table_hash_key(struct astrodb_db *db, int table_id, const char *key)
{
	struct astrodb_table *table;

	if (table_id < 0 || table_id > db->table_count)
		return -EINVAL;

	table = &db->table[table_id];

	if (table->hash.num == ADB_MAX_HASH_MAPS) {
		adb_error(db, "too many hashed keys %s\n", key);
		return -EINVAL;
	}

	table->hash.map[table->hash.num].offset =
		astrodb_table_get_field_offset(db, table_id, key);
	if (table->hash.map[table->hash.num].offset < 0) {
		adb_error(db, "invalid field offset %s\n", key);
		return -EINVAL;
	}

	table->hash.map[table->hash.num].size =
		astrodb_table_get_field_size(db, table_id, key);
	if (table->hash.map[table->hash.num].size <= 0) {
		adb_error(db, "invalid field size %s\n", key);
		return -EINVAL;
	}

	table->hash.map[table->hash.num].type =
		astrodb_table_get_field_type(db, table_id, key);
	switch (table->hash.map[table->hash.num].type) {
	case ADB_CTYPE_INT:
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_STRING:
		break;
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

	adb_info(db, ADB_LOG_CDS_TABLE, "added hash for key %s on table %d with length %d at "
		"offset %d type %d\n", key, table_id,
		table->hash.map[table->hash.num].size,
		table->hash.map[table->hash.num].offset,
		table->hash.map[table->hash.num].type);
	table->hash.map[table->hash.num].key = key;

	hash_build_table(table, table->hash.num);

	table->hash.num++;
	return 0;
}

int table_get_hashmap(struct astrodb_db *db, int table_id, const char *key)
{
	struct astrodb_table *table;
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
/*! \fn int astrodb_table_register_schema(astrodb_table* table, astrodb_schema_object* field, int idx_size);
 * \param table dataset
 * \param field Object field index
 * \param idx_size Number of fields in index
 * \return 0 on success
 *
 * Register a new custom object type
 */
int astrodb_table_insert_object(struct astrodb_db *db, int table_id,
		struct astrodb_object *object)
{
	struct astrodb_table *table;
	int insert;

	if (table_id < 0 || table_id > db->table_count)
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
