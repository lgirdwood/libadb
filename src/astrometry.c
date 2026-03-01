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

#ifdef DEBUG
#include <stdio.h>
#endif
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <pthread.h>

#include "solve.h"

/**
 * @brief Converts equatorial coordinates back to plate pixel coordinates.
 *
 * Uses two reference objects (o1, o2 and their corresponding plate coords p1, p2)
 * to calculate the transformation scale and rotation, and then applies this
 * transformation to find the plate position (x_, y_) for a target equatorial position.
 *
 * @param solution The active solve solution (unused in this function).
 * @param o1 The first reference object in equatorial coordinates.
 * @param o2 The second reference object in equatorial coordinates.
 * @param p1 The first reference object in plate coordinates.
 * @param p2 The second reference object in plate coordinates.
 * @param ra Target Right Ascension to convert.
 * @param dec Target Declination to convert.
 * @param x_ Output pointer for calculated plate X coordinate.
 * @param y_ Output pointer for calculated plate Y coordinate.
 */
static void equ_to_plate(struct adb_solve_solution *solution,
						 const struct adb_object *o1,
						 const struct adb_object *o2, struct adb_pobject *p1,
						 struct adb_pobject *p2, double ra, double dec, int *x_,
						 int *y_)
{
	(void)solution;
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

/**
 * @brief Converts plate pixel coordinates to equatorial coordinates.
 *
 * Uses two solved reference objects to calculate plate scale and rotation,
 * and then computes the RA/DEC corresponding to a target plate position
 * by interpolating from the references.
 *
 * @param solution The active solve solution (unused in this function).
 * @param o1 The first reference object in equatorial coordinates.
 * @param o2 The second reference object in equatorial coordinates.
 * @param p1 The first reference object in plate coordinates.
 * @param p2 The second reference object in plate coordinates.
 * @param ptarget The target plate object to convert.
 * @param ra_ Output pointer for calculated Right Ascension.
 * @param dec_ Output pointer for calculated Declination.
 */
static void plate_to_equ(struct adb_solve_solution *solution,
						 const struct adb_object *o1,
						 const struct adb_object *o2, struct adb_pobject *p1,
						 struct adb_pobject *p2, struct adb_pobject *ptarget,
						 double *ra_, double *dec_)
{
	(void)solution;
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

/**
 * @brief Calculates the mean positional difference ratio per reference target.
 *
 * Scans all reference objects to compute the average ratio of equatorial distance
 * to plate distance (radians per pixel), relative to the specified target.
 *
 * @param solution The active solve solution holding reference objects.
 * @param target The index of the reference object being evaluated.
 * @return The mean radians per pixel ratio for the target.
 */
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

	if (count == 0)
		return 0.0;

	return mean / (double)count;
}

/**
 * @brief Calculates the positional variance (sigma) for a reference target.
 *
 * Computes the standard deviation of the plate/equ_dist ratios for all
 * reference objects relative to the provided target object, against the mean.
 *
 * @param solution The active solve solution holding reference objects.
 * @param target The index of the reference object being evaluated.
 * @param mean The previously calculated mean distance ratio.
 * @return The standard deviation representing positional variance.
 */
static double get_ref_posn_delta_sigma(struct adb_solve_solution *solution,
									   int target, double mean)
{
	(void)mean;
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

	if (count == 0)
		return 0.0;

	sigma /= (double)count;
	return sqrtf(sigma);
}

/**
 * @brief Averages equatorial-to-plate coordinate transformations.
 *
 * Calculates plate X,Y coordinates for a given RA/DEC by independently computing
 * the transformation using every pair of reference objects and averaging the result.
 *
 * @param solution The active solve solution holding reference objects.
 * @param ra Target Right Ascension to convert.
 * @param dec Target Declination to convert.
 * @param x_ Output pointer for averaged plate X coordinate.
 * @param y_ Output pointer for averaged plate Y coordinate.
 */
void posn_equ_to_plate(struct adb_solve_solution *solution, double ra,
					   double dec, double *x_, double *y_)
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

			equ_to_plate(solution, ref->object, refn->object, &ref->pobject,
						 &refn->pobject, ra, dec, &x, &y);

			x_sum += x;
			y_sum += y;
			count++;
		}
	}

	if (count > 0) {
		*x_ = (double)x_sum / (double)count;
		*y_ = (double)y_sum / (double)count;
	} else {
		*x_ = 0;
		*y_ = 0;
	}
}

/**
 * @brief Quickly averages equatorial-to-plate coordinate transformations.
 *
 * Similar to `posn_equ_to_plate`, but limits the transformation iterations to
 * only the most confident reference objects (up to `MIN_PLATE_OBJECTS`).
 *
 * @param solution The active solve solution.
 * @param ra Target Right Ascension to convert.
 * @param dec Target Declination to convert.
 * @param x_ Output pointer for plate X coordinate.
 * @param y_ Output pointer for plate Y coordinate.
 */
