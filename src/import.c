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
 *  Copyright (C) 2010 - 2014 Liam Girdwood
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "table.h"
#include "schema.h"
#include "readme.h"
#include "private.h"
#include "debug.h"

/* table type import's */
static int int_import(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(int *) dest = strtol(src, &ptr, 10);

	if (src == ptr)
		return -1;
	return 0;
}

static int short_import(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(short *) dest = strtol(src, &ptr, 10);

	if (unlikely(src == ptr))
		return -1;
	return 0;
}

static int float_import(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(float *) dest = strtof(src, &ptr);

	if (unlikely(src == ptr)) {
		*(float*) dest = FP_NAN;
		return -1;
	}
	return 0;
}

static int double_import(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest = strtod(src, &ptr);

	if (unlikely(src == ptr)) {
		*(double*) dest = FP_NAN;
		return -1;
	}
	return 0;
}

static int double_degrees_import(struct adb_object *object, int offset,
	char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest = strtod(src, &ptr);

	if (unlikely(src == ptr)) {
		*(double*) dest = FP_NAN;
		return -1;
	}

	/* convert to radians */
	*(double *) dest *= D2R;

	return 0;
}

static int str_import(struct adb_object *object, int offset, char *src)
{
	char *dest = (char*)object + offset;

	/* copy string and terminating NULL */
	strcpy(dest, src);
	return 0;
}

static int double_dms_degs(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest = strtod(src, &ptr) * D2R;

	if (unlikely(src == ptr)) {
		*(double*) dest = FP_NAN;
		return -1;
	}
	return 0;
}

static int double_dms_mins(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest += (strtod(src, &ptr) / 60.0) * D2R;

	if (unlikely(src == ptr))
		return -1;
	return 0;
}

static int double_dms_secs(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest += (strtod(src, &ptr) / 3600.0) * D2R;

	if (unlikely(src == ptr))
		return -1;
	return 0;
}

static int sign_import(struct adb_object *object, int offset, char *src)
{
	char *dest = (char*)object + offset;

	if (*(char*)src == '-')
		*(double *) dest *= -1.0;
	return 0;
}

static int double_hms_hrs(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest = (strtod(src, &ptr) * 15.0) * D2R;

	if (unlikely(src == ptr))
		return -1;
	return 0;
}

static int double_hms_mins(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest += ((strtod(src, &ptr) / 60.0) * 15.0) * D2R;

	if (unlikely(src == ptr))
		return -1;
	return 0;
}

static int double_hms_secs(struct adb_object *object, int offset, char *src)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest += ((strtod(src, &ptr) / 3600.0) * 15.0) * D2R;

	if (unlikely(src == ptr))
		return -1;
	return 0;
}

static int float_alt_import(struct adb_object *object, int offset,
	char *src, char *src2)
{
	char *ptr, *dest = (char*)object + offset;

	*(float *) dest = strtof(src, &ptr);

	/* is primary source invalid then try alternate source */
	if (src == ptr) {
		*(float *) dest = strtof(src2, &ptr);
		if (unlikely(src2 == ptr)) {
			*(float*) dest = FP_NAN;
			return -1;
		}
	}
	return 0;
}

static int double_alt_import(struct adb_object *object, int offset,
	char *src, char *src2)
{
	char *ptr, *dest = (char*)object + offset;

	*(double *) dest = strtod(src, &ptr);

	/* is primary source invalid then try alternate source */
	if (src == ptr) {
		*(double *) dest = strtod(src2, &ptr);
		if (unlikely(src2 == ptr)) {
			*(double *) dest = FP_NAN;
			return -1;
		}
	}
	return 0;
}

/*! \fn adb_ctype table_get_ctype(char *type)
 * \brief Get C type from ASCII type
 */
adb_ctype table_get_column_ctype(char *type)
{
	if (*type == 'I')
		return ADB_CTYPE_INT;
	if (*type == 'A')
		return ADB_CTYPE_STRING;
	if (*type == 'F') {
		if (*(type + 1) > ADB_FLOAT_SIZE)
			return ADB_CTYPE_DOUBLE;
		else
			return ADB_CTYPE_FLOAT;
	}
	return ADB_CTYPE_NULL;
}

/*! \fn int table_get_csize(char *type)
 * \brief Get C size from ASCII size
 */
int table_get_column_csize(char *type)
{
	if (*type == 'I')
		return sizeof(int);
	if (*type == 'A')
		return strtol(type + 1, NULL, 10);
	if (*type == 'F') {
		if (*(type + 1) > ADB_FLOAT_SIZE)
			return sizeof(double);
		else
			return sizeof(float);
	}
	return 0;
}

