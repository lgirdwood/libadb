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

/* compare plate object brightness against the brightness of all the reference
 * plate objects and then use the average differences to calculate magnitude */
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
		plate_delta = get_plate_mag_diff(&reft->pobject, &ref->pobject);

		/* difference between catalog objects */
		db_delta = ref->object->mag - reft->object->mag;

		/* calculate average difference */
		mean += db_delta - plate_delta;
		count++;
	}

	mean /= count;
	return mean;
}

/* TODO: investigate speedup with only 4 ref objects found in soln */
/* compare plate object brightness against the brightness of all the refernce
 * plate objects and then use the average differences to calculate magnitude
 * sigma */
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
		plate_delta = get_plate_mag_diff(&ref->pobject, &reft->pobject);

		/* delta between target object mag and detected object mag */
		db_delta = ref->object->mag - reft->object->mag;

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

void calc_plate_magnitude_coefficients(struct adb_solve *solve,
	struct adb_solve_solution *solution)
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
				ref->mag_sigma = get_ref_mag_delta_sigma(solution, i, ref->mag_mean);
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
			t *=t;
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

/* TODO: investigate speedup with only 4 ref objects found in soln */
/* calculate the magnitude of an unsolved plate object */
void calc_unsolved_plate_magnitude(struct adb_solve *solve,
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
		if (ref->clip_mag)
			continue;

		/* calculate mean difference in magnitude */
		mean += ref->object->mag +
			get_plate_mag_diff(&ref->pobject,
			&solution->solve_object[target].pobject);
		count++;
	}

	/* use mean magnitude as estimated plate magnitude */
	solution->solve_object[target].mag = mean / count;
}

/* calculate the magnitude of all unsolved plate objects */
void calc_unsolved_plate_magnitudes(struct adb_solve *solve,
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

/* calculate the magnitude, mag delta mean and mag delta sigma
 * of a solved plate object */
void calc_solved_plate_magnitude(struct adb_solve *solve,
	struct adb_solve_solution *solution)
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

		solution->solve_object[idx].sigma =
			get_ref_mag_delta_sigma(solution, i, solution->solve_object[idx].mean);
	}
}
