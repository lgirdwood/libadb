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
#include <errno.h> // IWYU pragma: keep
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <pthread.h>

#include "debug.h"
#include "solve.h"

/**
 * \brief Comparison callback for sorting plate objects by ADU brightness.
 *
 * Used with `qsort` to order an array of photographic plate objects according to
 * their measured Analog-to-Digital Units (ADU) in descending order (brightest first).
 *
 * \param o1 Pointer to the first plate object.
 * \param o2 Pointer to the second plate object.
 * \return 1 if o1 is dimmer, -1 if o1 is brighter, 0 if equal.
 */
static int plate_object_cmp(const void *o1, const void *o2)
{
	const struct adb_pobject *p1 = o1, *p2 = o2;

	if (p1->adu < p2->adu)
		return 1;
	else if (p1->adu > p2->adu)
		return -1;
	else
		return 0;
}

/**
 * \brief Register a matched catalog reference object to an asterism solution.
 *
 * Anchors a successful catalog source star to its corresponding measured 
 * plate object within a given solver solution record. Eliminates duplicates.
 *
 * \param soln The target solution structure to populate.
 * \param id The index ID slot for this reference object.
 * \param object The confirmed catalog source object.
 * \param pobject The measured photographic plate object.
 * \return 1 if successfully added, 0 if it was already registered.
 */
int target_add_ref_object(struct adb_solve_solution *soln, int id,
						  const struct adb_object *object,
						  struct adb_pobject *pobject)
{
	struct adb_reference_object *ref;
	struct adb_solve *solve = soln->solve;
	int i;

	pthread_mutex_lock(&solve->mutex);

	/* first check to see if object is already present */
	for (i = 0; i < soln->num_ref_objects; i++) {
		ref = &soln->ref[i];
		if (ref->object == object && ref->id == id) {
			pthread_mutex_unlock(&solve->mutex);
			return 0;
		}
	}

	soln->ref[soln->num_ref_objects].object = object;
	soln->ref[soln->num_ref_objects].mag_mean = 0.0;
	soln->ref[soln->num_ref_objects].mag_sigma = 0.0;
	soln->ref[soln->num_ref_objects].id = id;
	soln->ref[soln->num_ref_objects].clip_mag = 0;
	soln->ref[soln->num_ref_objects].clip_posn = 0;
	soln->ref[soln->num_ref_objects++].pobject = *pobject;

	pthread_mutex_unlock(&solve->mutex);
	return 1;
}

/**
 * \brief Compute pattern characteristics for a secondary plate object relative to a primary.
 *
 * Populates a needle pattern object's relative baseline distance, magnitude difference, 
 * and position angle tolerances using the photographic plate measurements.
 *
 * \param solve The active solving session context.
 * \param target The secondary target needle index (0, 1, 2) in the asterism cluster.
 * \param primary Pointer to the center primary plate object.
 * \param secondary Pointer to the specific secondary plate object being evaluated.
 */
static void create_pattern_object(struct adb_solve *solve, int target,
								  struct adb_pobject *primary,
								  struct adb_pobject *secondary)
{
	struct needle_object *t = &solve->target.secondary[target];

	t->pobject = secondary;

	/* calculate plate distance and min,max to primary */
	t->distance.plate_actual = distance_get_plate(primary, secondary);
	t->distance.pattern_min = t->distance.plate_actual - solve->tolerance.dist;
	t->distance.pattern_max = t->distance.plate_actual + solve->tolerance.dist;

	/* calculate plate magnitude and min,max to primary */
	t->mag.plate_actual = mag_get_plate_diff(primary, secondary);
	t->mag.pattern_min = t->mag.plate_actual - solve->tolerance.mag;
	t->mag.pattern_max = t->mag.plate_actual + solve->tolerance.mag;

	/* calculate plate position angle to primary */
	t->pa.plate_actual = pa_get_plate(primary, secondary);
}

