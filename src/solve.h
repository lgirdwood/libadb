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

#ifndef __ADB_SOLVE_H
#define __ADB_SOLVE_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <libastrodb/db.h>
#include <libastrodb/solve.h>
#include <libastrodb/object.h>
#include "table.h"
#include "debug.h"

/* Turns on debug output for each solve stage */
//#define DEBUG

#define MIN_PLATE_OBJECTS	4
#define MAX_POTENTAL_MATCHES	256
#define MAX_ACTUAL_MATCHES	16
#define MAX_RT_SOLUTIONS	32
#define DELTA_MAG_COEFF		0.5
#define DELTA_DIST_COEFF	1.0
#define DELTA_PA_COEFF		1.0

struct tdata {
	double pattern_min;
	double pattern_max;
	double plate_actual;
};

struct target_object {
	struct adb_pobject *pobject;
	struct tdata distance;
	struct tdata pa;
	struct tdata pa_flip;
	struct tdata mag;
};

struct target_pattern {
	struct target_object primary;
	/* in order of brightness */
	struct target_object secondary[ADB_NUM_TARGETS - 1];
};

struct magnitude_range {
	/* plate object magnitude bounds */
	int start[MIN_PLATE_OBJECTS - 1];
	int end[MIN_PLATE_OBJECTS - 1];
};

struct solve_tolerance {
	/* tuning coefficients */
	double dist;
	double mag;
	double pa;
};

struct solve_constraint {
	/* solve constraints of plate/ccd */
	double min_ra;
	double max_ra;
	double min_dec;
	double max_dec;
	double min_mag;
	double max_mag;
	double min_fov;
	double max_fov;
	double min_area;
	double max_area;
	double min_JD;
	double max_JD;
	double min_pobjects;
	double max_pobjects;
};

struct adb_reference_object {
	const struct adb_object *object;
	struct adb_pobject pobject;
	int id;

	/* estimated magnitude */
	float mag_mean;
	float mag_sigma;

	/* estimated position */
	double dist_mean;
	double posn_sigma;

	int clip_mag;
	int clip_posn;
};

struct adb_solve_solution {
	struct adb_solve *solve;

	/* objects and plate objects used to find solution */
	const struct adb_object *object[ADB_NUM_TARGETS];
	struct adb_pobject soln_pobject[ADB_NUM_TARGETS];

	/* plate objects to solve */
	int num_pobjects;
	struct adb_pobject *pobjects;

	/* source object storage */
	struct adb_source_objects source;
	struct adb_object_set *set;
	struct adb_db *db;

	/* solution delta to db */
	struct solve_tolerance delta;
	double divergance;
	double rad_per_pix;
	int flip;

	/* solved objects from current table */
	struct adb_solve_object *solve_object;
	int num_solved_objects;
	int num_unsolved_objects;
	int total_objects;

	/* reference objects - total solved objects - can come from any table */
	struct adb_reference_object *ref;
	int num_ref_objects;
};

/* solver runtime data */
struct solve_runtime {
	struct adb_solve *solve;

	/* potential matches on magnitude */
	struct magnitude_range pot_magnitude;

	/* potential matches after magnitude and distance checks */
	struct adb_solve_solution pot_distance[MAX_POTENTAL_MATCHES];
	int num_pot_distance;
	int num_pot_distance_checked;

	/* potential matches after magnitude, distance and PA */
	struct adb_solve_solution pot_pa[MAX_POTENTAL_MATCHES];
	int num_pot_pa;

	/* target cluster */
	struct target_object soln_target[MIN_PLATE_OBJECTS];

#ifdef DEBUG
	int debug;
#endif
};

struct adb_solve {
	struct adb_db *db;
	struct adb_table *table;
	pthread_mutex_t mutex;

	/* plate objects */
	struct adb_pobject pobject[ADB_NUM_TARGETS];
	int num_plate_objects;
	int plate_idx_start;
	int plate_idx_end;

	struct solve_constraint constraint;

	/* target cluster */
	struct target_pattern target;

	/* source object set */
	struct adb_source_objects source;

	struct solve_tolerance tolerance;

	/* potential solutions from all runtimes */
	struct adb_solve_solution solution[MAX_RT_SOLUTIONS];
	int num_solutions;

	/* plate properties */
	int plate_width;
	int plate_height;
	double plate_ra;
	double plate_dec;

	/* loop exit */
	int exit;
	int progress;
};

#ifdef DEBUG
static const struct adb_object *dobj[4] = {NULL, NULL, NULL, NULL};
static const struct adb_object *sobj[4] = {NULL, NULL, NULL, NULL};

/* debug object designations for solution */
static const char *dnames[] = {
		"3992-746-1", "3992-942-1", "3992-193-1", "3996-1436-1",
};

