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

static double calc_magnitude_deltas(struct solve_runtime *runtime,
	int pot, int idx)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *s = &runtime->pot_pa[pot];
	double plate_diff, db_diff;

	plate_diff = mag_get_plate_diff(&solve->pobject[idx],
			&solve->pobject[idx + 1]);

	db_diff = s->object[idx]->mag -
			s->object[idx + 1]->mag;

	return plate_diff - db_diff;
}

static void calc_cluster_divergence(struct solve_runtime *runtime)
{
	int i;

	/* calculate differences in magnitude from DB and plate objects */
	for (i = 0; i < runtime->num_pot_pa; i++) {
		runtime->pot_pa[i].delta.mag =
			(calc_magnitude_deltas(runtime, i, 0) +
			calc_magnitude_deltas(runtime, i, 1) +
			calc_magnitude_deltas(runtime, i, 2)) / 3.0;

		runtime->pot_pa[i].divergance =
			runtime->pot_pa[i].delta.mag * DELTA_MAG_COEFF +
			runtime->pot_pa[i].delta.dist * DELTA_DIST_COEFF +
			runtime->pot_pa[i].delta.pa * DELTA_PA_COEFF;
	}
}

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

static int is_solution_dupe(struct adb_solve *solve,
		struct adb_solve_solution *s1)
{
	struct adb_solve_solution *s2;
	int i;

	for (i = 0; i < solve->num_solutions; i++) {
		s2 = &solve->solution[i];
		if (s2->object[0] == s1->object[0] &&
			s2->object[1] == s1->object[1] &&
			s2->object[2] == s1->object[2] &&
			s2->object[3] == s1->object[3]) {
			return 1;
		}
	}
	return 0;
}

static void copy_solution(struct solve_runtime *runtime)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *soln;
	struct adb_db *db;
	int i;

	pthread_mutex_lock(&solve->mutex);

	if (solve->num_solutions == MAX_RT_SOLUTIONS) {
		adb_error(solve->db, "too many solutions, narrow params\n");
		pthread_mutex_unlock(&solve->mutex);
		return;
	}

	for (i = 0; i < runtime->num_pot_pa; i++) {
		soln = &solve->solution[solve->num_solutions];
		db = soln->db;

		if (is_solution_dupe(solve, &runtime->pot_pa[i]))
			continue;

		/* copy solution */
		*soln = runtime->pot_pa[i];
		soln->solve = solve;
		soln->db = db;

		adb_info(db, ADB_LOG_SOLVE, "Adding solution %d\n",
			solve->num_solutions);
		adb_info(db, ADB_LOG_SOLVE, " plate 0: X %d Y %d ADU %d\n",
			soln->soln_pobject[0].x, soln->soln_pobject[0].y,
			soln->soln_pobject[0].adu);
		adb_info(db, ADB_LOG_SOLVE, " plate 1: X %d Y %d ADU %d\n",
			soln->soln_pobject[1].x, soln->soln_pobject[1].y,
			soln->soln_pobject[1].adu);
		adb_info(db, ADB_LOG_SOLVE, " plate 2: X %d Y %d ADU %d\n",
			soln->soln_pobject[2].x, soln->soln_pobject[2].y,
			soln->soln_pobject[2].adu);
		adb_info(db, ADB_LOG_SOLVE, " plate 3: X %d Y %d ADU %d\n",
			soln->soln_pobject[3].x, soln->soln_pobject[3].y,
			soln->soln_pobject[3].adu);
		solve->num_solutions++;

	}

	pthread_mutex_unlock(&solve->mutex);
}

/* solve plate cluster for this primary object */
static int try_object_as_primary(struct adb_solve *solve,
	const struct adb_object *primary, int primary_idx)
{
	struct solve_runtime runtime;
	int i, count;

	memset(&runtime, 0, sizeof(runtime));
	runtime.solve = solve;

	/* find secondary candidate adb_source_objects on magnitude */
	for (i = 0; i < MIN_PLATE_OBJECTS - 1; i++) {
		count = mag_solve_object(&runtime, primary, i);
		if (!count)
			return 0;
	}

	/* at this point we have a range of candidate stars that match the
	 * magnitude bounds of the primary object and each secondary object,
	 * now check secondary candidates for distance alignment */
	count = distance_solve_object(&runtime, primary);
	if (!count)
		return 0;

	/* At this point we have a list of clusters that match on magnitude and
	 * distance, so we finally check the candidates clusters for PA alignment*/
	count = pa_solve_object(&runtime, primary, i);
	if (!count)
		return 0;

	calc_cluster_divergence(&runtime);

	/* copy matching clusters to solver */
	copy_solution(&runtime);

	return solve->num_solutions;
}

static int solve_plate_cluster_for_set_all(struct adb_solve *solve,
	struct adb_object_set *set)
{
	int i, count = 0, progress = 0;