/**
 * \brief Generate a 4-star relative comparison pattern from plate targets.
 *
 * Sorts the available plate targets by brightness, defines a primary anchor object,
 * and configures the angular geometries and magnitude constraints of three surrounding
 * secondary objects. Used to create the search needle for asterism matching.
 *
 * \param solve The active solving session context carrying plate targets.
 */
void target_create_pattern(struct adb_solve *solve)
{
	struct needle_object *t0, *t1, *t2;
	int i, j;

	/* sort plate object on brightness */
	qsort(solve->plate.object, solve->plate.num_objects,
		  sizeof(struct adb_pobject), plate_object_cmp);

	/* create target pattern */
	for (i = solve->plate.window_start + 1, j = 0; i < solve->plate.window_end;
		 i++, j++)
		create_pattern_object(solve, j,
							  &solve->plate.object[solve->plate.window_start],
							  &solve->plate.object[i]);

	/* work out PA tolerances */
	t0 = &solve->target.secondary[0];
	t1 = &solve->target.secondary[1];
	t2 = &solve->target.secondary[2];

	t0->pa.pattern_min = t1->pa.plate_actual - t0->pa.plate_actual;
	if (t0->pa.pattern_min < 0.0)
		t0->pa.pattern_min += 2.0 * M_PI;
	t0->pa.pattern_max = t0->pa.pattern_min + solve->tolerance.pa;
	t0->pa.pattern_min -= solve->tolerance.pa;

	t1->pa.pattern_min = t2->pa.plate_actual - t1->pa.plate_actual;
	if (t1->pa.pattern_min < 0.0)
		t1->pa.pattern_min += 2.0 * M_PI;
	t1->pa.pattern_max = t1->pa.pattern_min + solve->tolerance.pa;
	t1->pa.pattern_min -= solve->tolerance.pa;

	t2->pa.pattern_min = t0->pa.plate_actual - t2->pa.plate_actual;
	if (t2->pa.pattern_min < 0.0)
		t2->pa.pattern_min += 2.0 * M_PI;
	t2->pa.pattern_max = t2->pa.pattern_min + solve->tolerance.pa;
	t2->pa.pattern_min -= solve->tolerance.pa;

	t0->pa.plate_actual =
		t0->pa.pattern_min + (t0->pa.pattern_max - t0->pa.pattern_min) / 2.0;
	if (t0->pa.plate_actual < 0.0)
		t0->pa.plate_actual += 2.0 * M_PI;
	t1->pa.plate_actual =
		t1->pa.pattern_min + (t1->pa.pattern_max - t1->pa.pattern_min) / 2.0;
	if (t1->pa.plate_actual < 0.0)
		t1->pa.plate_actual += 2.0 * M_PI;
	t2->pa.plate_actual =
		t2->pa.pattern_min + (t2->pa.pattern_max - t2->pa.pattern_min) / 2.0;
	if (t2->pa.plate_actual < 0.0)
		t2->pa.plate_actual += 2.0 * M_PI;

	/* calculate flip PA tolerances where image can be fliped */
	t0->pa_flip.plate_actual = 2.0 * M_PI - t0->pa.plate_actual;
	t0->pa_flip.pattern_min = 2.0 * M_PI - t0->pa.pattern_min;
	t0->pa_flip.pattern_max = 2.0 * M_PI - t0->pa.pattern_max;

	t1->pa_flip.plate_actual = 2.0 * M_PI - t1->pa.plate_actual;
	t1->pa_flip.pattern_min = 2.0 * M_PI - t1->pa.pattern_min;
	t1->pa_flip.pattern_max = 2.0 * M_PI - t1->pa.pattern_max;

	t2->pa_flip.plate_actual = 2.0 * M_PI - t2->pa.plate_actual;
	t2->pa_flip.pattern_min = 2.0 * M_PI - t2->pa.pattern_min;
	t2->pa_flip.pattern_max = 2.0 * M_PI - t2->pa.pattern_max;
}

