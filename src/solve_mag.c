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
 * \brief Comparison callback for sorting objects by magnitude.
 *
 * Used with `qsort` to order an array of celestial objects according to
 * their visual magnitude in ascending order (brightest to dimmest, 
 * noting that lower magnitude values mean brighter objects).
 *
 * \param o1 Pointer to the first object pointer.
 * \param o2 Pointer to the second object pointer.
 * \return -1 if o1 is brighter, 1 if o1 is dimmer, 0 if equal.
 */
int mag_object_cmp(const void *o1, const void *o2)
{
	const struct adb_object *p1 = *(const struct adb_object **)o1,
							*p2 = *(const struct adb_object **)o2;

	if (p1->mag < p2->mag)
		return -1;
	else if (p1->mag > p2->mag)
		return 1;
	else
		return 0;
}

/**
 * \brief Calculate the magnitude difference between two plate objects.
 *
 * Computes the relative visual magnitude difference between two objects A and B
 * based on their measured Analog-to-Digital Units (ADU) from the photographic plate.
 *
 * \param a Pointer to the first plate object (often the primary).
 * \param b Pointer to the second plate object.
 * \return The relative magnitude difference (-2.5 * log10(b_adu / a_adu)).
 */
double mag_get_plate_diff(struct adb_pobject *a, struct adb_pobject *b)
{
	double b_adu = b->adu, a_adu = a->adu;

	/* catch any objects with 0 ADU */
	if (b_adu == 0)
		b_adu = 1;
	if (a_adu == 0)
		a_adu = 1;

	return -2.5 * log10(b_adu / a_adu);
}

/**
 * \brief Estimate the absolute visual magnitude of a primary plate object.
 *
 * Uses successfully solved reference objects to calibrate the plate's
 * background magnitude offset, then applies this average offset to determine
 * the estimated true magnitude of the primary target object.
 *
 * \param solve The current active solving session context.
 * \param solution The current proposed solution with matched reference objects.
 * \param primary The undetermined primary plate object to estimate magnitude for.
 * \return The estimated absolute visual magnitude.
 */
float mag_get_plate(struct adb_solve *solve,
					struct adb_solve_solution *solution,
					struct adb_pobject *primary)
{
	struct adb_reference_object *ref;
	double mag = 0.0;
	int i, count = 0;

	/* get RA, DEC for each reference object */
	for (i = 0; i < solution->num_ref_objects; i++) {
		ref = &solution->ref[i];

		if (ref->clip_mag)
			continue;

		mag += ref->object->mag + mag_get_plate_diff(&ref->pobject, primary);
		count++;
	}

	/* catch div by 0 */
	if (count == 0)
		return mag;

	return mag / (float)count;
}

/**
 * \brief Recursive binary search to find an object near a specific magnitude.
 *
 * Searches the sorted haystack of source objects for an object that has 
 * a magnitude closely matching the requested `value`.
 *
 * \param adb_source_objects Array of source object pointers, sorted by magnitude.
 * \param value The target visual magnitude to search for.
 * \param start The starting index of the search bounds.
 * \param end The ending index of the search bounds.
 * \param pos The current midpoint index being evaluated.
 * \return The index of the nearest matching object.
 */
static int bsearch_object(const struct adb_object **adb_source_objects,
						  double value, int start, int end, int pos)
{
	const struct adb_object *object = adb_source_objects[pos];

	/* search complete  ? */
	if (end - start < 2)
		return pos; /* search exhausted - return current position */

	/* narrow search - binary chop it */
	if (object->mag > value)
		/* look ahead of current object */
		return bsearch_object(adb_source_objects, value,
							  start, /* current start position */
							  pos - 1, /* new end position */
							  start + ((pos - start) / 2)); /* new mid point */
	else if (object->mag < value)
		/* look behind current object */
		return bsearch_object(adb_source_objects, value,
							  pos + 1, /* new start position */
							  end, /* current end position */
							  pos + ((end - pos) / 2)); /* new mid point */
	else
		return pos; /* magnitude equal - return position */
}

/**
 * \brief Find the first object in a sorted array with magnitude >= the target.
 *
 * Uses binary search to quickly locate the boundary index where objects
 * begin to have a magnitude greater than or equal to `vmag`.
 *
 * \param source The container holding the sorted array of source objects.
 * \param vmag The target lower-bound visual magnitude limit.
 * \param start_idx The starting index to constrain the search.
 * \return The index of the first object meeting the magnitude criteria.
 */
static int object_get_first_on_mag(struct adb_source_objects *source,
								   double vmag, int start_idx)
{
	const struct adb_object *object;
	int idx;

	/* find one of the first adb_source_objects >= vmag  */
	idx = bsearch_object(source->objects, vmag, start_idx,
						 source->num_objects - 1,
						 (source->num_objects - 1) / 2);
	object = source->objects[idx];

	/* make sure the object is first in the array amongst equals */
	if (object->mag < vmag) {
		/* make sure we return the idx of the object with equal vmag */
		for (idx++; idx < source->num_objects; idx++) {
			object = source->objects[idx];

			if (object->mag >= vmag)
				return idx - 1;
		}
	} else {
		/* make sure we return the idx of the object with equal vmag */
		for (idx--; idx > start_idx; idx--) {
			object = source->objects[idx];

			if (object->mag < vmag)
				return idx + 1;
		}
	}

	/* not found */
	return 0;
}

