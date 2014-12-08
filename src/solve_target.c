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

#include "solve.h"

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

/* add a reference object to the solution */
int target_add_ref_object(struct adb_solve_solution *soln, int id,
	const struct adb_object *object, struct adb_pobject *pobject)
{
	struct adb_reference_object *ref;
	struct adb_solve *solve = soln->solve;
	int i;

	pthread_mutex_lock(&solve->mutex);

	/* first check to see if object is already present */
	for (i = 0; i < soln->num_ref_objects; i++) {
		ref = &soln->ref[i];
		if (ref->object == object) {
			pthread_mutex_unlock(&solve->mutex);
			return 0;
		}
	}

	soln->ref[soln->num_ref_objects].object = object;
	soln->ref[soln->num_ref_objects].mag_mean = 0.0;
	soln->ref[soln->num_ref_objects].mag_sigma = 0.0;
	soln->ref[soln->num_ref_objects].id = id;
	soln->ref[soln->num_ref_objects].clip_mag = 0;
	soln->ref[soln->num_ref_objects].clip_posn = 0;
	soln->ref[soln->num_ref_objects++].pobject = *pobject;

	pthread_mutex_unlock(&solve->mutex);
	return 0;
}

/* calculate object pattern variables to match against source objects */
static void create_pattern_object(struct adb_solve *solve, int target,
	struct adb_pobject *primary, struct adb_pobject *secondary)
{
	struct target_object *t = &solve->target.secondary[target];

	t->pobject = secondary;

	/* calculate plate distance and min,max to primary */
	t->distance.plate_actual = distance_get_plate(primary, secondary);
	t->distance.pattern_min =
		t->distance.plate_actual - solve->tolerance.dist;
	t->distance.pattern_max =
		t->distance.plate_actual + solve->tolerance.dist;

	/* calculate plate magnitude and min,max to primary */
	t->mag.plate_actual = mag_get_plate_diff(primary, secondary);
	t->mag.pattern_min = t->mag.plate_actual - solve->tolerance.mag;
	t->mag.pattern_max = t->mag.plate_actual + solve->tolerance.mag;

	/* calculate plate position angle to primary */
	t->pa.plate_actual = pa_get_plate(primary, secondary);
}

/* create a pattern of plate targets and sort by magnitude */
void target_create_pattern(struct adb_solve *solve)
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

	/* work out PA tolerances */
	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];

	t0->pa.pattern_min = t1->pa.plate_actual - t0->pa.plate_actual;
	if (t0->pa.pattern_min < 0.0)
		t0->pa.pattern_min += 2.0 * M_PI;
	t0->pa.pattern_max = t0->pa.pattern_min + solve->tolerance.pa;
	t0->pa.pattern_min -= solve->tolerance.pa;

	t1->pa.pattern_min = t2->pa.plate_actual - t1->pa.plate_actual;
	if (t1->pa.pattern_min < 0.0)
		t1->pa.pattern_min += 2.0 * M_PI;
	t1->pa.pattern_max = t1->pa.pattern_min + solve->tolerance.pa;
	t1->pa.pattern_min -= solve->tolerance.pa;

	t2->pa.pattern_min = t0->pa.plate_actual - t2->pa.plate_actual;
	if (t2->pa.pattern_min < 0.0)
		t2->pa.pattern_min += 2.0 * M_PI;
	t2->pa.pattern_max = t2->pa.pattern_min + solve->tolerance.pa;
	t2->pa.pattern_min -= solve->tolerance.pa;

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

	/* calculate flip PA tolerances where image can be fliped */
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
	t->distance.plate_actual = distance_get_plate(primary, secondary);
	t->distance.pattern_min = solution->rad_per_pix *
		(t->distance.plate_actual - solve->tolerance.dist);
	t->distance.pattern_max = solution->rad_per_pix *
		(t->distance.plate_actual + solve->tolerance.dist);

	/* calculate plate magnitude and min,max to primary */
	t->mag.plate_actual = mag_get_plate_diff(primary, secondary);
	t->mag.pattern_min = t->mag.plate_actual - solve->tolerance.mag;
	t->mag.pattern_max = t->mag.plate_actual + solve->tolerance.mag;

	/* calculate plate position angle to primary */
	t->pa.plate_actual = pa_get_plate(primary, secondary);
}

