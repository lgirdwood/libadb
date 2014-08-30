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
 *  Copyright (C) 2013 - 2014 Liam Girdwood
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <pthread.h>

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
	double max_fov1k;
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

	/* tuning coefficients */
	double dist_coeff;
	double mag_delta;
	double pa_delta;

	/* potential solutions from all runtimes */
	struct adb_solve_solution solution[MAX_RT_SOLUTIONS];
	int num_solutions;
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
					sobj[i]->designation, sobj[i]->key);
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
				stage, object1->designation, object1->key, \
				object2->designation, object2->key, dist, min, max, num, i); \
		} while (0)
#define DOBJ_LIST(stage, object1, object2, dist, i) \
		do { \
			if (stage <= runtime->debug) \
				fprintf(stdout, " check %d:object %s (%3.3f) --> %s (%3.3f) dist %f no %d\n", \
				stage, object1->designation, object1->key, \
				object2->designation, object2->key, dist, i); \
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
				num, object->designation, object->key, dist, min, max); \
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

static int solution_cmp(const void *o1, const void *o2)
{
	const struct adb_solve_solution *p1 = o1, *p2 = o2;

	if (p1->divergance < p2->divergance)
		return 1;
	else if (p1->divergance > p2->divergance)
		return -1;
	else
		return 0;
}

static int plate_object_cmp(const void *o1, const void *o2)
{
	const struct adb_pobject *p1 = o1, *p2 = o2;

	if (p1->adu < p2->adu)
		return 1;
	else if (p1->adu > p2->adu)
		return -1;
	else
		return 0;
}

static int object_cmp(const void *o1, const void *o2)
{
	const struct adb_object *p1 = *(const struct adb_object **)o1,
		*p2 = *(const struct adb_object **)o2;

	if (p1->key < p2->key)
		return -1;
	else if (p1->key > p2->key)
		return 1;
	else
		return 0;
}

/* plate distance squared between primary and secondary */
static double get_plate_distance(struct adb_pobject *primary,
	struct adb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return sqrt((x * x) + (y * y));
}

/* distance in between two adb_source_objects */
static double get_equ_distance(const struct adb_object *o1,
	const struct adb_object *o2)
{
	double x,y,z;

	x = (cos(o1->dec) * sin (o2->dec))
		- (sin(o1->dec) * cos(o2->dec) *
		cos(o2->ra - o1->ra));
	y = cos(o2->dec) * sin(o2->ra - o1->ra);
	z = (sin(o1->dec) * sin(o2->dec)) +
		(cos(o1->dec) * cos(o2->dec) *
		cos(o2->ra - o1->ra));

	x = x * x;
	y = y * y;

	return atan2(sqrt(x + y), z);
}

/* ratio of magnitude from primary to secondary */
static double get_plate_mag_diff(struct adb_pobject *primary,
	struct adb_pobject *secondary)
{
	double s_adu = secondary->adu, p_adu = primary->adu;

	return -2.5 * log10(s_adu / p_adu);
}

/* calculate the average difference between plate ADU values and solution
 * objects. Use this as basis for calculating magnitudes based on plate ADU.
 */
static float get_plate_magnitude(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *primary)
{
	struct adb_reference_object *ref;
	double mag = 0.0;
	int i;

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		mag += ref->object->key +
			get_plate_mag_diff(&ref->pobject, primary);
	}

	return mag / solution->num_ref_objects;
}

/* compare plate object brightness against the brightness of all the reference
 * plate objects and then use the average differences to calculate magnitude */
static float get_plate_mag_mean(struct adb_solve_solution *solution,
	int target)
{
	struct adb_reference_object *ref, *reft;
	float mean = 0.0, plate_delta, db_delta;
	int count = 0, i;

	reft = &solution->ref[target];

	/* compare target against each solved object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		/* dont compare object against itself */
		if (ref->pobject.x == reft->pobject.x &&
			ref->pobject.y == reft->pobject.y)
			continue;

		/* ignore any object with mean difference > sigma */
		if (fabsf(ref->mean) > ref->sigma)
			continue;

		/* difference between plate objects */
		plate_delta = get_plate_mag_diff(&reft->pobject, &ref->pobject);

		/* difference between catalog objects */
		db_delta = ref->object->key - reft->object->key;

		/* calculate average difference */
		mean += db_delta - plate_delta;
		count++;
	}

	mean /= count;
	return mean;
}

/* compare plate object brightness against the brightness of all the refernce
 * plate objects and then use the average differences to calculate magnitude
 * sigma */
static float get_plate_mag_sigma(struct adb_solve_solution *solution,
	int target, float mean)
{
	struct adb_reference_object *ref, *reft;
	float plate_delta, db_delta, diff, sigma = 0.0;
	int count = 0, i;

	reft = &solution->ref[target];

	/* compare target against each solved object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		/* dont compare object against itself */
		if (ref->pobject.x == reft->pobject.x &&
			ref->pobject.y == reft->pobject.y)
			continue;

		/* mag difference between target and solved plate object */
		plate_delta = get_plate_mag_diff(&ref->pobject, &reft->pobject);

		/* delta between target object mag and detected object mag */
		db_delta = ref->object->key - reft->object->key;

		/* calc sigma */
		diff = db_delta - plate_delta;
		diff -= mean;
		diff *= diff;
		sigma += diff;
		count++;
	}

	sigma /= count;
	return sqrtf(sigma);
}

/* calculate the magnitude of an unsolved plate object */
static void calc_unsolved_plate_magnitude(struct adb_solve *solve,
	struct adb_solve_solution *solution, int target)
{
	struct adb_reference_object *ref;
	int i, count = 0;
	float mean = 0.0;

	/* compare plate object to reference objects */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		/* make sure solved object is close to catalog mag */
		/* otherwise reject it for magnitude calculation */
		if (fabsf(ref->mean) > ref->sigma)
			continue;

		/* calculate mean difference in magnitude */
		mean += ref->object->key +
			get_plate_mag_diff(&ref->pobject,
			&solution->solve_object[target].pobject);
		count++;
	}

	/* use mean magnitude as estimated plate magnitude */
	solution->solve_object[target].mag = mean / count;
}

/* calculate the magnitude of all unsolved plate objects */
static void calc_unsolved_plate_magnitudes(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	struct adb_solve_object *solve_object;
	int i;

	/* compare each unsolved object against reference objects */
	for (i = 0; i < solution->total_objects; i++) {

		solve_object = &solution->solve_object[i];

		/* skip if the object is solved */
		if (solve_object->object)
				continue;

		calc_unsolved_plate_magnitude(solve, solution, i);
	}
}

/* calculate the magnitude, mag mdelta mean and mag delta sigma
 * of a solved plate object */
static void calc_solved_plate_magnitude(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	int i;

	/* compare each detected object against reference objects */
	for (i = 0; i < solution->total_objects; i++) {

		/* ignore objects that are unsolved */
		if (solution->solve_object[i].object == NULL)
				continue;

		solution->solve_object[i].mean = get_plate_mag_mean(solution, i);

		solution->solve_object[i].mag =
			solution->solve_object[i].object->key +
			solution->solve_object[i].mean;

		solution->solve_object[i].sigma =
			get_plate_mag_sigma(solution, i, solution->solve_object[i].mean);
	}
}

static void calc_solved_plate_magnitudes(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	/* run magnitude comparison twice to rule out any stars that differ
	 * greatly from their source catalog magnitude values.
	 */
	calc_solved_plate_magnitude(solve, solution);
	calc_solved_plate_magnitude(solve, solution);
}

/* position angle in radians relative to plate north */
static double get_plate_pa(struct adb_pobject *primary,
	struct adb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return atan2(y, x);
}

