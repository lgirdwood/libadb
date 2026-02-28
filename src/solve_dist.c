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

#include "debug.h"
#include "solve.h"

/**
 * \brief Calculate the distance between two objects on a photographic plate.
 *
 * Uses the Pythagorean theorem to determine the Cartesian distance in pixels
 * (or plate coordinates) between a primary and a secondary plate object.
 *
 * \param primary Pointer to the first plate object.
 * \param secondary Pointer to the second plate object.
 * \return The Cartesian distance between the two objects.
 */
double distance_get_plate(struct adb_pobject *primary,
						  struct adb_pobject *secondary)
{
	double x, y;

	x = (double)primary->x - (double)secondary->x;
	y = (double)primary->y - (double)secondary->y;

	return sqrt((x * x) + (y * y));
}

/**
 * \brief Calculate the great-circle distance between two equatorial coordinates.
 *
 * Computes the angular separation in radians between two celestial objects
 * given their Right Ascension (RA) and Declination (dec).
 *
 * \param o1 Pointer to the first celestial object.
 * \param o2 Pointer to the second celestial object.
 * \return The angular separation between the objects in radians.
 */
double distance_get_equ(const struct adb_object *o1,
						const struct adb_object *o2)
{
	double x, y, z;

	x = (cos(o1->dec) * sin(o2->dec)) -
		(sin(o1->dec) * cos(o2->dec) * cos(o2->ra - o1->ra));
	y = cos(o2->dec) * sin(o2->ra - o1->ra);
	z = (sin(o1->dec) * sin(o2->dec)) +
		(cos(o1->dec) * cos(o2->dec) * cos(o2->ra - o1->ra));

	x = x * x;
	y = y * y;

	return atan2(sqrt(x + y), z);
}

/**
 * \brief Check if a secondary object falls outside the Field of View (FoV) of a primary object.
 *
 * Provides a highly optimized, fast, and loose check to rule out matching
 * candidates that are definitely beyond the maximum allowed telescope
 * Field of View radius from the primary object before doing full rigorous math.
 *
 * \param solve The current active solving session context constraint settings.
 * \param p Pointer to the primary reference (center) object.
 * \param s Pointer to the secondary target object.
 * \return 1 if the object is strictly outside the maximum FoV, 0 if it may be inside.
 */
static inline int distance_not_within_fov(struct adb_solve *solve,
										  const struct adb_object *p,
										  const struct adb_object *s)
{
	double ra_diff = fabs(p->ra - s->ra);
	double dec_diff = s->dec + ((p->dec - s->dec) / 2.0);

	/* check for large angles near 0 and 2.0 * M_PI */
	if (ra_diff > M_PI)
		ra_diff -= 2.0 * M_PI;

	if (cos(dec_diff) * ra_diff > solve->constraint.max_fov)
		return 1;
	if (fabs(p->dec - s->dec) > solve->constraint.max_fov)
		return 1;
	return 0;
}

/**
 * \brief Attempt to solve a 4-star pattern match utilizing distance ratios targeting a single object.
 *
 * Evaluates candidate database "source" stars checking if their distances
 * to the proposed solution central object match the distances seen on the 
 * photographic plate. Records the divergence.
 *
 * \param runtime The active solver execution state containing candidate sets.
 * \param solution A proposed solution model tracking current matched objects.
 * \return The number of matched candidate objects verified for the target.
 */
int distance_solve_single_object(struct solve_runtime *runtime,
								 struct adb_solve_solution *solution)
{
	const struct adb_object *s;
	struct target_solve_mag *range = &runtime->pot_magnitude;
	int i, count = 0;
	double distance, diff[4], diverge;

	/* check distance ratio for each matching candidate against targets */
	runtime->num_pot_distance = 0;

