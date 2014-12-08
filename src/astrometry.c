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

static void equ_to_plate(struct adb_solve_solution *solution,
	const struct adb_object *o1, const struct adb_object *o2,
	struct adb_pobject *p1, struct adb_pobject *p2,
	double ra, double dec, int *x_, int *y_)
{
	struct adb_object otarget;
	double plate_pa, equ_pa, delta_pa;
	double plate_dist, equ_dist;
	double rad_per_pixel, target_pa, target_dist;

	/* delta PA between plate and equ */
	plate_pa = equ_quad(pa_get_plate(p1, p2));
	equ_pa = equ_quad(pa_get_equ(o1, o2));
	delta_pa = plate_pa - equ_pa;

	/* delta distance between plate and equ */
	plate_dist = distance_get_plate(p1, p2);
	equ_dist = distance_get_equ(o1, o2);
	rad_per_pixel = equ_dist / plate_dist;

	/* plate PA between object1 and target */
	otarget.ra = ra;
	otarget.dec = dec;
	target_pa = equ_quad(pa_get_equ(o1, &otarget));
	target_pa += delta_pa;

	/* EQU dist between object1 and target */
	target_dist = distance_get_equ(o1, &otarget);
	target_dist /= rad_per_pixel;

	/* add angle and distance onto P1 */
	*x_ = p1->x - cos(target_pa) * target_dist;
	*y_ = p1->y - sin(target_pa) * target_dist;
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
	double rad_per_pixel, target_pa, target_dist;
	double ra, dec, mid_dec;

	/* delta PA between plate and equ */
	plate_pa = equ_quad(pa_get_plate(p1, p2));
	equ_pa = equ_quad(pa_get_equ(o1, o2));
	delta_pa = plate_pa - equ_pa;

	/* delta distance between plate and equ */
	plate_dist = distance_get_plate(p1, p2);
	equ_dist = distance_get_equ(o1, o2);
	rad_per_pixel = equ_dist / plate_dist;

	/* EQU PA between object1 and target */
	target_pa = equ_quad(pa_get_plate(p1, ptarget));
	target_pa -= delta_pa;

	/* EQU dist between object1 and target */
	target_dist = distance_get_plate(p1, ptarget);
	target_dist *= rad_per_pixel;

	/* middle declination of line */
	mid_dec = o1->dec + ((o2->dec - o1->dec) / 2.0);

	/* Add line to object o1, reverse RA since RHS of plate increases X and
	 * RHS of sky is decreasing in RA.
	 */
	ra = -cos(target_pa) * target_dist / cos(mid_dec);
	dec = -sin(target_pa) * target_dist;

	/* make sure DEC is -90.0 .. 90.0 */
	dec = dec + o1->dec;
	if (dec > M_PI_2) {
		dec = M_PI_2 - (dec - M_PI_2);
		ra += M_PI; /* swap sides */
	} else if (dec < -M_PI_2) {
		dec = -M_PI_2 - (dec + M_PI_2);
		ra += M_PI; /* swap sides */
	}

	/* make sure RA is between 0..360 */
	ra = ra + o1->ra;
	if (ra > 2.0 * M_PI)
		ra -= 2.0 * M_PI;
	if (ra < 0.0)
		ra += 2.0 * M_PI;

	*ra_ = ra;
	*dec_ = dec;
}

/* compare plate object brightness against the brightness of all the reference
 * plate objects and then use the average differences to calculate magnitude */
static double get_ref_posn_delta_mean(struct adb_solve_solution *solution,
	int target)
{
	struct adb_reference_object *ref, *reft;
	double mean = 0.0, plate_dist, equ_dist, rad_per_pixel;
	int i, count = 0;

	reft = &solution->ref[target];

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		if (ref->clip_posn || i == target)
			continue;

		/* dont compare object against itself */
		if (ref->pobject.x == reft->pobject.x &&
			ref->pobject.y == reft->pobject.y)
			continue;

		/* get plate distance */
		plate_dist = distance_get_plate(&reft->pobject, &ref->pobject);

		/* get equ distance */
		equ_dist = distance_get_equ(reft->object, ref->object);

		/* get plate/equ ratio */
		rad_per_pixel = equ_dist / plate_dist;

		mean += rad_per_pixel;
		count++;
	}

	return mean / (double) count;
}

static double get_ref_posn_delta_sigma(struct adb_solve_solution *solution,
	int target, double mean)
{
	struct adb_reference_object *ref, *reft;
	double sigma = 0.0, plate_dist, equ_dist, rad_per_pixel;
	int i, count = 0;

	reft = &solution->ref[target];

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects - 1; i++) {
		ref = &solution->ref[i];

		if (ref->clip_posn || i == target)
			continue;

		/* dont compare object against itself */
		if (ref->pobject.x == reft->pobject.x &&
			ref->pobject.y == reft->pobject.y)
			continue;

		/* get plate distance */
		plate_dist = distance_get_plate(&reft->pobject, &ref->pobject);

		/* get equ distance */
		equ_dist = distance_get_equ(reft->object, ref->object);

		/* get plate/equ ratio */
		rad_per_pixel = equ_dist / plate_dist;

		rad_per_pixel -= reft->dist_mean;
		rad_per_pixel *= rad_per_pixel;
		sigma += rad_per_pixel;
		count++;
	}

	sigma /= (double)count;
	return sqrtf(sigma);
}