/*! \fn void *table_get_key_import(adb_ctype type)
 * Get dataset type import
 */
adb_field_import1 table_get_column_import(struct adb_db *db,
	adb_ctype type)
{
	switch (type) {
	case ADB_CTYPE_DOUBLE:
		return double_import;
	case ADB_CTYPE_DEGREES:
			return double_degrees_import;
	case ADB_CTYPE_INT:
		return int_import;
	case ADB_CTYPE_SHORT:
		return short_import;
	case ADB_CTYPE_STRING:
		return str_import;
	case ADB_CTYPE_FLOAT:
		return float_import;
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
		return double_dms_degs;
	case ADB_CTYPE_DOUBLE_DMS_MINS:
		return double_dms_mins;
	case ADB_CTYPE_DOUBLE_DMS_SECS:
		return double_dms_secs;
	case ADB_CTYPE_SIGN:
		return sign_import;
	case ADB_CTYPE_DOUBLE_MPC:
	case ADB_CTYPE_NULL:
		return NULL;
	case ADB_CTYPE_DOUBLE_HMS_HRS:
		return double_hms_hrs;
	case ADB_CTYPE_DOUBLE_HMS_MINS:
		return double_hms_mins;
	case ADB_CTYPE_DOUBLE_HMS_SECS:
		return double_hms_secs;
	}
	adb_error(db, "Invalid column import key type %d\n", type);
	return NULL;
}

/*! \fn void *table_get_alt_key_import(adb_ctype type)
 * Get dataset type import
 */
adb_field_import2 table_get_alt_key_import(struct adb_db *db,
	adb_ctype type)
{
	switch (type) {
	case ADB_CTYPE_DOUBLE:
		return double_alt_import;
	case ADB_CTYPE_FLOAT:
		return float_alt_import;
	case ADB_CTYPE_INT:
	case ADB_CTYPE_DEGREES:
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_STRING:
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_DOUBLE_MPC:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
		adb_error(db, "Invalid alternate column import key type %d\n", type);
		return NULL;
	}
	return NULL;
}

int import_get_object_depth_min(struct adb_table *table, float value)
{
	int depth;

	if (value > table->object.max_value || value < table->object.min_value)
		return -EINVAL;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {
		if (value > table->depth_map[depth].min_value &&
			value < table->depth_map[depth].max_value)
			return depth;
	}

	/* out of range */
	adb_vdebug(table->db, ADB_LOG_CDS_TABLE, "out of range for %f\n", value);
	return -EINVAL;
}

int import_get_object_depth_max(struct adb_table *table, float value)
{
	int depth;

	if (value > table->object.max_value || value < table->object.min_value)
		return -EINVAL;

	for (depth = 0; depth <= table->db->htm->depth; depth++) {
		if (value > table->depth_map[depth].min_value &&
			value < table->depth_map[depth].max_value)
			return depth;
	}

	/* out of range */
	adb_vdebug(table->db, ADB_LOG_CDS_TABLE, "out of range for %f\n", value);
	return -EINVAL;
}

static void get_import_buffer_size(struct adb_db *db,
	struct adb_table *table)
{
	int i;

	table->import.text_buffer_bytes = 0;

	for (i = 0; i <table->object.field_count; i++) {
		if (table->import.field[i].text_size > table->import.text_buffer_bytes)
			table->import.text_buffer_bytes = table->import.field[i].text_size;
	}
	adb_info(db, ADB_LOG_CDS_IMPORT, "Using import buffer size %d\n",
		table->import.text_buffer_bytes);
}

static void get_histogram_keys(struct adb_db *db, struct adb_table *table)
{
	int key_idx, alt_key_idx;

	key_idx = schema_get_field(db, table, table->import.depth_field);
	if (key_idx >= 0) {
		table->import.histogram_key = &table->import.field[key_idx];
	}

	alt_key_idx = schema_get_alt_field(db, table, table->import.depth_field);
	if (alt_key_idx >= 0) {
		table->import.histogram_alt_key = &table->import.alt_field[alt_key_idx];
	}
}

static float hlimit[] = {0.8, 0.75, 0.66, 0.5};