/* quickly check if object p is within FoV of object s */
static inline int not_within_fov_fast(struct adb_solve *solve,
		const struct adb_object *p, const struct adb_object *s)
{
	double ra_diff = fabs(p->ra - s->ra);
	double dec_diff = s->dec +
			((p->dec - s->dec) / 2.0);

	/* check for large angles near 0 and 2.0 * M_PI */
	if (ra_diff > M_PI)
		ra_diff -= 2.0 * M_PI;

	if (cos(dec_diff) * ra_diff > solve->constraint.max_fov)
		return 1;
	if (fabs(p->dec - s->dec) > solve->constraint.max_fov)
		return 1;
	return 0;
}

/* position angle in radians */
static double get_equ_pa(const struct adb_object *o1,
		const struct adb_object *o2)
{
	double k, ra_delta, x, y;
	double sin_dec, cos_dec, sin_ra, cos_ra_delta;
	double sin_pdec, cos_pdec;

	/* pre-calc common terms */
	sin_dec = sin(o1->dec);
	cos_dec = cos(o1->dec);
	sin_ra = sin(o1->ra);
	ra_delta = o1->ra - o2->ra;
	cos_ra_delta = cos(ra_delta);
	cos_pdec = cos(o2->dec);
	sin_pdec = sin(o2->dec);

	/* calc scaling factor */
	k = 2.0 / (1.0 + sin_pdec * sin_ra +
		cos_pdec * cos_dec * cos_ra_delta);

	/* calc plate X, Y */
	x = k * (cos_dec * sin(ra_delta));
	y = k * (cos_pdec * sin_dec - sin_pdec * cos_dec * cos_ra_delta);

	return atan2(y, x);
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

/* convert plate coordinates to EQU coordinates by comparing plate object
 * with solved object plate coordinates. This method could probably be improved
 * by someone more familiar with the problem.
 */
static void plate_to_equ(struct adb_solve_solution *solution,
	const struct adb_object *o1, const struct adb_object *o2,
	struct adb_pobject *p1, struct adb_pobject *p2,
	struct adb_pobject *ptarget, double *ra_, double *dec_)
{
	double plate_pa, equ_pa, delta_pa;
	double plate_dist, equ_dist;
	double rad_per_1kpixel, target_pa, target_dist;
	double ra, dec, mid_dec;

	/* delta PA between plate and equ */
	plate_pa = equ_quad(get_plate_pa(p1, p2));
	equ_pa = equ_quad(get_equ_pa(o1, o2));
	delta_pa = plate_pa - equ_pa;

	/* delta distance between plate and equ */
	plate_dist = get_plate_distance(p1, p2);
	equ_dist = get_equ_distance(o1, o2);
	rad_per_1kpixel = equ_dist / plate_dist;

	/* EQU PA between object1 and target */
	target_pa = equ_quad(get_plate_pa(p1, ptarget));
	target_pa -= delta_pa;

	/* EQU dist between object1 and target */
	target_dist = get_plate_distance(p1, ptarget);
	target_dist *= rad_per_1kpixel;

	/* middle declination of line */
	mid_dec = o1->dec + ((o2->dec - o1->dec) / 2.0);

	/* Add line to object o1, reverse RA since RHS of plate increases X and
	 * RHS of sky is decreasing in RA.
	 */
	ra = -cos(target_pa) * target_dist / cos(mid_dec);
	dec = -sin(target_pa) * target_dist;

	*ra_ = ra + o1->ra;
	*dec_ = dec + o1->dec;
}

/* calculate the average difference between plate position values and solution
 * objects. Use this as basis for calculating RA,DEC based on plate x,y.
 */
static void get_plate_position(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *primary, double *ra_, double *dec_)
{
	struct adb_reference_object *ref, *refn;
	double ra_sum = 0.0, dec_sum = 0.0, ra, dec;
	int i;

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects - 1; i++) {
		ref = &solution->ref[i];
		refn = &solution->ref[i + 1];
		plate_to_equ(solution, ref->object, refn->object,
			&ref->pobject, &refn->pobject, primary, &ra, &dec);

		ra_sum += ra;
		dec_sum += dec;
	}

	/* get final RA, DEC */
	ref = &solution->ref[solution->num_ref_objects - 1];
	refn = &solution->ref[0];
	plate_to_equ(solution, ref->object, refn->object,
			&ref->pobject, &refn->pobject, primary, &ra, &dec);

	ra_sum += ra;
	dec_sum += dec;

	*ra_ = ra_sum / solution->num_ref_objects;
	*dec_ = dec_sum / solution->num_ref_objects;
}

/* add a reference object to the solution */
static int add_reference_object(struct adb_solve_solution *soln,
	const struct adb_object *object, struct adb_pobject *pobject)
{
	struct adb_reference_object *ref;
	int i;

	/* first check to see if object is already present */
	for (i = 0; i < soln->num_ref_objects; i++) {
		ref = &soln->ref[i];
		if (ref->object == object)
			return 0;
	}

	soln->ref = realloc(soln->ref,
		sizeof(struct adb_reference_object) * (soln->num_ref_objects + 1));
	if (soln->ref == NULL)
		return -ENOMEM;

	soln->ref[soln->num_ref_objects].object = object;
	soln->ref[soln->num_ref_objects++].pobject = *pobject;
	return 0;
}

/* calculate object pattern variables to match against source objects */
static void create_pattern_object(struct adb_solve *solve, int target,
	struct adb_pobject *primary, struct adb_pobject *secondary)
{
	struct target_object *t = &solve->target.secondary[target];

	t->pobject = secondary;

	/* calculate plate distance and min,max to primary */
	t->distance.plate_actual = get_plate_distance(primary, secondary);
	t->distance.pattern_min =
		t->distance.plate_actual - solve->dist_coeff;
	t->distance.pattern_max =
		t->distance.plate_actual + solve->dist_coeff;

	/* calculate plate magnitude and min,max to primary */
	t->mag.plate_actual = get_plate_mag_diff(primary, secondary);
	t->mag.pattern_min = t->mag.plate_actual - solve->mag_delta;
	t->mag.pattern_max = t->mag.plate_actual + solve->mag_delta;

	/* calculate plate position angle to primary */
	t->pa.plate_actual = get_plate_pa(primary, secondary);
}

