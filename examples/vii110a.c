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
#include <sys/time.h>
#include <stdio.h>
#include <libastrodb/astrodb.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

static struct timeval start, end;

static inline void start_timer(void)
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
		fprintf(stdout,"   Time %3.1f msecs @ %3.3e objects / %3.3e bytes per sec\n",
			secs * 1000.0 , objects / secs , bytes / secs);
	else
		fprintf(stdout,"   Time %3.1f msecs @ %3.3e objects per sec\n",
			secs * 1000.0, objects / secs);
}

static void print_progress(float percent, char *msg, void *data)
{
	fprintf(stdout, "Progess %f %s\n", percent, msg);
}

/*
 * Search for red shift > 0
 */
static int search(struct astrodb_table * table)
{
#if 0
	struct astrodb_slist *res = NULL;
	struct astrodb_search *srch;
	int err;

	srch = astrodb_search_new(table);
	astrodb_search_add_comparator(srch, "z", ADB_COMP_GT, "0");
	astrodb_search_add_operator(srch, ADB_OP_OR);

	start_timer();
	
	err = astrodb_search_get_results(srch, &res, ADB_SMEM);
	if (err < 0)
		printf("Search init failed %d\n", err);

	end_timer(astrodb_search_get_tests(srch), 0);

	printf("   Search got %d objects out of %d tests\n", 
	       astrodb_search_get_hits(srch), astrodb_search_get_tests(srch));

	astrodb_search_put_results(res);
	astrodb_search_free(srch);
#endif
	return 0;
}

int init_table(void *data, void *user)
{
#if 0
	struct cds_file_info *i = (struct cds_file_info *) data;
	struct astrodb_db *db = (struct astrodb_db *) user;
	struct astrodb_table *table;
	struct astrodb_slist *res = NULL;
	int table_size, object_size, count;

	printf("\nDset: \t%s\nrecords:\t%d\nrec len: \t%d\ntitle: \t%s\n",
			i->name, i->records, i->length, i->title);

	table = astrodb_table_create(db, i->name, ADB_MEM | ADB_FILE, IT_POSN_MAG);
	if (table) {
		start_timer();
		//astrodb_table_add_custom_field(table, "*");

		if (astrodb_table_open(table, 0, 0, 0) < 0)
			exit(-1);

		table_size = astrodb_table_get_size(table);
		object_size = astrodb_table_get_object_size(table);
		end_timer(table_size, table_size * object_size);

		astrodb_table_unclip(table);
		count = astrodb_table_get_objects(table, &res, ADB_SMEM);
		printf("Got %d objects\n", count);
		astrodb_table_put_objects(res);

		search(table);
		astrodb_table_close(table);
	}
#endif
	return 0;
}

int main(int argc, char *argv[])
{
	struct astrodb_db *db;
	struct astrodb_library *lib;
	int table_id, table_size, object_size;

	printf("%s using libastrodb %s\n", argv[0], astrodb_get_version());

	/* set the remote db and initialise local repository/cache */
	lib = astrodb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", argv[1]);
	if (lib == NULL) {
		printf("failed to open library\n");
		return -1;
	}

	db = astrodb_create_db(lib, 1.0 * D2R, 1);
	if (db == NULL) {
		printf("failed to create db\n");
		return -1;
	}
	astrodb_set_msg_level(db, ADB_MSG_DEBUG);
	astrodb_set_log_level(db, ADB_LOG_ALL);

	/* use the first dataset in this example */
	table_id = astrodb_table_create(db, "VII", "110A", "s",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (table_id < 0) {
		printf("failed to create table\n");
		return -1;
	}

//	if (astrodb_table_register_schema(db, table_id, star_fields,
//		astrodb_size(star_fields), sizeof(struct sky2kv4_object)) < 0)
//		printf("%s: failed to register object type\n", __func__);

	if (astrodb_table_open(db, table_id, 0) < 0) {
		printf("failed to open table\n");
		return -1;
	}

	/* were done with the dataset */
	astrodb_table_close(db, table_id);

	/* were now done with dbalog */
	astrodb_db_free(db);
	astrodb_close_library(lib);
	return 0;
}