/* get index where limit% of remaining objects exist */
static int get_percent_limit(struct adb_db *db, struct adb_table *table,
	float limit, float histo, int index, int *remaining)
{
	int required;
	int i, c = 0, r = *remaining;

	required = 0.8 * r;

	for (i = index; i >= 0; i--) {
		c += table->file_index.histo[i];
		if (c >= required) {
			*remaining = r - c + table->file_index.histo[i];
			return i;
		}
	}
	return 0;
}

static void histo_depth_calc(struct adb_db *db,
	struct adb_table *table, float histo)
{
	int i, j, start, old_start, remaining, limit = 0, total = 0;

	start = 0;
	old_start = ADB_TABLE_HISTOGRAM_DIVS - 1;
	remaining = table->object.count;

	/* sort objects by magnitude into htm depth levels */
	for (j = db->htm->depth; j >= 0; j--) {

		start = get_percent_limit(db, table, hlimit[limit], histo,
			old_start, &remaining);

		table->depth_map[j].min_value =
			table->object.min_value + start * histo;
		table->depth_map[j].max_value =
			table->object.min_value + old_start * histo;
		old_start = start;

		adb_info(db, ADB_LOG_CDS_IMPORT, "depth %d %3.3f <-> %3.3f\n", j,
			table->depth_map[j].min_value, table->depth_map[j].max_value);
	}

	for (i = 0; i < ADB_TABLE_HISTOGRAM_DIVS; i++) {
		adb_info(db, ADB_LOG_CDS_IMPORT, "%3.3f <-> %3.3f %d\n",
			table->object.min_value + i * histo,
			table->object.min_value + (i + 1) * histo,
			table->file_index.histo[i]);
			total += table->file_index.histo[i];
	}
	adb_info(db, ADB_LOG_CDS_IMPORT, "total histo %d\n", total);
}

#define histo_inc(db, count) \
	if (count % 10000 == 0) \
		adb_info(db, ADB_LOG_CDS_IMPORT, "\r Parsed %d", count);

static int table_histogram_import(struct adb_db *db,
	struct adb_table *table, FILE *f)
{
	struct adb_object object;
	struct adb_schema_field *key = table->import.histogram_key;
	int j, rsize, import, used = 0, hindex, oor = 0;
	char *line;
	char buf[ADB_IMPORT_LINE_SIZE];
	size_t size;
	float histo;

	line = malloc(ADB_IMPORT_LINE_SIZE);
	if (line == NULL)
		return -ENOMEM;

	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Creating histogram using field %s for depth\n", key->symbol);

	histo = (table->object.max_value - table->object.min_value) /
		(ADB_TABLE_HISTOGRAM_DIVS - 1);

	size = table->import.text_length + 10;

	for (j = 0; j < table->object.count || table->object.count == 0; j++) {
		bzero(line, ADB_IMPORT_LINE_SIZE);

		/* try and read a little extra padding */
		rsize = getline(&line, &size, f);
		if (rsize <= 0) {
			adb_error(db, "cant read line %d\n", j);
			goto out;
		}

		bzero(buf, table->import.text_buffer_bytes);
		strncpy(buf, line + key->text_offset, key->text_size);

		/* terminate string */
		if (key->type == ADB_CTYPE_STRING)
			buf[key->text_size] = 0;

		import = key->import(&object, key->struct_offset, buf);
		if (import < 0) {
			adb_vdebug(db, ADB_LOG_CDS_IMPORT, " blank field %s on buf: %s\n",
				key->symbol, buf);
			continue;
		}

		if (object.mag < table->object.min_value ||
			object.mag > table->object.max_value) {
			oor++;
			continue;
		}

		hindex = (object.mag - table->object.min_value) / histo;
		if (hindex >= ADB_TABLE_HISTOGRAM_DIVS || hindex < 0) {
			adb_error(db, "hash index out of range %d for %s have val %f\n",
				hindex, buf, object.mag);
			oor++;
			continue;
		}

		table->file_index.histo[hindex]++;
		used++;
		histo_inc(db, j);
	}

out:
	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Used %d objects for histogram %d out of range\n", used, oor);
	if (table->object.count == 0)
		table->object.count = used;
	histo_depth_calc(db, table, histo);
	fseek(f, 0, SEEK_SET);
	free(line);
	return 0;
}

static int table_histogram_alt_import(struct adb_db *db,
	struct adb_table *table, FILE *f)
{
	struct adb_object object;
	struct alt_field *key = table->import.histogram_alt_key;
	int j, rsize, import, used = 0, hindex, oor = 0, blank = 0;
	char *line;
	char buf[ADB_IMPORT_LINE_SIZE], buf2[ADB_IMPORT_LINE_SIZE];
	size_t size;
	float histo;

