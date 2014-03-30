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
#include <libastrodb/astrodb.h>

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
//exit(0);
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
	adb_gmember("RA Hours", "RAh", struct hyperleda_object, d.posn_size.ra, 
		    ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2, NULL),
	adb_gmember("RA Minutes", "RAm", struct hyperleda_object, d.posn_size.ra, 
		    ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1, NULL),
	adb_gmember("RA Seconds", "RAs", struct hyperleda_object, d.posn_size.ra, 
		    ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0, NULL),
	adb_gmember("DEC Degrees", "DEd", struct hyperleda_object, d.posn_size.dec, 
		    ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3, NULL),
	adb_gmember("DEC Minutes", "DEm", struct hyperleda_object, d.posn_size.dec, 
		    ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2, NULL),
	adb_gmember("DEC Seconds", "DEs", struct hyperleda_object, d.posn_size.dec, 
		    ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1, NULL),
	adb_gmember("DEC sign", "DE-", struct hyperleda_object, d.posn_size.dec, 
		    ADB_CTYPE_SIGN, "", 0, NULL),
	adb_member("Type", "MType", struct hyperleda_object, MType, 
		   ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("OType", "OType", struct hyperleda_object, OType,
		   ADB_CTYPE_STRING, "", 0, otype_insert),
	adb_member("Diameter", "logD25", struct hyperleda_object,  d.posn_size.size, 
		   ADB_CTYPE_FLOAT, "0.1amin", 0, size_insert),
	adb_member("Axis Ratio", "logR25", struct hyperleda_object, axis_ratio, 
		   ADB_CTYPE_FLOAT, "0.1amin", 0, size_insert),
	adb_member("Position Angle", "PA", struct hyperleda_object, position_angle, 
		   ADB_CTYPE_FLOAT, "deg", 0, pa_insert),
};

int main (int argc, char* argv[])
{ 
	struct adb_db *db;
	struct adb_library *lib;
	int table_id, table_size, object_size;

	printf("%s using libastrodb %s\n", argv[0], adb_get_version());
	
	/* set the remote db and initialise local repository/cache */
	lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", argv[1]);
	if (lib == NULL) {
		printf("failed to open library\n");
		return -1;
	}

	/* create a dbalog, using class V and dbalog 109 (Skymap2000) */
	/* ra,dec,mag bounds are set here along with the 3d tile array size */
	db = adb_create_db(lib, 1.0 * D2R, 1);
	if (db == NULL) {
		printf("failed to create db\n");
		return -1;
	}
	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	/* use the first dataset in this example */
	table_id = adb_table_create(db, "VII", "237", "pgc",
			ADB_POSITION_SIZE, 0.0, 2.0, 1.0);
	if (table_id < 0) {
		printf("failed to create table\n");
		return -1;
	}

	if (adb_table_register_schema(db, table_id, hyperleda_fields,
		adb_size(hyperleda_fields), sizeof(struct hyperleda_object)) < 0)
		printf("%s: failed to register object type\n", __func__);

	adb_table_hash_key(db, table_id, "PGC");

	/* Import the dataset from remote/local repo into memory/disk cache */
	if (adb_table_open(db, table_id, 0) < 0) {
		printf("failed to open table\n");
		return -1;
	}

	table_size = adb_table_get_size(db, table_id);
	object_size = adb_table_get_object_size(db, table_id);

	/* were done with the dataset */
	adb_table_close(db, table_id);

	/* were now done with dbalog */
	adb_db_free(db);
	adb_close_library(lib);
	return 0;
}