/* debug object names for single objects */
static const char *snames[] = {
		"3992-648-1", "3992-645-1", "3992-349-1", "3992-882-1",
};

static void debug_init(struct adb_object_set *set)
{
	int i, found;

	fprintf(stdout, "!! enabling solve debug\n");
	adb_set_hash_key(set, ADB_FIELD_DESIGNATION);

	/* search for solution objects */
	for (i = 0; i < adb_size(dnames); i++) {
		found = adb_set_get_object(set, dnames[i], ADB_FIELD_DESIGNATION,
			&dobj[i]);
		if (found <= 0)
			fprintf(stderr, "can't find %s\n", dnames[i]);
	}

	/* search for single objects */
	for (i = 0; i < adb_size(snames); i++) {
			found = adb_set_get_object(set, snames[i], ADB_FIELD_DESIGNATION,
				&sobj[i]);
			if (found <= 0)
				fprintf(stderr, "can't find %s\n", snames[i]);
			else
				fprintf(stdout, "found %s mag %f\n",
					sobj[i]->designation, sobj[i]->mag);
		}
}

#define DOBJ_CHECK(stage, object) \
		do { \
			if (object == dobj[stage - 1] && runtime->debug == stage - 1) { \
				runtime->debug = stage; \
			} else { \
				if (runtime->debug == stage) \
					runtime->debug = stage - 1; \
			} \
		} while (0);
#define DOBJ_CHECK_DIST(stage, object1, object2, dist, min, max, num, i) \
		do { \
			if (stage < runtime->debug) \
				fprintf(stdout, "pass %d:object %s (%3.3f) --> %s (%3.3f) dist %f min %f max %f tests %d no %d\n", \
				stage, object1->designation, object1->mag, \
				object2->designation, object2->mag, dist, min, max, num, i); \
		} while (0)
#define DOBJ_LIST(stage, object1, object2, dist, i) \
		do { \
			if (stage <= runtime->debug) \
				fprintf(stdout, " check %d:object %s (%3.3f) --> %s (%3.3f) dist %f no %d\n", \
				stage, object1->designation, object1->mag, \
				object2->designation, object2->mag, dist, i); \
		} while (0)
#define DOBJ_PA_CHECK(object0, object1, object2, delta, min, max) \
		do { \
			if (object0 == dobj[0]) \
				fprintf(stdout, "check %s --> (%s) <-- %s is %3.3f min %3.3f max %3.3f\n", \
						object1->designation, object0->designation, object2->designation, \
						delta * R2D, min * R2D, max * R2D); \
		} while (0)
#define SOBJ_CHECK(object) \
		do { \
			if (object == sobj[0] || object == sobj[1] || \
				object == sobj[2] || object == sobj[3]) \
				runtime->debug = 1; \
			else \
				runtime->debug = 0;\
		} while (0);
#define SOBJ_CHECK_DIST(object, dist, min, max, num) \
		do { \
			if (runtime->debug) \
				fprintf(stdout, "%d: object %s (%3.3f) dist %f min %f max %f\n", \
				num, object->designation, object->mag, dist, min, max); \
		} while (0)
#define SOBJ_FOUND(object) \
		do { \
			if (runtime->debug) \
				fprintf(stdout, " *found %s\n", object->designation); \
			if (object == sobj[0]) \
				sobj[0] = NULL;\
			if (object == sobj[1]) \
				sobj[1] = NULL; \
			if (object == sobj[2]) \
				sobj[2] = NULL; \
			if (object == sobj[3]) \
				sobj[3] = NULL; \
		} while (0);
#define SOBJ_MAG(min, max) \
		fprintf(stdout, "mag min %f max %f\n", min, max);
#define SOBJ_CHECK_SET(object) \
		do { \
			if (object == sobj[0] || object == sobj[1] || \
				object == sobj[2] || object == sobj[3]) \
				fprintf(stdout, "set found: %s\n", object->designation); \
		} while (0);
#else
#define debug_init(set) while (0) {}
#define DOBJ_CHECK(stage, object)
#define DOBJ_CHECK_DIST(stage, object1, object2, dist, min, max, num, i)
#define DOBJ_LIST(stage, object1, object2, dist, i)
#define DOBJ_PA_CHECK(object0, object1, object2, delta, min, max)
#define SOBJ_CHECK(object)
#define SOBJ_CHECK_DIST(object, dist, min, max, num)
#define SOBJ_FOUND(object)
#define SOBJ_MAG(min, max)
#define SOBJ_CHECK_SET(object)
#endif

static inline double dmax(double a, double b)
{
	return a > b ? a : b;
}

static inline double dmin(double a, double b)
{
	return a < b ? a : b;
}

static inline double tri_max(double a, double b, double c)
{
	return dmax(dmax(a, b), c);
}