	line = malloc(ADB_IMPORT_LINE_SIZE);
	if (line == NULL)
		return -ENOMEM;

	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Creating histogram using field %s/%s for depth\n",
		key->key_field.symbol, key->alt_field.symbol);

	histo = (table->object.max_value - table->object.min_value) /
		(ADB_TABLE_HISTOGRAM_DIVS - 1);

	size = table->import.text_length + 10;

	for (j = 0; j < table->object.count || table->object.count == 0; j++) {
		bzero(line, ADB_IMPORT_LINE_SIZE);

		/* try and read a little extra padding */
		rsize = getline(&line, &size, f);
		if (rsize <= 0) {
			adb_error(db, "can't read line\n");
			goto out;
		}

		bzero(buf, table->import.text_buffer_bytes);
		bzero(buf2, table->import.text_buffer_bytes);
		strncpy(buf, line + key->key_field.text_offset,
			key->key_field.text_size);
		strncpy(buf2, line + key->alt_field.text_offset,
			key->alt_field.text_size);

		import = key->import(&object, key->key_field.struct_offset, buf, buf2);
		if (import < 0) {
			adb_vdebug(db, ADB_LOG_CDS_IMPORT, " blank field %s on buf: %s\n",
				key->key_field.symbol, buf);
			blank++;
			continue;
		}

		if (object.mag < table->object.min_value ||
			object.mag > table->object.max_value) {
			oor++;
			continue;
		}

		hindex = (object.mag - table->object.min_value) / histo;
		if (hindex >= ADB_TABLE_HISTOGRAM_DIVS || hindex < 0) {
			adb_error(db, "hash index out of range %d for %s (%s) have val %f\n",
				hindex, buf, buf2, object.mag);
			oor++;
			continue;
		}

		table->file_index.histo[hindex]++;
		used++;
		histo_inc(db, j);
	}

out:
	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Used %d objects for histogram %d out of range %d blank\n",
		used, oor, blank);
	if (table->object.count == 0)
		table->object.count = used;
	histo_depth_calc(db, table, histo);
	fseek(f, 0, SEEK_SET);
	free(line);
	return 0;
}

static int import_object_ascending(struct adb_db *db,
	struct adb_object *object, struct adb_table *table)
{
	struct htm_trixel *trixel;
	struct htm_vertex vertex;
	struct htm *htm = db->htm;
	int depth;

	depth = import_get_object_depth_min(table, adb_object_mag(object));
	if (depth < 0)
		return 0;

	vertex.ra = adb_object_ra(object);
	vertex.dec = adb_object_dec(object);
	trixel = htm_get_home_trixel(htm, &vertex, depth);
	if (!trixel) {
		adb_error(db, "no trixel at RA %f DEC %f\n",
			adb_object_ra(object) * R2D, adb_object_dec(object) * R2D);
		return -EINVAL;
	}

	htm_import_object_ascending(db, trixel, object, table);
	return 1;
}

static int import_object_descending(struct adb_db *db,
	struct adb_object *object, struct adb_table *table)
{
	struct htm_trixel *trixel;
	struct htm_vertex vertex;
	struct htm *htm = db->htm;
	int depth;

	depth = import_get_object_depth_max(table, adb_object_mag(object));
	if (depth < 0)
		return 0;

	vertex.ra = adb_object_ra(object);
	vertex.dec = adb_object_dec(object);
	trixel = htm_get_home_trixel(htm, &vertex, depth);
	if (!trixel) {
		adb_error(db, "no trixel at RA %f DEC %f\n",
			adb_object_ra(object) * R2D, adb_object_dec(object) * R2D);
		return -EINVAL;
	}

	htm_import_object_descending(db, trixel, object, table);
	return 1;
}

#define import_inc(db, count, pc, div) \
	count++; \
	if (count >= div) {\
		pc += 0.01; \
		count = 0; \
		adb_info(db, ADB_LOG_CDS_IMPORT, "\r Imported %3.1f percent", pc); \
	}

