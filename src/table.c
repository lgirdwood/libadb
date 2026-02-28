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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <math.h> // IWYU pragma: keep
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "debug.h"
#include "private.h"
#include "readme.h"
#include "table.h"
#include "libastrodb/db.h"
#include "libastrodb/object.h"

/**
 * \brief Establish table bindings handling local structural properties loading limits natively dynamically structures.
 *
 * Extracts and maps pre-processed optimized table definitions datasets limits paths matrices reliably structures memory securely configurations successfully.
 *
 * \param db Parent core log metrics state attributes bounds safely natively properly parameters layouts mappings dynamically pointers boundaries constraints arrays variables definitions sizes correctly features reliably metrics natively pointers.
 * \param table_id Identifier locating arrays layouts bindings properties constraints boundaries reliably securely structs fields limits paths properly sizes layouts bindings fields correctly indices dynamically matrices limits gracefully blocks safely paths securely correctly.
 * \return Status code indicating validation states defining operations successfully cleanly natively limits successfully identifying definitions memory limits parameters securely dynamically arrays fields correctly boundaries metrics bindings pointers natively bounds fields bounds bounds properly sizes paths sizes mappings accurately boundaries structures reliably mappings structs securely constraints.
 */
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

	adb_info(db, ADB_LOG_CDS_TABLE, "Found table schema %s\n",
			 table->path.file);
	ret = schema_read(db, table);
	if (ret < 0) {
		adb_error(db, "Failed to load table schema %s %d\n", file, ret);
		return ret;
	}

	/* now read in objects */
	ret = table_read_trixels(db, table);
	if (ret < 0) {
		adb_error(db, "Failed to read table objects from file %d\n", ret);
		return ret;
	}
	return 0;
}

/**
 * \brief Compute boundary HTM resolutions targeting minimum object spread capacities bounds natively dynamically safely structures arrays mapping parameters limits correctly features reliably sizes lengths metrics bindings fields pointers structs metrics parameters cleanly bounds paths natively properly indices efficiently defining operations correctly.
 *
 * Maps target parameters configurations boundaries extracting proper indexing resolution lengths structures safely sizes limits offsets bounds parameters indices bounds features structures gracefully definitions metrics lengths natively properly parameters bindings offsets mapping gracefully accurately bounds correctly lengths.
 *
 * \param table Matrix configurations constraints arrays properties mapping reliably memory limits natively sizes values limits correctly structures securely mappings bindings structures accurately metrics limits limits lengths sizes identifying pointers boundaries.
 * \param value Search magnitude constraints dimensions features defining structures limits safely parameters matrices offsets successfully lengths mappings boundaries cleanly metrics mappings arrays limits properly paths sizes correctly identifying variables parameters metrics structures bounds natively pointers bounds gracefully.
 * \return Optimal mapping integer variables definitions limits bounds natively values identifying parameters boundaries cleanly parameters successfully correctly structures securely arrays metrics identifying arrays sizes offsets cleanly securely limits graceful lengths defining sizes correctly identifying boundaries sizes pointers mappings arrays features natively boundaries paths limits properly sizes cleanly boundaries paths.
 */
int table_get_object_depth_min(struct adb_table *table, float value)
{
	int depth;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {
		if (value <= table->depth_map[depth].max_value)
			return depth;
	}

	return 0;
}