/* create a pattern of plate targets and sort by magnitude */
static void create_target_pattern(struct adb_solve *solve)
{
	struct target_object *t0, *t1, *t2;
	int i, j;

	/* sort plate object on brightness */
	qsort(solve->pobject, solve->num_plate_objects,
		sizeof(struct adb_pobject), plate_object_cmp);

	/* create target pattern */
	for (i = solve->plate_idx_start + 1, j = 0; i < solve->plate_idx_end; i++, j++)
		create_pattern_object(solve, j,
			&solve->pobject[solve->plate_idx_start], &solve->pobject[i]);

	/* work out PA deltas */
	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];

	t0->pa.pattern_min = t1->pa.plate_actual - t0->pa.plate_actual;
	if (t0->pa.pattern_min < 0.0)
		t0->pa.pattern_min += 2.0 * M_PI;
	t0->pa.pattern_max = t0->pa.pattern_min + solve->pa_delta;
	t0->pa.pattern_min -= solve->pa_delta;

	t1->pa.pattern_min = t2->pa.plate_actual - t1->pa.plate_actual;
	if (t1->pa.pattern_min < 0.0)
		t1->pa.pattern_min += 2.0 * M_PI;
	t1->pa.pattern_max = t1->pa.pattern_min + solve->pa_delta;
	t1->pa.pattern_min -= solve->pa_delta;

	t2->pa.pattern_min = t0->pa.plate_actual - t2->pa.plate_actual;
	if (t2->pa.pattern_min < 0.0)
		t2->pa.pattern_min += 2.0 * M_PI;
	t2->pa.pattern_max = t2->pa.pattern_min + solve->pa_delta;
	t2->pa.pattern_min -= solve->pa_delta;

	t0->pa.plate_actual = t0->pa.pattern_min +
			(t0->pa.pattern_max - t0->pa.pattern_min) / 2.0;
	if (t0->pa.plate_actual < 0.0)
			t0->pa.plate_actual += 2.0 * M_PI;
	t1->pa.plate_actual = t1->pa.pattern_min +
			(t1->pa.pattern_max - t1->pa.pattern_min) / 2.0;
	if (t1->pa.plate_actual < 0.0)
			t1->pa.plate_actual += 2.0 * M_PI;
	t2->pa.plate_actual = t2->pa.pattern_min +
			(t2->pa.pattern_max - t2->pa.pattern_min) / 2.0;
	if (t2->pa.plate_actual < 0.0)
			t2->pa.plate_actual += 2.0 * M_PI;

	/* calculate flip PA deltas where image can be fliped */
	t0->pa_flip.plate_actual = 2.0 * M_PI - t0->pa.plate_actual;
	t0->pa_flip.pattern_min = 2.0 * M_PI - t0->pa.pattern_min;
	t0->pa_flip.pattern_max = 2.0 * M_PI - t0->pa.pattern_max;

	t1->pa_flip.plate_actual = 2.0 * M_PI - t1->pa.plate_actual;
	t1->pa_flip.pattern_min = 2.0 * M_PI - t1->pa.pattern_min;
	t1->pa_flip.pattern_max = 2.0 * M_PI - t1->pa.pattern_max;

	t2->pa_flip.plate_actual = 2.0 * M_PI - t2->pa.plate_actual;
	t2->pa_flip.pattern_min = 2.0 * M_PI - t2->pa.pattern_min;
	t2->pa_flip.pattern_max = 2.0 * M_PI - t2->pa.pattern_max;
}

/* calculate object pattern variables to match against source objects */
static void create_single_object(struct adb_solve *solve, int target,
	struct adb_pobject *primary, struct adb_pobject *secondary,
	struct solve_runtime *runtime, struct adb_solve_solution *solution)
{
	struct target_object *t = &runtime->soln_target[target];

	t->pobject = secondary;

	/* calculate plate distance and min,max to primary */
	t->distance.plate_actual = get_plate_distance(primary, secondary);
	t->distance.pattern_min = solution->rad_per_1kpix *
		(t->distance.plate_actual - solve->dist_coeff);
	t->distance.pattern_max = solution->rad_per_1kpix *
		(t->distance.plate_actual + solve->dist_coeff);

	/* calculate plate magnitude and min,max to primary */
	t->mag.plate_actual = get_plate_mag_diff(primary, secondary);
	t->mag.pattern_min = t->mag.plate_actual - solution->delta_magnitude;
	t->mag.pattern_max = t->mag.plate_actual + solution->delta_magnitude;

	/* calculate plate position angle to primary */
	t->pa.plate_actual = get_plate_pa(primary, secondary);
}

/* create a pattern of plate targets and sort by magnitude */
static void create_target_single(struct adb_solve *solve,
	struct adb_pobject *pobject,
	struct adb_solve_solution *solution,
	struct solve_runtime *runtime)
{
	struct target_object *t0, *t1, *t2, *t3;
	int i, j;

	/* create target pattern - use pobject as primary  */
	for (i = solve->plate_idx_start, j = 0; i < solve->plate_idx_end; i++, j++)
		create_single_object(solve, j, pobject,
			&solve->pobject[i], runtime, solution);

	/* work out PA deltas */
	t0 = &runtime->soln_target[0];
	t1 = &runtime->soln_target[1];
	t2 = &runtime->soln_target[2];
	t3 = &runtime->soln_target[3];

	t0->pa.pattern_min = t1->pa.plate_actual - t0->pa.plate_actual;
	if (t0->pa.pattern_min < 0.0)
		t0->pa.pattern_min += 2.0 * M_PI;
	t0->pa.pattern_max = t0->pa.pattern_min + solve->pa_delta;
	t0->pa.pattern_min -= solve->pa_delta;

	t1->pa.pattern_min = t2->pa.plate_actual - t1->pa.plate_actual;
	if (t1->pa.pattern_min < 0.0)
		t1->pa.pattern_min += 2.0 * M_PI;
	t1->pa.pattern_max = t1->pa.pattern_min + solve->pa_delta;
	t1->pa.pattern_min -= solve->pa_delta;

	t2->pa.pattern_min = t3->pa.plate_actual - t2->pa.plate_actual;
	if (t2->pa.pattern_min < 0.0)
		t2->pa.pattern_min += 2.0 * M_PI;
	t2->pa.pattern_max = t2->pa.pattern_min + solve->pa_delta;
	t2->pa.pattern_min -= solve->pa_delta;

	t3->pa.pattern_min = t0->pa.plate_actual - t3->pa.plate_actual;
	if (t3->pa.pattern_min < 0.0)
		t3->pa.pattern_min += 2.0 * M_PI;
	t3->pa.pattern_max = t3->pa.pattern_min + solve->pa_delta;
	t3->pa.pattern_min -= solve->pa_delta;

	t0->pa.plate_actual = t0->pa.pattern_min +
			(t0->pa.pattern_max - t0->pa.pattern_min) / 2.0;
	if (t0->pa.plate_actual < 0.0)
			t0->pa.plate_actual += 2.0 * M_PI;
	t1->pa.plate_actual = t1->pa.pattern_min +
			(t1->pa.pattern_max - t1->pa.pattern_min) / 2.0;
	if (t1->pa.plate_actual < 0.0)
			t1->pa.plate_actual += 2.0 * M_PI;
	t2->pa.plate_actual = t2->pa.pattern_min +
			(t2->pa.pattern_max - t2->pa.pattern_min) / 2.0;
	if (t2->pa.plate_actual < 0.0)
			t2->pa.plate_actual += 2.0 * M_PI;
	t3->pa.plate_actual = t3->pa.pattern_min +
			(t3->pa.pattern_max - t3->pa.pattern_min) / 2.0;
	if (t3->pa.plate_actual < 0.0)
			t3->pa.plate_actual += 2.0 * M_PI;

	/* calculate flip PA deltas where image can be fliped */
	t0->pa_flip.plate_actual = 2.0 * M_PI - t0->pa.plate_actual;
	t0->pa_flip.pattern_min = 2.0 * M_PI - t0->pa.pattern_min;
	t0->pa_flip.pattern_max = 2.0 * M_PI - t0->pa.pattern_max;

	t1->pa_flip.plate_actual = 2.0 * M_PI - t1->pa.plate_actual;
	t1->pa_flip.pattern_min = 2.0 * M_PI - t1->pa.pattern_min;
	t1->pa_flip.pattern_max = 2.0 * M_PI - t1->pa.pattern_max;

	t2->pa_flip.plate_actual = 2.0 * M_PI - t2->pa.plate_actual;
	t2->pa_flip.pattern_min = 2.0 * M_PI - t2->pa.pattern_min;
	t2->pa_flip.pattern_max = 2.0 * M_PI - t2->pa.pattern_max;

	t3->pa_flip.plate_actual = 2.0 * M_PI - t3->pa.plate_actual;
	t3->pa_flip.pattern_min = 2.0 * M_PI - t3->pa.pattern_min;
	t3->pa_flip.pattern_max = 2.0 * M_PI - t3->pa.pattern_max;
}

