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
 *  Copyright (C) 2013 Liam Girdwood
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
#include <libastrodb/table.h>
#include <libastrodb/adbstdio.h>

#define MIN_PLATE_OBJECTS	4
#define MAX_POTENTAL_MATCHES	256
#define MAX_ACTUAL_MATCHES	16
#define MAX_RT_SOLUTIONS	16
#define DELTA_MAG_COEFF		0.5
#define DELTA_DIST_COEFF	1.0
#define DELTA_PA_COEFF		1.0

struct tdata {
	double pattern_min;
	double pattern_max;
	double plate_actual;
};

struct target_object {
	struct astrodb_pobject *pobject;
	struct tdata distance;
	struct tdata pa;
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

struct solve_runtime {
	struct astrodb_solve *solve;

	/* potential matches on magnitude */
	struct magnitude_range pot_magnitude;

	/* potential matches after magnitude and distance checks */
	struct astrodb_solve_objects pot_distance[MAX_POTENTAL_MATCHES];
	int num_pot_distance;
	int num_pot_distance_checked;

	/* potential matches after magnitude, distance and PA */
	struct astrodb_solve_objects pot_pa[MAX_POTENTAL_MATCHES];
	int num_pot_pa;
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
};

struct astrodb_solve {
	struct astrodb_db *db;
	struct astrodb_table *table;
	pthread_mutex_t mutex;

	/* plate objects */
	struct astrodb_pobject pobject[ADB_NUM_TARGETS];
	int num_plate_objects;

	/* detected objects */
	const struct astrodb_object **detected_objects;

	struct solve_constraint constraint;

	/* target cluster */
	struct target_pattern target;

	/* source object storage */
	const struct astrodb_object **source_objects;
	int num_source_objects;

	/* tuning coefficients */
	double dist_coeff;
	double mag_delta;
	double pa_delta;

	/* potential solutions from all runtimes */
	struct astrodb_solve_objects solve_objects[MAX_RT_SOLUTIONS];
	int num_solutions;
};

static int solution_cmp(const void *o1, const void *o2)
{
	const struct astrodb_solve_objects *p1 = o1, *p2 = o2;

	if (p1->divergance < p2->divergance)
		return 1;
	else if (p1->divergance > p2->divergance)
		return -1;
	else
		return 0;
}

static int plate_object_cmp(const void *o1, const void *o2)
{
	const struct astrodb_pobject *p1 = o1, *p2 = o2;

	if (p1->adu < p2->adu)
		return 1;
	else if (p1->adu > p2->adu)
		return -1;
	else
		return 0;
}

static int object_cmp(const void *o1, const void *o2)
{
	const struct astrodb_object *p1 = *(const struct astrodb_object **)o1,
		*p2 = *(const struct astrodb_object **)o2;

	if (p1->posn_mag.Vmag < p2->posn_mag.Vmag)
		return -1;
	else if (p1->posn_mag.Vmag > p2->posn_mag.Vmag)
		return 1;
	else
		return 0;
}

/* plate distance squared between primary and secondary */
static double get_plate_distance(struct astrodb_pobject *primary,
	struct astrodb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return sqrt((x * x) + (y * y));
}

/* distance in between two source_objects */
static double get_equ_distance(const struct astrodb_object *o1,
	const struct astrodb_object *o2)
{
	double x,y,z;

	x = (cos(o1->posn_mag.dec) * sin (o2->posn_mag.dec))
		- (sin(o1->posn_mag.dec) * cos(o2->posn_mag.dec) *
		cos(o2->posn_mag.ra - o1->posn_mag.ra));
	y = cos(o2->posn_mag.dec) * sin(o2->posn_mag.ra - o1->posn_mag.ra);
	z = (sin(o1->posn_mag.dec) * sin(o2->posn_mag.dec)) +
		(cos(o1->posn_mag.dec) * cos(o2->posn_mag.dec) *
		cos(o2->posn_mag.ra - o1->posn_mag.ra));

	x = x * x;
	y = y * y;

	return atan2(sqrt(x + y), z);
}

/* ratio of magnitude from primary to secondary */
static double get_plate_mag_diff(struct astrodb_pobject *primary,
	struct astrodb_pobject *secondary)
{
	double s_adu = secondary->adu, p_adu = primary->adu;

