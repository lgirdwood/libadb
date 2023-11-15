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

/* ratio of magnitude from primary to secondary */
double mag_get_plate_diff(struct adb_pobject *primary,
	struct adb_pobject *secondary)
{
	double s_adu = secondary->adu, p_adu = primary->adu;

	/* catch any objects with 0 ADU */
	if (s_adu == 0)
		s_adu = 1;
	if (p_adu == 0)
		p_adu = 1;

	return -2.5 * log10(s_adu / p_adu);
}

/* calculate the average difference between plate ADU values and solution
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

		mag += ref->object->mag +
			mag_get_plate_diff(&ref->pobject, primary);
		count++;
	}

	if (count == 0)
		return mag;

	return mag / (float)count;
}

/* binary search the set for magnitude head */
static int bsearch_head(const struct adb_object **adb_source_objects,
	double value, int start, int end, int idx)
{
	const struct adb_object *object = adb_source_objects[idx];

	/* search complete */
	if (end - start < 2)
		return idx;

	/* narrow search */
	if (object->mag > value)
		return bsearch_head(adb_source_objects, value, start, idx - 1,
				start + ((idx - start) / 2));
	else if (object->mag < value)
		return bsearch_head(adb_source_objects, value, idx + 1, end,
				idx + ((end - idx) / 2));
	else
		return idx;
}

/* get first object with magnitude >= Vmag */
static int object_get_first_on_mag(struct adb_source_objects *source,
	double vmag, int start_idx)
{
	const struct adb_object *object;
	int idx;

	/* find one of the first adb_source_objects >= vmag  */
	idx = bsearch_head(source->objects, vmag, start_idx,
		source->num_objects - 1, (source->num_objects - 1) / 2);
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

/* binary search the set for magnitude tail */
static int bsearch_tail(const struct adb_object **adb_source_objects,
	double value, int start, int end, int idx)
{
	const struct adb_object *object = adb_source_objects[idx];

	/* search complete */
	if (end - start < 2)
		return idx;

	/* narrow search */
	if (object->mag > value)
		return bsearch_tail(adb_source_objects, value, start, idx - 1,
				start + ((idx - start) / 2));
	else if (object->mag < value)
		return bsearch_tail(adb_source_objects, value, idx + 1, end,
				idx + ((end - idx) / 2));
	else
		return idx;
}

/* get last object with magnitude <= Vmag */
static int object_get_last_with_mag(struct adb_source_objects *source,
		double vmag, int start_idx)
{
	const struct adb_object *object;
	int idx;

	/* find one of the last adb_source_objects <= vmag  */
	idx = bsearch_tail(source->objects, vmag, start_idx,
		source->num_objects - 1, (source->num_objects - 1) / 2);
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
 * Search the db objects using the primary as the brightest object from
 * our object pattern and find candidate pattern[idx] objects
 */
int mag_solve_object(struct solve_runtime *runtime,
		const struct adb_object *primary, int idx)
{
	struct magnitude_range *range = &runtime->pot_magnitude;
	struct adb_solve *solve = runtime->solve;
	struct target_object *t = &solve->target.secondary[idx];
	struct adb_source_objects *source = &solve->source;
	int start, end, pos;

	/* get search start position */
	pos = object_get_first_on_mag(source,
		primary->mag - solve->tolerance.mag, 0);

	/* get start and end indices for secondary vmag */
	start = object_get_first_on_mag(source,
		t->mag.pattern_min + primary->mag, pos);
	end = object_get_last_with_mag(source,
		t->mag.pattern_max + primary->mag, pos);

	/* both out of range */
	if (start == end)
		return 0;

	range->end[idx] = end;

	/* no object */
	if (range->end[idx] < start)
		return 0;

	/* is start out of range */
	range->start[idx] = start;

	/* return number of candidate adb_source_objects based on vmag */
	return range->end[idx] - range->start[idx];
}

/* compare pattern objects magnitude against source objects */
int mag_solve_single_object(struct solve_runtime *runtime,
		struct adb_solve_solution *solution,
		struct adb_pobject *pobject)
{
	struct magnitude_range *range = &runtime->pot_magnitude;
	struct adb_solve *solve = runtime->solve;
	struct adb_source_objects *source = &solution->source;
	int start, end;
	float mag_min, mag_max, plate_mag;

	plate_mag = mag_get_plate(solve, solution, pobject);

	mag_min = plate_mag - solution->delta.mag;
	mag_max = plate_mag + solution->delta.mag;

	/* get start and end indices for secondary vmag */
	start = object_get_first_on_mag(source,
			mag_min - solution->delta.mag, 0);

	end = object_get_last_with_mag(source,
			mag_max + solution->delta.mag, 0);

	/* both out of range */
	if (start == end)
		return 0;

	SOBJ_MAG(mag_min - solution->delta.mag,
		mag_max + solution->delta.mag);

	range->end[0] = end;

	/* no object */
	if (range->end[0] < start)
		return 0;

	/* is start out of range */
	range->start[0] = start;

	/* return number of candidate adb_source_objects based on vmag */
	return range->end[0] - range->start[0];
}
