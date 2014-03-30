/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2005 Liam Girdwood
 */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <libastrodb/db.h>
#include <libastrodb/db-import.h>
#include <libastrodb/object.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

struct hyperleda_object {
	struct adb_object d;
	float position_angle;
	float axis_ratio;
	char MType[4];
	char OType[1];
	char other_name[15];
};

static int pa_insert(struct adb_object * object, int offset, char *src)
{
	float *dest = (float*)((char*)object + offset);
	char *ptr;

	*dest = strtof(src, &ptr);
	if (src == ptr) {
		*dest = FP_NAN;
		return -1;
	}
	if (*dest == (float) 999.0)
		*dest = FP_NAN;

	return 0;
}

static int size_insert(struct adb_object * object, int offset, char *src)
{
	float *dest = (float*)((char*)object + offset);
	char *ptr;

	*(float *) dest = strtof(src, &ptr);
	if (src == ptr) {
		*dest = FP_NAN;
		return -1;
	}

	if (*dest == (float) 9.99)
		*dest = FP_NAN;
	return 0;
}

static int otype_insert(struct adb_object * object, int offset, char *src)
{
	char *dest = (char*)object + offset;

	if (*src == 'M')
		*dest = 'M';
	else if (*src == 'G') {
		if (*(src + 1) == 'M')
			*dest = 'X';
		else
			*dest = 'G';
	}
	return 0;
}

static struct adb_schema_field hyperleda_fields[] = {
	adb_member("Name", "ANames", struct hyperleda_object, other_name,
		ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("ID", "PGC", struct hyperleda_object, d.id,
		ADB_CTYPE_INT, "", 0, NULL),
	adb_gmember("RA Hours", "RAh", struct hyperleda_object, d.ra,
		ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2, NULL),
	adb_gmember("RA Minutes", "RAm", struct hyperleda_object, d.ra,
		ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1, NULL),
	adb_gmember("RA Seconds", "RAs", struct hyperleda_object, d.ra,
		ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0, NULL),
	adb_gmember("DEC Degrees", "DEd", struct hyperleda_object, d.dec,
		ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3, NULL),
	adb_gmember("DEC Minutes", "DEm", struct hyperleda_object, d.dec,
		ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2, NULL),
	adb_gmember("DEC Seconds", "DEs", struct hyperleda_object, d.dec,
		ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1, NULL),
	adb_gmember("DEC sign", "DE-", struct hyperleda_object, d.dec,
		ADB_CTYPE_SIGN, "", 0, NULL),
	adb_member("Type", "MType", struct hyperleda_object, MType,
		ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("OType", "OType", struct hyperleda_object, OType,
		ADB_CTYPE_STRING, "", 0, otype_insert),
	adb_member("Diameter", "logD25", struct hyperleda_object,  d.key,
		ADB_CTYPE_FLOAT, "0.1amin", 0, size_insert),
	adb_member("Axis Ratio", "logR25", struct hyperleda_object, axis_ratio,
		ADB_CTYPE_FLOAT, "0.1amin", 0, size_insert),
	adb_member("Position Angle", "PA", struct hyperleda_object, position_angle,
		ADB_CTYPE_FLOAT, "deg", 0, pa_insert),
};

static int print = 0;

static void get_printf(const struct adb_object_head *object_head, int heads)
{
	const struct hyperleda_object *obj;
	int i, j;

	if (!print)
		return;

	for (i = 0; i < heads; i++) {
		obj = object_head->objects;

		for (j = 0; j < object_head->count; j++) {
			fprintf(stdout, "Obj: %s %ld RA: %f DEC: %f Size %f\n",
				obj->other_name, obj->d.id, obj->d.ra * R2D,
				obj->d.dec * R2D, obj->d.key);
			obj++;
		}
		object_head++;
	}
}

/*
 * Get all the objects in the dataset.
 */
static int get1(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count, heads;

	fprintf(stdout, "Get all objects\n");
	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	adb_table_set_constraints(set, 0.0, 0.0, 360.0, -2.0, 16.0);

	heads = adb_table_set_get_objects(set);
	count = adb_set_get_count(set);
	fprintf(stdout, " found %d object list heads %d objects\n\n", heads, count);

	get_printf(adb_set_get_head(set), heads);

	adb_table_set_free(set);
	return 0;
}

static int sky2k_import(char *lib_dir)
{
	struct adb_library *lib;
	struct adb_db *db;
	int ret, table_id;

	/* set the remote CDS server and initialise local repository/cache */
	lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir);
	if (lib == NULL) {
		fprintf(stderr, "failed to open library\n");
		return -ENOMEM;
	}

	db = adb_create_db(lib, 7, 1);
	if (db == NULL) {
		fprintf(stderr, "failed to create db\n");
		ret = -ENOMEM;
		goto out;
	}

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	table_id = adb_table_import_new(db, "VII", "237", "pgc",
			"logD25", 0.0, 2.0, ADB_IMPORT_DEC);
	if (table_id < 0) {
		fprintf(stderr, "failed to create import table\n");
		ret = table_id;
		goto out;
	}

	ret = adb_table_import_schema(db, table_id, hyperleda_fields,
		adb_size(hyperleda_fields), sizeof(struct hyperleda_object));
	if (ret < 0) {
		fprintf(stderr, "%s: failed to register object type\n", __func__);
		goto out;
	}

	ret = adb_table_import(db, table_id);
	if (ret < 0)
		fprintf(stderr, "failed to import\n");

out:
	adb_db_free(db);
	adb_close_library(lib);
	return ret;
}

static int sky2k_query(char *lib_dir)
{
	struct adb_library *lib;
	struct adb_db *db;
	int ret = 0, table_id;

	/* set the remote CDS server and initialise local repository/cache */
	lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir);
	if (lib == NULL) {
		fprintf(stderr, "failed to open library\n");
		return -ENOMEM;
	}

