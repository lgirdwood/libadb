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

#include "solve.h"

/**
 * \brief Calculate the mean magnitude difference between a target and reference objects.
 *
 * Compares the brightness of a specific plate object against all successfully
 * solved reference plate objects, calculating average differences to establish a 
 * magnitude baseline.
 *
 * \param solution Active solved layout containing both references and target.
 * \param target Internal index within the referenced array for the source object.
 * \return The computed baseline mean magnitude delta.
 */
static float get_ref_mag_delta_mean(struct adb_solve_solution *solution,
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
		if (ref->clip_mag)
			continue;

		/* difference between plate objects */
		plate_delta = mag_get_plate_diff(&reft->pobject, &ref->pobject);

		/* difference between catalog objects */
		db_delta = ref->object->mag - reft->object->mag;

		/* calculate average difference */
		mean += db_delta - plate_delta;
		count++;
	}

	if (count)
		mean /= count;

	return mean;
}

/* TODO: investigate speedup with only 4 ref objects found in soln */
/**
 * \brief Calculate the magnitude variance (sigma) for a target object.
 *
 * Compares a plate object's relative brightness against reference objects and
 * computes the standard deviation (sigma) of its magnitude deltas against the mean.
 *
 * \param solution Active solved layout parameters containing matched components.
 * \param target Numeric index defining the object within the reference stack.
 * \param mean Pre-computed mean magnitude delta for the evaluated object.
 * \return Standard deviation representing magnitude error constraints.
 */
static float get_ref_mag_delta_sigma(struct adb_solve_solution *solution,
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

		/* ignore any object with mean difference > sigma */
		if (ref->clip_mag)
			continue;

		/* mag difference between target and solved plate object */
		plate_delta = mag_get_plate_diff(&ref->pobject, &reft->pobject);

		/* delta between target object mag and detected object mag */
		db_delta = ref->object->mag - reft->object->mag;

		/* calc sigma */
		diff = db_delta - plate_delta;
		diff -= mean;
		diff *= diff;
		sigma += diff;
		count++;
	}

	if (count)
		sigma /= count;

	return sqrtf(sigma);
}

/* TODO: investigate speedup with only 4 ref objects found in soln */
/**
 * \brief Estimate the absolute magnitude of an unsolved plate object.
 *
 * Extracts mean magnitude differences against confirmed references applying average
 * delta to approximate the visual magnitude for the remaining unmatched targets.
 *
 * \param solution Executing layout framework bounds containing registered references.
 * \param target Index position of the specific unsolved plate target points.
 */
static void calc_unsolved_plate_magnitude(struct adb_solve_solution *solution,
										  int target)
{
	struct adb_reference_object *ref;
	int i, count = 0;
	float mean = 0.0;

	/* compare plate object to reference objects */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		/* make sure solved object is close to catalog mag */
		/* otherwise reject it for magnitude calculation */
		if (ref->clip_mag)
			continue;

		/* calculate mean difference in magnitude */
		mean += ref->object->mag +
				mag_get_plate_diff(&ref->pobject,
								   &solution->solve_object[target].pobject);
		count++;
	}

	/* use mean magnitude as estimated plate magnitude */
	solution->solve_object[target].mag = mean / count;
}

/**
 * \brief Compile active magnitude baseline statistical coefficients filtering outliers.
 *
 * Performs repetitive clipping filtering anomalous magnitude deviations computing
 * standard deviance means identifying valid ranges excluding clipped values outside bounds.
 *
 * \param solution Runtime framework handling extracted positional references metrics.
 */
void mag_calc_plate_coefficients(struct adb_solve_solution *solution)
{
	struct adb_reference_object *ref;
	int i, count = 0, tries = 10, lastcount;
	float mean_sigma = 0.0, sigma_sigma = 0.0, t, clip;

	if (solution->num_ref_objects < 3)
		return;

	do {
		lastcount = count;
		count = 0;

		/* calc mean delta per reference target */
		for (i = 0; i < solution->num_ref_objects; i++) {
			ref = &solution->ref[i];

			if (ref->clip_mag)
				continue;

			ref->mag_mean = get_ref_mag_delta_mean(solution, i);
			ref->mag_sigma =
				get_ref_mag_delta_sigma(solution, i, ref->mag_mean);
			mean_sigma += ref->mag_sigma;
			count++;
		}

		if (count == 0)
			return;

		mean_sigma /= count;

		count = 0;
		sigma_sigma = 0.0;
		for (i = 0; i < solution->num_ref_objects; i++) {
			ref = &solution->ref[i];

			if (ref->clip_mag)
				continue;

			t = ref->mag_sigma - mean_sigma;
			t *= t;
			sigma_sigma += t;
			count++;
		}

		if (count == 0)
			return;

		sigma_sigma /= count;
		sigma_sigma = sqrtf(sigma_sigma);
		clip = mean_sigma + sigma_sigma;

		count = 0;
		for (i = 0; i < solution->num_ref_objects; i++) {
			ref = &solution->ref[i];

			if (ref->mag_sigma >= clip) {
				ref->clip_mag = 1;
				count++;
			}
		}

		/* clip targets outside sigma */
	} while (count != lastcount && --tries);
}

/**
 * \brief Extrapolate estimated magnitudes across the entire unsolved array block.
 *
 * Scans all raw instrumental point arrays computing reasonable absolute visual magnitude
 * values calibrated against the successfully locked celestial reference components metrics.
 *
 * \param solution Executing layout parameters identifying missing mapping boundaries constraints.
 */
void mag_calc_unsolved_plate(struct adb_solve_solution *solution)
{
	struct adb_solve_object *solve_object;
	int i;

	/* compare each unsolved object against reference objects */
	for (i = 0; i < solution->total_objects; i++) {
		solve_object = &solution->solve_object[i];

		/* skip if the object is solved */
		if (solve_object->object)
			continue;

		calc_unsolved_plate_magnitude(solution, i);
	}
}

/**
 * \brief Finalize empirical magnitude tracking parameters mapping resolved structures.
 *
 * Formalizes magnitude, average magnitude delta, and statistical variance properties directly
 * mapped tracking against correctly validated solved arrays configurations attributes.
 *
 * \param solution Baseline defining extracted values pointers.
 */
void mag_calc_solved_plate(struct adb_solve_solution *solution)
{
	struct adb_reference_object *ref;
	int i, idx;

	/* compare each detected object against reference objects */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];
		idx = ref->id;

		/* ignore objects that are unsolved */
		if (solution->solve_object[idx].object == NULL)
			continue;

		solution->solve_object[idx].mean = get_ref_mag_delta_mean(solution, i);

		solution->solve_object[idx].mag =
			solution->solve_object[idx].object->mag +
			solution->solve_object[idx].mean;

		solution->solve_object[idx].sigma = get_ref_mag_delta_sigma(
			solution, i, solution->solve_object[idx].mean);
	}
}
