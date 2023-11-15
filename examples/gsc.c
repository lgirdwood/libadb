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
 * Copyright (C) 2015 Liam Girdwood
 */


#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libastrodb/db-import.h>
#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include <libastrodb/search.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

struct gsc_object {
	struct adb_object object;
	float pos_err;
	float pmag_err;
};

static struct adb_schema_field gsc_fields[] = {
	adb_member("Designation", "GSC", struct gsc_object,
		object.designation, ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("RA", "RAdeg", struct gsc_object, \
		object.ra,  ADB_CTYPE_DEGREES, "degrees", 1, NULL),
	adb_member("DEC", "DEdeg", struct gsc_object,
		object.dec, ADB_CTYPE_DEGREES, "degrees", 1, NULL),
	adb_member("Photographic Mag", "Pmag", struct gsc_object,
		object.mag, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Mag error", "e_Pmag", struct gsc_object,
		pmag_err, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Pos error", "PosErr", struct gsc_object,
		pos_err, ADB_CTYPE_FLOAT, "", 0, NULL)
};

static int print = 0;
static int sprint = 1;

static void get_printf(const struct adb_object_head *object_head, int heads)
{
	const struct gsc_object *obj;
	int i, j;

	if (!print)
		return;

	for (i = 0; i < heads; i++) {
		obj = object_head->objects;

		for (j = 0; j < object_head->count; j++) {
			fprintf(stdout, "Obj: %s RA: %f DEC: %f Mag %3.2f\n",
				obj->object.designation, obj->object.ra * R2D,
				obj->object.dec * R2D, obj->object.mag);
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

	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, 0.0, 16.0);

	heads = adb_set_get_objects(set);
	count = adb_set_get_count(set);
	fprintf(stdout, " found %d object list heads %d objects\n\n", heads, count);

	get_printf(adb_set_get_head(set), heads);

	adb_table_set_free(set);
	return 0;
}

static struct timeval start, end;

inline static void start_timer(void)
{
	gettimeofday(&start, NULL);
}

static void end_timer(int objects, int bytes)
{
	double secs;

	gettimeofday(&end, NULL);
	secs = ((end.tv_sec * 1000000 + end.tv_usec) -
		(start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0;

	if (bytes)
		fprintf(stdout, "   Time %3.1f msecs @ %3.3e objects / %3.3e bytes per sec\n",
			secs * 1000.0 , objects / secs , bytes / secs);
	else
		fprintf(stdout, "   Time %3.1f msecs @ %3.3e objects per sec\n",
			secs * 1000.0, objects / secs);
}

static void search_print(const struct adb_object *_objects[], int count)
{
	const struct gsc_object **objects =
		(const struct gsc_object **) _objects;
	int i;

	if (!sprint)
		return;

	for (i = 0; i < count; i++) {
		const struct gsc_object *obj = objects[i];
		fprintf(stdout, "Obj: %s RA: %f DEC: %f Mag %3.2f\n",
				obj->object.designation, obj->object.ra * R2D,
				obj->object.dec * R2D, obj->object.mag);
		obj++;
	}
}

static int search1(struct adb_db *db, int table_id)
{
	const struct adb_object **object;
	struct adb_search *search;
	struct adb_object_set *set;
	int err;

	fprintf(stdout, "Searching objects\n");

	search = adb_search_new(db, table_id);
	if (!search)
		return -ENOMEM;

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	if (adb_search_add_comparator(search, "DEdeg", ADB_COMP_LT, "58.434773"))
		fprintf(stderr, "failed to add comp DEdeg LT\n");
	if (adb_search_add_comparator(search, "DEdeg", ADB_COMP_GT, "57.678541"))
		fprintf(stderr, "failed to add comp DEdeg GT\n");
	if (adb_search_add_operator(search, ADB_OP_AND))
		fprintf(stderr, "failed to add op and\n");

	if (adb_search_add_comparator(search, "RAdeg", ADB_COMP_LT, "342.434232"))
		fprintf(stderr, "failed to add comp RAdeg LT\n");
	if (adb_search_add_comparator(search, "RAdeg", ADB_COMP_GT, "341.339925"))
		fprintf(stderr, "failed to add comp RAdeg GT\n");
	if (adb_search_add_operator(search, ADB_OP_AND))
		fprintf(stderr, "failed to add op and\n");

	if (adb_search_add_operator(search, ADB_OP_AND))
		fprintf(stderr, "failed to add op or\n");

	start_timer();
	if ((err = adb_search_get_results(search, set, &object)) < 0) {
		fprintf(stderr, "Search init failed %d\n", err);
		adb_search_free(search);
		return err;
	}
	end_timer(adb_search_get_tests(search), 0);

	fprintf(stdout, "   Search got %d objects out of %d tests\n\n",
		adb_search_get_hits(search),
		adb_search_get_tests(search));

	search_print(object, adb_search_get_hits(search));

	adb_search_free(search);
	adb_table_set_free(set);
	return 0;
}


int gsc_query(char *lib_dir)
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

	db = adb_create_db(lib, 9, 1);
	if (db == NULL) {
		fprintf(stderr, "failed to create db\n");
		ret = -ENOMEM;
		goto lib_err;
	}

	//adb_set_msg_level(db, ADB_MSG_DEBUG);
	//adb_set_log_level(db, ADB_LOG_ALL);

	/* use CDS catalog class I, #254, dataset gsc2000 */
	table_id = adb_table_open(db, "I", "254", "gsc");
	if (table_id < 0) {
		fprintf(stderr, "failed to create table\n");
		ret = table_id;
		goto table_err;
	}

	search1(db, table_id);

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

int gsc_import(char *lib_dir)
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

	db = adb_create_db(lib, 9, 1);
	if (db == NULL) {
		fprintf(stderr, "failed to create db\n");
		ret = -ENOMEM;
		goto out;
	}

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	table_id = adb_table_import_new(db, "I", "254", "out",
				"Pmag", -2.0, 17.0, ADB_IMPORT_INC);
	if (table_id < 0) {
		fprintf(stderr, "failed to create import table\n");
		ret = table_id;
		goto out;
	}

	ret = adb_table_import_alt_dataset(db, table_id, "gsc", 0);
	if (ret < 0) {
		fprintf(stderr, "%s: failed to register alt dataset\n", __func__);
		goto out;
	}

	ret = adb_table_import_schema(db, table_id, gsc_fields,
		adb_size(gsc_fields), sizeof(struct gsc_object));
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

static int gsc_solve(char *lib_dir)
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
			gsc_import(argv[i]);
			continue;
		}

		/* query */
		if (!strcmp("-q", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			gsc_query(argv[i]);
			continue;
		}

		/* solve */
		if (!strcmp("-s", argv[i])) {
			if (++i == argc)
				usage(argv[0]);
			gsc_solve(argv[i]);
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
