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
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "lib.h"
#include "solve.h"

/**
 * \brief Comparison callback for sorting solutions by divergence.
 *
 * Used by `qsort` to order an array of candidate solver solutions based on 
 * their computed divergence error. Lower divergence indicates a better match.
 *
 * \param o1 Pointer to the first solution.
 * \param o2 Pointer to the second solution.
 * \return 1 if o1 has higher divergence, -1 if o2 has higher divergence, 0 if equal.
 */
static int solution_cmp(const void *o1, const void *o2)
{
	const struct adb_solve_solution *p1 = o1, *p2 = o2;

	if (p1->divergance < p2->divergance)
		return 1;
	else if (p1->divergance > p2->divergance)
		return -1;
	else
		return 0;
}

/**
 * \brief Find the index of a plate object within a solution's plate object mapping.
 *
 * Scans the solution's recorded plate objects array searching for a structural
 * match containing matching physical pixel coordinates and ADU levels.
 *
 * \param solution The candidate solution containing matched tuples.
 * \param pobject The plate object needle to search for.
 * \return The internal array index of the matched plate object, or -1 if not found.
 */
static int get_solve_plate(struct adb_solve_solution *solution,
						   struct adb_pobject *pobject)
{
	int i;

	for (i = 0; i < solution->num_pobjects; i++) {
		if (solution->soln_pobject[i].adu == pobject->adu &&
			solution->soln_pobject[i].x == pobject->x &&
			solution->soln_pobject[i].y == pobject->y)
			return i;
	}

	return -1;
}

/**
 * \brief Calculate the divergence score mapping a potential solution to a plate object.
 *
 * Computes the aggregate geometric layout error between the current working
 * solution candidates and the singular physical plate object, weighting
 * magnitude, distance, and position angle differentials.
 *
 * \param runtime Target execution state context handling dynamic matches.
 * \param solution Extracted baseline baseline defining general astrometric characteristics.
 * \param pobject The test instrumental point being geometrically fitted.
 */
static void calc_object_divergence(struct solve_runtime *runtime,
								   struct adb_solve_solution *solution,
								   struct adb_pobject *pobject)
{
	int i;

	/* calculate differences in magnitude from DB and plate objects */
	for (i = 0; i < runtime->num_pot_pa; i++) {
		runtime->pot_pa[i].delta.mag =
			fabs(mag_get_plate(runtime->solve, solution, pobject) -
				 runtime->pot_pa[0].object[0]->mag);

		runtime->pot_pa[i].divergance =
			runtime->pot_pa[i].delta.mag * DELTA_MAG_COEFF +
			runtime->pot_pa[i].delta.dist * DELTA_DIST_COEFF +
			runtime->pot_pa[i].delta.pa * DELTA_PA_COEFF;
	}
}

/**
 * \brief Associate a raw photographic plate object with a catalog source identification.
 *
 * Locates the optimal physical matching source star mapping against a currently 
 * solved target baseline, building positional estimates and registering them into 
 * the working matched reference framework.
 *
 * \param solve Core structural execution state limiting overall bounds constraints.
 * \param object_id The internal local index tracing this primary structure mappings.
 * \param solution The underlying base calibration mapping model utilized evaluating components.
 * \param pobject Instrumental physical raw input target being evaluated mapped.
 * \param sobject Out parameter returning constructed mapped alignment mapping pointer variables.
 * \return Newly identified elements successfully inserted mapping state constraints indices.
 */
static int get_object(struct adb_solve *solve, int object_id,
					  struct adb_solve_solution *solution,
					  struct adb_pobject *pobject,
					  struct adb_solve_object *sobject)
{
	struct solve_runtime runtime;
	int count = 0, plate, new;

	if (solution->set == NULL)
		return -EINVAL;

	memset(&runtime, 0, sizeof(runtime));
	memset(sobject, 0, sizeof(*sobject));
	runtime.solve = solve;

	/* check if pobject is solve object from solution */
	if (solve->solution == solution) {
		plate = get_solve_plate(solution, pobject);

		if (plate >= 0) {
			sobject->object = solution->object[plate];
			new = target_add_ref_object(solution, object_id, sobject->object,
										pobject);
			return new;
		}
	}

	/* calculate plate parameters for new object */
	target_create_single(solve, pobject, solution, &runtime);

	/* find candidate adb_source_objects on magnitude */
	count = mag_solve_single_object(&runtime, solution, pobject);
	if (count == 0)
		return 0;

	/* at this point we have a range of candidate stars that match the
	 * magnitude bounds of the plate object now check for distance alignment */
	count = distance_solve_single_object(&runtime, solution);
	if (count == 0)
		return 0;