/**
 * \brief Extract comparison characteristics for solving a single generalized plate target.
 *
 * Builds tolerance profiles bounding angular distance, magnitude offsets, 
 * and position fields calibrated to expected radial pixel conversions.
 *
 * \param solve The active solver configuration setting limits.
 * \param target Secondary target index.
 * \param primary The plate anchor object.
 * \param secondary The specific secondary companion object.
 * \param runtime The execution state object.
 * \param solution Active solution model defining angular radial rates.
 */
static void create_single_object(struct adb_solve *solve, int target,
								 struct adb_pobject *primary,
								 struct adb_pobject *secondary,
								 struct solve_runtime *runtime,
								 struct adb_solve_solution *solution)
{
	struct needle_object *t = &runtime->soln_target[target];

	t->pobject = secondary;

	/* calculate plate distance and min,max to primary */
	t->distance.plate_actual = distance_get_plate(primary, secondary);
	t->distance.pattern_min =
		solution->rad_per_pix *
		(t->distance.plate_actual - solve->tolerance.dist);
	t->distance.pattern_max =
		solution->rad_per_pix *
		(t->distance.plate_actual + solve->tolerance.dist);

	/* calculate plate magnitude and min,max to primary */
	t->mag.plate_actual = mag_get_plate_diff(primary, secondary);
	t->mag.pattern_min = t->mag.plate_actual - solve->tolerance.mag;
	t->mag.pattern_max = t->mag.plate_actual + solve->tolerance.mag;

	/* calculate plate position angle to primary */
	t->pa.plate_actual = pa_get_plate(primary, secondary);
}

/**
 * \brief Create a broad single-object framework profile and initialize its tolerances.
 *
 * \param solve The active solver constraints definitions.
 * \param pobject The anchor primary plate object.
 * \param solution Current baseline solution context.
 * \param runtime Target execution space mapping search needles.
 */
void target_create_single(struct adb_solve *solve, struct adb_pobject *pobject,
						  struct adb_solve_solution *solution,
						  struct solve_runtime *runtime)
{
	struct needle_object *t0, *t1, *t2, *t3;
	int i, j;

	/* create target pattern - use pobject as primary  */
	for (i = solve->plate.window_start, j = 0; i < solve->plate.window_end;
		 i++, j++)
		create_single_object(solve, j, pobject, &solve->plate.object[i],
							 runtime, solution);

	/* work out PA tolerances */
	t0 = &runtime->soln_target[0];
	t1 = &runtime->soln_target[1];
	t2 = &runtime->soln_target[2];
	t3 = &runtime->soln_target[3];

	t0->pa.pattern_min = t1->pa.plate_actual - t0->pa.plate_actual;
	if (t0->pa.pattern_min < 0.0)
		t0->pa.pattern_min += 2.0 * M_PI;
	t0->pa.pattern_max = t0->pa.pattern_min + solve->tolerance.pa;
	t0->pa.pattern_min -= solve->tolerance.pa;

	t1->pa.pattern_min = t2->pa.plate_actual - t1->pa.plate_actual;
	if (t1->pa.pattern_min < 0.0)
		t1->pa.pattern_min += 2.0 * M_PI;
	t1->pa.pattern_max = t1->pa.pattern_min + solve->tolerance.pa;
	t1->pa.pattern_min -= solve->tolerance.pa;

	t2->pa.pattern_min = t3->pa.plate_actual - t2->pa.plate_actual;
	if (t2->pa.pattern_min < 0.0)
		t2->pa.pattern_min += 2.0 * M_PI;
	t2->pa.pattern_max = t2->pa.pattern_min + solve->tolerance.pa;
	t2->pa.pattern_min -= solve->tolerance.pa;