static int import_rows(struct adb_db *db, int table_id, FILE *f)
{
	struct adb_table *table;
	struct adb_object *object;
	int i, j, count = 0, short_records = 0;
	int import, warn = 0, line_count = 0, blank = 0;
	int div, pc_count = 0;
	char *line, buf[ADB_IMPORT_LINE_SIZE];
	float pc = 0.0;
	size_t size;
	ssize_t rsize;

	line = malloc(ADB_IMPORT_LINE_SIZE);
	if (line == NULL)
		return -ENOMEM;

	table = &db->table[table_id];
	if (table->object.bytes == 0) {
		adb_error(db, "error: object size is 0\n");
		return -EINVAL;
	}

	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Starting import with objects %d size %d bytes\n",
		table->object.count, table->object.bytes);

	size = table->import.text_length + 10;
	div = table->object.count / 10000;

	for (j = 0; j < table->object.count; j++) {

		object = calloc(1, table->object.bytes);
		if (object == NULL) {
			free(line);
			return -ENOMEM;
		}

		object->import.kd = calloc(1, sizeof(struct adb_kd_tree));
		if (object->import.kd == NULL) {
			free(object);
			free(line);
			return -ENOMEM;
		}

		bzero(line, ADB_IMPORT_LINE_SIZE);

		/* try and read a little extra padding */
		rsize = getline(&line, &size, f);
		if (rsize <= 0) {
			free(object);
			goto out;
		}
		if (rsize < table->import.text_length)
			short_records++;

		/* create row by importing column (field) data */
		for (i = 0; i < table->object.field_count; i++) {
			bzero(buf, table->import.text_buffer_bytes);
			strncpy(buf, line + table->import.field[i].text_offset,
				table->import.field[i].text_size);

			/* terminate string */
			if (table->import.field[i].type == ADB_CTYPE_STRING)
				buf[table->import.field[i].text_size] = 0;

			import = table->import.field[i].import(object,
					table->import.field[i].struct_offset, buf);
			if (import < 0) {
				adb_vdebug(db, ADB_LOG_CDS_IMPORT, " blank field %s\n",
					table->import.field[i].symbol);
				blank++;
			}
		}

		/* import row into table */
		import = table->object.import(db, object, table);
		if (import == 0)
			warn++;
		else if (import == 1)
			count++;
		else {
			adb_error(db, "failed to import object at line %d: %s\n", j, line);
			return -EINVAL;
		}

		if (warn) {
			adb_vdebug(db, ADB_LOG_CDS_IMPORT,
				"At line %d with length %d :-\n",
				line_count, strlen(line));
			adb_vdebug(db, ADB_LOG_CDS_IMPORT, "line %s\n\n", line);
		}
		line_count++;
		import_inc(db, pc_count, pc, div);
	}

out:
	adb_info(db, ADB_LOG_CDS_IMPORT, "Got %d short, %d blank records %d warnings\n",
		short_records, blank, warn);
	adb_info(db, ADB_LOG_CDS_IMPORT, "Imported %d records\n", count);
	table->object.count = count;
	free(line);
	return count;
}

static int import_rows_with_alternatives(struct adb_db *db,
		int table_id, FILE *f)
{
	struct adb_table *table;
	int i, j, k, count = 0, short_records = 0, warn, import, line_count = 0;
	int div, pc_count = 0;
	float pc = 0.0;
	char *line;
	char buf[ADB_IMPORT_LINE_SIZE], buf2[ADB_IMPORT_LINE_SIZE];
	struct adb_object *object;
	size_t size;
	ssize_t rsize;

	line = malloc(ADB_IMPORT_LINE_SIZE);
	if (line == NULL)
		return -ENOMEM;

	table = &db->table[table_id];
	if (table->object.bytes == 0) {
		adb_error(db, "error: object size is 0\n");
		return -EINVAL;
	}

	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Starting alt import with objects %d size %d bytes\n",
		table->object.count, table->object.bytes);
	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Importing %d alt fields\n", table->object.num_alt_fields);

	size = table->import.text_length + 10;
	div = table->object.count / 10000;

	for (j = 0; j < table->object.count; j++) {
		object = calloc(1, table->object.bytes);
		if (object == NULL) {
			free(line);
			return -ENOMEM;
		}

		object->import.kd = calloc(1, sizeof(struct adb_kd_tree));
		if (object->import.kd == NULL) {
			free(object);
			free(line);
			return -ENOMEM;
		}

		warn = 0;
		bzero(line, ADB_IMPORT_LINE_SIZE);

		/* try and read a little extra padding */
		rsize = getline(&line, &size, f);
		if (rsize < 0) {
			free(object);
			goto out;
		}
		if (rsize < table->import.text_length)
			short_records++;

		/* create row by importing field data */
		for (i = 0; i < table->object.field_count; i++) {
			bzero(buf, table->import.text_buffer_bytes);
			strncpy(buf, line + table->import.field[i].text_offset,
				table->import.field[i].text_size);

			/* terminate string */
			if (table->import.field[i].type == ADB_CTYPE_STRING)
				buf[table->import.field[i].text_size] = 0;

			import = table->import.field[i].import(
				object, table->import.field[i].struct_offset, buf);

			if (import < 0)
				adb_vdebug(db, ADB_LOG_CDS_IMPORT,
					" blank field %s\n",
					table->import.field[i].symbol);
		}

		/* complete row by importing alt field data */
		for (k = 0; k <table->object.num_alt_fields; k++) {
			bzero(buf, table->import.text_buffer_bytes);
			bzero(buf2, table->import.text_buffer_bytes);
			strncpy(buf, line + table->import.alt_field[k].key_field.text_offset,
				table->import.alt_field[k].key_field.text_size);
			strncpy(buf2, line + table->import.alt_field[k].alt_field.text_offset,
				table->import.alt_field[k].alt_field.text_size);

			import = table->import.alt_field[k].import(object,
						table->import.alt_field[k].key_field.struct_offset,
						buf, buf2);
			if (import < 0) {
				adb_vdebug(db, ADB_LOG_CDS_IMPORT, " blank fields %s %s\n",
					table->import.alt_field[k].key_field.symbol,
					table->import.alt_field[k].alt_field.symbol);
				warn = 1;
			}
		}

		/* import row into table */
		import = table->object.import(db, object, table);
		if (import == 0)
			warn = 1;
		else if (import == 1)
			count++;
		else {
			adb_error(db, "failed to import object at line %d: %s\n", j, line);
			return -EINVAL;
		}

		if (warn) {
			adb_vdebug(db, ADB_LOG_CDS_IMPORT,
				"At line %d with length %d :-\n",
				line_count, strlen(line));
			adb_vdebug(db, ADB_LOG_CDS_IMPORT, "line %s\n\n", line);
		}
		line_count++;
		import_inc(db, pc_count, pc, div);
	}