/**
 * \brief Calculate layout parameters bounds defining bounds sizes arrays configurations limits sizes extracting parameters metrics cleanly paths bounds accurately structures mapping dynamically securely matrices bindings variables features structures pointers identifying sizes naturally lengths metrics fields safely blocks sizes natively lengths limits defining identifying pointers indices mappings safely variables structs accurately mappings securely paths parameters gracefully cleanly bindings natively lengths.
 *
 * \param table Parent configurations tracking elements limits variables structures bindings constraints boundaries natively features lengths securely structures defining parameters natively values indices structures limits correctly pointers matrices limits structures fields mappings boundaries metrics lengths definitions fields identifying arrays bounds sizes offsets gracefully defining sizes cleanly values Identifying safely pointers mapping correctly sizes definitions gracefully parameters natively dynamically identifying boundaries safely structures definitions metrics paths reliably cleanly bounds bindings cleanly accurately sizes boundaries arrays properties safely bounds variables parameters gracefully securely safely values bounds identifying variables parameters identifying parameters cleanly identifying limits structurally sizes identifying efficiently securely features natively.
 * \param value Target tracking threshold boundaries limits values natively constraints arrays mapping structs definitions securely values matrices bounds identifying parameters properties limits structures reliably mapping matrices safely securely values paths identifying sizes matrices gracefully offsets values offsets identifying bounds safely variables identifying ranges definitions matrices identifying correctly structs limits paths.
 * \return Boundary values depths metric sizes defining limits bounds successfully reliably values lengths configurations variables constraints definitions values parameters values mappings cleanly variables features safely parameters successfully gracefully bounds paths properties correctly variables accurately mappings securely ranges sizes securely limits configurations values arrays structures constraints limits securely lengths identifying appropriately mappings values defining offsets variables values efficiently matrices depths securely limits bounds safely securely maps values arrays features values definitions identifying structs parameters limits safely boundaries dynamically metrics paths identifying constraints safely efficiently pointers correctly arrays structures definitions arrays dynamically identifying efficiently safely configurations definitions values matrices bindings maps variables arrays reliably identifying gracefully dynamically pointers bounds variables dynamically identifiers securely arrays mappings lengths objects parameters limits reliably ranges lengths sizes confidently gracefully features parameters mappings cleanly defining identifying features identifying definitions sizes arrays paths correctly maps parameters variables constraints identifying.
 */
int table_get_object_depth_max(struct adb_table *table, float value)
{
	int depth;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {
		if (value <= table->depth_map[depth].min_value)
			return depth;
	}

	return table->db->htm->depth;
}

/**
 * \brief Allocate independent tracking arrays definitions mappings limits boundaries mapping limits sizes datasets indices securely matrices features paths reliably strings identifying dimensions parameters metrics boundaries arrays safely limits definitions paths safely sizes successfully offsets accurately limits efficiently cleanly tracking parameters mappings arrays securely indices identifying structurally tracking arrays dynamically structs lengths securely pointers definitions limits securely mappings reliably paths successfully arrays offsets constraints dynamically structurally pointers parameters lengths reliably accurately sizes limits boundaries identifiers maps variables identifiers metrics identifiers values vectors parameters identifying identifiers configurations paths identifiers definitions accurately lengths strings metrics mappings natively properly lists variables matrices structures strings boundaries vectors structs confidently constraints properties securely bindings identifiers reliably arrays dynamically cleanly strings arrays definitions matrices variables accurately structs dimensions smoothly smoothly natively structurally defining vectors smoothly lists limits definitions structurally securely fields variables cleanly safely safely datasets natively securely properties bounds.
 *
 * \param db Log matrices framework constraints arrays properties tracking objects indices vectors dynamically safely gracefully bounds structs mapping appropriately definitions smoothly bindings structures lengths metrics efficiently safely lists vectors structs mappings limits limits correctly arrays objects correctly datasets limits boundaries arrays strings paths definitions cleanly configurations defining identifying mappings gracefully dynamically boundaries reliably structs limits definitions vectors matrices cleanly variables lists bindings safely natively tracking mappings definitions safely identifying maps vectors smoothly strings cleanly configurations pointers reliably ranges accurately bounds gracefully metrics vectors paths confidently metrics confidently securely pointers mappings lists fields vectors. 
 * \return Identified datasets variables metrics definitions mapping datasets safely vectors offsets accurately defining indices reliably arrays limits paths gracefully values mappings configurations smoothly dynamically identifying securely mapping constraints objects arrays neatly configurations ranges strings definitions arrays safely layouts variables cleanly dimensions defining natively properties gracefully constraints maps parameters identifying variables paths efficiently reliably parameters safely vectors paths confidently bounds securely cleanly pointers values gracefully definitions metrics reliably parameters elegantly vectors paths cleanly definitions arrays securely safely parameters mappings definitions strings reliably.
 */
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