/* add matching objects i,j,k to list of potentials on distance */
static void add_pot_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source,
	int i, int j, int k, double delta, double rad_per_1kpix)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];

	p->object[0] = primary;
	p->object[1] = source->objects[i];
	p->object[2] = source->objects[j];
	p->object[3] = source->objects[k];
	p->pobject[0] = solve->pobject[solve->plate_idx_start];
	p->pobject[1] = solve->pobject[solve->plate_idx_start + 1];
	p->pobject[2] = solve->pobject[solve->plate_idx_start + 2];
	p->pobject[3] = solve->pobject[solve->plate_idx_start + 3];
	p->delta_distance = delta;
	p->rad_per_1kpix = rad_per_1kpix;
	runtime->num_pot_distance++;
}

/* add single object to list of potentials on distance */
static void add_single_pot_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source, double delta, int flip)
{
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];
	p->delta_distance = delta;
	p->object[0] = primary;
	//p->pobject[0] = solve->pobject[solve->plate_idx_start];
	p->flip = flip;
	runtime->num_pot_distance++;
}

/* get a set of source objects to check the pattern against */
static int build_and_sort_object_set(struct adb_solve *solve,
	struct adb_object_set *set, struct adb_source_objects *source)
{
	int object_heads, i, j, count = 0;

	/* get object heads */
	object_heads = adb_set_get_objects(set);
	if (object_heads <= 0)
		return object_heads;

	debug_init(set);

	/* allocate space for adb_source_objects */
	source->objects = calloc(set->count, sizeof(struct adb_object *));
	if (source->objects == NULL)
		return -ENOMEM;

	/* copy adb_source_objects ptrs from head set */
	for (i = 0; i < object_heads; i++) {

		const void *object = set->object_heads[i].objects;

		/* copy individual objects */
		for (j = 0; j < set->object_heads[i].count; j++)  {
			const struct adb_object *o = object;

			/* dont copy objects outside mag limits */
			if (o->key <= solve->constraint.min_mag &&
				o->key >= solve->constraint.max_mag)
				source->objects[count++] = object;
			object += solve->table->object.bytes;
		}
	}

	/* sort adb_source_objects on magnitude */
	qsort(source->objects, count, sizeof(struct adb_object *),
		object_cmp);
	source->num_objects = count;

	return count;
}

/* binary search the set for magnitude head */
static int bsearch_head(const struct adb_object **adb_source_objects,
	double value, int start, int end, int idx)
{
	const struct adb_object *object = adb_source_objects[idx];

	/* search complete */
	if (end - start < 2)
		return idx;

	/* narrow search */
	if (object->key > value)
		return bsearch_head(adb_source_objects, value, start, idx - 1,
				start + ((idx - start) / 2));
	else if (object->key < value)
		return bsearch_head(adb_source_objects, value, idx + 1, end,
				idx + ((end - idx) / 2));
	else
		return idx;
}

/* get first object with magnitude >= Vmag */
static int object_get_first_on_mag(struct adb_source_objects *source,
	double vmag, int start_idx)
{
	const struct adb_object *object;
	int idx;

	/* find one of the first adb_source_objects >= vmag  */
	idx = bsearch_head(source->objects, vmag, start_idx,
		source->num_objects - 1, (source->num_objects - 1) / 2);
	object = source->objects[idx];

	/* make sure the object is first in the array amongst equals */
	if (object->key < vmag) {

		/* make sure we return the idx of the object with equal vmag */
		for (idx++; idx < source->num_objects; idx++) {
			object = source->objects[idx];

			if (object->key >= vmag)
				return idx - 1;
		}
	} else {
		/* make sure we return the idx of the object with equal vmag */
		for (idx--; idx > start_idx; idx--) {
			object = source->objects[idx];

			if (object->key < vmag)
				return idx + 1;
		}
	}

	/* not found */
	return 0;
}

/* binary search the set for magnitude tail */
static int bsearch_tail(const struct adb_object **adb_source_objects,
	double value, int start, int end, int idx)
{
	const struct adb_object *object = adb_source_objects[idx];

	/* search complete */
	if (end - start < 2)
		return idx;

	/* narrow search */
	if (object->key > value)
		return bsearch_tail(adb_source_objects, value, start, idx - 1,
				start + ((idx - start) / 2));
	else if (object->key < value)
		return bsearch_tail(adb_source_objects, value, idx + 1, end,
				idx + ((end - idx) / 2));
	else
		return idx;
}

/* get last object with magnitude <= Vmag */
static int object_get_last_with_mag(struct adb_source_objects *source,
		double vmag, int start_idx)
{
	const struct adb_object *object;
	int idx;

	/* find one of the last adb_source_objects <= vmag  */
	idx = bsearch_tail(source->objects, vmag, start_idx,
		source->num_objects - 1, (source->num_objects - 1) / 2);
	object = source->objects[idx];

	/* make sure the object is last in the array amongst equals */
	if (object->key > vmag) {

		/* make sure we return the idx of the object with equal vmag */
		for (idx--; idx > start_idx; idx--) {
			object = source->objects[idx];

			if (object->key <= vmag)
				return idx + 1;
		}
	} else {
		/* make sure we return the idx of the object with equal vmag */
		for (idx++; idx < source->num_objects; idx++) {
			object = source->objects[idx];

			if (object->key > vmag)
				return idx - 1;
		}
	}

	/* not found */
	return source->num_objects - 1;
}

/* compare pattern objects magnitude against source objects */
static int solve_object_on_magnitude(struct solve_runtime *runtime,
		const struct adb_object *primary, int idx)
{
	struct magnitude_range *range = &runtime->pot_magnitude;
	struct adb_solve *solve = runtime->solve;
	struct target_object *t = &solve->target.secondary[idx];
	struct adb_source_objects *source = &solve->source;
	int start, end, pos;

	/* get search start position */
	pos = object_get_first_on_mag(source,
		primary->key - solve->mag_delta, 0);

	/* get start and end indices for secondary vmag */
	start = object_get_first_on_mag(source,
		t->mag.pattern_min + primary->key, pos);
	end = object_get_last_with_mag(source,
		t->mag.pattern_max + primary->key, pos);

	/* both out of range */
	if (start == end)
		return 0;

	range->end[idx] = end;

	/* no object */
	if (range->end[idx] < start)
		return 0;

	/* is start out of range */
	range->start[idx] = start;

	/* return number of candidate adb_source_objects based on vmag */
	return range->end[idx] - range->start[idx];
}

/* compare pattern objects magnitude against source objects */
static int solve_single_object_on_magnitude(struct solve_runtime *runtime,
		struct adb_solve_solution *solution,
		struct adb_pobject *pobject)
{
	struct magnitude_range *range = &runtime->pot_magnitude;
	struct adb_solve *solve = runtime->solve;
	struct adb_source_objects *source = &solution->source;
	int start, end;
	float mag_min, mag_max, plate_mag;

	plate_mag = get_plate_magnitude(solve, solution, pobject);

	mag_min = plate_mag - solution->delta_magnitude;
	mag_max = plate_mag + solution->delta_magnitude;

	/* get start and end indices for secondary vmag */
	start = object_get_first_on_mag(source,
			mag_min - solution->delta_magnitude, 0);

	end = object_get_last_with_mag(source,
			mag_max + solution->delta_magnitude, 0);

	SOBJ_MAG(mag_min - solution->delta_magnitude,
			mag_max + solution->delta_magnitude);

	/* both out of range */
	if (start == end)
		return 0;

	range->end[0] = end;

	/* no object */
	if (range->end[0] < start)
		return 0;

	/* is start out of range */
	range->start[0] = start;

	/* return number of candidate adb_source_objects based on vmag */
	return range->end[0] - range->start[0];
}

