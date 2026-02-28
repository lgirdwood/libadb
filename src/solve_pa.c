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

#include <math.h>
#include <pthread.h>

#include "debug.h" // IWYU pragma: keep
#include "solve.h"

/**
 * \brief Calculate Position Angle (PA) between two objects on a photographic plate.
 *
 * Determines the angle (in radians) of the secondary object relative to the 
 * primary object, utilizing cartesian pixel coordinates.
 *
 * \param primary Pointer to the first (center) plate object.
 * \param secondary Pointer to the second plate object.
 * \return The relative position angle in radians.
 */
double pa_get_plate(struct adb_pobject *primary, struct adb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return atan2(y, x);
}

/**
 * \brief Calculate Position Angle (PA) between two equatorial coordinates.
 *
 * Computes the bearing angle from a primary celestial object to a secondary
 * celestial object utilizing spherical trigonometry on RA/Dec coordinates.
 *
 * \param o1 Pointer to the primary reference celestial object.
 * \param o2 Pointer to the secondary celestial object.
 * \return The equatorial position angle in radians.
 */
double pa_get_equ(const struct adb_object *o1, const struct adb_object *o2)
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
	k = 2.0 / (1.0 + sin_pdec * sin_ra + cos_pdec * cos_dec * cos_ra_delta);

	/* calc plate X, Y */
	x = k * (cos_dec * sin(ra_delta));
	y = k * (cos_pdec * sin_dec - sin_pdec * cos_dec * cos_ra_delta);

	return atan2(y, x);
}

/**
 * \brief Add a successful position angle match to the runtime's potential list.
 *
 * If a matching 4-star cluster successfully passes all relative position
 * angle checks, it gets registered into the runtime tracker for final divergence scoring.
 *
 * \param runtime The solver execution state.
 * \param p The matching solution cluster structure.
 * \param delta The cumulative error divergence of the position angles.
 */
static void add_pot_on_pa(struct solve_runtime *runtime,
						  struct adb_solve_solution *p, double delta)
{
	if (runtime->num_pot_pa >= MAX_ACTUAL_MATCHES)
		return;

	p->delta.pa = delta;
	runtime->pot_pa[runtime->num_pot_pa++] = *p;
}

/**
 * \brief Validate if a measured Position Angle delta falls within tolerance bounds.
 *
 * Checks if a candidate's angular separation conforms to the expected 
 * pattern constraints recorded from the original plate. Accounts for flipped
 * image logic where angles mirror.
 *
 * \param t The needle pattern object containing the allowed angle tolerances.
 * \param delta The actual measured position angle difference in radians.
 * \param flip Boolean flag indicating if the image/plate appears horizontally mirrored.
 * \return 1 if valid and within bounds, 0 if invalid.
 */
static inline int pa_valid(struct needle_object *t, double delta, int flip)
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

/**
 * \brief Evaluate candidate asterisms against measured position angles.
 *
 * Takes previously filtered clusters (that already passed distance ratio checks)
 * and measures the specific internal position angles between their stars. 
 * If the angles match the plate's asterism pattern, the cluster survives.
 *
 * \param runtime The current solver context containing potential candidate lists.
 * \param primary The target primary catalog object acting as the asterism center.
 * \param idx The needle index (unused logically in this specific function form).
 * \return The number of candidate asterisms successfully pattern matched on position angles.
 */
int pa_solve_object(struct solve_runtime *runtime,
					const struct adb_object *primary, int idx)
{
	struct adb_solve_solution *p;
	struct adb_solve *solve = runtime->solve;
	struct needle_object *t0, *t1, *t2;
	double pa1, pa2, pa3, pa_delta12, pa_delta23, pa_delta31, delta;
	int i, count = 0;

	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];

	/* for each potential cluster */
	for (i = runtime->num_pot_distance_checked; i < runtime->num_pot_distance;
		 i++) {
		p = &runtime->pot_distance[i];
		p->flip = 0;

		/* check PA for primary to each secondary object */
		pa1 = pa_get_equ(p->object[0], p->object[1]);
		pa2 = pa_get_equ(p->object[0], p->object[2]);
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
		pa3 = pa_get_equ(p->object[0], p->object[3]);
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

/**
 * \brief Evaluate candidate objects against measured position angles for a single target framework.
 *
 * Works through the set of potential candidates extending a single-object
 * solution frame, verifying if adding the candidate preserves the internal
 * position angle relationships required by the photographic plate measurements.
 *
 * \param runtime The active solver execution state containing candidates.
 * \param solution A proposed baseline solution model determining angle norms.
 * \return The number of successful candidate object matches verified by position angle.
 */
int pa_solve_single_object(struct solve_runtime *runtime,
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
		pa0 = pa_get_equ(p->object[0], solution->object[0]);
		pa1 = pa_get_equ(p->object[0], solution->object[1]);
		pa_delta01 = pa0 - pa1;
		if (pa_delta01 < 0.0)
			pa_delta01 += 2.0 * M_PI;
		if (!pa_valid(&runtime->soln_target[0], pa_delta01, p->flip))
			continue;

		/* check object against 1 -> 2 */
		pa2 = pa_get_equ(p->object[0], solution->object[2]);
		pa_delta12 = pa1 - pa2;
		if (pa_delta12 < 0.0)
			pa_delta12 += 2.0 * M_PI;
		if (!pa_valid(&runtime->soln_target[1], pa_delta12, p->flip))
			continue;

		/* check object against 2 -> 3 */
		pa3 = pa_get_equ(p->object[0], solution->object[3]);
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
