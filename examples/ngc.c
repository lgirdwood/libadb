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
 * Copyright (C) 2008 Liam Girdwood
 */


#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>

#include <libastrodb/astrodb.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

static struct timeval start, end;

struct ngc_object {
	struct adb_object object;
	unsigned char type[4];
	char desc[51];		/* description */
	float size;
};

static struct adb_schema_field star_fields[] = {
	adb_member("Name", "Name", struct ngc_object,
		object.designation, ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("Type", "Type", struct ngc_object,
		type, ADB_CTYPE_STRING, "", 0, NULL),
	adb_gmember("RA Hours", "RAh", struct ngc_object, \
		object.posn_mag.ra,  ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 1, NULL),
	adb_gmember("RA Minutes", "RAm", struct ngc_object,
		object.posn_mag.ra, ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 0, NULL),
	adb_gmember("DEC Degrees", "DEd", struct ngc_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 2, NULL),
	adb_gmember("DEC Minutes", "DEm", struct ngc_object,
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 1, NULL),
	adb_gmember("DEC sign", "DE-", struct ngc_object,
		object.posn_mag.dec, ADB_CTYPE_SIGN, "", 0, NULL),
	adb_member("Integrated Mag", "mag", struct ngc_object,
		object.posn_mag.key, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Description", "Desc", struct ngc_object,
		desc, ADB_CTYPE_STRING, "", 0, NULL),
	adb_gmember("Largest Dimension", "size", struct ngc_object, \
		size,  ADB_CTYPE_FLOAT, "arcmin", 0, NULL),
};

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
		fprintf(stdout,"   Time %3.1f msecs @ %3.3e objects / %3.3e bytes per sec\n",
			secs * 1000.0 , objects / secs , bytes / secs);
	else
		fprintf(stdout,"   Time %3.1f msecs @ %3.3e objects per sec\n",
			secs * 1000.0, objects / secs);
}

/* 
 * Get all the objects in the dataset.
 */
static int get_all(struct adb_db *db, int table_id)
{
	int count, heads;
	struct adb_object **object;

	fprintf(stdout,"Get all dataset objects\n");
	adb_table_unclip(db, table_id);
	adb_table_clip_on_fov(db, table_id, 0.0, 0.0, 360.0, -2.0, 16.0);
	heads = adb_table_get_objects(db, table_id, &object, &count);
	fprintf(stdout,"   found %d object list heads %d objects\n\n", heads, count);

	return 0;
}

int main(int argc, char *argv[])
{
	struct adb_db *db;
	struct adb_library *lib;
	int table_id, table_size, object_size;

	fprintf(stdout,"%s using libastrodb %s\n\n", argv[0], adb_get_version());
	
	/* set the remote db and initialise local repository/cache */
	lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats",  argv[1]);
	if (lib == NULL) {
		fprintf(stderr,"failed to open library\n");
		return -1;
	}

	db = adb_create_db(lib, 1.0 * D2R, 1);
	if (db == NULL) {
		fprintf(stderr,"failed to create db\n");
		return -1;
	}
	//adb_set_msg_level(db, ADB_MSG_DEBUG);
	//adb_set_log_level(db, ADB_LOG_ALL);

	/* use the first dataset in this example */
	table_id = adb_table_create(db, "VII", "118", "ngc2000.dat",
			ADB_POSITION_MAG, -2.0, 18.0, 1.0);
	if (table_id < 0) {
		fprintf(stderr,"failed to create table\n");
		return -1;
	}

	if (adb_table_register_schema(db, table_id, star_fields,
		adb_size(star_fields), sizeof(struct ngc_object)) < 0)
		fprintf(stderr,"%s: failed to register object type\n", __func__);

	adb_table_hash_key(db, table_id, "Name");

	/* Import the dataset from remote/local repo into memory/disk cache */
	start_timer();
	if (adb_table_open(db, table_id, 0) < 0) {
		fprintf(stderr,"failed to open table\n");
		return -1;
	}

	table_size = adb_table_get_size(db, table_id);
	object_size = adb_table_get_object_size(db, table_id);
	end_timer(table_size / object_size, table_size);

	/* we can now perform operations on the dbalog data !!! */
	get_all(db, table_id);

	/* were done with the dataset */
	adb_table_close(db, table_id);

	/* were now done with dbalog */
	adb_db_free(db);
	adb_close_library(lib);

	return 0;
}