	/* check t0 candidates */
	for (i = range->start_pos[0]; i < range->end_pos[0]; i++) {
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

/**
 * \brief Extended single object distance solver accounting for dynamic object sizes.
 *
 * A variation of `distance_solve_single_object` where the acceptance 
 * radius (distance constraints) is expanded depending on the physical or
 * angular size property assigned to the source object itself.
 *
 * \param runtime The active solver execution state.
 * \param solution A proposed solution model.
 * \return The number of successfully verified candidate source objects.
 */
int distance_solve_single_object_extended(struct solve_runtime *runtime,
										  struct adb_solve_solution *solution)
{
	const struct adb_object *s;
	struct target_solve_mag *range = &runtime->pot_magnitude;
	int i, count = 0;
	double distance, diff[4], diverge, size;

	/* check distance ratio for each matching candidate against targets */
	runtime->num_pot_distance = 0;

	/* check t0 candidates */
	for (i = range->start_pos[0]; i < range->end_pos[0]; i++) {
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

		target_add_single_match_extended(runtime, s, &solution->source, diverge,
										 solution->flip);
		count++;
	}

	return count;
}

/**
 * \brief Core astrometric solver routine utilizing asterism distance ratios.
 *
 * Performs a deep nested search through candidate source objects in the 
 * "haystack" catalog. For an unknown "primary" plate object, it builds
 * tuples of three neighboring stars and checks if the relative angular distances
 * between them match the pixel distance ratios measured off the image plate.
 * If a matching 4-star pattern is found, a potential solution is recorded.
 *
 * \param runtime The solver context state controlling current match iterations.
 * \param primary The target primary catalog object being evaluated as the center of the asterism.
 * \return The number of candidate asterisms successfully pattern matched.
 */
int distance_solve_object(struct solve_runtime *runtime,
						  const struct adb_object *primary)
{
	const struct adb_object *s[3];
	struct needle_object *t0, *t1, *t2;
	struct adb_solve *solve = runtime->solve;
	struct target_solve_mag *range = &runtime->pot_magnitude;
	int i, j, k, count = 0;
	double distance0, distance1, distance2, rad_per_pixel;
	double t1_min, t1_max, t2_max, t2_min;

	/* check distance ratio for each matching candidate against targets */
	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];
	runtime->num_pot_distance = 0;

	DOBJ_CHECK(1, primary);
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "0 start %d stop %d\n",
			   range->start_pos[0], range->end_pos[0]);
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "1 start %d stop %d\n",
			   range->start_pos[1], range->end_pos[1]);
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "2 start %d stop %d\n",
			   range->start_pos[2], range->end_pos[2]);
	/* check t0 candidates */
	for (i = range->start_pos[0]; i < range->end_pos[0]; i++) {
		/* dont solve against ourself */
		s[0] = solve->haystack.objects[i];
		if (s[0] == primary)
			continue;

		DOBJ_CHECK(2, s[0]);

		/* fast check: skip if primary and secondary not within plate fov */
		if (distance_not_within_fov(solve, primary, s[0]))
			continue;

		/* slower check: rule out any distances > plate FOV */
		distance0 = distance_get_equ(primary, s[0]);
		if (distance0 > solve->constraint.max_fov)
			continue;

		rad_per_pixel = distance0 / t0->distance.plate_actual;

		/* use ratio based on t0 <-> primary distance for t1 */
		t1_min = t1->distance.pattern_min * rad_per_pixel;
		t1_max = t1->distance.pattern_max * rad_per_pixel;

		DOBJ_CHECK_DIST(1, primary, s[0], distance0, 0.0, 0.0,
						range->end_pos[0] - range->start_pos[0], i);

		/* check each t1 candidates against t0 <-> primary distance ratio */
		for (j = range->start_pos[1]; j < range->end_pos[1]; j++) {
			/* dont solve against ourself */
			s[1] = solve->haystack.objects[j];
			if (s[0] == s[1] || s[1] == primary)
				continue;

			DOBJ_CHECK(3, s[1]);

			/* fast check: skip if primary and secondary not within plate fov */
			if (distance_not_within_fov(solve, primary, s[1]))
				continue;

			/* slower check: rule out any distances > plate FOV */
			distance1 = distance_get_equ(primary, s[1]);

			DOBJ_LIST(2, primary, s[1], distance1, j);

			/* rule out any distances > plate FOV */
			if (distance1 > solve->constraint.max_fov)
				continue;

			DOBJ_CHECK_DIST(2, primary, s[1], distance1, t1_min, t1_max,
							range->end_pos[1] - range->start_pos[1], j);

			/* is this t1 candidate within t0 primary ratio */
			if (distance1 >= t1_min && distance1 <= t1_max) {
				t2_min = t2->distance.pattern_min * rad_per_pixel;
				t2_max = t2->distance.pattern_max * rad_per_pixel;

				/* check t2 candidates */
				for (k = range->start_pos[2]; k < range->end_pos[2]; k++) {
					/* dont solve against ourself */
					s[2] = solve->haystack.objects[k];
					if (s[0] == s[2] || s[1] == s[2] || s[2] == primary)
						continue;

					DOBJ_CHECK(4, s[2]);

					/* fast check: skip if primary and secondary not within plate fov */
					if (distance_not_within_fov(solve, primary, s[2]))
						continue;

					/* slower check: rule out any distances > plate FOV */
					distance2 = distance_get_equ(primary, s[2]);

					DOBJ_LIST(3, primary, s[2], distance2, k);

					/* rule out any distances > FOV */
					if (distance2 > solve->constraint.max_fov)
						continue;

					DOBJ_CHECK_DIST(3, primary, s[2], distance2, t2_min, t2_max,
									0, k);

					if (distance2 >= t2_min && distance2 <= t2_max) {
						double ratio1, ratio2, delta;

						DOBJ_CHECK_DIST(4, primary, s[2], distance2, t2_min,
										t2_max, 0, 0);

						ratio1 = distance1 / t1->distance.plate_actual;
						ratio2 = distance2 / t2->distance.plate_actual;

						delta = tri_diff(rad_per_pixel, ratio1, ratio2);

						target_add_match_on_distance(
							runtime, primary, &solve->haystack, i, j, k, delta,
							tri_avg(rad_per_pixel, ratio1, ratio2));
						count++;
					}
				}
			}
		}
	}

	return count;
}
