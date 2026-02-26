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

/* qsort() compare object magnitude callback */
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

/*
 * Ratio of magnitude from objects A,B
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

/*
 * Calculate the average difference between plate ADU values and solution
 * objects. Use this as basis for calculating magnitudes based on plate ADU.
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

/*
 * Binary search the haystack for object with magnitude nearest value
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

/*
 * Find first object with in haystack magnitude >= Vmag
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

/*
 * Find last object with in haystack magnitude <= Vmag
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

/*
 * Search the haystack db objects using the primary as the reference object and
 * then search the haystack for objects within the brightness ration for the target
 * pattern[idx] object.
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

/* compare pattern objects magnitude against source objects */
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
