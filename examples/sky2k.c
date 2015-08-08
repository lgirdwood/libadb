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
 * Copyright (C) 2008,2012 Liam Girdwood
 */

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <libastrodb/solve.h>
#include <libastrodb/search.h>
#include <libastrodb/db-import.h>
#include <libastrodb/db.h>
#include <libastrodb/object.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

#define SP_NUM 180

struct sky2kv4_object {
	struct adb_object object;
	unsigned char type;
	char name[11];		/* Name or AGK3 number */
	char sp[4];		/* One dimensional SP class */
	int HD;		/* HD number */
	int SAO;		/* SAO number */
	int PPM;		/* PPM number */
	double pmRA;		/* proper motion in RA */
	double pmDEC;		/* proper motion in DEC */
	double RV;		/* Radial velocity */

	double sep;		/* separation between 1st and 2nd brightest */
	float Dmag;		/* mag difference */
	double orbPer;		/* orbital period - years */
	short PA;		/* position angle */
	double date;		/* observation date */
	int ID_A;		/* primary component ID */
	int ID_B;		/* primary component ID */
	int ID_C;		/* primary component ID */

	float magMax;		/* max variable mag */
	float magMin;		/* min variable mag */
	float varAmp;		/* variability magnitude */
	double varPer;		/* period of variability - days */
	double varEpo;		/* epoch of variability */
	short varType;		/* type of variable star */
};

struct star_colour_data {
	char *sp;
	double r,g,b;
};