	/* At this point we have a list of objects that match on magnitude and
	 * distance, so we finally check the objects for PA alignment*/
	count = pa_solve_single_object(&runtime, solution);
	if (count == 0)
		return 0;

	/* it's possible we may have > 1 potential object so order them */
	qsort(&runtime.pot_pa, count, sizeof(struct adb_solve_solution),
		  solution_cmp);

	/* assign closest object */
	sobject->object = runtime.pot_pa[0].object[0];

	/* add object as reference */
	new = target_add_ref_object(solution, object_id, sobject->object, pobject);

	calc_object_divergence(&runtime, solution, pobject);
	return new;
}

/**
 * \brief Correlate a single photographic object onto catalog data utilizing an extended loose bound.
 *
 * Expands physical positional heuristics bounding individual loosely aligned instrumental inputs, 
 * accommodating noise variances identifying nearest neighbor optimal alignments matching general 
 * layout geometry attributes mapping specific index tracking references.
 *
 * \param solve Parent master runtime framework managing threads configurations safely.
 * \param object_id Numerical index referencing source physical point alignment mapping indices.
 * \param solution Active matrix parameters identifying expected structural layouts baseline dimensions. 
 * \param pobject Source instrumental target pointer defining coordinates matrices metrics evaluating features.
 * \param sobject Solved destination pointer array mapped storing matched associations properties links.
 * \return Result metric evaluating positive identified components registered correctly.
 */
static int get_extended_object(struct adb_solve *solve, int object_id,
							   struct adb_solve_solution *solution,
							   struct adb_pobject *pobject,
							   struct adb_solve_object *sobject)
{
	struct solve_runtime runtime;
	int count = 0;

	if (solution->set == NULL)
		return -EINVAL;

	memset(&runtime, 0, sizeof(runtime));
	memset(sobject, 0, sizeof(*sobject));
	runtime.solve = solve;

	/* calculate plate parameters for new object */
	target_create_single(solve, pobject, solution, &runtime);

	/* find candidate adb_source_objects on magnitude */
	count = mag_solve_single_object(&runtime, solution, pobject);
	if (count == 0)
		return 0;

	/* at this point we have a range of candidate stars that match the
	 * magnitude bounds of the plate object now check for distance alignment */
	count = distance_solve_single_object_extended(&runtime, solution);
	if (count == 0)
		return 0;

	/* assign closest object */
	sobject->object = runtime.pot_distance[0].object[0];

	return count;
}

/**
 * \brief Configure search query boundaries narrowing expected object returns based on image calibration.
 *
 * Initializes spatial search tree heuristics constraining search bounds limiting fetched catalog
 * nodes matching local image scale metrics, bounding radius limits limiting magnitudes and coordinates.
 *
 * \param solution Functional active mapping trace containing baseline celestial definitions pointers.
 * \param fov Radius boundary representing maximum search spread limit filtering distant points.
 * \param mag_limit Operational visual magnitude numeric ceiling.
 * \param table_id Core catalog dataset reference numerical index locating structural blocks targets.
 * \return Output identifier indicating successfully built filtered array spaces.
 */
int adb_solution_set_search_limits(struct adb_solve_solution *solution,
								   double fov, double mag_limit, int table_id)
{
	struct adb_table *table;
	struct adb_object_set *set;
	double centre_ra, centre_dec;
	int object_heads, i, count = 0, j;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return -EINVAL;
	table = &solution->db->table[table_id];

	/* check for existing set and free it */
	set = solution->set;
	if (set)
		adb_table_set_free(set);

	centre_ra = solution->object[0]->ra;
	centre_dec = solution->object[0]->dec;

	/* create new set based on image fov and mag limits */
	set = adb_table_set_new(solution->db, table_id);
	if (!set)
		return -ENOMEM;
	solution->set = set;

	adb_table_set_constraints(set, centre_ra, centre_dec, fov, -10.0,
							  mag_limit);

	/* get object heads */
	object_heads = adb_set_get_objects(set);
	if (object_heads <= 0) {
		free(set);
		solution->set = NULL;
		return object_heads;
	}

	/* allocate space for adb_source_objects */
	solution->source.objects = calloc(set->count, sizeof(struct adb_object *));
	if (solution->source.objects == NULL) {
		free(set);
		solution->set = NULL;
		return -ENOMEM;
	}

	/* copy adb_source_objects ptrs from head set */
	for (i = 0; i < object_heads; i++) {
		const void *object = set->object_heads[i].objects;

		for (j = 0; j < set->object_heads[i].count; j++) {
			solution->source.objects[count++] = object;
			SOBJ_CHECK_SET(((struct adb_object *)object));
			object += table->object.bytes;
		}
	}

	/* sort adb_source_objects on magnitude */
	qsort(solution->source.objects, set->count, sizeof(struct adb_object *),
		  mag_object_cmp);
	solution->source.num_objects = count;

	/* allocate initial reference objects */
	solution->ref = realloc(solution->ref, sizeof(struct adb_reference_object) *
											   (solution->num_ref_objects + 4 +
												solution->num_pobjects));
	if (solution->ref == NULL) {
		free(solution->source.objects);
		solution->source.objects = NULL;
		solution->source.num_objects = 0;
		free(set);
		solution->set = NULL;
		return -ENOMEM;
	}

	return 0;
}