	return -2.5 * log10(s_adu / p_adu);
}

/* position angle in radians relative to plate north */
static double get_plate_pa(struct astrodb_pobject *primary,
	struct astrodb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return atan2(y, x);
}

/* pposition angle in radians */
static double get_equ_pa(const struct astrodb_object *o1,
		const struct astrodb_object *o2)
{
	double k, ra_delta, x, y;
	double sin_dec, cos_dec, sin_ra, cos_ra_delta;
	double sin_pdec, cos_pdec;

	/* pre-calc common terms */
	sin_dec = sin(o1->posn_mag.dec);
	cos_dec = cos(o1->posn_mag.dec);
	sin_ra = sin(o1->posn_mag.ra);
	ra_delta = o1->posn_mag.ra - o2->posn_mag.ra;
	cos_ra_delta = cos(ra_delta);
	cos_pdec = cos(o2->posn_mag.dec);
	sin_pdec = sin(o2->posn_mag.dec);

	/* calc scaling factor */
	k = 2.0 / (1.0 + sin_pdec * sin_ra +
		cos_pdec * cos_dec * cos_ra_delta);

	/* calc plate X, Y */
	x = k * (cos_dec * sin(ra_delta));
	y = k * (cos_pdec * sin_dec - sin_pdec * cos_dec * cos_ra_delta);

	return atan2(y, x);
}

/* calculate object pattern variables to match against source objects */
static void create_pattern_object(struct astrodb_solve *solve, int target,
	struct astrodb_pobject *primary, struct astrodb_pobject *secondary)
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
static void create_target_pattern(struct astrodb_solve *solve)
{
	struct target_object *t0, *t1, *t2;
	int i;

	/* sort plate object on brightness */
	qsort(solve->pobject, solve->num_plate_objects,
		sizeof(struct astrodb_pobject), plate_object_cmp);

	/* create target pattern */
	for (i = 1; i < solve->num_plate_objects; i++)
		create_pattern_object(solve, i - 1, &solve->pobject[0],
			&solve->pobject[i]);

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
}

/* get a set of source objects to chack the pattern against */
static int build_and_sort_object_set(struct astrodb_solve *solve,
	struct astrodb_object_set *set)
{
	int object_heads, i, j, count = 0;

	/* get object heads */
	object_heads = astrodb_table_set_get_objects(set);
	if (object_heads <= 0)
		return object_heads;

	/* allocate space for source_objects */
	solve->source_objects = calloc(set->count, sizeof(struct astrodb_object *));
	if (solve->source_objects == NULL)
		return -ENOMEM;

	/* copy source_objects ptrs from head set */
	for (i = 0; i < object_heads; i++) {

		const void *object = set->object_heads[i].objects;

		for (j = 0; j < set->object_heads[i].count; j++)  {

			solve->source_objects[count++] = object;
			object += solve->table->object.bytes;
		}
	}

	/* sort source_objects on magnitude */
	qsort(solve->source_objects, set->count, sizeof(struct astrodb_object *),
		object_cmp);
	solve->num_source_objects = count;

