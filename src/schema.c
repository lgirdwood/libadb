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

#include <errno.h> // IWYU pragma: keep
#include <string.h>

#include "debug.h"
#include "readme.h"
#include "table.h"
#include "libastrodb/db.h"

/**
 * \brief Verify if a specific field symbol exists in the table.
 *
 * Checks both primary mapped fields and alternative tracking matrices ensuring
 * the requested column string aligns with established catalog schema bounds.
 *
 * \param table Active instantiated dataset schema structure.
 * \param field String name indexing the extracted definition constraint mapping.
 * \return 1 if field exists, 0 otherwise.
 */
static int schema_is_field(struct adb_table *table, char *field)
{
	int i;

	for (i = 0; i < table->object.field_count; i++) {
		if (!strcmp(table->import.field[i].symbol, field))
			return 1;
	}

	for (i = 0; i < table->object.num_alt_fields; i++) {
		if (!strcmp(table->import.alt_field[i].key_field.symbol, field))
			return 1;
	}

	return 0;
}

/**
 * \brief Find primary mapping index evaluating schema symbols matrices.
 *
 * Scans primary sequential arrays extracting array relative bounds constraints offsets.
 *
 * \param db Catalog matrix pointer checking validation mapping logs.
 * \param table Working dataset mapping pointer.
 * \param field Requested identification character string symbol mapping fields.
 * \return Discovered integer indexing identifying active arrays configurations, or -EINVAL on fail.
 */
int schema_get_field(struct adb_db *db, struct adb_table *table,
					 const char *field)
{
	int i;

	for (i = 0; i < table->object.field_count; i++) {
		if (!strcmp(field, table->import.field[i].symbol))
			return i;
	}

	adb_warn(db, ADB_LOG_CDS_SCHEMA, "field %s does not exist\n", field);
	return -EINVAL;
}

/**
 * \brief Extract alternative mapping column structural lookup indexes.
 *
 * Query resolving secondary alias structural columns mapping underlying byte arrays blocks.
 *
 * \param db Catalog database log constraints reference.
 * \param table Operational dataset table pointers instance tracking schemas.
 * \param field Lookup identification definitions symbol.
 * \return Index number handling secondary associations elements mapping attributes constraints (-EINVAL on fail).
 */
int schema_get_alt_field(struct adb_db *db, struct adb_table *table,
						 const char *field)
{
	int i;

	for (i = 0; i < table->object.num_alt_fields; i++) {
		if (!strcmp(field, table->import.alt_field[i].key_field.symbol))
			return i;
	}

	adb_warn(db, ADB_LOG_CDS_SCHEMA, "field %s does not exist\n", field);
	return -EINVAL;
}

/**
 * \brief Transfer a primary structural definition to a secondary alternative link map.
 *
 * Extracts primary structural column bindings from main iteration indexes moving elements 
 * sequentially into custom optional attributes matrices preventing standard iterators dependencies.
 *
 * \param db Base layout framework handling log state mapping definitions.
 * \param table Executing bounds arrays storing loaded values layouts safely.
 * \param field Symbolic textual identification naming extracted data block paths.
 * \param pri_idx Initial location identifying primary structures data offsets blocks array positions.
 * \return Mapping boolean state verifying successful structural pointer offsets transfers.
 */