	/* attempt to solve for each object in set */
#pragma omp parallel for schedule(dynamic, 10) reduction(+:count, progress)
	for (i = 0; i < solve->source.num_objects; i++) {

		progress++;
		solve->progress++;

		/* we cant break OpenMP loops */
		if (solve->exit)
			continue;

		count += try_object_as_primary(solve, solve->source.objects[i], i);
	}

	if (count >= MAX_RT_SOLUTIONS)
		count = MAX_RT_SOLUTIONS - 1;

	return count;
}

static int solve_plate_cluster_for_set_first(struct adb_solve *solve,
	struct adb_object_set *set)
{
	int i, count = 0, progress = 0;

	/* attempt to solve for each object in set */
#pragma omp parallel for schedule(dynamic, 10) reduction(+:progress)
	for (i = 0; i < solve->source.num_objects; i++) {

		progress++;
		solve->progress++;

		/* we cant break OpenMP loops */
		if (count || solve->exit)
			continue;

		if (try_object_as_primary(solve, solve->source.objects[i], i)) {
			count = 1;
/* TODO OpenMP cancel is supported in gcc 4.9 */
/* #pragma omp cancel for */
		}
	}
/* #pragma omp cancellation point for */

	return count;
}

struct adb_solve *adb_solve_new(struct adb_db *db, int table_id)
{
	struct adb_solve *solve;
	int i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return NULL;

	solve = calloc(1, sizeof(struct adb_solve));
	if (solve == NULL)
		return NULL;

	pthread_mutex_init(&solve->mutex, NULL);

	solve->db = db;
	solve->table = &db->table[table_id];

	/* set initial constraints */
	solve->constraint.min_ra = 0.0;
	solve->constraint.max_ra =  2.0 * M_PI;
	solve->constraint.min_dec = 0.0;
	solve->constraint.max_dec = M_PI_2;
	solve->constraint.min_mag = 16.0;
	solve->constraint.max_mag = -2.0;
	solve->constraint.min_fov = 0.1 * D2R;
	solve->constraint.max_fov = M_PI_2;
	solve->num_solutions = 0;

	for (i = 0; i < MAX_RT_SOLUTIONS; i++) {
		solve->solution[i].db = db;
		solve->solution[i].solve = solve;
	}

	return solve;
}

void adb_solve_free(struct adb_solve *solve)
{
	struct adb_solve_solution *solution;
	int i;

	/* free each solution */
	for (i = 0; i < MAX_RT_SOLUTIONS; i++) {
		solution = &solve->solution[i];

		adb_table_set_free(solution->set);
		free(solution->solve_object);
		free(solution->ref);
		free(solution->source.objects);
	}

	free(solve->source.objects);
	free(solve);
}

int adb_solve_add_plate_object(struct adb_solve *solve,
				struct adb_pobject *pobject)
{
	if (solve->num_plate_objects == ADB_NUM_TARGETS - 1) {
		adb_error(solve->db, "too many adb_source_objects %d\n",
			ADB_NUM_TARGETS);
		return -EINVAL;
	}

	if (pobject->adu == 0) {
		adb_error(solve->db, "object has no ADUs\n");
		return -EINVAL;
	}

	memcpy(&solve->pobject[solve->num_plate_objects++], pobject,
		sizeof(struct adb_pobject));

	return 0;
}

int adb_solve_constraint(struct adb_solve *solve,
		enum adb_constraint type, double min, double max)
{
	switch(type) {
	case ADB_CONSTRAINT_MAG:
		solve->constraint.min_mag = min;
		solve->constraint.max_mag = max;
		break;
	case ADB_CONSTRAINT_FOV:
		solve->constraint.min_fov = min;
		solve->constraint.max_fov = max;
		break;
	case ADB_CONSTRAINT_RA:
		solve->constraint.min_ra = min;
		solve->constraint.max_ra = max;
		break;
	case ADB_CONSTRAINT_DEC:
		solve->constraint.min_dec = min;
		solve->constraint.max_dec = max;
		break;
	default:
		adb_error(solve->db, "unknown constraint type %d\n", type);
		return -EINVAL;
	}
	return 0;
}

/*! \fn int adb_solve_get_results(struct adb_solve *solve,
				struct adb_object_set *set,
				const struct adb_object **objects[],
				double delta.dist, double mag_coeff,
				double pa_coeff)
* \param image Image
* \param num_scales Number of wavelet scales.
* \return Wavelet pointer on success or NULL on failure..
*
*
*/
int adb_solve(struct adb_solve *solve,
		struct adb_object_set *set, enum adb_find find)
{
	int ret, i, count = 0;

	/* do we have enough plate adb_source_objects to solve */
	if (solve->num_plate_objects < MIN_PLATE_OBJECTS) {
		adb_error(solve->db, "not enough plate adb_source_objects, need %d have %d\n",
			MIN_PLATE_OBJECTS, solve->num_plate_objects);
		return -EINVAL;
	}