	t3->pa.pattern_min = t0->pa.plate_actual - t3->pa.plate_actual;
	if (t3->pa.pattern_min < 0.0)
		t3->pa.pattern_min += 2.0 * M_PI;
	t3->pa.pattern_max = t3->pa.pattern_min + solve->tolerance.pa;
	t3->pa.pattern_min -= solve->tolerance.pa;

	t0->pa.plate_actual =
		t0->pa.pattern_min + (t0->pa.pattern_max - t0->pa.pattern_min) / 2.0;
	if (t0->pa.plate_actual < 0.0)
		t0->pa.plate_actual += 2.0 * M_PI;
	t1->pa.plate_actual =
		t1->pa.pattern_min + (t1->pa.pattern_max - t1->pa.pattern_min) / 2.0;
	if (t1->pa.plate_actual < 0.0)
		t1->pa.plate_actual += 2.0 * M_PI;
	t2->pa.plate_actual =
		t2->pa.pattern_min + (t2->pa.pattern_max - t2->pa.pattern_min) / 2.0;
	if (t2->pa.plate_actual < 0.0)
		t2->pa.plate_actual += 2.0 * M_PI;
	t3->pa.plate_actual =
		t3->pa.pattern_min + (t3->pa.pattern_max - t3->pa.pattern_min) / 2.0;
	if (t3->pa.plate_actual < 0.0)
		t3->pa.plate_actual += 2.0 * M_PI;

	/* calculate flip PA tolerances where image can be fliped */
	t0->pa_flip.plate_actual = 2.0 * M_PI - t0->pa.plate_actual;
	t0->pa_flip.pattern_min = 2.0 * M_PI - t0->pa.pattern_min;
	t0->pa_flip.pattern_max = 2.0 * M_PI - t0->pa.pattern_max;

	t1->pa_flip.plate_actual = 2.0 * M_PI - t1->pa.plate_actual;
	t1->pa_flip.pattern_min = 2.0 * M_PI - t1->pa.pattern_min;
	t1->pa_flip.pattern_max = 2.0 * M_PI - t1->pa.pattern_max;

	t2->pa_flip.plate_actual = 2.0 * M_PI - t2->pa.plate_actual;
	t2->pa_flip.pattern_min = 2.0 * M_PI - t2->pa.pattern_min;
	t2->pa_flip.pattern_max = 2.0 * M_PI - t2->pa.pattern_max;

	t3->pa_flip.plate_actual = 2.0 * M_PI - t3->pa.plate_actual;
	t3->pa_flip.pattern_min = 2.0 * M_PI - t3->pa.pattern_min;
	t3->pa_flip.pattern_max = 2.0 * M_PI - t3->pa.pattern_max;
}

/**
 * \brief Register a potential 4-star candidate combination passing initial distance checks.
 *
 * Adds a catalog cluster `(primary, i, j, k)` to the distance matches array, tracking
 * divergence so it can proceed to stricter position angle validations.
 *
 * \param runtime Execution state collecting potential matches.
 * \param primary The proposed candidate anchor object.
 * \param source The haystack containing the remaining secondary nodes.
 * \param i Index of secondary star 1.
 * \param j Index of secondary star 2.
 * \param k Index of secondary star 3.
 * \param delta Measured variance error from target geometry.
 * \param rad_per_pix Effective angular scale derivation for this match.
 */
void target_add_match_on_distance(struct solve_runtime *runtime,
								  const struct adb_object *primary,
								  struct adb_source_objects *source, int i,
								  int j, int k, double delta,
								  double rad_per_pix)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];

	p->object[0] = primary;
	p->object[1] = source->objects[i];
	p->object[2] = source->objects[j];
	p->object[3] = source->objects[k];
	p->soln_pobject[0] = solve->plate.object[solve->plate.window_start];
	p->soln_pobject[1] = solve->plate.object[solve->plate.window_start + 1];
	p->soln_pobject[2] = solve->plate.object[solve->plate.window_start + 2];
	p->soln_pobject[3] = solve->plate.object[solve->plate.window_start + 3];
	p->delta.dist = delta;
	p->rad_per_pix = rad_per_pix;
	runtime->num_pot_distance++;
}

