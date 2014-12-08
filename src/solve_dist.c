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

/* plate distance squared between primary and secondary */
double distance_get_plate(struct adb_pobject *primary,
	struct adb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return sqrt((x * x) + (y * y));
}

/* distance in between two adb_source_objects */
double distance_get_equ(const struct adb_object *o1,
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

/* quickly check if object p is within FoV of object s */
static inline int distance_not_within_fov(struct adb_solve *solve,
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

int distance_solve_single_object(struct solve_runtime *runtime,
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
		distance = distance_get_equ(solution->object[0], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[0].distance.pattern_min,
			runtime->soln_target[0].distance.pattern_max, 1);

		if (distance > runtime->soln_target[0].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[0].distance.pattern_min)
			continue;
		diff[0] = distance / runtime->soln_target[0].distance.plate_actual;

		/* plate object to candidate object 1 */
		distance = distance_get_equ(solution->object[1], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[1].distance.pattern_min,
			runtime->soln_target[1].distance.pattern_max, 2);

		if (distance > runtime->soln_target[1].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[1].distance.pattern_min)
			continue;
		diff[1] = distance / runtime->soln_target[1].distance.plate_actual;

		/* plate object to candidate object 2 */
		distance = distance_get_equ(solution->object[2], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[2].distance.pattern_min,
			runtime->soln_target[2].distance.pattern_max, 3);

		if (distance > runtime->soln_target[2].distance.pattern_max)
			continue;
		if (distance < runtime->soln_target[2].distance.pattern_min)
			continue;
		diff[2] = distance / runtime->soln_target[2].distance.plate_actual;

		/* plate object to candidate object 3 */
		distance = distance_get_equ(solution->object[3], s);

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

		target_add_single_match_on_distance(runtime, s, &solution->source,
				diverge, solution->flip);
		count++;
	}

	return count;
}

int distance_solve_single_object_extended(struct solve_runtime *runtime,
	struct adb_solve_solution *solution)
{
	const struct adb_object *s;
	struct magnitude_range *range = &runtime->pot_magnitude;
	int i, count = 0;
	double distance, diff[4], diverge, size;

	/* check distance ratio for each matching candidate against targets */
	runtime->num_pot_distance = 0;

	/* check t0 candidates */
	for (i = range->start[0]; i < range->end[0]; i++) {

		s = solution->source.objects[i];

		/* convert object size from arcmin to radians */
		size = (s->size / 60.0) * D2R;

		SOBJ_CHECK(s);

		/* plate object to candidate object 0 */
		distance = distance_get_equ(solution->object[0], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[0].distance.pattern_min,
			runtime->soln_target[0].distance.pattern_max, 1);

		if (distance > runtime->soln_target[0].distance.pattern_max + size)
			continue;
		if (distance < runtime->soln_target[0].distance.pattern_min - size)
			continue;
		diff[0] = distance / runtime->soln_target[0].distance.plate_actual;

		/* plate object to candidate object 1 */
		distance = distance_get_equ(solution->object[1], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[1].distance.pattern_min,
			runtime->soln_target[1].distance.pattern_max, 2);

		if (distance > runtime->soln_target[1].distance.pattern_max + size)
			continue;
		if (distance < runtime->soln_target[1].distance.pattern_min - size)
			continue;
		diff[1] = distance / runtime->soln_target[1].distance.plate_actual;

		/* plate object to candidate object 2 */
		distance = distance_get_equ(solution->object[2], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[2].distance.pattern_min,
			runtime->soln_target[2].distance.pattern_max, 3);

		if (distance > runtime->soln_target[2].distance.pattern_max + size)
			continue;
		if (distance < runtime->soln_target[2].distance.pattern_min - size)
			continue;
		diff[2] = distance / runtime->soln_target[2].distance.plate_actual;

		/* plate object to candidate object 3 */
		distance = distance_get_equ(solution->object[3], s);

		SOBJ_CHECK_DIST(s, distance,
			runtime->soln_target[3].distance.pattern_min,
			runtime->soln_target[3].distance.pattern_max, 4);

		if (distance > runtime->soln_target[3].distance.pattern_max + size)
			continue;
		if (distance < runtime->soln_target[3].distance.pattern_min - size)
			continue;
		diff[3] = distance / runtime->soln_target[3].distance.plate_actual;

		diverge = quad_diff(diff[0], diff[1], diff[2], diff[3]);

		SOBJ_FOUND(s);

		target_add_single_match_extended(runtime, s, &solution->source,
				diverge, solution->flip);
		count++;
	}

	return count;
}

/* check magnitude matched objects on pattern distance */
int distance_solve_object(struct solve_runtime *runtime,
	const struct adb_object *primary)
{
	const struct adb_object *s[3];
	struct target_object *t0, *t1, *t2;
	struct adb_solve *solve = runtime->solve;
	struct magnitude_range *range = &runtime->pot_magnitude;
	int i, j, k, count = 0;
	double distance0, distance1, distance2, rad_per_pixel;
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

		if (distance_not_within_fov(solve, primary, s[0]))
			continue;

		distance0 = distance_get_equ(primary, s[0]);

		/* rule out any distances > FOV */
		if (distance0 > solve->constraint.max_fov)
			continue;

		rad_per_pixel = distance0 / t0->distance.plate_actual;

		/* use ratio based on t0 <-> primary distance for t1 */
		t1_min = t1->distance.pattern_min * rad_per_pixel;
		t1_max = t1->distance.pattern_max * rad_per_pixel;

		DOBJ_CHECK_DIST(1, primary, s[0], distance0, 0.0, 0.0,
				range->end[0] - range->start[0], i);

		/* check each t1 candidates against t0 <-> primary distance ratio */
		for (j = range->start[1]; j < range->end[1]; j++) {

			s[1] = solve->source.objects[j];
			if (s[0] == s[1] || s[1] == primary)
				continue;

			DOBJ_CHECK(3, s[1]);

			if (distance_not_within_fov(solve, primary, s[1]))
				continue;

			distance1 = distance_get_equ(primary, s[1]);

			DOBJ_LIST(2, primary, s[1], distance1, j);

			/* rule out any distances > FOV */
			if (distance1 > solve->constraint.max_fov)
				continue;

			DOBJ_CHECK_DIST(2, primary, s[1], distance1, t1_min, t1_max,
					range->end[1] - range->start[1], j);

			/* is this t1 candidate within t0 primary ratio */
			if (distance1 >= t1_min && distance1 <= t1_max) {

				t2_min = t2->distance.pattern_min * rad_per_pixel;
				t2_max = t2->distance.pattern_max * rad_per_pixel;

				/* check t2 candidates */
				for (k = range->start[2]; k < range->end[2]; k++) {

					s[2] = solve->source.objects[k];
					if (s[0] == s[2] || s[1] == s[2] || s[2] == primary)
						continue;

					DOBJ_CHECK(4, s[2]);

					if (distance_not_within_fov(solve, primary, s[2]))
						continue;

					distance2 = distance_get_equ(primary, s[2]);

					DOBJ_LIST(3, primary, s[2], distance2, k);

					/* rule out any distances > FOV */
					if (distance2 > solve->constraint.max_fov)
						continue;

					DOBJ_CHECK_DIST(3, primary, s[2], distance2, t2_min, t2_max, 0, k);

					if (distance2 >= t2_min && distance2 <= t2_max) {

						double ratio1, ratio2, delta;

						DOBJ_CHECK_DIST(4, primary, s[2], distance2, t2_min, t2_max, 0, 0);

						ratio1 = distance1 / t1->distance.plate_actual;
						ratio2 = distance2 / t2->distance.plate_actual;

						delta = tri_diff(rad_per_pixel, ratio1, ratio2);

						target_add_match_on_distance(runtime, primary, &solve->source,
							i, j, k, delta, tri_avg(rad_per_pixel, ratio1, ratio2));
						count++;
					}
				}
			}
		}
	}

	return count;
}