out:
	adb_info(db, ADB_LOG_CDS_IMPORT, "Got %d short records\n", short_records);
	adb_info(db, ADB_LOG_CDS_IMPORT, "Imported %d records\n", count);
	table->object.count = count;
	free(line);
	return count;
}

/*! \fn int table_import(adb_table * table, char *file_name, adb_progress progress)
 * \brief Import an ASCII dataset into table tile array
 */
int table_import(struct adb_db *db, int table_id, char *file)
{
	struct adb_table *table;
	FILE *f;
	char file_path[ADB_PATH_SIZE];
	int ret;

	table = &db->table[table_id];
	sprintf(file_path, "%s%s", table->path.local, file);
	adb_info(db, ADB_LOG_CDS_IMPORT,
		"Importing table %s from CDS ASCII format file %s\n", file_path, file);

	/* make sure we have valid import type */
	if (!table->object.import) {
		adb_error(db, "Invalid object import\n");
		return -EINVAL;
	}

	/* open data file */
	f = fopen(file_path, "r");
	if (f == NULL) {
		adb_error(db, "Error failed to open file %s\n", file_path);
		return -EIO;
	}

	/* order the into into ascending order */
	schema_order_import_index(db, table);

	/* calculate import buffer size */
	get_import_buffer_size(db, table);

	/* calculate histogram of size/magnitude */
	get_histogram_keys(db, table);
	if (table->import.histogram_key)
		table_histogram_import(db, table, f);
	else
		table_histogram_alt_import(db, table, f);

	/* import rows */
	if (table->object.num_alt_fields)
		ret = import_rows_with_alternatives(db, table_id, f);
	else
		ret = import_rows(db, table_id, f);
	if (ret < 0)
		goto out;

	/* build KD-Tree */
	ret = import_build_kdtree(db, table);

out:
	fclose(f);
	return ret;
}

/*! \fn int adb_table_alt_field(adb_table* table, char* field, char* alt, int flags)
 * \param table dataset
 * \param field Primary field
 * \param alt Alternative field
 * \param flags flags
 *
 * Set an alternative import field if the primary field is blank.
 */
int adb_table_import_field(struct adb_db *db, int table_id,
	const char *field, const char *alt, int flags)
{
	struct adb_table *table;
	int idx, err;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	if (table->object.num_alt_fields >= ADB_TABLE_MAX_ALT_FIELDS) {
		adb_error(db, "too many alt fields %s\n", field);
		return -EINVAL;
	}

	idx = schema_get_field(db, table, field);
	if (idx < 0) {
		adb_error(db, "field not found %s\n", field);
		return -EINVAL;
	}

	err = schema_add_alternative_field(db, table, alt, idx);
	if (err < 0) {
		adb_error(db, "failed to add field %s at idx %d\n", field, idx);
		return -EINVAL;
	}

	return 0;
}