	ret = target_prepare_source_objects(solve, set, &solve->source);
	if (ret <= 0) {
		adb_error(solve->db, "cant get trixels %d\n", ret);
		return ret;
	}

	/* status reporting and exit */
	solve->progress = 0.0;
	solve->exit = 0;

	for (i = 0; i <= solve->num_plate_objects - MIN_PLATE_OBJECTS; i++) {

		solve->plate_idx_start = i;
		solve->plate_idx_end = MIN_PLATE_OBJECTS + i;

		adb_info(solve->db, ADB_LOG_SOLVE, "solving plate objects %d -> %d from %d\n",
			solve->plate_idx_start, solve->plate_idx_end - 1,
			solve->num_plate_objects);

		target_create_pattern(solve);

		if (find == ADB_FIND_ALL)
			ret = solve_plate_cluster_for_set_all(solve, set);
		else
			ret = solve_plate_cluster_for_set_first(solve, set);

		if (ret < 0)
			return ret;
		count += ret;
	}

	/* it's possible we may have > 1 solution so order them */
	qsort(solve->solution, solve->num_solutions,
		sizeof(struct adb_solve_solution), solution_cmp);

	adb_info(solve->db, ADB_LOG_SOLVE, "Total %d solutions\n",
		solve->num_solutions);
	return solve->num_solutions;
}

int adb_solve_set_magnitude_delta(struct adb_solve *solve,
		double delta_mag)
{
	solve->tolerance.mag = delta_mag;
	return 0;
}

int adb_solve_set_distance_delta(struct adb_solve *solve,
		double delta_pixels)
{
	solve->tolerance.dist = delta_pixels;
	return 0;
}

int adb_solve_set_pa_delta(struct adb_solve *solve,
		double delta_rad)
{
	solve->tolerance.pa = delta_rad;
	return 0;
}

void adb_solve_image_set_properties(struct adb_solve *solve, int width,
		int height,  double ra, double dec)
{
	solve->plate_width = width;
	solve->plate_height = height;
	solve->plate_ra = ra ;
	solve->plate_dec = dec;
}

struct adb_solve_solution *adb_solve_get_solution(struct adb_solve *solve,
	unsigned int index)
{
	if (solve->num_solutions == 0 || index >= solve->num_solutions)
		return NULL;

	return &solve->solution[index];
}

/* prepare solution for finding other objects */
int adb_solve_prep_solution(struct adb_solve_solution *solution,
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

	adb_table_set_constraints(set, centre_ra, centre_dec,
			fov, -10.0, mag_limit);

	/* get object heads */
	object_heads = adb_set_get_objects(set);
	if (object_heads <= 0) {
		free(set);
		solution->set = NULL;
		return object_heads;
	}

	/* allocate space for adb_source_objects */
	solution->source.objects =
		calloc(set->count, sizeof(struct adb_object *));
	if (solution->source.objects == NULL) {
		free(set);
		solution->set = NULL;
		return -ENOMEM;
	}

	/* copy adb_source_objects ptrs from head set */
	for (i = 0; i < object_heads; i++) {

		const void *object = set->object_heads[i].objects;

		for (j = 0; j < set->object_heads[i].count; j++)  {
			solution->source.objects[count++] = object;
			SOBJ_CHECK_SET(((struct adb_object*)object));
			object += table->object.bytes;
		}
	}

	/* sort adb_source_objects on magnitude */
	qsort(solution->source.objects, set->count,
		sizeof(struct adb_object *), mag_object_cmp);
	solution->source.num_objects = count;

	/* allocate initial reference objects */
	solution->ref = realloc(solution->ref,
			sizeof(struct adb_reference_object) *
			(solution->num_ref_objects + 4 + solution->num_pobjects));
	if (solution->ref == NULL) {
		free(set);
		solution->set = NULL;
		return -ENOMEM;
	}

	return 0;
}

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

/* get object or estimated object magnitude & position for plate object */
static int get_extended_object(struct adb_solve *solve, int object_id,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobject, struct adb_solve_object *sobject)
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

/* get object or estimated object magnitude & position for plate object */
static int get_object(struct adb_solve *solve, int object_id,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobject, struct adb_solve_object *sobject)
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
			new = target_add_ref_object(solution, object_id,
				sobject->object, pobject);
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
	qsort(&runtime.pot_pa, count,
			sizeof(struct adb_solve_solution), solution_cmp);

	/* assign closest object */
	sobject->object = runtime.pot_pa[0].object[0];

	/* add object as reference */
	new = target_add_ref_object(solution, object_id, sobject->object, pobject);

	calc_object_divergence(&runtime, solution, pobject);
	return new;
}