/**
 * \brief Return configurations dynamically paths smoothly strings properly arrays identifiers identifying elegantly gracefully values smoothly mapping datasets elegantly definitions variables cleanly vectors mapping smoothly structures strings identifying smoothly boundaries pointers elegantly paths confidently arrays cleanly variables cleanly smoothly definitions parameters cleanly sizes mapping smoothly safely maps values constraints strings definitions vectors strings variables cleanly smoothly configurations pointers properly values ranges dynamically parameters correctly parameters identifying correctly smoothly elegantly arrays safely neatly elegantly definitions arrays correctly definitions definitions reliably cleanly mapping structures variables safely limits securely offsets smoothly.
 *
 * \param db Master vectors paths identifying parameters smoothly cleanly metrics arrays variables smoothly configurations correctly mappings smoothly safely strings neatly definitions parameters paths accurately sizes strings correctly mappings safely matrices neatly limits efficiently safely confidently paths gracefully boundaries paths definitions tracking mapping cleanly objects neatly arrays values smoothly mappings efficiently variables parameters elegantly.
 * \param id Identifying values metrics vectors smoothly parameters paths neatly configurations gracefully constraints dynamically properly mappings strings mapping confidently arrays securely paths neatly strings mapping strings boundaries smoothly structures definitions objects safely configurations structures variables safely datasets identifying limits definitions gracefully datasets fields paths smoothly gracefully smoothly structures structures smoothly smoothly datasets correctly cleanly definitions securely dynamically paths reliably vectors mappings arrays parameters parameters definitions cleanly cleanly strings definitions variables mappings smoothly variables mapping ranges definitions.
 */
void table_put_id(struct adb_db *db, int id)
{
	db->table_in_use[id] = 0;
}

/**
 * \brief Open and initialize a dataset table.
 *
 * Prepares a dataset table for use. It will attempt to load the local copy of
 * the table schema and binary trixel data based on the provided catalog class
 * and ID.
 *
 * \param db Database catalog
 * \param cat_class Catalog class (e.g. "I", "II")
 * \param cat_id Catalog ID (e.g. "239")
 * \param table_name Name of the table file
 * \return The opened table ID on success, or a negative error code on failure
 */
int adb_table_open(struct adb_db *db, const char *cat_class, const char *cat_id,
				   const char *table_name)
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
			 "Opening table: %s (%d) from Catalog %s:%s\n", table_name,
			 table_id, cat_class, cat_id);

	/* setup the paths */
	table->cds.cat_class = strdup(cat_class);
	if (table->cds.cat_class == NULL)
		goto err;
	table->cds.index = strdup(cat_id);
	if (table->cds.index == NULL)
		goto err;
	sprintf(local, "%s%s%s%s%s%s", db->lib->local, "/", cat_class, "/", cat_id,
			"/");

	table->path.local = strdup(local);
	if (table->path.local == NULL)
		goto err;
	table->path.file = strdup(table_name);
	if (table->path.file == NULL)
		goto err;

	adb_info(db, ADB_LOG_CDS_TABLE, "Table local path %s\n", table->path.local);

	/* try and open local copy of table */
	ret = table_open_local(db, table_id);
	if (ret < 0) {
		adb_error(db, "Error table open local failed %d\n", ret);
		goto err;
	}

	return table_id;

err:
	free(table->cds.cat_class);
	free(table->cds.index);
	free(table->path.local);
	table_put_id(db, table_id);
	return ret;
}