/* rgb star colours */
static const struct star_colour_data sp [] = {
	{"O5V", 0.607843, 0.690196, 1.000000},
	{"O6V", 0.635294, 0.721569, 1.000000},
	{"O7V", 0.615686, 0.694118, 1.000000},
	{"O8V", 0.615686, 0.694118, 1.000000},
	{"O9V", 0.603922, 0.698039, 1.000000},
	{"O9.5V", 0.643137, 0.729412, 1.000000},
	{"B0V", 0.611765, 0.698039, 1.000000},
	{"B0.5V", 0.654902, 0.737255, 1.000000},
	{"B1V", 0.627451, 0.713725, 1.000000},
	{"B2V", 0.627451, 0.705882, 1.000000},
	{"B3V", 0.647059, 0.725490, 1.000000},
	{"B4V", 0.643137, 0.721569, 1.000000},
	{"B5V", 0.666667, 0.749020, 1.000000},
	{"B6V", 0.674510, 0.741176, 1.000000},
	{"B7V", 0.678431, 0.749020, 1.000000},
	{"B8V", 0.694118, 0.764706, 1.000000},
	{"B9V", 0.709804, 0.776471, 1.000000},
	{"A0V", 0.725490, 0.788235, 1.000000},
	{"A1V", 0.709804, 0.780392, 1.000000},
	{"A2V", 0.733333, 0.796078, 1.000000},
	{"A5V", 0.792157, 0.843137, 1.000000},
	{"A6V", 0.780392, 0.831373, 1.000000},
	{"A7V", 0.784314, 0.835294, 1.000000},
	{"A8V", 0.835294, 0.870588, 1.000000},
	{"A9V", 0.858824, 0.878431, 1.000000},
	{"F0V", 0.878431, 0.898039, 1.000000},
	{"F2V", 0.925490, 0.937255, 1.000000},
	{"F4V", 0.878431, 0.886275, 1.000000},
	{"F5V", 0.972549, 0.968627, 1.000000},
	{"F6V", 0.956863, 0.945098, 1.000000},
	{"F7V", 0.964706, 0.952941, 1.000000},
	{"F8V", 1.000000, 0.968627, 0.988235},
	{"F9V", 1.000000, 0.968627, 0.988235},
	{"G0V", 1.000000, 0.972549, 0.988235},
	{"G1V", 1.000000, 0.968627, 0.972549},
	{"G2V", 1.000000, 0.960784, 0.949020},
	{"G4V", 1.000000, 0.945098, 0.898039},
	{"G5V", 1.000000, 0.956863, 0.917647},
	{"G6V", 1.000000, 0.956863, 0.921569},
	{"G7V", 1.000000, 0.956863, 0.921569},
	{"G8V", 1.000000, 0.929412, 0.870588},
	{"G9V", 1.000000, 0.937255, 0.866667},
	{"K0V", 1.000000, 0.933333, 0.866667},
	{"K1V", 1.000000, 0.878431, 0.737255},
	{"K2V", 1.000000, 0.890196, 0.768627},
	{"K3V", 1.000000, 0.870588, 0.764706},
	{"K4V", 1.000000, 0.847059, 0.709804},
	{"K5V", 1.000000, 0.823529, 0.631373},
	{"K7V", 1.000000, 0.780392, 0.556863},
	{"K8V", 1.000000, 0.819608, 0.682353},
	{"M0V", 1.000000, 0.764706, 0.545098},
	{"M1V", 1.000000, 0.800000, 0.556863},
	{"M2V", 1.000000, 0.768627, 0.513725},
	{"M3V", 1.000000, 0.807843, 0.505882},
	{"M4V", 1.000000, 0.788235, 0.498039},
	{"M5V", 1.000000, 0.800000, 0.435294},
	{"M6V", 1.000000, 0.764706, 0.439216},
	{"M8V", 1.000000, 0.776471, 0.427451},
	{"B1IV", 0.615686, 0.705882, 1.000000},
	{"B2IV", 0.623529, 0.701961, 1.000000},
	{"B3IV", 0.650980, 0.737255, 1.000000},
	{"B6IV", 0.686275, 0.760784, 1.000000},
	{"B7IV", 0.666667, 0.741176, 1.000000},
	{"B9IV", 0.705882, 0.772549, 1.000000},
	{"A0IV", 0.701961, 0.772549, 1.000000},
	{"A3IV", 0.745098, 0.803922, 1.000000},
	{"A4IV", 0.764706, 0.823529, 1.000000},
	{"A5IV", 0.831373, 0.862745, 1.000000},
	{"A7IV", 0.752941, 0.811765, 1.000000},
	{"A9IV", 0.878431, 0.890196, 1.000000},
	{"F0IV", 0.854902, 0.878431, 1.000000},
	{"F2IV", 0.890196, 0.901961, 1.000000},
	{"F3IV", 0.890196, 0.901961, 1.000000},
	{"F5IV", 0.945098, 0.937255, 1.000000},
	{"F7IV", 0.941176, 0.937255, 1.000000},
	{"F8IV", 1.000000, 0.988235, 0.992157},
	{"G0IV", 1.000000, 0.972549, 0.960784},
	{"G2IV", 1.000000, 0.956863, 0.949020},
	{"G3IV", 1.000000, 0.933333, 0.886275},
	{"G4IV", 1.000000, 0.960784, 0.933333},
	{"G5IV", 1.000000, 0.921569, 0.835294},
	{"G6IV", 1.000000, 0.949020, 0.917647},
	{"G7IV", 1.000000, 0.905882, 0.803922},
	{"G8IV", 1.000000, 0.913725, 0.827451},
	{"K0IV", 1.000000, 0.882353, 0.741176},
	{"K1IV", 1.000000, 0.847059, 0.670588},
	{"K2IV", 1.000000, 0.898039, 0.792157},
	{"K3IV", 1.000000, 0.858824, 0.654902},
	{"O7III", 0.619608, 0.694118, 1.000000},
	{"O8III", 0.615686, 0.698039, 1.000000},
	{"O9III", 0.619608, 0.694118, 1.000000},
	{"B0III", 0.619608, 0.694118, 1.000000},
	{"B1III", 0.619608, 0.694118, 1.000000},
	{"B2III", 0.623529, 0.705882, 1.000000},
	{"B3III", 0.639216, 0.733333, 1.000000},
	{"B5III", 0.658824, 0.741176, 1.000000},
	{"B7III", 0.670588, 0.749020, 1.000000},
	{"B9III", 0.698039, 0.764706, 1.000000},
	{"A0III", 0.737255, 0.803922, 1.000000},
	{"A3III", 0.741176, 0.796078, 1.000000},
	{"A5III", 0.792157, 0.843137, 1.000000},
	{"A6III", 0.819608, 0.858824, 1.000000},
	{"A7III", 0.823529, 0.858824, 1.000000},
	{"A8III", 0.819608, 0.858824, 1.000000},
	{"A9III", 0.819608, 0.858824, 1.000000},
	{"F0III", 0.835294, 0.870588, 1.000000},
	{"F2III", 0.945098, 0.945098, 1.000000},
	{"F4III", 0.945098, 0.941176, 1.000000},
	{"F5III", 0.949020, 0.941176, 1.000000},
	{"F6III", 0.945098, 0.941176, 1.000000},
	{"F7III", 0.945098, 0.941176, 1.000000},
	{"G0III", 1.000000, 0.949020, 0.913725},
	{"G1III", 1.000000, 0.952941, 0.913725},
	{"G2III", 1.000000, 0.952941, 0.913725},
	{"G3III", 1.000000, 0.952941, 0.913725},
	{"G4III", 1.000000, 0.952941, 0.913725},
	{"G5III", 1.000000, 0.925490, 0.827451},
	{"G6III", 1.000000, 0.925490, 0.843137},
	{"G8III", 1.000000, 0.905882, 0.780392},
	{"G9III", 1.000000, 0.905882, 0.768627},
	{"K0III", 1.000000, 0.890196, 0.745098},
	{"K1III", 1.000000, 0.874510, 0.709804},
	{"K2III", 1.000000, 0.866667, 0.686275},
	{"K3III", 1.000000, 0.847059, 0.654902},
	{"K4III", 1.000000, 0.827451, 0.572549},
	{"K5III", 1.000000, 0.800000, 0.541176},
	{"K7III", 1.000000, 0.815686, 0.556863},
	{"M0III", 1.000000, 0.796078, 0.517647},
	{"M1III", 1.000000, 0.784314, 0.474510},
	{"M2III", 1.000000, 0.776471, 0.462745},
	{"M3III", 1.000000, 0.784314, 0.466667},
	{"M4III", 1.000000, 0.807843, 0.498039},
	{"M5III", 1.000000, 0.772549, 0.486275},
	{"M6III", 1.000000, 0.698039, 0.474510},
	{"M7III", 1.000000, 0.647059, 0.380392},
	{"M8III", 1.000000, 0.654902, 0.380392},
	{"M9III", 1.000000, 0.913725, 0.603922},
	{"B2II", 0.647059, 0.752941, 1.000000},
	{"B5II", 0.686275, 0.764706, 1.000000},
	{"F0II", 0.796078, 0.850980, 1.000000},
	{"F2II", 0.898039, 0.913725, 1.000000},
	{"G5II", 1.000000, 0.921569, 0.796078},
	{"M3II", 1.000000, 0.788235, 0.466667},
	{"O9I", 0.643137, 0.725490, 1.000000},
	{"B0I", 0.631373, 0.741176, 1.000000},
	{"B1I", 0.658824, 0.756863, 1.000000},
	{"B2I", 0.694118, 0.768627, 1.000000},
	{"B3I", 0.686275, 0.760784, 1.000000},
	{"B4I", 0.733333, 0.796078, 1.000000},
	{"B5I", 0.701961, 0.792157, 1.000000},
	{"B6I", 0.749020, 0.811765, 1.000000},
	{"B7I", 0.764706, 0.819608, 1.000000},
	{"B8I", 0.713725, 0.807843, 1.000000},
	{"B9I", 0.800000, 0.847059, 1.000000},
	{"A0I", 0.733333, 0.807843, 1.000000},
	{"A1I", 0.839216, 0.874510, 1.000000},
	{"A2I", 0.780392, 0.839216, 1.000000},
	{"A5I", 0.874510, 0.898039, 1.000000},
	{"F0I", 0.792157, 0.843137, 1.000000},
	{"F2I", 0.956863, 0.952941, 1.000000},
	{"F5I", 0.858824, 0.882353, 1.000000},
	{"F8I", 1.000000, 0.988235, 0.968627},
	{"G0I", 1.000000, 0.937255, 0.858824},
	{"G2I", 1.000000, 0.925490, 0.803922},
	{"G3I", 1.000000, 0.905882, 0.796078},
	{"G5I", 1.000000, 0.901961, 0.717647},
	{"G8I", 1.000000, 0.862745, 0.654902},
	{"K0I", 1.000000, 0.866667, 0.709804},
	{"K1I", 1.000000, 0.862745, 0.694118},
	{"K2I", 1.000000, 0.827451, 0.529412},
	{"K3I", 1.000000, 0.800000, 0.501961},
	{"K4I", 1.000000, 0.788235, 0.462745},
	{"K5I", 1.000000, 0.819608, 0.603922},
	{"M0I", 1.000000, 0.800000, 0.560784},
	{"M1I", 1.000000, 0.792157, 0.541176},
	{"M2I", 1.000000, 0.756863, 0.407843},
	{"M3I", 1.000000, 0.752941, 0.462745},
	{"M4I", 1.000000, 0.725490, 0.407843},
	{"N", 1.000000, 0.615686, 0.000000},
	{"", 1.0, 1.0, 1.0},
};