	return set->count;
}

/* binary search the set for magnitude head */
static int bsearch_head(struct astrodb_solve *solve, double value,
	int start, int end, int idx)
{
	const struct astrodb_object *object = solve->source_objects[idx];

	/* search complete */
	if (end - start < 2)
		return idx;

	/* narrow search */
	if (object->posn_mag.Vmag > value)
		return bsearch_head(solve, value, start, idx - 1,
				start + ((idx - start) / 2));
	else if (object->posn_mag.Vmag < value)
		return bsearch_head(solve, value, idx + 1, end,
				idx + ((end - idx) / 2));
	else
		return idx;
}

/* get first object with magnitude >= Vmag */
static int object_get_first_on_mag(struct astrodb_solve *solve,
	double vmag, int start_idx)
{
	const struct astrodb_object *object;
	int idx;

	/* find one of the first source_objects >= vmag  */
	idx = bsearch_head(solve, vmag, start_idx, solve->num_source_objects - 1,
		(solve->num_source_objects - 1) / 2);
	object = solve->source_objects[idx];

	/* make sure the object is first in the array amongst equals */
	if (object->posn_mag.Vmag < vmag) {

		/* make sure we return the idx of the object with equal vmag */
		for (idx++; idx < solve->num_source_objects; idx++) {
			object = solve->source_objects[idx];

			if (object->posn_mag.Vmag >= vmag)
				return idx - 1;
		}
	} else {
		/* make sure we return the idx of the object with equal vmag */
		for (idx--; idx > start_idx; idx--) {
			object = solve->source_objects[idx];

			if (object->posn_mag.Vmag < vmag)
				return idx + 1;
		}
	}

	/* not found */
	return 0;
}

/* binary search the set for magnitude tail */
static int bsearch_tail(struct astrodb_solve *solve, double value,
	int start, int end, int idx)
{
	const struct astrodb_object *object = solve->source_objects[idx];

	/* search complete */
	if (end - start < 2)
		return idx;

	/* narrow search */
	if (object->posn_mag.Vmag > value)
		return bsearch_tail(solve, value, start, idx - 1,
				start + ((idx - start) / 2));
	else if (object->posn_mag.Vmag < value)
		return bsearch_tail(solve, value, idx + 1, end,
				idx + ((end - idx) / 2));
	else
		return idx;
}

/* get last object with magnitude <= Vmag */
static int object_get_last_with_mag(struct astrodb_solve *solve,
	double vmag, int start_idx)
{
	const struct astrodb_object *object;
	int idx;

	/* find one of the last source_objects <= vmag  */
	idx = bsearch_tail(solve, vmag, start_idx, solve->num_source_objects - 1,
		(solve->num_source_objects - 1) / 2);
	object = solve->source_objects[idx];

	/* make sure the object is last in the array amongst equals */
	if (object->posn_mag.Vmag > vmag) {

		/* make sure we return the idx of the object with equal vmag */
		for (idx--; idx > start_idx; idx--) {
			object = solve->source_objects[idx];

			if (object->posn_mag.Vmag <= vmag)
				return idx + 1;
		}
	} else {
		/* make sure we return the idx of the object with equal vmag */
		for (idx++; idx < solve->num_source_objects; idx++) {
			object = solve->source_objects[idx];

			if (object->posn_mag.Vmag > vmag)
				return idx - 1;
		}
	}

	/* not found */
	return solve->num_source_objects - 1;
}

/* compare pattern objects magnitude against source objects */
static int solve_object_on_magnitude(struct solve_runtime *runtime,
		const struct astrodb_object *primary, int idx)
{
	struct magnitude_range *range = &runtime->pot_magnitude;
	struct astrodb_solve *solve = runtime->solve;
	struct target_object *t = &solve->target.secondary[idx];
	int start, end, pos;

	/* get search start position */
	pos = object_get_first_on_mag(solve,
			primary->posn_mag.Vmag - solve->mag_delta, 0);

	/* get start and end indices for secondary vmag */
	start = object_get_first_on_mag(solve,
		t->mag.pattern_min + primary->posn_mag.Vmag, pos);
	end = object_get_last_with_mag(solve,
		t->mag.pattern_max + primary->posn_mag.Vmag, pos);

	/* both out of range */
	if (start == end)
		return 0;

	range->end[idx] = end;

	/* no object */
	if (range->end[idx] < start)
		return 0;

	/* is start out of range */
	range->start[idx] = start;

	/* return number of candidate source_objects based on vmag */
	return range->end[idx] - range->start[idx];
}

/* add matching cluster to list of potentials */
static void add_pot_on_distance(struct solve_runtime *runtime,
	const struct astrodb_object *primary, int i, int j, int k, double delta)
{
	struct astrodb_solve *solve = runtime->solve;
	struct astrodb_solve_objects *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];

	p->object[0] = primary;
	p->object[1] = solve->source_objects[i];
	p->object[2] = solve->source_objects[j];
	p->object[3] = solve->source_objects[k];
	p->delta_distance = delta;
	runtime->num_pot_distance++;
}

static inline int not_within_fov_fast(struct astrodb_solve *solve,
		const struct astrodb_object *p, const struct astrodb_object *s)
{
	if (fabs(p->posn_mag.ra - s->posn_mag.ra) > solve->constraint.max_fov)
		return 1;
	if (fabs(p->posn_mag.dec - s->posn_mag.dec) > solve->constraint.max_fov)
		return 1;
	return 0;
}