/* get plate objects or estimates of plate object position and magnitude */
int adb_solve_get_objects(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	int i, ret = 0, fail = 0, num_solved = 0, num_unsolved = 0;

	if (solution->num_pobjects == 0)
		return 0;

	/* reallocate memory for new solved objects */
	solution->solve_object = realloc(solution->solve_object,
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
#if 0 /* race condition somewhere cause a few differences in detected objects */
#pragma omp parallel for schedule(dynamic, 10) \
		private(ret) reduction (+:num_solved, num_unsolved, fail)
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

	solution->total_objects = solution->num_solved_objects +
		solution->num_unsolved_objects;

	return solution->num_solved_objects;
}

int adb_solve_astrometry(struct adb_solve *solve,
		struct adb_solve_solution *solution)
{
	posn_clip_plate_coefficients(solve, solution);

	/* calc positions for each object in image */
	posn_calc_solved_plate(solve, solution);
	posn_calc_unsolved_plate(solve, solution);

	return 0;
}

int adb_solve_photometry(struct adb_solve *solve,
		struct adb_solve_solution *solution)
{
	/* make sure we have enough reference objects */
	mag_calc_plate_coefficients(solve, solution);

	/* calculate magnitude for each object in image */
	mag_calc_solved_plate(solve, solution);
	mag_calc_unsolved_plate(solve, solution);

	return 0;
}

/* get plate objects or estimates of plate object position and magnitude */
int adb_solve_get_objects_extended(struct adb_solve *solve,
	struct adb_solve_solution *solution)
{
	int i, ret = 0, fail = 0, num_solved = 0, num_unsolved = 0;

	if (solution->num_pobjects == 0)
		return 0;

	/* reallocate memory for new solved objects */
	solution->solve_object = realloc(solution->solve_object,
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
#pragma omp parallel for schedule(dynamic, 10) \
	private(ret) reduction (+:num_solved, num_unsolved)
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

	solution->total_objects = solution->num_solved_objects +
		solution->num_unsolved_objects;

	return solution->num_solved_objects;
}

int adb_solve_add_pobjects(struct adb_solve *solve,
		struct adb_solve_solution *solution,
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
	solution->ref = realloc(solution->ref,
		sizeof(struct adb_reference_object) *
		(solution->num_ref_objects + num_pobjects + solution->num_pobjects));
	if (solution->ref == NULL) {
		free(solution->pobjects);
		return -ENOMEM;
	}

	for (i = solution->num_pobjects, j = 0;
			i < solution->num_pobjects + num_pobjects; i++, j++) {
		solution->pobjects[i] = pobjects[j];
	}

	solution->num_pobjects += num_pobjects;
	return 0;
}

int adb_solve_get_pobject_count(struct adb_solve *solve,
		struct adb_solve_solution *solution)
{
	return solution->num_pobjects;
}

double adb_solution_divergence(struct adb_solve_solution *solution)
{
	return solution->divergance;
}

struct adb_solve_object *adb_solution_get_object(
	struct adb_solve_solution *solution, int index)
{
	if (index >= solution->num_pobjects)
		return NULL;

	return &solution->solve_object[index];
}

struct adb_solve_object *adb_solution_get_object_at(
	struct adb_solve_solution *solution, int x, int y)
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
		double ra, double dec, double *x,  double *y)
{
	posn_equ_to_plate(solution, ra, dec,  x,  y);
}

void adb_solution_plate_to_equ_position(struct adb_solve_solution *solution,
		int x, int y, double *ra, double *dec)
{
	struct adb_pobject p;

	p.x = x;
	p.y = y;

	posn_plate_to_equ(solution, &p, ra, dec);
}

void adb_solution_get_plate_equ_bounds(struct adb_solve_solution *solution,
		enum adb_plate_bounds bounds, double *ra, double *dec)
{
	struct adb_solve *solve = solution->solve;
	struct adb_pobject p;

	switch (bounds) {
	case ADB_BOUND_TOP_RIGHT:
		p.x = solve->plate_width;
		p.y = solve->plate_height;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_TOP_LEFT:
		p.x = 0;
		p.y = solve->plate_height;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_BOTTOM_RIGHT:
		p.x = solve->plate_width;
		p.y = 0;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_BOTTOM_LEFT:
		p.x = 0;
		p.y = 0;
		posn_plate_to_equ(solution, &p, ra, dec);
		break;
	case ADB_BOUND_CENTRE:
		p.x = solve->plate_width / 2;
		p.y = solve->plate_height / 2;
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

void adb_solve_stop(struct adb_solve *solve)
{
	solve->exit = 1;
}

float adb_solve_get_progress(struct adb_solve *solve)
{
	return (float)solve->source.num_objects / solve->progress;
}

float adb_solution_get_progress(struct adb_solve_solution *solution)
{
	return (float)solution->num_pobjects / solution->solve->progress;
}