/*! \fn int adb_table_register_schema(adb_table* table, adb_schema_object* field, int idx_size);
 * \param table dataset
 * \param field Object field index
 * \param idx_size Number of fields in index
 * \return 0 on success
 *
 * Register a new custom object type
 */
int adb_table_import_schema(struct adb_db *db, int table_id,
					struct adb_schema_field *schema,
					int num_schema_fields, int object_size)
{
	struct adb_table *table;
	int n, i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	/* check for depth field */
	for (i = 0, n = 0; i < num_schema_fields; i++) {
		if (!strncmp(schema[i].symbol, table->import.depth_field,
			ADB_SCHEMA_NAME_SIZE))
			goto add_fields;
	}
	adb_error(db, "failed to find depth field %s in schema\n",
		table->import.depth_field);
	return -EINVAL;

add_fields:
	for (i = 0, n = 0; i < num_schema_fields; i++) {
		if (!schema_add_field(db, table, schema + i))
			n++;
	}
	table->object.bytes = object_size;

	return n;
}

int adb_table_import_alt_dataset(struct adb_db *db, int table_id,
	const char *dataset, int num_objects)
{
	struct adb_table *table;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &db->table[table_id];

	table->import.alt_dataset = dataset;
	table->object.count = num_objects;
	return 0;
}

/*! \fn adb_table *adb_table_create(adb_db *db, char *table_name, unsigned int flags)
 * \param db Catalog
 * \param table_name Dataset name (dataset file_name name in ReadMe)
 * \param flags Dataset creation flags.
 * \return A adb_table* for success or NULL on error
 *
 * Creates a dataset object.
 */
int adb_table_import_new(struct adb_db *db,
		const char *cat_class, const char *cat_id, const char *table_name,
		const char *depth_field, float min_limit, float max_limit,
		adb_import_type otype)
{
	struct readme *readme;
	struct adb_table *table;
	int files, table_id, err = -EINVAL;
	struct stat stat_info;
	char local[ADB_PATH_SIZE];
	char remote[ADB_PATH_SIZE];

	table_id = table_get_id(db);
	if (table_id < 0)
		return -EINVAL;

	table = &db->table[table_id];
	table->db = db;
	table->object.otype = otype;

	adb_info(db, ADB_LOG_CDS_TABLE,
		"Creating table: %s (%d) from Catalog %s:%s with depth on %s\n",
		table_name, table_id, cat_class, cat_id, depth_field);

	/* setup the paths */
	table->cds.class = strdup(cat_class);
	if (table->cds.class == NULL)
		goto err;
	table->cds.index = strdup(cat_id);
	if (table->cds.index == NULL)
		goto err;
	sprintf(remote, "%s%s%s%s%s%s", db->lib->remote, "/",
		cat_class, "/", cat_id, "/");
	sprintf(local, "%s%s%s%s%s%s", db->lib->local, "/",
		cat_class, "/", cat_id, "/");
	table->path.remote = strdup(remote);
	if (table->path.remote == NULL)
		goto err;
	table->path.local = strdup(local);
	if (table->path.local == NULL)
		goto err;
	table->cds.host = db->lib->host;
	table->import.depth_field = depth_field;

	adb_info(db, ADB_LOG_CDS_TABLE, "Table local path %s\n",
		table->path.local);
	adb_info(db, ADB_LOG_CDS_TABLE, "Table remote path %s:%s \n",
		table->cds.host, table->path.remote);

	/* create local path */
	err = stat(table->path.local, &stat_info);
	if (err < 0) {
		err = mkdir(table->path.local, S_IRWXU | S_IRWXG);
		if (err < 0) {
			adb_error(db, "failed to create dir %s %d\n",
					table->path.local, err);
			goto err;
		}
	}

	/* get ReadMe */
	err = cds_get_readme(db, table_id);
	if (err < 0) {
		adb_error(db, "Failed to create table %s err %d\n",
			table_name, err);
		return err;
	}

	/* parse ReadMe */
	err = table_parse_readme(db, table_id);
	if (err < 0) {
		adb_error(db, "Failed to create table %s err %d\n",
			table_name, err);
		return err;
	}

	adb_info(db, ADB_LOG_CDS_TABLE, "Scanning for key fields in table: %s\n",
		table_name);
	readme = table->import.readme;

	for (files = 0; files < readme->num_files; files++) {
		struct cds_file_info *file_info = &readme->file[files];
		struct cds_byte_desc *byte_desc = &file_info->byte_desc[0];

		adb_debug(db, ADB_LOG_CDS_TABLE,
			"table %s filename %s num desc %d\n",
			table_name, file_info->name, file_info->num_desc);

		/* create a tile array per file_name for files that have a byte desc */
		if (file_info->num_desc &&
			!strncmp(table_name, file_info->name, strlen(table_name))) {

			/* init table fields from ReadMe information */
			table->path.file = strdup(table_name);
			table->import.byte_desc = byte_desc;
			table->import.text_length = file_info->length;
			table->import.file_info = file_info;

			/* set initial object count - will be updated by import */
			table->object.count = file_info->records;

			/* import type */
			switch (table->object.otype) {
			case ADB_IMPORT_INC:
				table->object.import = &import_object_ascending;
				break;
			case ADB_IMPORT_DEC:
				table->object.import = &import_object_descending;
				break;
			}

			table->object.max_value = max_limit;
			table->object.min_value = min_limit;
			adb_info(db, ADB_LOG_CDS_TABLE,
				"  creating table: %s\n", file_info->name);
			return table_id;
		}
	}

	/* this path is for a blank and empty new DB */
	adb_error(db, "Failed to find file with records %s\n", table_name);

err:
	adb_error(db, "Failed to create new table %s\n", table_name);
	free(table->cds.class);
	free(table->cds.index);
	free(table->path.remote);
	free(table->path.local);
	table_put_id(db, table_id);
	return err;
}