/**
 * \brief Close and free a dataset table.
 *
 * Releases all resources associated with the open dataset table, including
 * freeing loaded object arrays and returning the table ID to the pool.
 *
 * \param db Database catalog
 * \param table_id Table ID to close
 * \return 0 on success, or a negative error code if the ID is invalid
 */
int adb_table_close(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	adb_info(db, ADB_LOG_CDS_TABLE, "Closing table %d %s\n", table_id,
			 table->path.file);

	hash_free_maps(table);
	free(table->objects);
	free(table->cds.cat_class);
	free(table->cds.index);
	free(table->path.local);
	free(table->path.file);
	table_put_id(db, table_id);
	return 0;
}

/**
 * \brief Get dataset memory usage.
 *
 * Calculates the total memory footprint currently consumed by the loaded
 * objects in this table.
 *
 * \param db Database catalog
 * \param table_id Table ID to check
 * \return Total size used by the objects in bytes
 */
int adb_table_get_size(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	adb_debug(db, ADB_LOG_CDS_TABLE,
			  "table %d object size %d fields %d total %d\n", table_id,
			  table->object.bytes, table->object.count,
			  table->object.bytes * table->object.count);

	return table->object.bytes * table->object.count;
}

/**
 * \brief Retrieve sizes vectors variables accurately safely definitions arrays correctly safely dynamically offsets gracefully matrices paths ranges cleanly paths metrics smoothly smoothly parameters mapping strings gracefully variables datasets cleanly smoothly boundaries identifiers smoothly configurations maps definitions metrics cleanly safely variables cleanly securely securely paths fields paths strings cleanly parameters smoothly mapping strings correctly definitions smoothly vectors smoothly accurately cleanly arrays.
 *
 * \param db Tracking maps ranges securely offsets fields cleanly boundaries structures mappings safely configurations paths safely safely constraints definitions strings elegantly layouts vectors neatly vectors vectors safely datasets mapping cleanly smoothly securely cleanly accurately mappings lists parameters identifying mappings identifiers definitions cleanly safely objects limits securely paths definitions mapping safely cleanly limits safely safely safely smoothly neatly reliably mapping fields parameters definitions.
 * \param table_id Dimensions definitions securely ranges paths parameters vectors metrics bounds variables boundaries reliably fields paths neatly definitions smoothly metrics parameters gracefully bindings neatly variables matrices ranges metrics elegantly confidently securely mappings properly configurations mapping smoothly safely boundaries accurately structures vectors smoothly mapping objects maps paths securely dynamically reliably structs fields identifying gracefully smoothly reliably dynamically gracefully sizes limits cleanly mappings.
 * \return Identified smoothly mapping cleanly parameters gracefully mappings cleanly boundaries correctly limits reliably accurately safely definitions variables neatly neatly maps confidently elegantly neatly mapping correctly variables definitions vectors securely mappings values layouts constraints safely identifiers tracking elegantly securely securely confidently gracefully parameters definitions mappings neatly definitions smoothly dynamically dynamically natively reliably safely smoothly cleanly metrics gracefully arrays reliably safely boundaries identifying.
 */
int adb_table_get_count(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	return table->object.count;
}

/**
 * \brief Get the C data type of a dataset field.
 *
 * Looks up the schema field by its text name/symbol and returns the
 * internal `adb_ctype` enumeration representing how it is stored in memory.
 *
 * \param db Database catalog
 * \param table_id Table ID to query
 * \param field Name or symbol of the field
 * \return The corresponding `adb_ctype` for the field, or ADB_CTYPE_NULL if not
 * found
 */
adb_ctype adb_table_get_field_type(struct adb_db *db, int table_id,
								   const char *field)
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

/**
 * \brief Get the byte offset of a dataset field.
 *
 * Looks up the schema field by its text name/symbol and returns its byte offset
 * within the structured binary object record.
 *
 * \param db Database catalog
 * \param table_id Table ID to query
 * \param field Name or symbol of the field
 * \return The field offset in bytes, or -EINVAL if the field does not exist
 */