static int solve_single_object_on_distance(struct solve_runtime *runtime,
	struct adb_solve_solution *solution)
{
	const struct adb_object *s;
	struct magnitude_range *range = &runtime->pot_magnitude;
	int i, count = 0;
	double distance, diff[4], diverge;

	/* check distance ratio for each matching candidate against targets */
	runtime->num_pot_distance = 0;

	/* check t0 candidates */
	for (i = range->start[0]; i < range->end[0]; i++) {

		s = solution->source.objects[i];

		SOBJ_CHECK(s);

		/* plate object to candidate object 0 */
		distance = get_equ_distance(solution->object[0], s) * 1000.0;

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[0].distance.pattern_min,
			runtime->soln_target[0].distance.pattern_max, 1);

		if (distance > runtime->soln_target[0].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[0].distance.pattern_min)
			continue;
		diff[0] = distance / runtime->soln_target[0].distance.plate_actual;

		/* plate object to candidate object 1 */
		distance = get_equ_distance(solution->object[1], s) * 1000.0;

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[1].distance.pattern_min,
			runtime->soln_target[1].distance.pattern_max, 2);

		if (distance > runtime->soln_target[1].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[1].distance.pattern_min)
			continue;
		diff[1] = distance / runtime->soln_target[1].distance.plate_actual;

		/* plate object to candidate object 2 */
		distance = get_equ_distance(solution->object[2], s) * 1000.0;

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[2].distance.pattern_min,
			runtime->soln_target[2].distance.pattern_max, 3);

		if (distance > runtime->soln_target[2].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[2].distance.pattern_min)
			continue;
		diff[2] = distance / runtime->soln_target[2].distance.plate_actual;

		/* plate object to candidate object 3 */
		distance = get_equ_distance(solution->object[3], s) * 1000.0;

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[3].distance.pattern_min,
			runtime->soln_target[3].distance.pattern_max, 4);

		if (distance > runtime->soln_target[3].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[3].distance.pattern_min)
			continue;
		diff[3] = distance / runtime->soln_target[3].distance.plate_actual;

		diverge = quad_diff(diff[0], diff[1], diff[2], diff[3]);

		SOBJ_FOUND(s);

		add_single_pot_on_distance(runtime, s, &solution->source,
				diverge, solution->flip);
		count++;
	}

	return count;
}

/* check magnitude matched objects on pattern distance */
static int solve_object_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary)
{
	const struct adb_object *s[3];
	struct target_object *t0, *t1, *t2;
	struct adb_solve *solve = runtime->solve;
	struct magnitude_range *range = &runtime->pot_magnitude;
	int i, j, k, count = 0;
	double distance0, distance1, distance2, rad_per_1kpixel;
	double t1_min, t1_max, t2_max, t2_min;

	/* check distance ratio for each matching candidate against targets */
	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];
	runtime->num_pot_distance = 0;

	DOBJ_CHECK(1, primary);

	/* check t0 candidates */
	for (i = range->start[0]; i < range->end[0]; i++) {

		s[0] = solve->source.objects[i];
		if (s[0] == primary)
			continue;

		DOBJ_CHECK(2, s[0]);

		if (not_within_fov_fast(solve, primary, s[0]))
			continue;

		distance0 = get_equ_distance(primary, s[0]) * 1000.0;

		/* rule out any distances > FOV */
		if (distance0 > solve->constraint.max_fov1k)
			continue;

		rad_per_1kpixel = distance0 / t0->distance.plate_actual;

		/* use ratio based on t0 <-> primary distance for t1 */
		t1_min = t1->distance.pattern_min * rad_per_1kpixel;
		t1_max = t1->distance.pattern_max * rad_per_1kpixel;

		DOBJ_CHECK_DIST(1, primary, s[0], distance0, 0.0, 0.0,
				range->end[0] - range->start[0], i);

		/* check each t1 candidates against t0 <-> primary distance ratio */
		for (j = range->start[1]; j < range->end[1]; j++) {

			s[1] = solve->source.objects[j];
			if (s[0] == s[1] || s[1] == primary)
				continue;

			DOBJ_CHECK(3, s[1]);

			if (not_within_fov_fast(solve, primary, s[1]))
				continue;

			distance1 = get_equ_distance(primary, s[1]) * 1000.0;

			DOBJ_LIST(2, primary, s[1], distance1, j);

			/* rule out any distances > FOV */
			if (distance1 > solve->constraint.max_fov1k)
				continue;

			DOBJ_CHECK_DIST(2, primary, s[1], distance1, t1_min, t1_max,
					range->end[1] - range->start[1], j);

			/* is this t1 candidate within t0 primary ratio */
			if (distance1 >= t1_min && distance1 <= t1_max) {

				t2_min = t2->distance.pattern_min * rad_per_1kpixel;
				t2_max = t2->distance.pattern_max * rad_per_1kpixel;

				/* check t2 candidates */
				for (k = range->start[2]; k < range->end[2]; k++) {

					s[2] = solve->source.objects[k];
					if (s[0] == s[2] || s[1] == s[2] || s[2] == primary)
						continue;

					DOBJ_CHECK(4, s[2]);

					if (not_within_fov_fast(solve, primary, s[2]))
						continue;

					distance2 = get_equ_distance(primary, s[2]) * 1000.0;

					DOBJ_LIST(3, primary, s[2], distance2, k);

					/* rule out any distances > FOV */
					if (distance2 > solve->constraint.max_fov1k)
						continue;

					DOBJ_CHECK_DIST(3, primary, s[2], distance2, t2_min, t2_max, 0, k);

					if (distance2 >= t2_min && distance2 <= t2_max) {

						double ratio1, ratio2, delta;

						DOBJ_CHECK_DIST(4, primary, s[2], distance2, t2_min, t2_max, 0, 0);

						ratio1 = distance1 / t1->distance.plate_actual;
						ratio2 = distance2 / t2->distance.plate_actual;

						delta = tri_diff(rad_per_1kpixel, ratio1, ratio2);

						add_pot_on_distance(runtime, primary, &solve->source,
							i, j, k, delta, tri_avg(rad_per_1kpixel, ratio1, ratio2));
						count++;
					}
				}
			}
		}
	}

	return count;
}

/* add matching cluster to list of potentials */
static void add_pot_on_pa(struct solve_runtime *runtime,
		struct adb_solve_solution *p, double delta)
{
	if (runtime->num_pot_pa >= MAX_ACTUAL_MATCHES)
		return;

	p->delta_pa = delta;
	runtime->pot_pa[runtime->num_pot_pa++] = *p;
}

static inline int pa_valid(struct target_object *t, double delta, int flip)
{
	if (flip) {
		/* min/max swapped for flipped */
		if (delta >= t->pa_flip.pattern_max && delta <= t->pa_flip.pattern_min)
			return 1;
		else
			return 0;
	} else {
		if (delta >= t->pa.pattern_min && delta <= t->pa.pattern_max)
			return 1;
		else
			return 0;
	}
}