static inline double tri_avg(double a, double b, double c)
{
	return (a + b + c) / 3.0;
}

static inline double quad_max(double a, double b, double c, double d)
{
	return dmax(dmax(a, b), dmax(c, d));
}

static inline double tri_diff(double a, double b, double c)
{
	return tri_max(a, b, c);
}

static inline double quad_diff(double a, double b, double c, double d)
{
	return quad_max(a, b, c, d);
}

static inline float quad_avg(float a, float b, float c, float d)
{
	return (a + b + c + d) / 4.0;
}

/* put PA in correct quadrant */
static inline double equ_quad(double pa)
{
	double pa_ = pa;

	if (pa > M_PI * 2.0)
		pa_ = pa - (M_PI * 2.0);
	if  (pa < 0.0)
		pa_ = pa + (M_PI * 2.0);
	return pa_;
}
/* plate distance squared between primary and secondary */
double distance_get_plate(struct adb_pobject *primary,
	struct adb_pobject *secondary);

/* distance in between two adb_source_objects */
double distance_get_equ(const struct adb_object *o1,
	const struct adb_object *o2);

/* position angle in radians relative to plate north */
double pa_get_plate(struct adb_pobject *primary,
	struct adb_pobject *secondary);

/* position angle in radians */
double pa_get_equ(const struct adb_object *o1,
		const struct adb_object *o2);

/* ratio of magnitude from primary to secondary */
double mag_get_plate_diff(struct adb_pobject *primary,
	struct adb_pobject *secondary);

/* calculate the average difference between plate ADU values and solution
 * objects. Use this as basis for calculating magnitudes based on plate ADU.
 */
float mag_get_plate(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *primary);

/* compare pattern objects magnitude against source objects */
int mag_solve_object(struct solve_runtime *runtime,
		const struct adb_object *primary, int idx);

/* compare pattern objects magnitude against source objects */
int mag_solve_single_object(struct solve_runtime *runtime,
		struct adb_solve_solution *solution,
		struct adb_pobject *pobject);

int mag_object_cmp(const void *o1, const void *o2);

int pa_solve_object(struct solve_runtime *runtime,
	const struct adb_object *primary, int idx);

int pa_solve_single_object(struct solve_runtime *runtime,
	struct adb_solve_solution *solution);

/* check magnitude matched objects on pattern distance */
int distance_solve_object(struct solve_runtime *runtime,
	const struct adb_object *primary);

int distance_solve_single_object(struct solve_runtime *runtime,
	struct adb_solve_solution *solution);

int distance_solve_single_object_extended(struct solve_runtime *runtime,
	struct adb_solve_solution *solution);

void posn_plate_to_equ(struct adb_solve_solution *solution,
	struct adb_pobject *primary, double *ra_, double *dec_);
void posn_equ_to_plate(struct adb_solve_solution *solution,
	double ra, double dec, double *x_, double *y_);

/* add matching objects i,j,k to list of potentials on distance */
void target_add_match_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source,
	int i, int j, int k, double delta, double rad_per_pix);

/* add single object to list of potentials on distance */
void target_add_single_match_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source, double delta, int flip);

int distance_solve_single_object_extended(struct solve_runtime *runtime,
	struct adb_solve_solution *solution);

void target_add_single_match_extended(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source, double delta, int flip);

/* get a set of source objects to check the pattern against */
int target_prepare_source_objects(struct adb_solve *solve,
	struct adb_object_set *set, struct adb_source_objects *source);

int target_add_ref_object(struct adb_solve_solution *soln, int id,
	const struct adb_object *object, struct adb_pobject *pobject);

void target_create_pattern(struct adb_solve *solve);

/* create a pattern of plate targets and sort by magnitude */
void target_create_single(struct adb_solve *solve,
	struct adb_pobject *pobject,
	struct adb_solve_solution *solution,
	struct solve_runtime *runtime);

void mag_calc_plate_coefficients(struct adb_solve_solution *solution);

/* calculate the magnitude of all unsolved plate objects */
void mag_calc_unsolved_plate(struct adb_solve_solution *solution);

/* calculate the magnitude, mag delta mean and mag delta sigma
 * of a solved plate object */
void mag_calc_solved_plate(struct adb_solve_solution *solution);

void posn_clip_plate_coefficients(struct adb_solve_solution *solution);

/* calculate the position of unsolved plate objects */
void posn_calc_unsolved_plate(struct adb_solve_solution *solution);

/* calculate the position of solved plate objects */
void posn_calc_solved_plate(struct adb_solve_solution *solution);

void posn_equ_to_plate_fast(struct adb_solve_solution *solution,
	double ra, double dec, double *x_, double *y_);

void posn_plate_to_equ_fast(struct adb_solve_solution *solution,
	struct adb_pobject *primary, double *ra_, double *dec_);

#endif

#endif