/**
 * \brief Record a potential candidate passing a single-object relative distance check.
 *
 * \param runtime Execution state tracking candidates.
 * \param primary The solitary target object being vetted.
 * \param source Available source catalog catalog.
 * \param delta Cumulative discrepancy metric.
 * \param flip Binary identifier tracing mirror alignment.
 */
void target_add_single_match_on_distance(struct solve_runtime *runtime,
										 const struct adb_object *primary,
										 struct adb_source_objects *source,
										 double delta, int flip)
{
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];
	p->delta.dist = delta;
	p->object[0] = primary;
	p->flip = flip;
	runtime->num_pot_distance++;
}

/**
 * \brief Log a single-object candidate hit utilizing an extended boundary frame.
 *
 * \param runtime Execution state tracking candidates.
 * \param primary The unanchored target candidate.
 * \param source Haystack array holding related neighbors.
 * \param delta Assessed geometrical error.
 * \param flip Layout geometry mirroring identifier.
 */
void target_add_single_match_extended(struct solve_runtime *runtime,
									  const struct adb_object *primary,
									  struct adb_source_objects *source,
									  double delta, int flip)
{
	struct adb_solve_solution *p;

	if (runtime->num_pot_distance >= MAX_POTENTAL_MATCHES)
		return;

	p = &runtime->pot_distance[runtime->num_pot_distance];
	p->delta.dist = delta;
	p->object[0] = primary;
	p->flip = flip;
	runtime->num_pot_distance++;
}

/**
 * \brief Pre-process and order raw catalog data for optimal searching.
 *
 * Sorts through hierarchical index sets generating a flat linearly array of
 * sorted sky catalog pointer objects constrained by solver magnitude ceilings,
 * dropping broken or invalid imported structures.
 *
 * \param solve Internal solver bounds configuring cutoffs (e.g. brightness thresholds).
 * \param set Target block sector of hierarchical data nodes.
 * \param source Structural container taking ownership of the generated flat arrays.
 * \return Total valid contiguous objects pushed into the search array.
 */
int target_prepare_source_objects(struct adb_solve *solve,
								  struct adb_object_set *set,
								  struct adb_source_objects *source)
{
	int object_heads, i, j, count = 0, warn_once = 1;

	/* get object heads */
	object_heads = adb_set_get_objects(set);
	if (object_heads <= 0)
		return object_heads;

	debug_init(set);

	/* allocate space for adb_source_objects */
	source->objects = calloc(set->count, sizeof(struct adb_object *));
	if (source->objects == NULL)
		return -ENOMEM;

	/* copy adb_source_objects ptrs from head set */
	for (i = 0; i < object_heads; i++) {
		const void *object = set->object_heads[i].objects;

		/* copy individual objects */
		for (j = 0; j < set->object_heads[i].count; j++) {
			const struct adb_object *o = object;

			/* ignore bogus objects - import errors ?*/
			if (o->dec == 0.0 || o->ra == 0.0 || o->mag == 0.0) {
				if (warn_once) {
					warn_once = 0;
					adb_error(
						solve->db,
						"object with zeroed attributes in db at head %d !\n");
				}
			} else {
				/* valid object */
				/* dont copy objects outside mag limits */
				if (o->mag <= solve->constraint.min_mag &&
					o->mag >= solve->constraint.max_mag) {
					source->objects[count++] = object;
				}
			}

			/* next object */
			object += solve->table->object.bytes;
		}
	}

	/* sort adb_source_objects on magnitude */
	qsort(source->objects, count, sizeof(struct adb_object *), mag_object_cmp);
	source->num_objects = count;

	adb_info(solve->db, ADB_LOG_SOLVE,
			 "using %d solver source objects from %d heads\n", count,
			 object_heads);

	return count;
}
