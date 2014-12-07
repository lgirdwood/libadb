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

/* position angle in radians relative to plate north */
double get_plate_pa(struct adb_pobject *primary,
	struct adb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return atan2(y, x);
}

/* position angle in radians */
double get_equ_pa(const struct adb_object *o1, const struct adb_object *o2)
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

/* add matching cluster to list of potentials */
static void add_pot_on_pa(struct solve_runtime *runtime,
		struct adb_solve_solution *p, double delta)
{
	if (runtime->num_pot_pa >= MAX_ACTUAL_MATCHES)
		return;

	p->delta.pa = delta;
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

int solve_object_on_pa(struct solve_runtime *runtime,
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

int solve_single_object_on_pa(struct solve_runtime *runtime,
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