/* check magnitude matched objects on pattern distance */
static int solve_object_on_distance(struct solve_runtime *runtime,
	const struct astrodb_object *primary)
{
	const struct astrodb_object *s[3];
	struct target_object *t0, *t1, *t2;
	struct astrodb_solve *solve = runtime->solve;
	struct magnitude_range *range = &runtime->pot_magnitude;
	int i, j, k, count = 0;
	double distance0, distance1, distance2, ratio0;
	double t1_min, t1_max, t2_max, t2_min;

	/* check distance ratio for each matching candidate against targets */
	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];
	runtime->num_pot_distance = 0;

	/* check t0 candidates */
	for (i = range->start[0]; i < range->end[0]; i++) {

		s[0] = solve->source_objects[i];
		if (s[0] == primary)
			continue;

		if (not_within_fov_fast(solve, primary, s[0]))
			continue;

		distance0 = get_equ_distance(primary, s[0]);

		/* rule out any distances > FOV */
		if (distance0 > solve->constraint.max_fov)
			continue;

		ratio0 = distance0 / t0->distance.plate_actual;

		/* use ratio based on t0 <-> primary distance for t1 */
		t1_min = t1->distance.pattern_min * ratio0;
		t1_max = t1->distance.pattern_max * ratio0;

		/* check each t1 candidates against t0 <-> primary distance ratio */
		for (j = range->start[1]; j < range->end[1]; j++) {

			s[1] = solve->source_objects[j];
			if (s[0] == s[1] || s[1] == primary)
				continue;

			if (not_within_fov_fast(solve, primary, s[1]))
				continue;

			distance1 = get_equ_distance(primary, s[1]);

			/* rule out any distances > FOV */
			if (distance1 > solve->constraint.max_fov)
				continue;

			/* is this t1 candidate within t0 primary ratio */
			if (distance1 >= t1_min && distance1 <= t1_max) {

				t2_min = t2->distance.pattern_min * ratio0;
				t2_max = t2->distance.pattern_max * ratio0;

				/* check t2 candidates */
				for (k = range->start[2]; k < range->end[2]; k++) {

					s[2] = solve->source_objects[k];
					if (s[0] == s[2] || s[1] == s[2] || s[2] == primary)
						continue;

					if (not_within_fov_fast(solve, primary, s[2]))
						continue;

					distance2 = get_equ_distance(primary, s[2]);

					/* rule out any distances > FOV */
					if (distance2 > solve->constraint.max_fov)
						continue;

					if (distance2 >= t2_min && distance2 <= t2_max) {

						double ratio1, ratio2, delta;

						ratio1 = distance1 / t1->distance.plate_actual;
						ratio2 = distance2 / t2->distance.plate_actual;

						delta = fabs(ratio0 -
							((ratio0 + ratio1 + ratio2) / 3.0));

						add_pot_on_distance(runtime, primary, i, j, k, delta);
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
		struct astrodb_solve_objects *p, double delta)
{
	if (runtime->num_pot_pa >= MAX_ACTUAL_MATCHES)
		return;

	p->delta_pa = delta;
	runtime->pot_pa[runtime->num_pot_pa++] = *p;
}

static int solve_object_on_pa(struct solve_runtime *runtime,
	const struct astrodb_object *primary, int idx)
{
	struct astrodb_solve_objects *p;
	struct astrodb_solve *solve = runtime->solve;
	struct target_object *t0, *t1, *t2;
	double pa1, pa2, pa3, pa_delta12, pa_delta23, pa_delta31, delta;
	int i, count = 0;

	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];

	/* for each potential cluster */
	for (i = runtime->num_pot_distance_checked; i < runtime->num_pot_distance; i++) {

		p = &runtime->pot_distance[i];

		/* check PA for primary to each secondary object */
		pa1 = get_equ_pa(p->object[0], p->object[1]);
		pa2 = get_equ_pa(p->object[0], p->object[2]);
		pa_delta12 = pa1 - pa2;
		if (pa_delta12 < 0.0)
			pa_delta12 += 2.0 * M_PI;
		if (pa_delta12 < t0->pa.pattern_min || pa_delta12 > t0->pa.pattern_max)
			continue;

		/* matches delta 1 - 2, now try 2 - 3 */
		pa3 = get_equ_pa(p->object[0], p->object[3]);
		pa_delta23 = pa2 - pa3;
		if (pa_delta23 < 0.0)
			pa_delta23 += 2.0 * M_PI;
		if (pa_delta23 < t1->pa.pattern_min || pa_delta23 > t1->pa.pattern_max)
			continue;

		/* matches delta 2 -3, now try 3 - 1 */
		pa_delta31 = pa3 - pa1;
		if (pa_delta31 < 0.0)
			pa_delta31 += 2.0 * M_PI;
		if (pa_delta31 < t2->pa.pattern_min || pa_delta31 > t2->pa.pattern_max)
			continue;

		delta = (fabs(pa_delta12 - t0->pa.plate_actual) +
				fabs(pa_delta23 - t1->pa.plate_actual) +
				fabs(pa_delta31 - t2->pa.plate_actual)) / 3.0;
		add_pot_on_pa(runtime, p, delta);
		count++;
	}

	runtime->num_pot_distance_checked = runtime->num_pot_distance;
	return count;
}

static double calc_magnitude_deltas(struct solve_runtime *runtime,
	int pot, int idx)
{
	struct astrodb_solve *solve = runtime->solve;
	struct astrodb_solve_objects *s = &runtime->pot_pa[pot];
	double plate_diff, db_diff;

	plate_diff = get_plate_mag_diff(&solve->pobject[idx],
			&solve->pobject[idx + 1]);

	db_diff = s->object[idx]->posn_mag.Vmag -
			s->object[idx + 1]->posn_mag.Vmag;

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
		printf("div %f mag %f dist %9.9f pa %f\n",
			runtime->pot_pa[i].divergance,
			runtime->pot_pa[i].delta_magnitude,
			runtime->pot_pa[i].delta_distance,
			runtime->pot_pa[i].delta_pa);
	}
}

static int is_solution_dupe(struct astrodb_solve *solve,
		struct astrodb_solve_objects *soln)
{
	struct astrodb_solve_objects *s;
	int i;

	for (i = 0; i < solve->num_solutions; i++) {
		s = &solve->solve_objects[i];
		if (s->object[0] == soln->object[0] &&
			s->object[1] == soln->object[1] &&
			s->object[2] == soln->object[2] &&
			s->object[3] == soln->object[3])
			return 1;
	}
	return 0;
}

static void copy_solution(struct solve_runtime *runtime)
{
	struct astrodb_solve *solve = runtime->solve;
	struct astrodb_solve_objects *soln;
	int i;

	pthread_mutex_lock(&solve->mutex);

	for (i = 0; i < runtime->num_pot_pa; i++) {
		soln = &solve->solve_objects[solve->num_solutions];

		if (is_solution_dupe(solve, &runtime->pot_pa[i]))
			continue;

		/* copy solution */
		soln->delta_distance = runtime->pot_pa[i].delta_distance;
		soln->delta_magnitude = runtime->pot_pa[i].delta_magnitude;
		soln->delta_pa = runtime->pot_pa[i].delta_pa;
		soln->object[0] = runtime->pot_pa[i].object[0];
		soln->object[1] = runtime->pot_pa[i].object[1];
		soln->object[2] = runtime->pot_pa[i].object[2];
		soln->object[3] = runtime->pot_pa[i].object[3];
		solve->num_solutions++;
	}

	pthread_mutex_unlock(&solve->mutex);
}

/* solve plate cluster for this primary object */
static int try_object_as_primary(struct astrodb_solve *solve,
	const struct astrodb_object *primary, int primary_idx)
{
	struct solve_runtime runtime;
	int i, count;

	runtime.num_pot_distance = 0;
	runtime.num_pot_distance_checked = 0;
	runtime.num_pot_pa = 0;
	runtime.solve = solve;

	/* find secondary candidate source_objects on magnitude */
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

static int solve_plate_cluster_for_set_all(struct astrodb_solve *solve,
	struct astrodb_object_set *set)
{
	int i, count = 0;

	/* attempt to solve for each object in set */
#pragma omp parallel for schedule(dynamic, 10) reduction(+:count)
	for (i = 0; i < solve->num_source_objects; i++)
		count += try_object_as_primary(solve, solve->source_objects[i], i);

	qsort(solve->solve_objects, count,
			sizeof(struct astrodb_solve_objects), solution_cmp);

	return count;
}

static int solve_plate_cluster_for_set_first(struct astrodb_solve *solve,
	struct astrodb_object_set *set)
{
	int i, count = 0;

	/* attempt to solve for each object in set */
#pragma omp parallel for schedule(dynamic, 10)
	for (i = 0; i < solve->num_source_objects; i++) {

		if (count)
			continue;

		if (try_object_as_primary(solve, solve->source_objects[i], i)) {
			count = 1;
/* TODO OpenMP cancel is supported in gcc 4.9 */
/* #pragma omp cancel for */
		}
	}
/* #pragma omp cancellation point for */

	/* it's possible we may have > 1 solution so order them */
	qsort(solve->solve_objects, count,
		sizeof(struct astrodb_solve_objects), solution_cmp);

	return count;
}

struct astrodb_solve *astrodb_solve_new(struct astrodb_db *db, int table_id)
{
	struct astrodb_solve *solve;

	if (table_id < 0 || table_id > db->table_count)
		return NULL;

	solve = calloc(1, sizeof(struct astrodb_solve));
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
	solve->num_solutions = 0;

	return solve;
}

void astrodb_solve_free(struct astrodb_solve *solve)
{
	free(solve->source_objects);
	free(solve);
}

int astrodb_solve_add_plate_object(struct astrodb_solve *solve,
				struct astrodb_pobject *pobject)
{
	if (solve->num_plate_objects == ADB_NUM_TARGETS - 1) {
		adb_error(solve->db, "too many source_objects %d\n", ADB_NUM_TARGETS);
		return -EINVAL;
	}

	if (pobject->adu == 0) {
		adb_error(solve->db, "object has no ADUs\n");
		return -EINVAL;
	}

	memcpy(&solve->pobject[solve->num_plate_objects++], pobject,
		sizeof(struct astrodb_pobject));

	return 0;
}

int astrodb_solve_constraint(struct astrodb_solve *solve,
		enum astrodb_constraint type, double min, double max)
{
	switch(type) {
	case ADB_CONSTRAINT_MAG:
		solve->constraint.min_mag = min;
		solve->constraint.max_mag = max;
		break;
	case ADB_CONSTRAINT_FOV:
		solve->constraint.min_fov = min * D2R;
		solve->constraint.max_fov = max * D2R;
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

/*! \fn int astrodb_solve_get_results(struct astrodb_solve *solve,
				struct astrodb_object_set *set,
				const struct astrodb_object **objects[],
				double dist_coeff, double mag_coeff,
				double pa_coeff)
* \param image Image
* \param num_scales Number of wavelet scales.
* \return Wavelet pointer on success or NULL on failure..
*
*
*/
int astrodb_solve(struct astrodb_solve *solve,
				struct astrodb_object_set *set, enum astrodb_find find)
{
	int ret;

	/* do we have enough plate source_objects to solve */
	if (solve->num_plate_objects < MIN_PLATE_OBJECTS) {
		adb_error(solve->db, "not enough plate source_objects, need %d have %d\n",
			MIN_PLATE_OBJECTS, solve->num_plate_objects);
		return -EINVAL;
	}

	create_target_pattern(solve);

	ret = build_and_sort_object_set(solve, set);
	if (ret <= 0) {
		adb_error(solve->db, "cant get trixels %d\n", ret);
		return ret;
	}

	if (find == ADB_FIND_ALL)
		return solve_plate_cluster_for_set_all(solve, set);
	else
		return solve_plate_cluster_for_set_first(solve, set);
}

int astrodb_solve_set_magnitude_delta(struct astrodb_solve *solve,
		double delta_mag)
{
	solve->mag_delta = delta_mag;
	return 0;
}

int astrodb_solve_set_distance_delta(struct astrodb_solve *solve,
		double delta_pixels)
{
	solve->dist_coeff = delta_pixels;
	return 0;
}

int astrodb_solve_set_pa_delta(struct astrodb_solve *solve,
		double delta_degrees)
{
	solve->pa_delta = delta_degrees * D2R;
	return 0;
}

int astrodb_solve_get_solutions(struct astrodb_solve *solve,
	unsigned int solution, struct astrodb_solve_objects **solve_objects)
{
	if (solution >= solve->num_solutions)
		return -EINVAL;

	if (solve->num_solutions)
		*solve_objects = &solve->solve_objects[solution];
	else
		*solve_objects = NULL;

	return 0;
}

int astrodb_solve_get_object(struct astrodb_solve *solve,
	struct astrodb_solve_objects *solve_objects,
	struct astrodb_pobject *pobject, const struct astrodb_object **object)
{
	/* sanity checks on inputs */
	if (solve_objects == NULL)
		return -EINVAL;
	if (pobject == NULL)
		return -EINVAL;
	if (object == NULL)
		return -EINVAL;

	// add object to plate

	// calc differences to other plate objects

	// search set on mag

	// search on distance

	// search on PA

	return 0;
}
