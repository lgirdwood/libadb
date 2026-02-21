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

/* qsort comparison func */
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

/*
 * Calculate magnitude delta difference between potential solution objects
 * and plate objects.
 */
static double calc_magnitude_deltas(struct solve_runtime *runtime,
	int pot, int idx)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *s = &runtime->pot_pa[pot];
	double plate_diff, db_diff;

	plate_diff = mag_get_plate_diff(&solve->plate.object[idx],
			&solve->plate.object[idx + 1]);

	db_diff = s->object[idx]->mag -
			s->object[idx + 1]->mag;

	return plate_diff - db_diff;
}

/*
 * Calculate divergence between potential solution cluster and
 * plate cluster.
 */
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



/*
 * Is new solution s1 a duplicate of an existing solution ?
 */
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

/*
 * Copy new solution to solver array of solutions.
 * We lock access to the array to support multiple solver threads.
 */
static void copy_solution(struct solve_runtime *runtime)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *soln;
	struct adb_db *db;
	int i;

	/* solution array is shared between threads */
	pthread_mutex_lock(&solve->mutex);

	/* too many solutions ? */
	if (solve->num_solutions == MAX_RT_SOLUTIONS) {
		adb_error(solve->db, "too many solutions, narrow params\n");
		pthread_mutex_unlock(&solve->mutex);
		return;
	}

	for (i = 0; i < runtime->num_pot_pa; i++) {
		soln = &solve->solution[solve->num_solutions];
		db = soln->db;

		/* is duplicate then try next. FIXME, just return here ? */
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

/*
 * Solve plate cluster for this primary object.
 *
 * Try and match this object to the primary
 */
static int try_object_as_primary(struct adb_solve *solve,
	const struct adb_object *primary)
{
	struct solve_runtime runtime;
	int i, count;

	memset(&runtime, 0, sizeof(runtime));
	runtime.solve = solve;
	adb_info(solve->db, ADB_LOG_SOLVE, "\n");
	/* find secondary candidate adb_source_objects on magnitude */
	for (i = 0; i < MIN_PLATE_OBJECTS - 1; i++) {
		count = mag_solve_object(&runtime, primary, i);
		if (!count)
			return 0;
	}
	adb_info(solve->db, ADB_LOG_SOLVE, "\n");
	/* at this point we have a range of candidate stars that match the
	 * magnitude bounds of the primary object and each secondary object,
	 * now check secondary candidates for distance alignment */
	count = distance_solve_object(&runtime, primary);
	if (!count)
		return 0;
	adb_info(solve->db, ADB_LOG_SOLVE, "\n");
	/* At this point we have a list of clusters that match on magnitude and
	 * distance, so we finally check the candidates clusters for PA alignment*/
	count = pa_solve_object(&runtime, primary, i);
	if (!count)
		return 0;
	adb_info(solve->db, ADB_LOG_SOLVE, "\n");
	calc_cluster_divergence(&runtime);

	/* copy matching clusters to solver */
	copy_solution(&runtime);

	return solve->num_solutions;
}

/*
 * Solve the plate cluster star pattern and find all matches.
 */
static int solve_plate_cluster_for_set_all(struct adb_solve *solve,
	struct adb_object_set *set)
{
	int i, count = 0, progress = 0;

	/* attempt to solve for each object in set */
#if HAVE_OPENMP
#pragma omp parallel for schedule(dynamic, 10) reduction(+:count, progress)
#endif
	for (i = 0; i < solve->haystack.num_objects; i++) {

		progress++;
		solve->progress++;

		/* we cant break OpenMP loops */
		if (solve->exit)
			continue;

		count += try_object_as_primary(solve, solve->haystack.objects[i]);
	}

	if (count >= MAX_RT_SOLUTIONS)
		count = MAX_RT_SOLUTIONS - 1;

	return count;
}

/*
 * Solve the plate cluster star pattern and find first match.
 */
static int solve_plate_cluster_for_set_first(struct adb_solve *solve,
	struct adb_object_set *set)
{
	int i, count = 0, progress = 0;

	/* attempt to solve for each object in set */
#if HAVE_OPENMP
#pragma omp parallel for schedule(dynamic, 10) reduction(+:progress)
#endif
	for (i = 0; i < solve->haystack.num_objects; i++) {

		adb_info(solve->db, ADB_LOG_SOLVE, " check %d\n", i);
		progress++;
		solve->progress++;

		/* we cant break OpenMP loops */
		if (count || solve->exit)
			continue;

		if (try_object_as_primary(solve, solve->haystack.objects[i])) {
			count = 1;
/* TODO OpenMP cancel is supported in gcc 4.9 */
/* #pragma omp cancel for */
		}
	}
/* #pragma omp cancellation point for */

	return count;
}

/*
 * Add a plate object to the solver.
 */
int adb_solve_add_plate_object(struct adb_solve *solve,
				struct adb_pobject *pobject)
{
	/* must be within target limit */
	if (solve->plate.num_objects == ADB_NUM_TARGETS - 1) {
		adb_error(solve->db, "too many adb_source_objects %d\n",
			ADB_NUM_TARGETS);
		return -EINVAL;
	}

	/* reject if plate object has no ADU */
	if (pobject->adu == 0) {
		adb_error(solve->db, "object has no ADUs\n");
		return -EINVAL;
	}

	/* copy to free plate object entry in array */
	memcpy(&solve->plate.object[solve->plate.num_objects++], pobject,
		sizeof(struct adb_pobject));

	return 0;
}

/*
 * Fine tune the solver constraints to minimise solve time.
 */
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
	case ADB_CONSTRAINT_AREA:
		solve->constraint.min_area = min;
		solve->constraint.max_area = max;
		break;
	case ADB_CONSTRAINT_JD:
		solve->constraint.min_JD = min;
		solve->constraint.max_JD = max;
		break;
	case ADB_CONSTRAINT_POBJECTS:
		solve->constraint.min_pobjects = min;
		solve->constraint.max_pobjects = max;
			break;
	default:
		adb_error(solve->db, "unknown constraint type %d\n", type);
		return -EINVAL;
	}
	return 0;
}