/**
 * \brief Complete full plate extraction registering all valid matches expanding original matrices.
 *
 * Performs broad sweep iterating across all loaded raw instrumental values querying matched elements,
 * inserting mapping combinations directly tracking resulting positional and magnitude metrics definitions.
 *
 * \param solution Dynamic working baseline target model defining expected scale properties values arrays.
 * \return Result value accumulating final verified count of mapped positional items matching configurations safely. 
 */
int adb_solution_get_objects_extended(struct adb_solve_solution *solution)
{
	struct adb_solve *solve = solution->solve;
	int i, ret = 0, fail = 0, num_solved = 0, num_unsolved = 0;

	if (solution->num_pobjects == 0)
		return 0;

	/* reallocate memory for new solved objects */
	solution->solve_object =
		realloc(solution->solve_object,
				sizeof(struct adb_solve_object) * solution->num_pobjects);
	memset(solution->solve_object, 0,
		   sizeof(struct adb_solve_object) * solution->num_pobjects);
	if (solution->solve_object == NULL)
		return -ENOMEM;

	solution->num_solved_objects = 0;
	solution->num_unsolved_objects = 0;
	solve->exit = 0;
	solve->progress = 0;

	/* solve each new plate object */
#if HAVE_OPENMP
#pragma omp parallel for schedule(dynamic, 10) private(ret) \
	reduction(+ : num_solved, num_unsolved)
#endif
	for (i = 0; i < solution->num_pobjects; i++) {
		if (solve->exit)
			continue;

		solve->progress++;
		if (!solution->pobjects[i].extended)
			continue;

		ret = get_extended_object(solve, i, solution, &solution->pobjects[i],
								  &solution->solve_object[i]);

		if (ret < 0) {
			fail = 1;
			continue;
		} else if (ret == 0)
			num_unsolved++;
		else
			num_solved++;

		solution->solve_object[i].pobject = solution->pobjects[i];
	}

	if (fail) {
		free(solution->solve_object);
		solution->solve_object = NULL;
		return ret;
	}

	solution->num_solved_objects = num_solved;
	solution->num_unsolved_objects = num_unsolved;

	solution->total_objects =
		solution->num_solved_objects + solution->num_unsolved_objects;

	return solution->num_solved_objects;
}

/*
 * Add new plate objects to solution for searching.
 */
int adb_solution_add_pobjects(struct adb_solve_solution *solution,
							  struct adb_pobject *pobjects, int num_pobjects)
{
	int i, j;

	/* reallocate memory for new plate objects */
	solution->pobjects = realloc(solution->pobjects,
								 sizeof(struct adb_solve_object) *
									 (solution->num_pobjects + num_pobjects));
	if (solution->pobjects == NULL)
		return -ENOMEM;

	/* add new reference object to end */
	solution->ref =
		realloc(solution->ref, sizeof(struct adb_reference_object) *
								   (solution->num_ref_objects + num_pobjects +
									solution->num_pobjects));
	if (solution->ref == NULL) {
		free(solution->pobjects);
		return -ENOMEM;
	}

	/* copy each new plate object */
	for (i = solution->num_pobjects, j = 0;
		 i < solution->num_pobjects + num_pobjects; i++, j++) {
		solution->pobjects[i] = pobjects[j];
	}

	solution->num_pobjects += num_pobjects;
	return 0;
}