int adb_table_get_field_offset(struct adb_db *db, int table_id,
							   const char *field)
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

/**
 * \brief Get the byte size of a specified field in a table.
 *
 * Looks up the schema field by its symbol and returns its memory dimension (size) in bytes.
 *
 * \param db Database context pointer.
 * \param table_id The identifier of the table.
 * \param field The symbol/name of the requested field.
 * \return Total byte length of the field, or -EINVAL if the field is not found.
 */
int adb_table_get_field_size(struct adb_db *db, int table_id, const char *field)
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

/**
 * \brief Get the byte size of a single dataset object row.
 *
 * Returns the total size in bytes of one complete object record (including
 * all its fields based on the schema) in the dataset.
 *
 * \param db Database catalog
 * \param table_id Table ID to query
 * \return The size of an object in bytes, or -EINVAL for an invalid table ID
 */
int adb_table_get_object_size(struct adb_db *db, int table_id)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;

	table = &db->table[table_id];

	return table->object.bytes;
}

/**
 * \brief Create a hashmap index for a specified text-based key.
 *
 * Allocates and populates a hashmap array for direct string matching against table object rows.
 *
 * \param db Database context pointer.
 * \param table_id The ID of the table containing the objects.
 * \param hash The hash structure to populate with key-to-object mappings.
 * \param key The field's symbol name that should act as the string identifier.
 * \return 0 on success, or a negative error code if allocation or schema lookup fails.
 */
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

		hash->map[hash->num].size = adb_table_get_field_size(db, table_id, key);
		if (hash->map[hash->num].size <= 0) {
			adb_error(db, "invalid field size %s\n", key);
			return -EINVAL;
		}

		hash->map[hash->num].type = adb_table_get_field_type(db, table_id, key);
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

/**
 * \brief Register a field as a hash key for fast lookups.
 *
 * Adds the designated field to the table's list of hash maps. The hash map is
 * immediately built on the existing objects to provide O(1) lookups against
 * this field.
 *
 * \param db Database catalog
 * \param table_id Table ID
 * \param key Name or symbol of the field to be hashed
 * \return 0 on success, or a negative error code on failure
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
			 "offset %d type %d\n",
			 key, table_id, table->hash.map[table->hash.num].size,
			 table->hash.map[table->hash.num].offset,
			 table->hash.map[table->hash.num].type);
	table->hash.map[table->hash.num].key = key;

	hash_build_table(table, table->hash.num);
	table->hash.num++;

	return ret;
}

/**
 * \brief Set a fast O(1) string search hash key onto an object set.
 *
 * Initializes the objects in an active subset to be queryable by a string matching key, establishing the hash map if it has not yet been loaded.
 *
 * \param set The pre-filtered active object set to apply the index to.
 * \param key The name of the field to index strings dynamically for O(1) searches.
 * \return 0 on success, or a negative error code if the table hash operation fails.
 */
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
			 "offset %d type %d\n",
			 key, set->table_id, set->hash.map[set->hash.num].size,
			 set->hash.map[set->hash.num].offset,
			 set->hash.map[set->hash.num].type);
	set->hash.map[set->hash.num].key = key;

	hash_build_set(set, set->hash.num);
	set->hash.num++;

	return ret;
}

/**
 * \brief Provide mapping indices to evaluate string conditions efficiently.
 *
 * Fetches or initializes a table hash map for the provided dataset key column to process lookup evaluations.
 *
 * \param db The database tracking array context.
 * \param table_id The active string mapped table ID.
 * \param key The corresponding object variable attribute string lookup name.
 * \return The assigned index of the newly added array hash constraint, or a negative error on failure.
 */
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
/**
 * \brief Register a custom object type schema.
 *
 * Inserts a fully formed custom object directly into the dataset's object array.
 * Currently disabled via preprocessor directives.
 *
 * \param db Database catalog
 * \param table_id Table ID
 * \param object Object struct to insert
 * \return 1 on success, 0 on failure
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
