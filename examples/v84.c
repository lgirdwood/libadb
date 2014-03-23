/*
 * Copyright (C) 2008 Liam Girdwood
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <libastrodb/astrodb.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

static int main_id, diam_id, dist_id, cstar_id, vel_id, png_id;

struct planetary_nebula_object {
	struct adb_object object; /* Vmag, RA, DEC */
	char name[16];		/* Name - main */
	float oDiam;		/* optical diameter (arcsecs) - diam */
	float rDiam;		/* radio diameter (arcsecs) - diam */
	float Dist;		/* distance (kpc) - dist */
	float Rvel;		/* radial velocity (kms) - vel */
};

#define v84_schema \
	adb_member("Name", "Name", struct planetary_nebula_object, \
		name, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_member("ID", "PNG", struct planetary_nebula_object, \
		object.designation, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_gmember("RA Hours", "RAh", struct planetary_nebula_object, \
		object.posn_mag.ra,  ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2, NULL), \
	adb_gmember("RA Minutes", "RAm", struct planetary_nebula_object,\
		object.posn_mag.ra, ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1, NULL), \
	adb_gmember("RA Seconds", "RAs", struct planetary_nebula_object, \
		object.posn_mag.ra, ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0, NULL), \
	adb_gmember("DEC Degrees", "DEd", struct planetary_nebula_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3, NULL), \
	adb_gmember("DEC Minutes", "DEm", struct planetary_nebula_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2, NULL), \
	adb_gmember("DEC Seconds", "DEs", struct planetary_nebula_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1, NULL), \
	adb_gmember("DEC sign", "DE-", struct planetary_nebula_object, \
		object.posn_mag.dec, ADB_CTYPE_SIGN, "", 0, NULL), \
	adb_member("Visual Mag", "Vmag", struct planetary_nebula_object, \
		object.posn_mag.Vmag, ADB_CTYPE_FLOAT, "", 0, NULL), \
	adb_member("Opt Diam", "oDiam", struct planetary_nebula_object, \
		oDiam, ADB_CTYPE_FLOAT, "arcsec", 0, NULL), \
	adb_member("Rad Diam", "rDiam", struct planetary_nebula_object, \
		rDiam, ADB_CTYPE_FLOAT, "arcsec", 0, NULL), \
	adb_member("Distance", "Dist", struct planetary_nebula_object, \
		Dist, ADB_CTYPE_FLOAT, "kpc", 0, NULL), \
	adb_member("Radial Vel", "Rvel", struct planetary_nebula_object, \
		Rvel, ADB_CTYPE_FLOAT, "kms", 0, NULL),



struct main_object {
	struct adb_object object; /* Vmag, RA, DEC */
	char name[16];		/* Name - main */
};

#define main_schema \
	adb_member("Name", "Name", struct main_object, \
		name, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_member("ID", "PNG", struct main_object, \
		object.designation, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_gmember("RA Hours", "RAh", struct main_object, \
		object.posn_mag.ra,  ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2, NULL), \
	adb_gmember("RA Minutes", "RAm", struct main_object,\
		object.posn_mag.ra, ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1, NULL), \
	adb_gmember("RA Seconds", "RAs", struct main_object, \
		object.posn_mag.ra, ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0, NULL), \
	adb_gmember("DEC Degrees", "DEd", struct main_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3, NULL), \
	adb_gmember("DEC Minutes", "DEm", struct main_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2, NULL), \
	adb_gmember("DEC Seconds", "DEs", struct main_object, \
		object.posn_mag.dec, ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1, NULL), \
	adb_gmember("DEC sign", "DE-", struct main_object, \
		object.posn_mag.dec, ADB_CTYPE_SIGN, "", 0, NULL), \
	
static struct adb_schema_field main_fields[] = {
	main_schema
};

struct diam_object {
	struct adb_object object; /* Vmag, RA, DEC */
	float oDiam;		/* optical diameter (arcsecs) - diam */
	float rDiam;		/* radio diameter (arcsecs) - diam */
};

#define diam_schema \
	adb_member("ID", "PNG", struct diam_object, \
		object.designation, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_member("Opt Diam", "oDiam", struct diam_object, \
		oDiam, ADB_CTYPE_FLOAT, "arcsec", 0, NULL), \
	adb_member("Rad Diam", "rDiam", struct diam_object, \
		rDiam, ADB_CTYPE_FLOAT, "arcsec", 0, NULL)

static struct adb_schema_field diam_fields[] = {
	diam_schema
};

struct dist_object {
	struct adb_object object; /* Vmag, RA, DEC */
	float Dist;		/* distance (kpc) - dist */
};

#define dist_schema \
	adb_member("ID", "PNG", struct dist_object, \
		object.designation, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_member("Distance", "Dist", struct dist_object, \
		Dist, ADB_CTYPE_FLOAT, "kpc", 0, NULL)

static struct adb_schema_field dist_fields[] = {
	dist_schema
};

struct vel_object {
	struct adb_object object; /* Vmag, RA, DEC */
	float Rvel;		/* radial velocity (kms) - vel */
};

#define vel_schema \
	adb_member("ID", "PNG", struct vel_object, \
		object.designation, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_member("Radial Vel", "RVel", struct vel_object, \
		Rvel, ADB_CTYPE_FLOAT, "kms", 0, NULL),

static struct adb_schema_field vel_fields[] = {
	vel_schema
};

struct cstar_object {
	struct adb_object object; /* Vmag, RA, DEC */
};

#define cstar_schema \
	adb_member("ID", "PNG", struct cstar_object, \
		object.designation, ADB_CTYPE_STRING, "", 0, NULL), \
	adb_member("Visual Mag", "Vmag", struct cstar_object, \
		object.posn_mag.Vmag, ADB_CTYPE_FLOAT, "", 0, NULL)

static struct adb_schema_field cstar_fields[] = {
	cstar_schema
};

static struct adb_schema_field planetary_nebula_fields[] = {
	v84_schema
};

int added = 0;

static int insert_row(struct adb_db *db, int table_id,
		struct adb_object *object)
{
	struct planetary_nebula_object *pn_obj;
	struct adb_object *gobject = NULL;

	pn_obj = calloc(1, sizeof(*pn_obj));
	if (pn_obj == NULL)
		return -ENOMEM;

	memcpy(pn_obj, object, sizeof(*object));

	adb_table_get_object(db, diam_id,
			object->designation, "PNG", &gobject);
	if (gobject) {
		struct diam_object *diam_obj = (struct diam_object *)gobject;
		pn_obj->oDiam = diam_obj->oDiam;
		pn_obj->rDiam = diam_obj->rDiam;
	}

	adb_table_get_object(db, dist_id,
		object->designation, "PNG", &gobject);
	if (gobject) {
		struct dist_object *dist_obj = (struct dist_object *)gobject;
		pn_obj->Dist = dist_obj->Dist;
	}

	adb_table_get_object(db, vel_id,
		object->designation, "PNG", &gobject);
	if (gobject) {
		struct vel_object *vel_obj = (struct vel_object *)gobject;
		pn_obj->Rvel = vel_obj->Rvel;
	}

	adb_table_get_object(db, cstar_id,
		object->designation, "PNG", &gobject);
	if (gobject) {
		struct cstar_object *cstar_obj = (struct cstar_object *)gobject;
		pn_obj->object.posn_mag.Vmag =
			cstar_obj->object.posn_mag.Vmag;
	}

	added += adb_table_insert_object(db, table_id, (struct adb_object*) pn_obj);

	return 0;	
}

/* we only accept the --prefix as our 1 arg*/
int main (int argc, char* argv[])
{ 
	struct adb_db *db;
	struct adb_library *lib;
	struct adb_object **objects;
	int i, err, count, heads, new = 0;

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

	/* main table */
	main_id = adb_table_create(db, "V", "84", "main",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (main_id < 0) {
		printf("failed to create table\n");
		return main_id;
	}

	err = adb_table_register_schema(db, main_id, main_fields,
			adb_size(main_fields), sizeof(struct main_object));
	if (err < 0) {
		printf("failed to register object main\n");
		return err;
	}
	adb_table_hash_key(db, main_id, "PNG");

	err = adb_table_open(db, main_id, 0);
	if (err < 0) {
		printf("failed to open table\n");
		return err;
	}

	/* diameter table */
	diam_id = adb_table_create(db, "V", "84", "diam",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (diam_id < 0) {
		printf("failed to create table\n");
		return diam_id;
	}

	err = adb_table_register_schema(db, diam_id, diam_fields,
			adb_size(diam_fields), sizeof(struct diam_object));
	if (err < 0) {
		printf("failed to register object diam\n");
		return err;
	}
	adb_table_hash_key(db, diam_id, "PNG");

	err = adb_table_open(db, diam_id, 0);
	if (err < 0) {
		printf("failed to open table\n");
		return err;
	}
	
	/* distance table */
	dist_id = adb_table_create(db, "V", "84", "dist",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (dist_id < 0) {
		printf("failed to create table\n");
		return dist_id;
	}

	err = adb_table_register_schema(db, dist_id, dist_fields,
			adb_size(dist_fields), sizeof(struct dist_object));
	if (err < 0) {
		printf("failed to register object dist\n");
		return err;
	}
	adb_table_hash_key(db, dist_id, "PNG");

	err = adb_table_open(db, dist_id, 0);
	if (err < 0) {
		printf("failed to open table\n");
		return err;
	}
	
	/* velocity table */
	vel_id = adb_table_create(db, "V", "84", "vel",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (vel_id < 0) {
		printf("failed to create table\n");
		return vel_id;
	}

	err = adb_table_register_schema(db, vel_id, vel_fields,
			adb_size(vel_fields), sizeof(struct vel_object));
	if (err < 0) {
		printf("failed to register object vel\n");
		return err;
	}
	adb_table_hash_key(db, vel_id, "PNG");

	err = adb_table_open(db, vel_id, 0);
	if (err < 0) {
		printf("failed to open table\n");
		return err;
	}
	
	/* cstar table */
	cstar_id = adb_table_create(db, "V", "84", "cstar",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (cstar_id < 0) {
		printf("failed to create table\n");
		return cstar_id;
	}

	err = adb_table_register_schema(db, cstar_id, cstar_fields,
			adb_size(cstar_fields), sizeof(struct cstar_object));
	if (err < 0) {
		printf("failed to register object cstar\n");
		return err;
	}
	adb_table_hash_key(db, cstar_id, "PNG");

	err = adb_table_open(db, cstar_id, 0);
	if (err < 0) {
		printf("failed to open table\n");
		return err;
	}
printf("%d\n", __LINE__);	
	/* PNG table */
	png_id = adb_table_create(db, "V", "84", "png",
			ADB_POSITION_MAG, -2.0, 8.0, 1.0);
	if (png_id < 0) {
		printf("failed to create table PNG\n");
		return png_id;
	}
printf("%d\n", __LINE__);
	err = adb_table_register_schema(db, png_id, planetary_nebula_fields,
			adb_size(planetary_nebula_fields),
			sizeof(struct planetary_nebula_object));
	if (err < 0) {
		printf("failed to register object main\n");
		return err;
	}
printf("%d\n", __LINE__);
//#error hash map not initalised for PNG
	adb_table_hash_key(db, png_id, "PNG");
printf("%d\n", __LINE__);
	err = adb_table_open(db, png_id, 1);
	if (err < 0) {
		printf("failed to open table\n");
		return err;
	} // check code here and skip new
printf("%d\n", __LINE__);
	adb_table_unclip(db, main_id);
printf("%d\n", __LINE__);
	heads = adb_table_get_objects(db, main_id, &objects, &count);
	if (heads == 0 || count == 0)
		goto out;
printf("%d\n", __LINE__);
	printf("Got %d heads %d objects\n", heads, count);

	for (i = 0; i < heads; i++) {
		struct adb_object *object = objects[i];

		while (object) {
			insert_row(db, png_id, object);
			object = object->next;
			new++;
		}
	}
	printf("created %d new %d objects\n", new, added);

out:
	/* were done with the dataset */
	adb_table_close(db, main_id);
	adb_table_close(db, diam_id);
	adb_table_close(db, dist_id);
	adb_table_close(db, vel_id);
	adb_table_close(db, cstar_id);
printf("%d\n", __LINE__);
	adb_table_close(db, png_id);
printf("%d\n", __LINE__);	
	/* were now done with dbalog */
	adb_db_free(db);
printf("%d\n", __LINE__);
	adb_close_library(lib);
printf("%d\n", __LINE__);
	return 0;
}