static const char *file_extensions[] = {
	".gz", ".dat.gz", ".dat", "",
};

int adb_table_import(struct adb_db *db, int table_id)
{
	struct adb_table *table = &db->table[table_id];
	int ret = -EINVAL, num_files, i;
	char file[ADB_PATH_SIZE];

	/* do we have an alternate dataset configured ? */
	if (table->import.alt_dataset) {
		table->path.file = table->import.alt_dataset;
		goto import;
	}

	/* do the ASCII data files exist locally ?  */
	for (i = 0; i < adb_size(file_extensions); i++) {
		num_files = cds_prepare_files(db, table, file_extensions[i]);
		if (num_files > 0)
			goto import;
	}

	/* none local, so try for files over FTP */
	for (i = 0; i < adb_size(file_extensions); i++) {
		/* download single file */
		ret = cds_get_dataset(db, table, file_extensions[i]);
		if (ret == 0)
			goto prepare;
	}

	/* now try split files over FTP */
	for (i = 0; i < adb_size(file_extensions); i++) {
		/* try split files */
		ret = cds_get_split_dataset(db, table, file_extensions[i]);
		if (ret == 0)
			goto prepare;
	}

	adb_warn(db, ADB_LOG_CDS_TABLE,
		"Error failed to FTP CDS data files for %s\n", table->path.file);
	return ret;

prepare:
	/* at this point we should have the CDS files -
	 * try compressed files first, then ASCII */
	for (i = 0; i < adb_size(file_extensions); i++) {
		num_files = cds_prepare_files(db, table, file_extensions[i]);
		if (num_files > 0)
			goto import;
	}
	if (num_files == 0) {
		adb_warn(db, ADB_LOG_CDS_TABLE,
			"Error failed to find CDS data files for %s\n", table->path.file);
		return ret;
	}

import:
	/* now import the CDS data into table and save schema and table objects*/
	for (i = 0; i < adb_size(file_extensions); i++) {
		sprintf(file, "%s%s", table->path.file, file_extensions[i]);
		adb_info(db, ADB_LOG_CDS_TABLE, "Importing CDS ASCII data %s\n", file);
		ret = table_import(db, table_id, file);
		if (ret < 0) {
			adb_warn(db, ADB_LOG_CDS_TABLE,
				"Error failed to import CDS table %s %d\n", table->path.file, ret);
		} else
			goto schema;
	}
	adb_error(db, "Error failed to import CDS table %s %d\n",
		table->path.file, ret);
	return ret;

schema:
	ret = schema_write(db, table);
	if (ret < 0) {
		adb_error(db, "Error failed to save table schema %d\n", ret);
		return ret;
	}

	ret = table_write_trixels(db, table);
	if (ret < 0) {
		adb_error(db, "Error failed write table objects %d\n", ret);
		return ret;
	}

	return table->object.count;
}