/*! \fn int adb_solve(struct adb_solve *solve,
		struct adb_object_set *set, enum adb_find find)
* \param solve Solver context
* \param set Set of object to use for solving.
* \return number of solutions found or negative error.
*
* Run the solver using the prec-onfigured constraints and plate objects.
*/
int adb_solve(struct adb_solve *solve,
		struct adb_object_set *set, enum adb_find find)
{
	int ret, i, count = 0;

	/* do we have enough plate adb_source_objects to solve */
	if (solve->plate.num_objects < MIN_PLATE_OBJECTS) {
		adb_error(solve->db,
			"not enough plate adb_source_objects, need %d have %d\n",
			MIN_PLATE_OBJECTS, solve->plate.num_objects);
		return -EINVAL;
	}

	/* prepare the set of objects to use for solving */
	ret = target_prepare_source_objects(solve, set, &solve->haystack);
	if (ret <= 0) {
		adb_error(solve->db, "cant get trixels %d\n", ret);
		return ret;
	}

	/* status reporting and exit */
	solve->progress = 0.0;
	solve->exit = 0;

	/*
	 * Iterate through plate objects using a window that is used to generate
	 * the solve pattern. We do this as some objects may not be in a catalog
	 * like planets, asteroids, comets and man made objects.
	 */
	for (i = 0; i <= solve->plate.num_objects - MIN_PLATE_OBJECTS; i++) {

		/* set the window bounds */
		solve->plate.window_start = i;
		solve->plate.window_end = MIN_PLATE_OBJECTS + i;

		adb_info(solve->db, ADB_LOG_SOLVE,
			"solving plate object[%d] -> object[%d] window from total %d\n",
			solve->plate.window_start, solve->plate.window_end - 1,
			solve->plate.num_objects);

		/* create the target pattern from the current window */
		target_create_pattern(solve);

		/* now look for the window pattern in the object set */
		if (find & ADB_FIND_ALL)
			ret = solve_plate_cluster_for_set_all(solve, set);
		else
			ret = solve_plate_cluster_for_set_first(solve, set);

		/* move on to next if good */
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
	solve->plate.width = width;
	solve->plate.height = height;
	solve->plate.ra = ra ;
	solve->plate.dec = dec;
}

struct adb_solve_solution *adb_solve_get_solution(struct adb_solve *solve,
	unsigned int index)
{
	if (solve->num_solutions == 0 || index >= solve->num_solutions)
		return NULL;

	return &solve->solution[index];
}

/*
 * Get number of plate objects used by solver.
 */
int adb_solve_get_pobject_count(struct adb_solve *solve,
		struct adb_solve_solution *solution)
{
	return solution->num_pobjects;
}

/*
 * Stop the solver. Must wait on next loop iteration.
 */
void adb_solve_stop(struct adb_solve *solve)
{
	solve->exit = 1;
}

/*
 * Get the solver progress between 0.0 and 1.0. 1 being complete.
 */
float adb_solve_get_progress(struct adb_solve *solve)
{
	return (float)solve->haystack.num_objects / solve->progress;
}

/*
 * Create a new solver context for table id using database db.
 */
struct adb_solve *adb_solve_new(struct adb_db *db, int table_id)
{
	struct adb_solve *solve;
	int i;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return NULL;

	solve = calloc(1, sizeof(struct adb_solve));
	if (solve == NULL)
		return NULL;

	/* use for multithread solving */
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
	solve->constraint.min_area = 0;
	solve->constraint.max_area = M_PI_2;
	solve->constraint.min_JD = 0;
	solve->constraint.max_JD = 0;
	solve->constraint.max_pobjects = 1000;
	solve->constraint.min_pobjects = MIN_PLATE_OBJECTS;
	solve->num_solutions = 0;

	for (i = 0; i < MAX_RT_SOLUTIONS; i++) {
		solve->solution[i].db = db;
		solve->solution[i].solve = solve;
	}

	return solve;
}

/*
 * Free all resources in solver context.
 */
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

	free(solve->haystack.objects);
	free(solve);
}