/* get plate objects or estimates of plate object position and magnitude */
int adb_solution_get_objects(struct adb_solve_solution *solution)
{
	struct adb_solve *solve = solution->solve;
	int i, ret = 0, fail = 0, num_solved = 0, num_unsolved = 0;

	if (solution->num_pobjects == 0)
		return 0;

	/* reallocate memory for new solved objects */
	solution->solve_object =
		realloc(solution->solve_object,
				sizeof(struct adb_solve_object) * solution->num_pobjects);
	if (!solution->solve_object)
		return -ENOMEM;
	memset(solution->solve_object, 0,
		   sizeof(struct adb_solve_object) * solution->num_pobjects);

	solution->num_solved_objects = 0;
	solution->num_unsolved_objects = 0;
	solve->exit = 0;
	solve->progress = 0;

	/* solve each new plate object */
#if 0 /* race condition somewhere cause a few differences in detected objects */
#pragma omp parallel for schedule(dynamic, 10) private(ret) \
	reduction(+ : num_solved, num_unsolved, fail)
#endif
	for (i = 0; i < solution->num_pobjects; i++) {
		if (solve->exit)
			continue;

		solve->progress++;

		if (solution->pobjects[i].extended)
			continue;

		ret = get_object(solve, i, solution, &solution->pobjects[i],
						 &solution->solve_object[i]);

		if (ret < 0) {
			fail++;
			continue;
		} else if (ret == 0)
			num_unsolved++;
		else
			num_solved++;

		solution->solve_object[i].pobject = solution->pobjects[i];
	}

	if (fail) {
		free(solution->solve_object);
		solution->solve_object = NULL;
		return ret;
	}

	solution->num_solved_objects = num_solved;
	solution->num_unsolved_objects = num_unsolved;

	solution->total_objects =
		solution->num_solved_objects + solution->num_unsolved_objects;

	return solution->num_solved_objects;
}

int adb_solution_calc_astrometry(struct adb_solve_solution *solution)
{
	posn_clip_plate_coefficients(solution);

	/* calc positions for each object in image */
	posn_calc_solved_plate(solution);
	posn_calc_unsolved_plate(solution);

	return 0;
}

int adb_solution_calc_photometry(struct adb_solve_solution *solution)
{
	/* make sure we have enough reference objects */
	mag_calc_plate_coefficients(solution);

	/* calculate magnitude for each object in image */
	mag_calc_solved_plate(solution);
	mag_calc_unsolved_plate(solution);

	return 0;
}

double adb_solution_divergence(struct adb_solve_solution *solution)
{
	return solution->divergance;
}

struct adb_solve_object *
adb_solution_get_object(struct adb_solve_solution *solution, int index)
{
	if (index >= solution->num_pobjects)
		return NULL;

	return &solution->solve_object[index];
}

struct adb_solve_object *
adb_solution_get_object_at(struct adb_solve_solution *solution, int x, int y)
{
	int i;

	for (i = 0; i < solution->num_pobjects; i++) {
		if (x == solution->solve_object[i].pobject.x &&
			y == solution->solve_object[i].pobject.y)
			return &solution->solve_object[i];
	}
	return NULL;
}

double adb_solution_get_pixel_size(struct adb_solve_solution *solution)
{
	return solution->rad_per_pix;
}

void adb_solution_equ_to_plate_position(struct adb_solve_solution *solution,
										double ra, double dec, double *x,
										double *y)
{
	posn_equ_to_plate(solution, ra, dec, x, y);
}

void adb_solution_plate_to_equ_position(struct adb_solve_solution *solution,
										int x, int y, double *ra, double *dec)
{
	struct adb_pobject p;

	p.x = x;
	p.y = y;

	posn_plate_to_equ(solution, &p, ra, dec);
}

void adb_solution_equ_to_plate_position_fast(struct adb_solve_solution *solution,
											 double ra, double dec, double *x,
											 double *y)
{
	posn_equ_to_plate_fast(solution, ra, dec, x, y);
}

void adb_solution_plate_to_equ_position_fast(
	struct adb_solve_solution *solution, int x, int y, double *ra, double *dec)
{
	struct adb_pobject p;

	p.x = x;
	p.y = y;

	posn_plate_to_equ_fast(solution, &p, ra, dec);
}

/*
 * Get the plate boundaries using solution.
 */
void adb_solution_get_plate_equ_bounds(struct adb_solve_solution *solution,
									   enum adb_plate_bounds bounds, double *ra,
									   double *dec)
{
	struct adb_solve *solve = solution->solve;
	struct adb_pobject p;

	switch (bounds) {
	case ADB_BOUND_TOP_RIGHT:
		p.x = solve->plate.width;
		p.y = solve->plate.height;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_TOP_LEFT:
		p.x = 0;
		p.y = solve->plate.height;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_BOTTOM_RIGHT:
		p.x = solve->plate.width;
		p.y = 0;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_BOTTOM_LEFT:
		p.x = 0;
		p.y = 0;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_CENTRE:
		p.x = solve->plate.width / 2;
		p.y = solve->plate.height / 2;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	default:
		*ra = 0.0;
		*dec = 0.0;
		break;
	}
}

void adb_solution_recalc_objects(struct adb_solve_solution *solution)
{
}

float adb_solution_get_progress(struct adb_solve_solution *solution)
{
	return (float)solution->num_pobjects / solution->solve->progress;
}