int schema_add_alternative_field(struct adb_db *db, struct adb_table *table,
								 const char *field, int pri_idx)
{
	struct cds_file_info *file_info = table->import.file_info;
	struct adb_schema_field *sec =
		&table->import.alt_field[table->object.num_alt_fields].alt_field;
	int desc;

	/* find field in table */
	for (desc = 0; desc < file_info->num_desc; desc++) {
		struct cds_byte_desc *byte_desc = &file_info->byte_desc[desc];

		if (!strncmp(field, byte_desc->label, strlen(byte_desc->label))) {
			sec->struct_offset = table->object.bytes;
			sec->text_offset = byte_desc->start;
			sec->text_size = byte_desc->end - byte_desc->start + 1;
			sec->type = table_get_column_ctype(byte_desc->type);
			sec->struct_bytes = table_get_column_csize(byte_desc->type);
			strncpy(sec->name, byte_desc->explanation, ADB_SCHEMA_NAME_SIZE);
			strncpy(sec->units, byte_desc->units, ADB_SCHEMA_UNITS_SIZE);
			strncpy(sec->symbol, byte_desc->label, ADB_SCHEMA_SYMBOL_SIZE);
			table->import.alt_field[table->object.num_alt_fields].import =
				table_get_alt_key_import(db, sec->type);

			/* move primary field index and insert */
			memcpy(
				&table->import.alt_field[table->object.num_alt_fields].key_field,
				&table->import.field[pri_idx], sizeof(struct adb_schema_field));

			adb_info(db, ADB_LOG_CDS_SCHEMA, "Object fields %d\n",
					 table->object.field_count);

			if (table->object.field_count > pri_idx) {
				memmove(&table->import.field[pri_idx],
						&table->import.field[pri_idx + 1],
						sizeof(struct adb_schema_field) *
							(table->object.field_count - pri_idx));
			}

			table->object.field_count--;
			table->object.num_alt_fields++;
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Emit schema execution metrics layout properties debugging limits matrices safely.
 *
 * Prints underlying field identification mappings detailing array limits lengths and data types contexts dynamically.
 *
 * \param db Parent core component state map definitions limits pointers.
 * \param table Source mapping components configuration blocks layouts.
 */
static void schema_dump(struct adb_db *db, struct adb_table *table)
{
	int i;

	adb_info(db, ADB_LOG_CDS_SCHEMA, "Table Schema:\n");
	adb_info(db, ADB_LOG_CDS_SCHEMA,
			 "Index\tSymbol\tOffset\tSize\tLine\tLSize\tType\tUnits\t\n");

	for (i = 0; i < table->object.field_count; i++) {
		adb_info(db, ADB_LOG_CDS_SCHEMA, "%d\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n", i,
				 table->import.field[i].symbol,
				 table->import.field[i].struct_offset,
				 table->import.field[i].struct_bytes,
				 table->import.field[i].text_offset,
				 table->import.field[i].text_size, table->import.field[i].type,
				 table->import.field[i].units);
	}

	adb_info(db, ADB_LOG_CDS_SCHEMA, "Alternative Column Sources:\n");

	for (i = 0; i < table->object.num_alt_fields; i++) {
		adb_info(db, ADB_LOG_CDS_SCHEMA, "Pri\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n",
				 table->import.alt_field[i].key_field.symbol,
				 table->import.alt_field[i].key_field.struct_offset,
				 table->import.alt_field[i].key_field.struct_bytes,
				 table->import.alt_field[i].key_field.text_offset,
				 table->import.alt_field[i].key_field.text_size,
				 table->import.alt_field[i].key_field.type,
				 table->import.alt_field[i].key_field.units);
		adb_info(db, ADB_LOG_CDS_SCHEMA, "Sec\t%s\t\t\t%d\t%d\t%d\t%s\n",
				 table->import.alt_field[i].alt_field.symbol,
				 table->import.alt_field[i].alt_field.text_offset,
				 table->import.alt_field[i].alt_field.text_size,
				 table->import.alt_field[i].alt_field.type,
				 table->import.alt_field[i].alt_field.units);
	}
}

/**
 * \brief Reallocate schema reading indexing ensuring optimal consecutive sequence execution reads.
 *
 * Reorders underlying evaluation structure iteration loops mitigating non-sequential seek patterns safely
 * preserving native source line execution properties sequentially.
 *
 * \param db Foundation matrix evaluating mappings paths definitions log contexts.
 * \param table Executing array constraints boundaries elements blocks properties offsets pointers matrices.
 * \return Status numeric verification verifying successful structural layout limits indices reordering layouts safely.
 */
int schema_order_import_index(struct adb_db *db, struct adb_table *table)
{
	int i, j, lowest_src, src = 0, group;
	struct adb_schema_field *idx, *idx_start;

	idx_start = idx =
		calloc(1, sizeof(struct adb_schema_field) * table->object.field_count);
	if (idx == NULL)
		return -ENOMEM;

	/* iterate through index, starting with lowest line pos first */
	for (j = 0; j < table->object.field_count; j++) {
		lowest_src = 1024;

		for (i = 0; i < table->object.field_count; i++) {
			if (lowest_src > table->import.field[i].text_offset &&
				table->import.field[i].struct_bytes) {
				lowest_src = table->import.field[i].text_offset;
				src = i;
			}
		}

		/* sort into group order highest group pos first */
		if (table->import.field[src].group_offset) {
			group = 0;

			for (i = 0; i < table->object.field_count; i++) {
				if (table->import.field[i].group_offset ==
						table->import.field[src].group_offset &&
					group < table->import.field[i].group_posn &&
					table->import.field[i].struct_bytes) {
					group = table->import.field[i].group_posn;
					src = i;
				}
			}
		}

		/* do copy */
		memcpy(idx, &table->import.field[src], sizeof(struct adb_schema_field));
		table->import.field[src].struct_bytes = 0;
		idx++;
	}

	/* copy back to table */
	memcpy(table->import.field, idx_start,
		   sizeof(struct adb_schema_field) * table->object.field_count);
	free(idx_start);

	schema_dump(db, table);
	return 0;
}

/**
 * \brief Interlink layout structures arrays based against standard byte definition maps layouts safely.
 *
 * Automatically computes explicit mappings metrics configurations using source CDS metadata definitions indices blocks natively.
 *
 * \param db State root context definitions bounds logging variables metrics pointers layout securely.
 * \param table Output framework limits target allocating matrices links structures arrays memory states appropriately mapping cleanly offsets.
 * \param new_schema_object Raw layout definition block injecting structural attributes identifying properties keys contexts natively reliably limiting bounds properties successfully defining features boundaries safely properly cleanly limits indices.
 * \return Native numerical status identifier tracking allocation successes limits boundaries successfully defining structure.
 */
static int schema_add_field_file(struct adb_db *db, struct adb_table *table,
								 struct adb_schema_field *new_schema_object)
{
	struct cds_file_info *file_info = table->import.file_info;
	struct adb_schema_field *didx;
	int desc;

	for (desc = 0; desc < file_info->num_desc; desc++) {
		struct cds_byte_desc *byte_desc = &file_info->byte_desc[desc];

		didx = &table->import.field[table->object.field_count];

		if (!strncmp(new_schema_object->symbol, byte_desc->label,
					 strlen(byte_desc->label)) &&
			!schema_is_field(table, byte_desc->label)) {
			memcpy(didx, new_schema_object, sizeof(struct adb_schema_field));
			didx->text_offset = byte_desc->start;
			didx->text_size = byte_desc->end - byte_desc->start + 1;

			if (didx->import == NULL)
				didx->import = table_get_column_import(db, didx->type);

			/* make sure we are not over writing str data */
			if (didx->type == ADB_CTYPE_STRING) {
				if (didx->struct_bytes < didx->text_size + 1) {
					adb_info(
						db, ADB_LOG_CDS_SCHEMA,
						"%s string too long at %d bytes resized to %d bytes\n",
						didx->symbol, didx->text_size, didx->struct_bytes - 1);
					didx->text_size = didx->struct_bytes - 1;
				}
			}

			table->object.field_count++;
			table->object.bytes += new_schema_object->struct_bytes;
			return 0;
		}
	}

	adb_error(db, "field %s does not exist\n", new_schema_object->name);
	return -EINVAL;
}

/**
 * \brief Setup synthetic explicit matrix column layouts bypassing metadata source files structures attributes bounds dynamically arrays correctly memory states properly securely safely natively limits features.
 *
 * Creates mapped offset structural definitions metrics arrays directly from parameter structures indices sizes securely natively mapping.
 *
 * \param db System root state bounds limits handling parameters execution mappings bounds definitions blocks metrics cleanly paths reliably correctly natively pointers.
 * \param table Active structure mapping indices dimensions parameters dynamically correctly boundaries limiting safely constraints features limits correctly pointers. 
 * \param new_schema_object Object reference pointer mapping sizes constraints defining properties layouts cleanly features dynamically structures mapping definitions securely natively attributes cleanly safely limits memory states safely limits pointers.
 * \return Native return configuration metrics pointer status limit safely bound structure matrices reliably safely limits cleanly.
 */
static int schema_add_field_nofile(struct adb_db *db, struct adb_table *table,
								   struct adb_schema_field *new_schema_object)
{
	struct adb_schema_field *didx;

	didx = &table->import.field[table->object.field_count];

	memcpy(didx, new_schema_object, sizeof(struct adb_schema_field));
	didx->text_offset = 0;
	didx->text_size = 0;

	if (didx->import == NULL)
		didx->import = table_get_column_import(db, didx->type);

	/* make sure we are not over writing str data */
	if (didx->type == ADB_CTYPE_STRING) {
		if (didx->struct_bytes < didx->text_size + 1) {
			adb_info(db, ADB_LOG_CDS_SCHEMA,
					 "%s string too long at %d bytes resized to %d bytes\n",
					 didx->symbol, didx->text_size, didx->struct_bytes - 1);
			didx->text_size = didx->struct_bytes - 1;
		}
	}

	table->object.field_count++;
	table->object.bytes += new_schema_object->struct_bytes;
	return 0;
}

/**
 * \brief Add a custom struct field to a dataset for import.
 *
 * Registers a new custom schema field definition to the specified dataset
 * table. Depending on whether the file information is available, it adds a
 * field using the file-based schema or a completely custom schema without file
 * backing.
 *
 * \param db Catalog database pointer
 * \param table Dataset to add the field to
 * \param new_schema_object Field specification to add
 * \return 0 on success, or a negative error code on failure
 */
int schema_add_field(struct adb_db *db, struct adb_table *table,
					 struct adb_schema_field *new_schema_object)
{
	struct cds_file_info *file_info = table->import.file_info;

	/* check whether we have custom schema or fike based schema */
	if (file_info)
		return schema_add_field_file(db, table, new_schema_object);
	else
		return schema_add_field_nofile(db, table, new_schema_object);
}

/**
 * \brief Serialize the compiled structural schema definition.
 *
 * Writes the metadata (such as field types, lengths, and byte offsets) to the persistent binary table schema layout file.
 *
 * \param db Pointer to the database context.
 * \param table The table reference containing the populated schema to serialize.
 * \return 0 on success, or a negative error code on failure.
 */
int schema_write(struct adb_db *db, struct adb_table *table)
{
	struct table_file_index *hdr = &table->file_index;
	FILE *f;
	size_t size;
	char file[128];
	int ret = 0, i;

	hdr->catalog_magic = ADB_IDX_VERSION;
	hdr->max_depth = table->max_depth;
	hdr->object_bytes = table->object.bytes;
	hdr->field_count = table->object.field_count + table->object.num_alt_fields;
	hdr->object_count = table->object.count;
	hdr->kd_root = table->import.kd_root;

	for (i = 0; i <= db->htm->depth; i++) {
		hdr->depth[i].min = table->depth_map[i].min_value;
		hdr->depth[i].max = table->depth_map[i].max_value;
	}

	sprintf(file, "%s%s%s", table->path.local, table->path.file, ".schema");
	adb_info(db, ADB_LOG_CDS_SCHEMA, "Writing schema to %s\n", file);

	if (table->object.field_count == 0) {
		adb_error(db, "schema %s has no fields\n", file);
		return -EIO;
	}

	if (table->object.count == 0) {
		adb_error(db, "schema %s has no objects\n", file);
		return -EIO;
	}

	/* open schema file for writing */
	f = fopen(file, "w+");
	if (f == NULL) {
		adb_error(db, "Error can't open schema file %s for writing\n", file);
		return -EIO;
	}

	/* write schema header */
	size = fwrite((char *)hdr, sizeof(struct table_file_index), 1, f);
	if (size == 0) {
		adb_error(db, "Error failed to write schema %s header\n", file);
		ret = -EIO;
		goto out;
	}

	/* write schema row descriptors */
	size = fwrite((char *)table->import.field, sizeof(struct adb_schema_field),
				  table->object.field_count, f);
	if (size == 0) {
		adb_error(db, "Error failed to write %d fields for schema %s\n",
				  table->object.field_count, file);
		ret = -EIO;
		goto out;
	}

	/* write alt fields */
	for (i = 0; i < table->object.num_alt_fields; i++) {
		size = fwrite((char *)&table->import.alt_field[i].key_field,
					  sizeof(struct adb_schema_field), 1, f);
		if (size == 0) {
			adb_error(db,
					  "Error failed to write alternate fields for schema %s\n",
					  file);
			ret = -EIO;
			goto out;
		}
	}
out:
	fclose(f);
	return ret;
}

/**
 * \brief Load a dataset header and schema from disk.
 *
 * Opens the `.schema` file associated with the table, reads its header and
 * field descriptors, and populates the table structure with the loaded
 * object count, schema fields, and depth map limits.
 *
 * \param db Database catalog to log against
 * \param table Table object whose schema should be loaded
 * \return 0 on success, or a negative error code on failure (-EIO or -EINVAL)
 */
int schema_read(struct adb_db *db, struct adb_table *table)
{
	struct table_file_index *hdr = &table->file_index;
	FILE *f;
	size_t size;
	char file[128];
	int ret = 0, i;

	sprintf(file, "%s%s%s", table->path.local, table->path.file, ".schema");
	adb_info(db, ADB_LOG_CDS_SCHEMA, "Reading schema from %s\n", file);

	/* open schema for reading */
	f = fopen(file, "r");
	if (f == NULL) {
		adb_error(db, "Error can't open schema file %s for reading\n", file);
		return -EIO;
	}

	/* read schema header */
	size = fread((char *)hdr, sizeof(struct table_file_index), 1, f);
	if (size == 0) {
		adb_error(db, "Error failed to read schema %s header\n", file);
		ret = -EIO;
		goto out;
	}

	if (hdr->catalog_magic != ADB_IDX_VERSION) {
		adb_error(db, "Error schema %s is version %d need %d\n", file,
				  hdr->catalog_magic, ADB_IDX_VERSION);
		ret = -EIO;
		goto out;
	}

	/* read schema row descriptors */
	size = fread((char *)table->import.field, sizeof(struct adb_schema_field),
				 hdr->field_count, f);
	if (size == 0) {
		adb_error(db, "Error failed to read schema %s rows %d\n", file);
		ret = -EIO;
		goto out;
	}

	table->object.bytes = hdr->object_bytes;
	table->object.field_count = hdr->field_count;
	table->object.count = hdr->object_count;
	table->kd_root = hdr->kd_root;

	for (i = 0; i <= db->htm->depth; i++) {
		table->depth_map[i].min_value = hdr->depth[i].min;
		table->depth_map[i].max_value = hdr->depth[i].max;
		adb_info(db, ADB_LOG_CDS_SCHEMA, "depth level %d min %f max %f\n", i,
				 table->depth_map[i].min_value, table->depth_map[i].max_value);
	}

	if (table->object.count == 0) {
		adb_error(db, "no objects in table %s\n", file);
		ret = -EINVAL;
	}

out:
	fclose(f);
	return ret;
}