static int solve_object_on_pa(struct solve_runtime *runtime,
	const struct adb_object *primary, int idx)
{
	struct adb_solve_solution *p;
	struct adb_solve *solve = runtime->solve;
	struct target_object *t0, *t1, *t2;
	double pa1, pa2, pa3, pa_delta12, pa_delta23, pa_delta31, delta;
	int i, count = 0;

	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];

	/* for each potential cluster */
	for (i = runtime->num_pot_distance_checked;
		i < runtime->num_pot_distance; i++) {

		p = &runtime->pot_distance[i];
		p->flip = 0;

		/* check PA for primary to each secondary object */
		pa1 = get_equ_pa(p->object[0], p->object[1]);
		pa2 = get_equ_pa(p->object[0], p->object[2]);
		pa_delta12 = pa1 - pa2;
		if (pa_delta12 < 0.0)
			pa_delta12 += 2.0 * M_PI;

		DOBJ_PA_CHECK(p->object[0], p->object[1], p->object[2], pa_delta12,
					t0->pa.pattern_min, t0->pa.pattern_max);

		if (!pa_valid(t0, pa_delta12, 0)) {
			DOBJ_PA_CHECK(p->object[0], p->object[1], p->object[2], pa_delta12,
								t0->pa_flip.pattern_min, t0->pa_flip.pattern_max);
			if (pa_valid(t0, pa_delta12, 1)) {
				p->flip = 1;
				goto next;
			}
			continue;
		}

next:
		/* matches delta 1 - 2, now try 2 - 3 */
		pa3 = get_equ_pa(p->object[0], p->object[3]);
		pa_delta23 = pa2 - pa3;
		if (pa_delta23 < 0.0)
			pa_delta23 += 2.0 * M_PI;

		DOBJ_PA_CHECK(p->object[0], p->object[2], p->object[3], pa_delta23,
					t1->pa.pattern_min, t1->pa.pattern_max);

		if (!pa_valid(t1, pa_delta23, p->flip))
			continue;

		/* matches delta 2 -3, now try 3 - 1 */
		pa_delta31 = pa3 - pa1;
		if (pa_delta31 < 0.0)
			pa_delta31 += 2.0 * M_PI;

		DOBJ_PA_CHECK(p->object[0], p->object[3], p->object[1], pa_delta31,
					t2->pa.pattern_min, t2->pa.pattern_max);

		if (!pa_valid(t2, pa_delta31, p->flip))
			continue;

		delta = tri_diff(pa_delta12 - t0->pa.plate_actual,
				pa_delta23 - t1->pa.plate_actual,
				pa_delta31 - t2->pa.plate_actual);
		add_pot_on_pa(runtime, p, delta);
		count++;
	}

	runtime->num_pot_distance_checked = runtime->num_pot_distance;
	return count;
}

static int solve_single_object_on_pa(struct solve_runtime *runtime,
	struct adb_solve_solution *solution)
{
	struct adb_solve_solution *p;
	double pa0, pa1, pa2, pa3;
	double pa_delta01, pa_delta12, pa_delta23, pa_delta30, delta;
	int i, count = 0;

	/* for each potential cluster */
	for (i = 0; i < runtime->num_pot_distance; i++) {

		p = &runtime->pot_distance[i];

		/* check object against 0 -> 1 */
		pa0 = get_equ_pa(p->object[0], solution->object[0]);
		pa1 = get_equ_pa(p->object[0], solution->object[1]);
		pa_delta01 = pa0 - pa1;
		if (pa_delta01 < 0.0)
			pa_delta01 += 2.0 * M_PI;
		if (!pa_valid(&runtime->soln_target[0], pa_delta01, p->flip))
			continue;

		/* check object against 1 -> 2 */
		pa2 = get_equ_pa(p->object[0], solution->object[2]);
		pa_delta12 = pa1 - pa2;
		if (pa_delta12 < 0.0)
			pa_delta12 += 2.0 * M_PI;
		if (!pa_valid(&runtime->soln_target[1], pa_delta12, p->flip))
			continue;

		/* check object against 2 -> 3 */
		pa3 = get_equ_pa(p->object[0], solution->object[3]);
		pa_delta23 = pa2 - pa3;
		if (pa_delta23 < 0.0)
			pa_delta23 += 2.0 * M_PI;
		if (!pa_valid(&runtime->soln_target[2], pa_delta23, p->flip))
			continue;

		/* check object against 3 -> 0 */
		pa_delta30 = pa3 - pa0;
		if (pa_delta30 < 0.0)
			pa_delta30 += 2.0 * M_PI;

		if (!pa_valid(&runtime->soln_target[3], pa_delta30, p->flip))
			continue;

		delta = quad_diff(pa_delta01 - runtime->soln_target[0].pa.plate_actual,
				pa_delta12 - runtime->soln_target[1].pa.plate_actual,
				pa_delta23 - runtime->soln_target[2].pa.plate_actual,
				pa_delta30 - runtime->soln_target[3].pa.plate_actual);
		add_pot_on_pa(runtime, p, delta);
		count++;
	}

	return count;
}

static double calc_magnitude_deltas(struct solve_runtime *runtime,
	int pot, int idx)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *s = &runtime->pot_pa[pot];
	double plate_diff, db_diff;

	plate_diff = get_plate_mag_diff(&solve->pobject[idx],
			&solve->pobject[idx + 1]);

	db_diff = s->object[idx]->key -
			s->object[idx + 1]->key;

	return plate_diff - db_diff;
}

static void calc_cluster_divergence(struct solve_runtime *runtime)
{
	int i;

	/* calculate differences in magnitude from DB and plate objects */
	for (i = 0; i < runtime->num_pot_pa; i++) {
		runtime->pot_pa[i].delta_magnitude =
			(calc_magnitude_deltas(runtime, i, 0) +
			calc_magnitude_deltas(runtime, i, 1) +
			calc_magnitude_deltas(runtime, i, 2)) / 3.0;

		runtime->pot_pa[i].divergance =
			runtime->pot_pa[i].delta_magnitude * DELTA_MAG_COEFF +
			runtime->pot_pa[i].delta_distance * DELTA_DIST_COEFF +
			runtime->pot_pa[i].delta_pa * DELTA_PA_COEFF;
	}
}

static void calc_object_divergence(struct solve_runtime *runtime,
		struct adb_solve_solution *solution,
		struct adb_pobject *pobject)
{
	int i;

	/* calculate differences in magnitude from DB and plate objects */
	for (i = 0; i < runtime->num_pot_pa; i++) {
		runtime->pot_pa[i].delta_magnitude =
			fabs(get_plate_magnitude(runtime->solve, solution, pobject) -
			runtime->pot_pa[0].object[0]->key);

		runtime->pot_pa[i].divergance =
			runtime->pot_pa[i].delta_magnitude * DELTA_MAG_COEFF +
			runtime->pot_pa[i].delta_distance * DELTA_DIST_COEFF +
			runtime->pot_pa[i].delta_pa * DELTA_PA_COEFF;
	}
}

static int is_solution_dupe(struct adb_solve *solve,
		struct adb_solve_solution *s1)
{
	struct adb_solve_solution *s2;
	int i;

	for (i = 0; i < solve->num_solutions; i++) {
		s2 = &solve->solution[i];
		if (s2->object[0] == s1->object[0] &&
			s2->object[1] == s1->object[1] &&
			s2->object[2] == s1->object[2] &&
			s2->object[3] == s1->object[3]) {
			return 1;
		}
	}
	return 0;
}

static void copy_solution(struct solve_runtime *runtime)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *soln;
	struct adb_db *db;
	int i;

	pthread_mutex_lock(&solve->mutex);

	if (solve->num_solutions == MAX_RT_SOLUTIONS) {
		adb_error(solve->db, "too many solutions, narrow params\n");
		pthread_mutex_unlock(&solve->mutex);
		return;
	}

	for (i = 0; i < runtime->num_pot_pa; i++) {
		soln = &solve->solution[solve->num_solutions];
		db = soln->db;

		if (is_solution_dupe(solve, &runtime->pot_pa[i]))
			continue;

		/* copy solution */
		*soln = runtime->pot_pa[i];
		soln->db = db;
		solve->num_solutions++;
	}

	pthread_mutex_unlock(&solve->mutex);
}