void posn_equ_to_plate_fast(struct adb_solve_solution *solution, double ra,
							double dec, double *x_, double *y_)
{
	struct adb_reference_object *ref, *refn;
	int x_sum = 0, y_sum = 0, i, j, count = 0, x, y, objects;

	if (solution->num_ref_objects < MIN_PLATE_OBJECTS)
		objects = solution->num_ref_objects;
	else
		objects = MIN_PLATE_OBJECTS;

	/* get RA, DEC for each reference object */
	for (i = 0; i < objects; i++) {
		for (j = 0; j < objects; j++) {
			ref = &solution->ref[i];
			refn = &solution->ref[j];

			if (j == i)
				continue;

			/* dont compare objects against itself */
			if (ref->pobject.x == refn->pobject.x &&
				ref->pobject.y == refn->pobject.y)
				continue;

			equ_to_plate(solution, ref->object, refn->object, &ref->pobject,
						 &refn->pobject, ra, dec, &x, &y);

			x_sum += x;
			y_sum += y;
			count++;
		}
	}

	if (count > 0) {
		*x_ = (double)x_sum / (double)count;
		*y_ = (double)y_sum / (double)count;
	} else {
		*x_ = 0;
		*y_ = 0;
	}
}

/**
 * @brief Averages plate-to-equatorial coordinate transformations.
 *
 * Calculates RA/DEC corresponding to a plate position by independently computing
 * the transformation using every pair of reference objects and averaging the result.
 *
 * @param solution The active solve solution holding reference objects.
 * @param primary The target plate object coordinates.
 * @param ra_ Output pointer for averaged Right Ascension.
 * @param dec_ Output pointer for averaged Declination.
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
			if (ref->pobject.x == primary->x && ref->pobject.y == primary->y)
				continue;

			/* dont primary object against itself */
			if (refn->pobject.x == primary->x && refn->pobject.y == primary->y)
				continue;

			/* dont compare objects against itself */
			if (ref->pobject.x == refn->pobject.x &&
				ref->pobject.y == refn->pobject.y)
				continue;

			plate_to_equ(solution, ref->object, refn->object, &ref->pobject,
						 &refn->pobject, primary, &ra, &dec);

			ra_sum += ra;
			dec_sum += dec;
			count++;
		}
	}

	if (count > 0) {
		*ra_ = ra_sum / count;
		*dec_ = dec_sum / count;
	} else {
		*ra_ = 0.0;
		*dec_ = 0.0;
	}
}

/**
 * @brief Quickly averages plate-to-equatorial coordinate transformations.
 *
 * Faster version of `posn_plate_to_equ` that only tests up to `MIN_PLATE_OBJECTS`
 * combinations of reference objects to derive transformation geometry.
 *
 * @param solution The active solve solution holding reference objects.
 * @param primary The target plate object coordinates.
 * @param ra_ Output pointer for calculated Right Ascension.
 * @param dec_ Output pointer for calculated Declination.
 */
void posn_plate_to_equ_fast(struct adb_solve_solution *solution,
							struct adb_pobject *primary, double *ra_,
							double *dec_)
{
	struct adb_reference_object *ref, *refn;
	double ra_sum = 0.0, dec_sum = 0.0, ra, dec;
	int i, j, count = 0, objects;

	if (solution->num_ref_objects < MIN_PLATE_OBJECTS)
		objects = solution->num_ref_objects;
	else
		objects = MIN_PLATE_OBJECTS;

	/* get RA, DEC for each reference object */
	for (i = 0; i < objects; i++) {
		for (j = 0; j < objects; j++) {
			ref = &solution->ref[i];
			refn = &solution->ref[j];

			if (j == i)
				continue;

			/* dont primary object against itself */
			if (ref->pobject.x == primary->x && ref->pobject.y == primary->y)
				continue;

			/* dont primary object against itself */
			if (refn->pobject.x == primary->x && refn->pobject.y == primary->y)
				continue;

			/* dont compare objects against itself */
			if (ref->pobject.x == refn->pobject.x &&
				ref->pobject.y == refn->pobject.y)
				continue;

			plate_to_equ(solution, ref->object, refn->object, &ref->pobject,
						 &refn->pobject, primary, &ra, &dec);

			ra_sum += ra;
			dec_sum += dec;
			count++;
		}
	}

	if (count > 0) {
		*ra_ = ra_sum / count;
		*dec_ = dec_sum / count;
	} else {
		*ra_ = 0.0;
		*dec_ = 0.0;
	}
}

/**
 * @brief Clips out reference objects with anomalous positional divergence.
 *
 * Iteratively computes the standard deviation of positional ratios for all
 * reference objects and flags out any elements that deviate beyond 1-sigma.
 * This statistically stabilizes the celestial coordinate transformations.
 *
 * @param solution The active solve solution holding reference objects.
 */
void posn_clip_plate_coefficients(struct adb_solve_solution *solution)
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
			ref->posn_sigma =
				get_ref_posn_delta_sigma(solution, i, ref->dist_mean);

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

/**
 * @brief Computes RA/DEC coordinates for all successfully matched plate objects.
 *
 * Applies the `posn_plate_to_equ` coordinate transformation to each
 * solved reference object mapping plate space to sky space.
 *
 * @param solution Executing layout framework bounds containing references.
 */
void posn_calc_solved_plate(struct adb_solve_solution *solution)
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

		posn_plate_to_equ(solution, &ref->pobject,
						  &solution->solve_object[idx].ra,
						  &solution->solve_object[idx].dec);
	}
}

/**
 * @brief Computes RA/DEC coordinates for all completely unrecognized plate objects.
 *
 * Extrapolates world celestial coordinates for extracted pixel point targets
 * lacking any explicit catalog counterpart map links, using matched catalog features
 * to anchor coordinate mappings.
 *
 * @param solution Executing layout framework bounds containing references.
 */
void posn_calc_unsolved_plate(struct adb_solve_solution *solution)
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