/* calculate the average difference between plate position values and solution
 * objects. Use this as basis for calculating RA,DEC based on plate x,y.
 */
void posn_equ_to_plate(struct adb_solve_solution *solution,
	double ra, double dec, double *x_, double *y_)
{
	struct adb_reference_object *ref, *refn;
	int x_sum = 0, y_sum = 0, i, j, count = 0, x, y;

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		for (j = 0; j < solution->num_ref_objects; j++) {
			ref = &solution->ref[i];
			refn = &solution->ref[j];

			if (j == i)
				continue;

			if (ref->clip_posn || refn->clip_posn)
					continue;

			/* dont compare objects against itself */
			if (ref->pobject.x == refn->pobject.x &&
				ref->pobject.y == refn->pobject.y)
					continue;

			equ_to_plate(solution, ref->object, refn->object,
						&ref->pobject, &refn->pobject, ra, dec, &x, &y);

			x_sum += x;
			y_sum += y;
			count++;
		}
	}

	*x_ = (double)x_sum / (double)count;
	*y_ = (double)y_sum / (double)count;
}

/* calculate the average difference between plate position values and solution
 * objects. Use this as basis for calculating RA,DEC based on plate x,y.
 */
void posn_plate_to_equ(struct adb_solve_solution *solution,
	struct adb_pobject *primary, double *ra_, double *dec_)
{
	struct adb_reference_object *ref, *refn;
	double ra_sum = 0.0, dec_sum = 0.0, ra, dec;
	int i, j, count = 0;

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		for (j = 0; j < solution->num_ref_objects; j++) {
			ref = &solution->ref[i];
			refn = &solution->ref[j];

			if (j == i)
				continue;

			if (ref->clip_posn || refn->clip_posn)
					continue;

			/* dont primary object against itself */
			if (ref->pobject.x == primary->x &&
				ref->pobject.y == primary->y)
				continue;

			/* dont primary object against itself */
			if (refn->pobject.x == primary->x &&
				refn->pobject.y == primary->y)
				continue;

			/* dont compare objects against itself */
			if (ref->pobject.x == refn->pobject.x &&
				ref->pobject.y == refn->pobject.y)
					continue;

			plate_to_equ(solution, ref->object, refn->object,
						&ref->pobject, &refn->pobject, primary, &ra, &dec);

			ra_sum += ra;
			dec_sum += dec;
			count++;
		}
	}

	*ra_ = ra_sum / count;
	*dec_ = dec_sum / count;
}

void posn_clip_plate_coefficients(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	struct adb_reference_object *ref;
	int i, count = 0, tries = 10, lastcount;
	double mean_sigma = 0.0, sigma_sigma = 0.0, t, clip;

	/* we need at least 3 reference objects */
	if (solution->num_ref_objects < 3)
		return;

	do {

		lastcount = count;
		count = 0;

		/* calc mean sigma delta per reference target */
		for (i = 0; i < solution->num_ref_objects; i++) {
				ref = &solution->ref[i];

				if (ref->clip_posn)
						continue;

				ref->dist_mean = get_ref_posn_delta_mean(solution, i);
				ref->posn_sigma = get_ref_posn_delta_sigma(solution, i, ref->dist_mean);

				mean_sigma += ref->posn_sigma;
				count++;
		}

		if (count == 0)
			return;

		mean_sigma /= count;

		/* calc sigm sigma delta per target */
		count = 0;
		sigma_sigma = 0.0;
		for (i = 0; i < solution->num_ref_objects; i++) {
			ref = &solution->ref[i];

			if (ref->clip_posn)
				continue;

			t = ref->posn_sigma - mean_sigma;
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
		/* clip objects outside sigma */
		for (i = 0; i < solution->num_ref_objects; i++) {
						ref = &solution->ref[i];

						if (ref->posn_sigma >= clip) {
							ref->clip_posn = 1;
							count++;
						}
		}

		/* clip targets outside sigma */
	} while (count != lastcount && --tries);
}

/* calculate the position of solved plate objects */
void posn_calc_solved_plate(struct adb_solve *solve,
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

		posn_plate_to_equ(solution, &ref->pobject, &solution->solve_object[idx].ra,
				&solution->solve_object[idx].dec);
	}
}

/* calculate the position of unsolved plate objects */
void posn_calc_unsolved_plate(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	struct adb_solve_object *solve_object;
	int i;

	/* compare each detected object against reference objects */
	for (i = 0; i < solution->num_ref_objects; i++) {

		solve_object = &solution->solve_object[i];

		/* skip if the object is solved */
		if (solve_object->object)
				continue;

		posn_plate_to_equ(solution, &solution->pobjects[i],
				&solution->solve_object[i].ra,
				&solution->solve_object[i].dec);
	}
}