	db = adb_create_db(lib, 7, 1);
	if (db == NULL) {
		fprintf(stderr, "failed to create db\n");
		ret = -ENOMEM;
		goto lib_err;
	}

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	/* use CDS catalog class V, #109, dataset skykv4 */
	table_id = adb_table_open(db, "VII", "237", "pgc");
	if (table_id < 0) {
		fprintf(stderr, "failed to create table\n");
		ret = table_id;
		goto table_err;
	}

	/* create a fast lookup hash on object PGC number */
	adb_table_hash_key(db, table_id, "PGC");

	/* we can now perform operations on the db data !!! */
	get1(db, table_id);

	/* were done with the db */
table_err:
	adb_table_close(db, table_id);
	adb_db_free(db);

lib_err:
	/* were now done with library */
	adb_close_library(lib);
	return ret;
}

static int sky2k_solve(char *lib_dir)
{
	/* TODO: */
	return 0;
}

static void usage(char *argv)
{
	fprintf(stdout, "Import: %s: -i [import dir]", argv);
	fprintf(stdout, "Query: %s: -q [library dir]", argv);
	fprintf(stdout, "Solve: %s: -s [library dir]", argv);

	exit(0);
}

int main(int argc, char *argv[])
{
	int i;

	fprintf(stdout, "%s using libastrodb %s\n\n", argv[0], adb_get_version());

	if (argc < 3)
		usage(argv[0]);

	for (i = 1 ; i < argc - 1; i++) {

		/* import */
		if (!strcmp("-i", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			sky2k_import(argv[i]);
			continue;
		}

		/* query */
		if (!strcmp("-q", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			sky2k_query(argv[i]);
			continue;
		}

		/* solve */
		if (!strcmp("-s", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			sky2k_solve(argv[i]);
			continue;
		}

		/* print */
		if (!strcmp("-p", argv[i])) {
			print = 1;
			continue;
		}
	}

	return 0;
}