static int print = 0;

int sky2kv4_sp_insert(struct adb_object *object, int offset, char *src);

static struct adb_schema_field star_fields[] = {
	adb_member("Name", "Name", struct sky2kv4_object,
		name, ADB_CTYPE_STRING, "", 0, NULL),
	adb_member("ID", "ID", struct sky2kv4_object,
		object.id, ADB_CTYPE_INT, "", 0, NULL),
	adb_gmember("RA Hours", "RAh", struct sky2kv4_object, \
		object.ra,  ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2, NULL),
	adb_gmember("RA Minutes", "RAm", struct sky2kv4_object,
		object.ra, ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1, NULL),
	adb_gmember("RA Seconds", "RAs", struct sky2kv4_object,
		object.ra, ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0, NULL),
	adb_gmember("DEC Degrees", "DEd", struct sky2kv4_object, \
		object.dec, ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3, NULL),
	adb_gmember("DEC Minutes", "DEm", struct sky2kv4_object,
		object.dec, ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2, NULL),
	adb_gmember("DEC Seconds", "DEs", struct sky2kv4_object,
		object.dec, ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1, NULL),
	adb_gmember("DEC sign", "DE-", struct sky2kv4_object,
		object.dec, ADB_CTYPE_SIGN, "", 0, NULL),
	adb_member("Visual Mag", "Vmag", struct sky2kv4_object,
		object.mag, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("sp", "Sp", struct sky2kv4_object,
		sp, ADB_CTYPE_STRING, "", 0, sky2kv4_sp_insert),
	adb_member("HD", "HD", struct sky2kv4_object,
		HD, ADB_CTYPE_INT, "", 0, NULL),
	adb_member("SAO", "SAO", struct sky2kv4_object,
		SAO, ADB_CTYPE_INT, "", 0, NULL),
	adb_member("PPM", "PPM", struct sky2kv4_object,
		PPM, ADB_CTYPE_INT, "", 0, NULL),
	adb_member("pmRA", "pmRA", struct sky2kv4_object,
		pmRA, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("pmDEC", "pmDEC", struct sky2kv4_object,
		pmDEC, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("Radial Vel", "RV", struct sky2kv4_object,
		RV, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("Binary sep", "sep", struct sky2kv4_object,
		sep, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("Dmag", "Dmag", struct sky2kv4_object,
		Dmag, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Orb Per", "orbPer", struct sky2kv4_object,
		orbPer, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("Pos Angle", "PA", struct sky2kv4_object,
		PA, ADB_CTYPE_SHORT, "", 0, NULL),
	adb_member("Obs Date", "date", struct sky2kv4_object,
		date, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("ID_A", "ID_A", struct sky2kv4_object,
		ID_A, ADB_CTYPE_INT, "", 0, NULL),
	adb_member("ID_B", "ID_B", struct sky2kv4_object,
		ID_B, ADB_CTYPE_INT, "", 0, NULL),
	adb_member("ID_C", "ID_C", struct sky2kv4_object,
		ID_C, ADB_CTYPE_INT, "", 0, NULL),
	adb_member("Var max mag", "magMax", struct sky2kv4_object,
		magMax, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Var min mag", "magMin", struct sky2kv4_object,
		magMin, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Var Amp", "varAmp", struct sky2kv4_object,
		varAmp, ADB_CTYPE_FLOAT, "", 0, NULL),
	adb_member("Var Period", "varPer", struct sky2kv4_object,
		varPer, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("Var Epoch", "varEpo", struct sky2kv4_object,
		varEpo, ADB_CTYPE_DOUBLE, "", 0, NULL),
	adb_member("Var Type", "varType", struct sky2kv4_object,
		varType, ADB_CTYPE_SHORT, "", 0, NULL),
};

static void sky2kv4_get_sp_index(struct sky2kv4_object *object, char *spect,
	int match)
{
	int m = match;

	for (; m > 0; m--) {
		for (object->type = 0; object->type < SP_NUM; object->type++) {
			if (!strncmp(sp[object->type].sp, spect, m))
				return;
		}
	}
}

int sky2kv4_sp_insert(struct adb_object *object, int offset, char *src)
{
	struct sky2kv4_object *so = (struct sky2kv4_object*)object;
	char *dest = (char*)object + offset;

	strcpy(dest, src);
	sky2kv4_get_sp_index(so, src, strlen(src));
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
	const struct sky2kv4_object **objects =
		(const struct sky2kv4_object **) _objects;
	int i;

	if (!print)
		return;

	for (i = 0; i < count; i++) {
		const struct sky2kv4_object *obj = objects[i];
		fprintf(stdout, "Obj:%s %ld RA: %f DEC: %f Mag %f Type %s HD %d\n",
			obj->name, obj->object.id, obj->object.ra * R2D,
			obj->object.dec * R2D, obj->object.mag,
			obj->sp, obj->HD);
		obj++;
	}
}

/*
 * Search for all stars with:-
 *
 * ((pmRA < 0.2 && pmRA > 0.05) ||
 * (pmDE < 0.2 && pmDE > 0.05)) ||
 * (RV < 40 && RV > 25)
 *
 * Uses Reverse Polish Notation to define search parameters
 */
static int search1(struct adb_db *db, int table_id)
{
	const struct adb_object **object;
	struct adb_search *search;
	struct adb_object_set *set;
	int err;

	fprintf(stdout, "Searching for high PM or RV objects\n");

	search = adb_search_new(db, table_id);
	if (!search)
		return -ENOMEM;

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	if (adb_search_add_comparator(search, "pmRA", ADB_COMP_LT, "0.4"))
		fprintf(stderr, "failed to add comp pmRA\n");
	if (adb_search_add_comparator(search, "pmRA", ADB_COMP_GT, "0.01"))
		fprintf(stderr, "failed to add comp pmRA\n");
	if (adb_search_add_operator(search, ADB_OP_AND))
		fprintf(stderr, "failed to add op and\n");

	if (adb_search_add_comparator(search, "pmDEC", ADB_COMP_LT, "0.4"))
		fprintf(stderr, "failed to add comp pmDEC\n");
	if (adb_search_add_comparator(search, "pmDEC", ADB_COMP_GT, "0.01"))
		fprintf(stderr, "failed to add comp pmDEC\n");
	if (adb_search_add_operator(search, ADB_OP_AND))
		fprintf(stderr, "failed to add op and\n");

	if (adb_search_add_comparator(search, "RV", ADB_COMP_LT, "40"))
		fprintf(stderr, "failed to add comp RV\n");
	if (adb_search_add_comparator(search, "RV", ADB_COMP_GT, "25"))
		fprintf(stderr, "failed to add comp RV\n");
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

/*
 * Search for all G type stars
 * Uses Wildcard "G5*" to match with Sp
 */
static int search2(struct adb_db *db, int table_id)
{
	const struct adb_object **object;
	struct adb_search *search;
	struct adb_object_set *set;
	int err;

	search = adb_search_new(db, table_id);
	if (!search)
		return -ENOMEM;

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	fprintf(stdout, "Searching for G5 class objects\n");
	if (adb_search_add_comparator(search, "Sp", ADB_COMP_EQ, "G5*"))
		fprintf(stderr, "failed to add comp G5*\n");
	if (adb_search_add_operator(search, ADB_OP_OR))
		fprintf(stderr, "failed to add op or\n");

	start_timer();
	err = adb_search_get_results(search, set, &object);
	if (err < 0) {
		fprintf(stderr, "Search init failed %d\n", err);
		adb_search_free(search);
		return err;
	}

	end_timer(adb_search_get_tests(search), 0);
	fprintf(stdout, "   Search found %d objects out of %d tests\n\n",
		adb_search_get_hits(search),
		adb_search_get_tests(search));

	search_print(object, adb_search_get_hits(search));

	adb_search_free(search);
	adb_table_set_free(set);
	return 0;
}

/*
 * Search for all M1 type stars
 * Uses Wildcard "M1" to match with Sp
 */
static int search3(struct adb_db *db, int table_id)
{
	const struct adb_object **object;
	struct adb_search *search;
	struct adb_object_set *set;
	int err;

	search = adb_search_new(db, table_id);
	if (!search)
		return -ENOMEM;

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	fprintf(stdout, "Searching for M1 class objects\n");

	if (adb_search_add_comparator(search, "Sp", ADB_COMP_EQ, "M1"))
		fprintf(stderr, "failed to add comp M1*\n");
	if (adb_search_add_operator(search, ADB_OP_OR))
		fprintf(stderr, "failed to add op or\n");

	start_timer();
	err = adb_search_get_results(search, set, &object);

	if (err < 0) {
		fprintf(stderr, "Search init failed %d\n", err);
		adb_search_free(search);
		return err;
	}

	end_timer(adb_search_get_tests(search), 0);
	fprintf(stdout, "   Found %d objects out of %d tests\n\n",
		adb_search_get_hits(search),
		adb_search_get_tests(search));

	search_print(object, adb_search_get_hits(search));

	adb_search_free(search);
	adb_table_set_free(set);
	return 0;
}

static void get_printf(const struct adb_object_head *object_head, int heads)
{
	const struct sky2kv4_object *obj;
	int i, j;

	if (!print)
		return;

	for (i = 0; i < heads; i++) {
		obj = object_head->objects;

		for (j = 0; j < object_head->count; j++) {
			fprintf(stdout, "Obj: %s %ld RA: %f DEC: %f Mag %f Type %s HD %d\n",
				obj->name, obj->object.id, obj->object.ra * R2D,
				obj->object.dec * R2D, obj->object.mag,
				obj->sp, obj->HD);
			obj++;
		}
		object_head++;
	}
}

static void object_printf(const struct adb_object *object)
{
	const struct sky2kv4_object *obj =
		(const struct sky2kv4_object *)object;

	fprintf(stdout, "Obj: %s %ld RA: %f DEC: %f Mag %f Type %s HD %d\n",
		obj->name, obj->object.id, obj->object.ra * R2D,
		obj->object.dec * R2D, obj->object.mag,
		obj->sp, obj->HD);
}

static void sobject_printf(struct adb_solve_object *sobject)
{
	const struct sky2kv4_object *obj =
		(const struct sky2kv4_object *)sobject->object;

	fprintf(stdout, "Plate object X %d Y %d ADU %d\n", sobject->pobject.x,
		sobject->pobject.y, sobject->pobject.adu);

	if (obj != NULL) {
		fprintf(stdout, " Obj: %s %ld RA: %f DEC: %f Mag %f Type %s HD %d\n",
			obj->name, obj->object.id, obj->object.ra * R2D,
			obj->object.dec * R2D, obj->object.mag,
			obj->sp, obj->HD);
	}

	fprintf(stdout, " Estimated plate RA: %f DEC: %f Mag: %f\n",
		sobject->ra * R2D, sobject->dec * R2D, sobject->mag);
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

	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, -2.0, 16.0);

	heads = adb_set_get_objects(set);
	count = adb_set_get_count(set);
	fprintf(stdout, " found %d object list heads %d objects\n\n", heads, count);

	get_printf(adb_set_get_head(set), heads);

	adb_table_set_free(set);
	return 0;
}

/*
 * Get all the objects brighter than mag 3 in the dataset.
 */
static int get2(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count, heads;

	fprintf(stdout, "Get all objects < mag 2\n");

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	/* we clip the db in terms of RA, DEC and mag, this is much faster
	 * than searching, but suffers from boundary overlaps i.e.
	 * we may get some objects fainter than mag 3 depending on the mag clip
	 * boundary. This is not a problem for rendering sky views.
	 */
	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, -2.0, 2.0);

	heads = adb_set_get_objects(set);
	count = adb_set_get_count(set);
	fprintf(stdout, " found %d object list heads %d objects\n\n", heads, count);

	get_printf(adb_set_get_head(set), heads);

	adb_table_set_free(set);
	return 0;
}

/*
 * Get all the objects brighter than mag 3 in the dataset.
 */
static int get3(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count, heads;

	fprintf(stdout, "Get all objects < mag 2, in radius 30 deg around 0,0\n");

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	/* we clip the db in terms of RA, DEC and mag, this is much faster
	 * than searching, but suffers from boundary overlaps i.e.
	 * we may get some objects fainter than mag 3 depending on the mag clip
	 * boundary. This is not a problem for rendering sky views.
	 */
	adb_table_set_constraints(set, 0.0, 0.0, 30.0 * D2R, -2.0, 2.0);

	heads = adb_set_get_objects(set);
	count = adb_set_get_count(set);
	fprintf(stdout, " found %d object list heads %d objects\n\n", heads, count);

	get_printf(adb_set_get_head(set), heads);

	adb_table_set_free(set);
	return 0;
}

static int get4(struct adb_db *db, int table_id)
{
	const struct adb_object *object, *objectn;
	struct adb_object_set *set;
	int found, id = 58977;

	set = adb_table_set_new(db, table_id);
	if (!set)
		return -ENOMEM;

	fprintf(stdout, "Get object HD 58977 \n");
	found = adb_set_get_object(set, &id, "HD", &object);
	if (found)
		object_printf(object);

	fprintf(stdout, "Get object 21alp Sc\n");
	found = adb_set_get_object(set, "21alp Sc", "Name", &object);
	if (found)
		object_printf(object);

	fprintf(stdout, "Get nearest object in db to 21alp Sc\n");
	objectn = adb_table_set_get_nearest_on_object(set, object);
	if (objectn)
		object_printf(objectn);

	fprintf(stdout, "Get nearest object in db to north pole\n");
	objectn = adb_table_set_get_nearest_on_pos(set, 0.0, M_PI_2);
	if (objectn)
		object_printf(objectn);

	objectn = adb_table_set_get_nearest_on_pos(set, 181.0 * D2R, 90.0 * D2R);
	if (objectn)
		object_printf(objectn);

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

	table_id = adb_table_import_new(db, "V", "109", "sky2kv4",
			"Vmag", -2.0, 12.0, ADB_IMPORT_INC);
	if (table_id < 0) {
		fprintf(stderr, "failed to create import table\n");
		ret = table_id;
		goto out;
	}

	ret = adb_table_import_schema(db, table_id, star_fields,
		adb_size(star_fields), sizeof(struct sky2kv4_object));
	if (ret < 0) {
		fprintf(stderr, "%s: failed to register object type\n", __func__);
		goto out;
	}

	/* Vmag is blank in some records in the dataset, so we use can Vder
	 * as an alternate field.
	 */
	ret = adb_table_import_field(db, table_id, "Vmag", "Vder", 0);
	if (ret < 0) {
		fprintf(stderr, "failed to add alt index\n");
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
	table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id < 0) {
		fprintf(stderr, "failed to create table\n");
		ret = table_id;
		goto table_err;
	}

	/* create a fast lookup hash on object HD number and name */
	adb_table_hash_key(db, table_id, "HD");
	adb_table_hash_key(db, table_id, "Name");

	/* we can now perform operations on the db data !!! */
	get1(db, table_id);
	get2(db, table_id);
	get3(db, table_id);
	search1(db, table_id);
	search2(db, table_id);
	search3(db, table_id);
	get4(db, table_id);

	/* were done with the db */
table_err:
	adb_table_close(db, table_id);
	adb_db_free(db);

lib_err:
	/* were now done with library */
	adb_close_library(lib);
	return ret;
}

/* Pleiades M45 plate objects */
static struct adb_pobject pobject[] = {
	{513, 434, 408725},  /* Alcyone 25 - RA 3h47m29s DEC 24d06m18s Mag 2.86 */
	{141, 545, 123643},  /* 1 Atlas 27  - RA 3h49m09s DEC 24d03m12s Mag 3.62 */
	{1049, 197, 128424}, /* P Electra 17 - RA 3h44m52s DEC 24d06m48s Mag 3.70 */
	{956, 517, 106906},  /* 2 Maia 20   - RA 3h45m49s DEC 24d22m03s Mag 3.86 */
	{682, 180, 98841},    /* 3 Morope 23 - RA 3h46m19s DEC 23d56m54s Mag 4.11 */
	{173, 623, 37537},   /* Plione 28 - RA 3h49m11s DEC 24d08m12s Mag 5.04 */
};

static int sky2k_solve(char *lib_dir)
{
	struct adb_library *lib;
	struct adb_db *db;
	struct adb_solve *solve;
	struct adb_object_set *set;
	struct adb_solve_solution *solution;
	int ret = 0, table_id, found, i, j;

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
	table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id < 0) {
		fprintf(stderr, "failed to create table\n");
		ret = table_id;
		goto table_err;
	}

	/* create a new set for solver */
	set = adb_table_set_new(db, table_id);
	if (!set)
		goto set_err;

	/* set sky area constraints for solver */
	adb_table_set_constraints(set, 57.0 * D2R, 24.0 * D2R, 5.0 * D2R, -2.0, 5.0);

	/* we can now solve images */
	solve = adb_solve_new(db, table_id);

	/* set magnitude and distance constraints */
	adb_solve_constraint(solve, ADB_CONSTRAINT_MAG, 6.0, -2.0);
	adb_solve_constraint(solve, ADB_CONSTRAINT_FOV, 0.1 * D2R, 2.0 * D2R);

	/* add plate/ccd objects */
	adb_solve_add_plate_object(solve, &pobject[0]);
	adb_solve_add_plate_object(solve, &pobject[1]);
	adb_solve_add_plate_object(solve, &pobject[2]);
	adb_solve_add_plate_object(solve, &pobject[3]);
	adb_solve_add_plate_object(solve, &pobject[4]);

	/* set image tolerances */
	adb_solve_set_magnitude_delta(solve, 0.25);
	adb_solve_set_distance_delta(solve, 5.0);
	adb_solve_set_pa_delta(solve, 2.0 * D2R);

	start_timer();
	found = adb_solve(solve, set, ADB_FIND_FIRST);
	end_timer(found, 0);
	fprintf(stdout, "found %d solutions\n", found);
	if (found == 0)
		goto out;

	for (i = 0; i < found; i++) {

		/* dump first set of objects */
		solution = adb_solve_get_solution(solve, i);

		/* set FoV and mag limits for single object searches */
		adb_solve_prep_solution(solution, 2.0, 8.0, table_id);

		fprintf(stdout, "Solution %d score %f\n", i,
				adb_solution_divergence(solution));

		/* get subsequent objects */
		ret = adb_solve_add_pobjects(solve, solution, pobject, 6);
		if (ret < 0)
			goto set_err;

		ret = adb_solve_get_objects(solve, solution);
		if (ret < 0)
			goto set_err;

		for (j = 0; j < 6; j++)
			sobject_printf(adb_solution_get_object(solution, j));
	}
out:
	/* were done with the db */
	adb_solve_free(solve);

	adb_table_set_free(set);

set_err:
	adb_table_close(db, table_id);
table_err:
	adb_db_free(db);

lib_err:
	/* were now done with library */
	adb_close_library(lib);
	return ret;
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