/* create a pattern of plate targets and sort by magnitude */
void target_create_single(struct adb_solve *solve,
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

	/* work out PA tolerances */
	t0 = &runtime->soln_target[0];
	t1 = &runtime->soln_target[1];
	t2 = &runtime->soln_target[2];
	t3 = &runtime->soln_target[3];

	t0->pa.pattern_min = t1->pa.plate_actual - t0->pa.plate_actual;
	if (t0->pa.pattern_min < 0.0)
		t0->pa.pattern_min += 2.0 * M_PI;
	t0->pa.pattern_max = t0->pa.pattern_min + solve->tolerance.pa;
	t0->pa.pattern_min -= solve->tolerance.pa;

	t1->pa.pattern_min = t2->pa.plate_actual - t1->pa.plate_actual;
	if (t1->pa.pattern_min < 0.0)
		t1->pa.pattern_min += 2.0 * M_PI;
	t1->pa.pattern_max = t1->pa.pattern_min + solve->tolerance.pa;
	t1->pa.pattern_min -= solve->tolerance.pa;

	t2->pa.pattern_min = t3->pa.plate_actual - t2->pa.plate_actual;
	if (t2->pa.pattern_min < 0.0)
		t2->pa.pattern_min += 2.0 * M_PI;
	t2->pa.pattern_max = t2->pa.pattern_min + solve->tolerance.pa;
	t2->pa.pattern_min -= solve->tolerance.pa;

	t3->pa.pattern_min = t0->pa.plate_actual - t3->pa.plate_actual;
	if (t3->pa.pattern_min < 0.0)
		t3->pa.pattern_min += 2.0 * M_PI;
	t3->pa.pattern_max = t3->pa.pattern_min + solve->tolerance.pa;
	t3->pa.pattern_min -= solve->tolerance.pa;

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

	/* calculate flip PA tolerances where image can be fliped */
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
void target_add_match_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source,
	int i, int j, int k, double delta, double rad_per_pix)
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
	p->soln_pobject[0] = solve->pobject[solve->plate_idx_start];
	p->soln_pobject[1] = solve->pobject[solve->plate_idx_start + 1];
	p->soln_pobject[2] = solve->pobject[solve->plate_idx_start + 2];
	p->soln_pobject[3] = solve->pobject[solve->plate_idx_start + 3];
	p->delta.dist = delta;
	p->rad_per_pix = rad_per_pix;
	runtime->num_pot_distance++;
}

/* add single object to list of potentials on distance */
void target_add_single_match_on_distance(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source, double delta, int flip)
{
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];
	p->delta.dist = delta;
	p->object[0] = primary;
	p->flip = flip;
	runtime->num_pot_distance++;
}

/* add single object to list of potentials on distance */
void target_add_single_match_extended(struct solve_runtime *runtime,
	const struct adb_object *primary,
	struct adb_source_objects *source, double delta, int flip)
{
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];
	p->delta.dist = delta;
	p->object[0] = primary;
	p->flip = flip;
	runtime->num_pot_distance++;
}

/* get a set of source objects to check the pattern against */
int target_prepare_source_objects(struct adb_solve *solve,
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
			if (o->mag <= solve->constraint.min_mag &&
				o->mag >= solve->constraint.max_mag)
				source->objects[count++] = object;
			object += solve->table->object.bytes;
		}
	}

	/* sort adb_source_objects on magnitude */
	qsort(source->objects, count, sizeof(struct adb_object *),
		mag_object_cmp);
	source->num_objects = count;

	return count;
}
