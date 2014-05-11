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
 * Copyright (C) 2014 Liam Girdwood
 */


#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#include <libastrodb/db-import.h>
#include <libastrodb/db.h>
#include <libastrodb/object.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

struct tycho_object {
	struct adb_object object;
	float pmRA;
	float pmDEC;
	float plx;
	unsigned int HD;
	unsigned int HIP;
};

int tycid2_sp_insert(struct adb_object *object, int offset, char *src)
{
	char *dest = (char*)object + offset;

	sprintf(dest, "-%s", src);
	return 0;
}

static struct adb_schema_field tycho_fields[] = {
	adb_member("Name", "TYCID1", struct tycho_object,
		object.designation, ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("Name", "TYCID2", struct tycho_object,
		object.designation, ADB_CTYPE_STRING, "", 0, tycid2_sp_insert),
	adb_member("Name", "TYCID3", struct tycho_object,
		object.designation, ADB_CTYPE_STRING, "", 0, tycid2_sp_insert),
	adb_member("RA", "RAdeg", struct tycho_object, \
		object.ra,  ADB_CTYPE_DEGREES, "degrees", 0, NULL),
	adb_member("DEC", "DEdeg", struct tycho_object, \
		object.dec, ADB_CTYPE_DEGREES, "degrees", 0, NULL),
	adb_member("Mag", "VT", struct tycho_object,
		object.key, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("pmRA", "pmRA", struct tycho_object,
		pmRA, ADB_CTYPE_FLOAT, "mas/a", 0, NULL),
	adb_member("pmDEC", "pmDEC", struct tycho_object,
		pmDEC, ADB_CTYPE_FLOAT, "mas/a", 0, NULL),
	adb_member("Parallax", "Plx", struct tycho_object, \
		plx,  ADB_CTYPE_FLOAT, "mas", 0, NULL),
	adb_member("HD Number", "HD", struct tycho_object, \
		HD,  ADB_CTYPE_INT, "", 0, NULL),
	adb_member("HIP Number", "HIP", struct tycho_object, \
		HIP,  ADB_CTYPE_INT, "", 0, NULL),
};

static int print = 0;

static void get_printf(const struct adb_object_head *object_head, int heads)
{
	const struct tycho_object *obj;
	int i, j;

	if (!print)
		return;

	for (i = 0; i < heads; i++) {
		obj = object_head->objects;

		for (j = 0; j < object_head->count; j++) {
			fprintf(stdout, "Obj: %s %ld RA: %f DEC: %f Mag %f\n",
				obj->object.designation, obj->object.id, obj->object.ra * R2D,
				obj->object.dec * R2D, obj->object.key);
			obj++;
		}
		object_head++;
	}
}

/*
 * Get all the objects in the dataset.
 */
static int get_all(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count, heads;

	fprintf(stdout, "Get all objects\n");
	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	adb_table_set_constraints(set, 0.0, 0.0, 360.0, 0.0, 16.0);

	heads = adb_table_set_get_objects(set);
	count = adb_set_get_count(set);
	fprintf(stdout, " found %d object list heads %d objects\n\n", heads, count);

	get_printf(adb_set_get_head(set), heads);

	adb_table_set_free(set);
	return 0;
}

int tycho_query(char *lib_dir)
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

	db = adb_create_db(lib, 5, 1);
	if (db == NULL) {
		fprintf(stderr, "failed to create db\n");
		ret = -ENOMEM;
		goto lib_err;
	}

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	/* use CDS catalog class I, #250, dataset tycho */
	table_id = adb_table_open(db, "I", "250", "catalog");
	if (table_id < 0) {
		fprintf(stderr, "failed to create table\n");
		ret = table_id;
		goto table_err;
	}

	/* create a fast lookup hash on object HD & HIP numbers */
	adb_table_hash_key(db, table_id, "HD");
	adb_table_hash_key(db, table_id, "HIP");

	/* we can now perform operations on the db data !!! */
	get_all(db, table_id);

	/* were done with the db */
table_err:
	adb_table_close(db, table_id);
	adb_db_free(db);

lib_err:
	/* were now done with library */
	adb_close_library(lib);
	return ret;
}

int tycho_import(char *lib_dir)
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

	db = adb_create_db(lib, 5, 1);
	if (db == NULL) {
		fprintf(stderr, "failed to create db\n");
		ret = -ENOMEM;
		goto out;
	}

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	table_id = adb_table_import_new(db, "I", "250", "catalog",
				"VT", 0.0, 16.0, ADB_IMPORT_DEC);
	if (table_id < 0) {
		fprintf(stderr, "failed to create import table\n");
		ret = table_id;
		goto out;
	}

	ret = adb_table_import_schema(db, table_id, tycho_fields,
		adb_size(tycho_fields), sizeof(struct tycho_object));
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

static int tycho_solve(char *lib_dir)
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
			tycho_import(argv[i]);
			continue;
		}

		/* query */
		if (!strcmp("-q", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			tycho_query(argv[i]);
			continue;
		}

		/* solve */
		if (!strcmp("-s", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			tycho_solve(argv[i]);
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