/* solve plate cluster for this primary object */
static int try_object_as_primary(struct adb_solve *solve,
	const struct adb_object *primary, int primary_idx)
{
	struct solve_runtime runtime;
	int i, count;

	memset(&runtime, 0, sizeof(runtime));
	runtime.solve = solve;

	/* find secondary candidate adb_source_objects on magnitude */
	for (i = 0; i < MIN_PLATE_OBJECTS - 1; i++) {
		count = solve_object_on_magnitude(&runtime, primary, i);
		if (!count)
			return 0;
	}

	/* at this point we have a range of candidate stars that match the
	 * magnitude bounds of the primary object and each secondary object,
	 * now check secondary candidates for distance alignment */
	count = solve_object_on_distance(&runtime, primary);
	if (!count)
		return 0;

	/* At this point we have a list of clusters that match on magnitude and
	 * distance, so we finally check the candidates clusters for PA alignment*/
	count = solve_object_on_pa(&runtime, primary, i);
	if (!count)
		return 0;

	calc_cluster_divergence(&runtime);

	/* copy matching clusters to solver */
	copy_solution(&runtime);

	return solve->num_solutions;
}

static int solve_plate_cluster_for_set_all(struct adb_solve *solve,
	struct adb_object_set *set)
{
	int i, count = 0;

	/* attempt to solve for each object in set */
#pragma omp parallel for schedule(dynamic, 10) reduction(+:count)
	for (i = 0; i < solve->source.num_objects; i++)
		count += try_object_as_primary(solve, solve->source.objects[i], i);

	if (count >= MAX_RT_SOLUTIONS)
		count = MAX_RT_SOLUTIONS - 1;

	return count;
}

static int solve_plate_cluster_for_set_first(struct adb_solve *solve,
	struct adb_object_set *set)
{
	int i, count = 0;

	/* attempt to solve for each object in set */
#pragma omp parallel for schedule(dynamic, 10)
	for (i = 0; i < solve->source.num_objects; i++) {

		if (count)
			continue;

		if (try_object_as_primary(solve, solve->source.objects[i], i)) {
			count = 1;
/* TODO OpenMP cancel is supported in gcc 4.9 */
/* #pragma omp cancel for */
		}
	}
/* #pragma omp cancellation point for */

	return count;
}

struct adb_solve *adb_solve_new(struct adb_db *db, int table_id)
{
	struct adb_solve *solve;
	int i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return NULL;

	solve = calloc(1, sizeof(struct adb_solve));
	if (solve == NULL)
		return NULL;

	pthread_mutex_init(&solve->mutex, NULL);

	solve->db = db;
	solve->table = &db->table[table_id];

	/* set initial constraints */
	solve->constraint.min_ra = 0.0 * D2R;
	solve->constraint.max_ra = 360.0 * D2R;
	solve->constraint.min_dec = 0.0 * D2R;
	solve->constraint.max_dec = 90.0 * D2R;
	solve->constraint.min_mag = 16.0;
	solve->constraint.max_mag = -2.0;
	solve->constraint.min_fov = 0.1 * D2R;
	solve->constraint.max_fov = 90.0 * D2R;
	solve->constraint.max_fov = 90.0 * D2R * 1000.0;
	solve->num_solutions = 0;

	for (i = 0; i < MAX_RT_SOLUTIONS; i++)
		solve->solution[i].db = db;

	return solve;
}

void adb_solve_free(struct adb_solve *solve)
{
	free(solve->source.objects);
	free(solve);
}

int adb_solve_add_plate_object(struct adb_solve *solve,
				struct adb_pobject *pobject)
{
	if (solve->num_plate_objects == ADB_NUM_TARGETS - 1) {
		adb_error(solve->db, "too many adb_source_objects %d\n",
			ADB_NUM_TARGETS);
		return -EINVAL;
	}

	if (pobject->adu == 0) {
		adb_error(solve->db, "object has no ADUs\n");
		return -EINVAL;
	}

	memcpy(&solve->pobject[solve->num_plate_objects++], pobject,
		sizeof(struct adb_pobject));

	return 0;
}

int adb_solve_constraint(struct adb_solve *solve,
		enum adb_constraint type, double min, double max)
{
	switch(type) {
	case ADB_CONSTRAINT_MAG:
		solve->constraint.min_mag = min;
		solve->constraint.max_mag = max;
		break;
	case ADB_CONSTRAINT_FOV:
		solve->constraint.min_fov = min * D2R;
		solve->constraint.max_fov = max * D2R;
		solve->constraint.max_fov1k = max * D2R * 1000.0;
		break;
	case ADB_CONSTRAINT_RA:
		solve->constraint.min_ra = min * D2R;
		solve->constraint.max_ra = max * D2R;
		break;
	case ADB_CONSTRAINT_DEC:
		solve->constraint.min_dec = min * D2R;
		solve->constraint.max_dec = max * D2R;
		break;
	default:
		adb_error(solve->db, "unknown constraint type %d\n", type);
		return -EINVAL;
	}
	return 0;
}

/*! \fn int adb_solve_get_results(struct adb_solve *solve,
				struct adb_object_set *set,
				const struct adb_object **objects[],
				double dist_coeff, double mag_coeff,
				double pa_coeff)
* \param image Image
* \param num_scales Number of wavelet scales.
* \return Wavelet pointer on success or NULL on failure..
*
*
*/
int adb_solve(struct adb_solve *solve,
		struct adb_object_set *set, enum adb_find find)
{
	int ret, i, count = 0;

	/* do we have enough plate adb_source_objects to solve */
	if (solve->num_plate_objects < MIN_PLATE_OBJECTS) {
		adb_error(solve->db, "not enough plate adb_source_objects, need %d have %d\n",
			MIN_PLATE_OBJECTS, solve->num_plate_objects);
		return -EINVAL;
	}

	ret = build_and_sort_object_set(solve, set, &solve->source);
	if (ret <= 0) {
		adb_error(solve->db, "cant get trixels %d\n", ret);
		return ret;
	}

	for (i = 0; i <= solve->num_plate_objects - MIN_PLATE_OBJECTS; i++) {

		solve->plate_idx_start = i;
		solve->plate_idx_end = MIN_PLATE_OBJECTS + i;

		adb_info(solve->db, ADB_LOG_SOLVE, "solving plate objects %d -> %d from %d\n",
			solve->plate_idx_start, solve->plate_idx_end - 1,
			solve->num_plate_objects);

		create_target_pattern(solve);

		if (find == ADB_FIND_ALL)
			ret = solve_plate_cluster_for_set_all(solve, set);
		else
			ret = solve_plate_cluster_for_set_first(solve, set);

		if (ret < 0)
			return ret;
		count += ret;
	}

	/* it's possible we may have > 1 solution so order them */
	qsort(solve->solution, solve->num_solutions,
		sizeof(struct adb_solve_solution), solution_cmp);

	adb_info(solve->db, ADB_LOG_SOLVE, "Total %d solutions\n",
		solve->num_solutions);
	return solve->num_solutions;
}

int adb_solve_set_magnitude_delta(struct adb_solve *solve,
		double delta_mag)
{
	solve->mag_delta = delta_mag;
	return 0;
}

int adb_solve_set_distance_delta(struct adb_solve *solve,
		double delta_pixels)
{
	solve->dist_coeff = delta_pixels;
	return 0;
}

int adb_solve_set_pa_delta(struct adb_solve *solve,
		double delta_degrees)
{
	solve->pa_delta = delta_degrees * D2R;
	return 0;
}

int adb_solve_get_solutions(struct adb_solve *solve,
	unsigned int index, struct adb_solve_solution **solution)
{
	if (solve->num_solutions == 0 || index >= solve->num_solutions) {
		*solution = NULL;
		return -EINVAL;
	}

	*solution = &solve->solution[index];
	return 0;
}