/**
 * \brief Find the last object in a sorted array with magnitude <= the target.
 *
 * Uses binary search to quickly locate the boundary index where objects
 * stop having a magnitude less than or equal to `vmag`.
 *
 * \param source The container holding the sorted array of source objects.
 * \param vmag The target upper-bound visual magnitude limit.
 * \param start_idx The starting index to constrain the search.
 * \return The index of the last object meeting the magnitude criteria.
 */
static int object_get_last_with_mag(struct adb_source_objects *source,
									double vmag, int start_idx)
{
	const struct adb_object *object;
	int idx;

	/* find one of the last adb_source_objects <= vmag  */
	idx = bsearch_object(source->objects, vmag, start_idx,
						 source->num_objects - 1,
						 (source->num_objects - 1) / 2);
	object = source->objects[idx];

	/* make sure the object is last in the array amongst equals */
	if (object->mag > vmag) {
		/* make sure we return the idx of the object with equal vmag */
		for (idx--; idx > start_idx; idx--) {
			object = source->objects[idx];

			if (object->mag <= vmag)
				return idx + 1;
		}
	} else {
		/* make sure we return the idx of the object with equal vmag */
		for (idx++; idx < source->num_objects; idx++) {
			object = source->objects[idx];

			if (object->mag > vmag)
				return idx - 1;
		}
	}

	/* not found */
	return source->num_objects - 1;
}

/**
 * \brief Filter candidate matching objects based on relative pattern magnitude.
 *
 * Searches the catalog haystack utilizing the proposed primary object 
 * as the anchor, and bounds the search for secondary star tuples depending 
 * on the allowed relative brightness ratio seen on the plate.
 *
 * \param runtime The solver execution state containing current matching subsets.
 * \param primary The target primary catalog object being evaluated.
 * \param idx The index of the needle pattern object dictating the magnitude ratio.
 * \return The number of candidate objects falling within the valid magnitude range.
 */
int mag_solve_object(struct solve_runtime *runtime,
					 const struct adb_object *primary, int idx)
{
	struct target_solve_mag *pot_mag = &runtime->pot_magnitude;
	struct adb_solve *solve = runtime->solve;
	struct needle_object *t = &solve->target.secondary[idx];
	struct adb_source_objects *source = &solve->haystack;
	int start, end, pos;

	/* get search start position */
	pos =
		object_get_first_on_mag(source, primary->mag - solve->tolerance.mag, 0);

	/* get start and end indices for secondary vmag */
	start =
		object_get_first_on_mag(source, t->mag.pattern_min + primary->mag, pos);
	end = object_get_last_with_mag(source, t->mag.pattern_max + primary->mag,
								   pos);

	/* both out of range */
	if (start == end)
		return 0;

	pot_mag->end_pos[idx] = end;

	/* no object */
	if (pot_mag->end_pos[idx] < start)
		return 0;

	/* is start out of range */
	pot_mag->start_pos[idx] = start;

	/* return number of candidate adb_source_objects based on vmag */
	return pot_mag->end_pos[idx] - pot_mag->start_pos[idx];
}

/**
 * \brief Filter solitary candidate matching objects utilizing calibrated absolute magnitude.
 *
 * Constrains potential candidate matches for a single unanchored plate object
 * by comparing its calibrated absolute magnitude against catalog stars.
 *
 * \param runtime The solver execution state.
 * \param solution A proposed solution model providing the reference calibration.
 * \param pobject The plate object being evaluated for magnitude limits.
 * \return The number of source objects passing the absolute magnitude check.
 */
int mag_solve_single_object(struct solve_runtime *runtime,
							struct adb_solve_solution *solution,
							struct adb_pobject *pobject)
{
	struct target_solve_mag *pot_mag = &runtime->pot_magnitude;
	struct adb_solve *solve = runtime->solve;
	struct adb_source_objects *source = &solution->source;
	int start, end;
	float mag_min, mag_max, plate_mag;

	plate_mag = mag_get_plate(solve, solution, pobject);

	mag_min = plate_mag - solution->delta.mag;
	mag_max = plate_mag + solution->delta.mag;

	/* get start and end indices for secondary vmag */
	start = object_get_first_on_mag(source, mag_min - solution->delta.mag, 0);

	end = object_get_last_with_mag(source, mag_max + solution->delta.mag, 0);

	/* both out of range */
	if (start == end)
		return 0;

	SOBJ_MAG(mag_min - solution->delta.mag, mag_max + solution->delta.mag);

	pot_mag->end_pos[0] = end;

	/* no object */
	if (pot_mag->end_pos[0] < start)
		return 0;

	/* is start out of range */
	pot_mag->start_pos[0] = start;

	/* return number of candidate adb_source_objects based on vmag */
	return pot_mag->end_pos[0] - pot_mag->start_pos[0];
}