/* prepare solution for finding other objects */
int adb_solve_prep_solution(struct adb_solve_solution *solution,
	double fov, double mag_limit, int table_id)
{
	struct adb_table *table;
	struct adb_object_set *set;
	double centre_ra, centre_dec;
	int object_heads, i, count = 0, j;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &solution->db->table[table_id];

	/* check for existing set and free it */
	set = solution->set;
	if (set)
		free(set);

	centre_ra = solution->object[0]->ra;
	centre_dec = solution->object[0]->dec;

	/* create new set based on image fov and mag limits */
	set = adb_table_set_new(solution->db, table_id);
	if (!set)
		return -ENOMEM;
	solution->set = set;

	adb_table_set_constraints(set, centre_ra * R2D, centre_dec *R2D,
			fov, -10.0, mag_limit);

	/* get object heads */
	object_heads = adb_set_get_objects(set);
	if (object_heads <= 0) {
		free(set);
		solution->set = NULL;
		return object_heads;
	}

	/* allocate space for adb_source_objects */
	solution->source.objects =
		calloc(set->count, sizeof(struct adb_object *));
	if (solution->source.objects == NULL) {
		free(set);
		solution->set = NULL;
		return -ENOMEM;
	}

	/* copy adb_source_objects ptrs from head set */
	for (i = 0; i < object_heads; i++) {

		const void *object = set->object_heads[i].objects;

		for (j = 0; j < set->object_heads[i].count; j++)  {
			solution->source.objects[count++] = object;
			SOBJ_CHECK_SET(((struct adb_object*)object));
			object += table->object.bytes;
		}
	}

	/* sort adb_source_objects on magnitude */
	qsort(solution->source.objects, set->count,
		sizeof(struct adb_object *), object_cmp);
	solution->source.num_objects = count;

	/* use the 4 solution objects as first reference objects */
	for (i = 0; i < 4; i++)
		add_reference_object(solution, solution->object[i],
			&solution->pobject[i]);

	return 0;
}

/* get object or estimated object magnitude & position for plate object */
static int get_object(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobject, struct adb_solve_object *sobject)
{
	struct solve_runtime runtime;
	int count = 0;

	if (solution->set == NULL)
		return -EINVAL;

	memset(&runtime, 0, sizeof(runtime));
	memset(sobject, 0, sizeof(*sobject));
	runtime.solve = solve;

	/* calculate plate parameters for new object */
	create_target_single(solve, pobject, solution, &runtime);

	/* find candidate adb_source_objects on magnitude */
	count = solve_single_object_on_magnitude(&runtime, solution, pobject);
	if (count == 0)
		goto estimate;

	/* at this point we have a range of candidate stars that match the
	 * magnitude bounds of the plate object now check for distance alignment */
	count = solve_single_object_on_distance(&runtime, solution);
	if (count == 0)
		goto estimate;

	/* At this point we have a list of objects that match on magnitude and
	 * distance, so we finally check the objects for PA alignment*/
	count = solve_single_object_on_pa(&runtime, solution);
	if (count > 0) {
		/* add object as reference */
		add_reference_object(solution, runtime.pot_pa[0].object[0], pobject);
	}

estimate:
	/* nothing found so return object magnitude and position */
	sobject->mag = get_plate_magnitude(solve, solution, pobject);
	get_plate_position(solve, solution, pobject,
			&sobject->ra, &sobject->dec);
	if (count == 0)
		return 0;

	calc_object_divergence(&runtime, solution, pobject);

	/* it's possible we may have > 1 solution so order them */
	qsort(solve->solution, count,
		sizeof(struct adb_solve_solution), solution_cmp);

	/* assign closest object */
	sobject->object = runtime.pot_pa[0].object[0];
	return count;
}

/* get objects using same solution */
static int solve_get_objects_soln(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobjects, int num_pobjects)
{
	int i, j, ret;

	/* reallocate memory for new pobjects */
	solution->solve_object = realloc(solution->solve_object,
		sizeof(struct adb_solve_object) *
		(num_pobjects + solve->num_plate_objects));
	if (solution->solve_object == NULL)
		return -ENOMEM;

	solution->num_solved_objects = 0;
	solution->num_unsolved_objects = 0;

	/* copy existing plate solutions */
	for (i = 0; i < solve->plate_idx_end; i++) {

		solution->solve_object[i].object = solution->object[i];
		solution->solve_object[i].pobject = solve->pobject[i];
		solution->solve_object[i].mag =
			get_plate_magnitude(solve, solution, &solve->pobject[i]);
		get_plate_position(solve, solution, &solve->pobject[i],
			&solution->solve_object[i].ra, &solution->solve_object[i].dec);

		if (solution->solve_object[i].object)
			solution->num_solved_objects++;
		else
			solution->num_unsolved_objects++;
	}

	/* find initial plate objects remaining that are not used in solution  */
	for (i = solve->plate_idx_end; i < solve->num_plate_objects; i++) {
		ret = get_object(solve, solution, &solve->pobject[i],
			&solution->solve_object[i]);
		if (ret < 0) {
			free(solution->solve_object);
			solution->solve_object = NULL;
			return ret;
		} else if (ret == 0)
			solution->num_unsolved_objects++;
		else
			solution->num_solved_objects++;

		solution->solve_object[i].pobject = pobjects[i];
	}

	solution->total_objects = solution->num_unsolved_objects +
		solution->num_solved_objects;

	/* solve each new plate object */
	for (i = 0, j = solution->total_objects; i < num_pobjects; i++, j++) {

		ret = get_object(solve, solution, &pobjects[i],
			&solution->solve_object[j]);
		if (ret < 0) {
			free(solution->solve_object);
			solution->solve_object = NULL;
			return ret;
		} else if (ret == 0)
			solution->num_unsolved_objects++;
		else
			solution->num_solved_objects++;

		solution->solve_object[j].pobject = pobjects[i];
	}

	solution->total_objects = solution->num_solved_objects +
		solution->num_unsolved_objects;

	/* recalculate magnitude for each object in image */
	calc_solved_plate_magnitudes(solve, solution);
	calc_unsolved_plate_magnitudes(solve, solution);

	return solution->num_solved_objects;
}

/* get objects using a different solution */
static int solve_get_objects_non_soln(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobjects, int num_pobjects)
{
	int i, ret;

	/* reallocate memory for new pobjects */
	solution->solve_object = realloc(solution->solve_object,
		sizeof(struct adb_solve_object) * num_pobjects);
	if (solution->solve_object == NULL)
		return -ENOMEM;

	solution->num_solved_objects = 0;
	solution->num_unsolved_objects = 0;

	/* solve each new plate object */
	for (i = 0; i < num_pobjects; i++) {

		ret = get_object(solve, solution, &pobjects[i],
			&solution->solve_object[i]);
		if (ret < 0) {
			free(solution->solve_object);
			solution->solve_object = NULL;
			return ret;
		} else if (ret == 0)
			solution->num_unsolved_objects++;
		else
			solution->num_solved_objects++;

		solution->solve_object[i].pobject = pobjects[i];
	}

	solution->total_objects = solution->num_solved_objects +
		solution->num_unsolved_objects;

	/* recalculate magnitude for each object in image */
	calc_solved_plate_magnitudes(solve, solution);
	calc_unsolved_plate_magnitudes(solve, solution);

	return solution->num_solved_objects;
}

/* get plate objects or estimates of plate object position and magnitude */
int adb_solve_get_objects(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobjects, int num_pobjects)
{
	/* check whether we are getting objects for the same table as the db
	 * or a new table.
	 */
	if (solve->solution != solution)
		return solve_get_objects_non_soln(solve, solution, pobjects,
			num_pobjects);
	else
		return solve_get_objects_soln(solve, solution, pobjects,
			num_pobjects);
}
