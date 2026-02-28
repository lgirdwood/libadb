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

#include <errno.h> // IWYU pragma: keep
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "lib.h"
#include "solve.h"

/**
 * \brief Sorts solver solutions sequentially determining the best match.
 *
 * Determines hierarchy of 4-star match tuples via `qsort` measuring explicit 
 * variance "divergence" error values. Lower error implies tighter structural match.
 *
 * \param o1 Primary candidate solution cluster.
 * \param o2 Competitor candidate solution cluster.
 * \return 1 if o1 has higher error, -1 if o2 has higher error, 0 if even.
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
 * \brief Measure combined magnitude residual variance on a candidate tuple.
 *
 * Calculates discrepancy error between the relative instrumental brightness
 * ratio modeled from the plate targets, against the fixed catalog absolute
 * magnitudes observed in the proposed counterpart objects.
 *
 * \param runtime Core runtime execution solver space.
 * \param pot Target index tracking the specific potential asterism being measured.
 * \param idx The internal star correlation pairing needle index.
 * \return Magnitude offset differential variance.
 */
static double calc_magnitude_deltas(struct solve_runtime *runtime, int pot,
									int idx)
{
	struct adb_solve *solve = runtime->solve;
	struct adb_solve_solution *s = &runtime->pot_pa[pot];
	double plate_diff, db_diff;

	plate_diff = mag_get_plate_diff(&solve->plate.object[idx],
									&solve->plate.object[idx + 1]);

	db_diff = s->object[idx]->mag - s->object[idx + 1]->mag;

	return plate_diff - db_diff;
}

/**
 * \brief Calculate collective variance combining magnitude, distances, and PA arrays.
 *
 * Derives a weighted aggregate standard-error "divergence" score assessing 
 * the mathematical fitness of candidate asterism associations across 
 * magnitude, geometry lengths, and interior geometry rotation characteristics.
 *
 * \param runtime Core runtime state housing the candidate matches.
 */
static void calc_cluster_divergence(struct solve_runtime *runtime)
{
	int i;

	/* calculate differences in magnitude from DB and plate objects */
	for (i = 0; i < runtime->num_pot_pa; i++) {
		runtime->pot_pa[i].delta.mag = (calc_magnitude_deltas(runtime, i, 0) +
										calc_magnitude_deltas(runtime, i, 1) +
										calc_magnitude_deltas(runtime, i, 2)) /
									   3.0;

		runtime->pot_pa[i].divergance =
			runtime->pot_pa[i].delta.mag * DELTA_MAG_COEFF +
			runtime->pot_pa[i].delta.dist * DELTA_DIST_COEFF +
			runtime->pot_pa[i].delta.pa * DELTA_PA_COEFF;
	}
}

/**
 * \brief Verify if a candidate configuration mirrors an existing recorded solution.
 *
 * Iterates across current finalized solutions checking explicit celestial
 * component structures addressing the identical 4 stars.
 *
 * \param solve Core solver environment.
 * \param s1 The new solution layout proposing 4 catalog objects.
 * \return 1 if identical to prior knowledge, 0 for unique new record.
 */
static int is_solution_dupe(struct adb_solve *solve,
							struct adb_solve_solution *s1)
{
	struct adb_solve_solution *s2;
	int i;

	for (i = 0; i < solve->num_solutions; i++) {
		s2 = &solve->solution[i];
		if (s2->object[0] == s1->object[0] && s2->object[1] == s1->object[1] &&
			s2->object[2] == s1->object[2] && s2->object[3] == s1->object[3]) {
			return 1;
		}
	}
	return 0;
}

/**
 * \brief Lock and duplicate functional valid runtime candidates to public arrays.
 *
 * Operates a mutex-controlled commit step for threaded concurrent search spaces,
 * registering any candidates overcoming thresholds safely into parent state buffers.
 *
 * \param runtime Executed runtime structure mapping valid position angles buffers.
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

/**
 * \brief Orchestrate an anchor evaluation sequence against the catalog target.
 *
 * Sweeps progressively escalating filtration constraints isolating an specific anchor
 * entity acting as a tuple primary core. Validates against magnitude range bounds,
 * verifies secondary geometries by separation distances, validates asterisms against
 * exact position angles layouts, scoring surviving outputs via discrepancy coefficients.
 *
 * \param solve Orchestrating astrometrist context.
 * \param primary Specific catalog item tested as the anchor center node.
 * \return Overall system solutions accumulated so far during the validation execution.
 */
static int try_object_as_primary(struct adb_solve *solve,
								 const struct adb_object *primary)
{
	struct solve_runtime runtime;
	int i, count;

	memset(&runtime, 0, sizeof(runtime));
	runtime.solve = solve;
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "\n");
	/* find secondary candidate adb_source_objects on magnitude */
	for (i = 0; i < MIN_PLATE_OBJECTS - 1; i++) {
		count = mag_solve_object(&runtime, primary, i);
		if (!count)
			return 0;
	}
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "\n");
	/* at this point we have a range of candidate stars that match the
   * magnitude bounds of the primary object and each secondary object,
   * now check secondary candidates for distance alignment */
	count = distance_solve_object(&runtime, primary);
	if (!count)
		return 0;
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "\n");
	/* At this point we have a list of clusters that match on magnitude and
   * distance, so we finally check the candidates clusters for PA alignment*/
	count = pa_solve_object(&runtime, primary, i);
	if (!count)
		return 0;
	adb_vdebug(solve->db, ADB_LOG_SOLVE, "\n");
	calc_cluster_divergence(&runtime);

	/* copy matching clusters to solver */
	copy_solution(&runtime);

	return solve->num_solutions;
}

/**
 * \brief Iteratively scan ALL subset entities checking matches generating solutions parallel bounds.
 *
 * \param solve Primary execution constraints state framework context.
 * \param set Core physical source space defining raw object mappings structures.
 * \return Number of matching candidate geometries located during exploration matrix.
 */
static int solve_plate_cluster_for_set_all(struct adb_solve *solve,
										   struct adb_object_set *set)
{
	int i, count = 0;

	/* attempt to solve for each object in set */
#if HAVE_OPENMP
#pragma omp parallel for schedule(dynamic, 10) reduction(+ : count)
#endif
	for (i = 0; i < solve->haystack.num_objects; i++) {
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

/**
 * \brief Fast path search returning control block after identifying initial success tuple.
 *
 * Operates an accelerated exploration matrix isolating target correlations, actively
 * interrupting concurrent parallel streams once positive correlation verification locks.
 *
 * \param solve General solver bounds limit contexts.
 * \param set Active target database segment map.
 * \return Valid count equal to 1 reflecting rapid success or 0 failing check thresholds.
 */
static int solve_plate_cluster_for_set_first(struct adb_solve *solve,
											 struct adb_object_set *set)
{
	int i, count = 0;

	/* attempt to solve for each object in set */
#if HAVE_OPENMP
#pragma omp parallel for schedule(dynamic, 10)
#endif
	for (i = 0; i < solve->haystack.num_objects; i++) {
		adb_vdebug(solve->db, ADB_LOG_SOLVE, " check %d\n", i);
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

/**
 * \brief Queue a user-supplied instrumental object point onto the solver alignment stack.
 *
 * Injects a physical cartesian feature detected within original raw exposure images, 
 * utilizing associated radiometric ADU profiles tracking expected coordinates relationships.
 *
 * \param solve Active session receiving new point structure definitions.
 * \param pobject Fully populated instrumental component pointer.
 * \return Standard status success 0 or descriptive error code on failures.
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

/**
 * \brief Modify active tolerance limits shaping generalized search spaces and cutoffs.
 *
 * Tunes heuristic rules checking constraints (FOV spreads, target magnitude minimums,
 * bounding coordinate spaces) altering total work performed.
 *
 * \param solve Orchestrating layout context map tracking options.
 * \param type Identifier tracing specific variable setting targets.
 * \param min Baseline limiting ceiling value bounding rule block limits.
 * \param max Top limiting cutoff threshold applied filtering arrays. 
 * \return Typical exit conditions standard identifier 0 upon acceptable parameters.
 */
int adb_solve_constraint(struct adb_solve *solve, enum adb_constraint type,
						 double min, double max)
{
	switch (type) {
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

/**
 * \brief Execute plate solving against a set of objects.
 *
 * Analyzes the pre-configured set of target plate objects against the reference
 * stars in `set`, applying matching combinations based on magnitudes, spherical
 * distances, and position angles (PA), subject to predefined constraints.
 *
 * \param solve Solver context fully populated with constraints and plate
 * targets
 * \param set Bounded reference dataset subset of known stars
 * \param find Specification of matching rule (e.g. `ADB_FIND_ALL` or
 * `ADB_FIND_FIRST`)
 * \return The number of acceptable solutions found, or a negative error code
 */
int adb_solve(struct adb_solve *solve, struct adb_object_set *set,
			  enum adb_find find)
{
	int ret, i;

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

		adb_info(
			solve->db, ADB_LOG_SOLVE,
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
	}

	/* it's possible we may have > 1 solution so order them */
	qsort(solve->solution, solve->num_solutions,
		  sizeof(struct adb_solve_solution), solution_cmp);

	adb_info(solve->db, ADB_LOG_SOLVE, "Total %d solutions\n",
			 solve->num_solutions);
	return solve->num_solutions;
}

/**
 * \brief Bind upper thresholds constraining magnitude matching correlations.
 *
 * \param solve Operational memory configuration node mapping properties.
 * \param delta_mag Raw relative boundary limitation standard.
 * \return Success flag constant.
 */
int adb_solve_set_magnitude_delta(struct adb_solve *solve, double delta_mag)
{
	solve->tolerance.mag = delta_mag;
	return 0;
}

/**
 * \brief Bind upper thresholds constraining pixel radial distance matching correlations.
 *
 * \param solve Operational memory configuration node mapping properties.
 * \param delta_pixels Raw relative boundary limitation standard.
 * \return Success flag constant.
 */
int adb_solve_set_distance_delta(struct adb_solve *solve, double delta_pixels)
{
	solve->tolerance.dist = delta_pixels;
	return 0;
}

/**
 * \brief Bind upper thresholds constraining positional mapping angle matching correlations.
 *
 * \param solve Operational memory configuration node mapping properties.
 * \param delta_rad Raw relative boundary limitation standard in radians.
 * \return Success flag constant.
 */
int adb_solve_set_pa_delta(struct adb_solve *solve, double delta_rad)
{
	solve->tolerance.pa = delta_rad;
	return 0;
}

/**
 * \brief Calibrate central dimensions outlining standard background field environments.
 *
 * \param solve Master contextual node handling structural records.
 * \param width Span mapping array dimensional X layouts.
 * \param height Span mapping array dimensional Y layouts.
 * \param ra Preliminary general equatorial alignment axis indicator.
 * \param dec Preliminary general spatial declination geometry marker.
 */
void adb_solve_image_set_properties(struct adb_solve *solve, int width,
									int height, double ra, double dec)
{
	solve->plate.width = width;
	solve->plate.height = height;
	solve->plate.ra = ra;
	solve->plate.dec = dec;
}

/**
 * \brief Recover generated astrometric matrix output data points matching conditions.
 *
 * \param solve Parent session capturing final executed state array layouts.
 * \param index Ordered element tracing specific output data structures requested.
 * \return Solution construct pointers holding physical layout values.
 */
struct adb_solve_solution *adb_solve_get_solution(struct adb_solve *solve,
												  unsigned int index)
{
	if (solve->num_solutions == 0 || index >= solve->num_solutions)
		return NULL;

	return &solve->solution[index];
}

/**
 * \brief Trace instrumental points effectively processed establishing layout structures.
 *
 * \param solve Running configuration map bounds.
 * \param solution Extracted matched tuple result layout points.
 * \return Total items checked across active matrix parameters establishing baseline targets.
 */
int adb_solve_get_pobject_count(struct adb_solve *solve,
								struct adb_solve_solution *solution)
{
	return solution->num_pobjects;
}

/**
 * \brief Pause or abort heavy loop iteration evaluations gracefully terminating queries.
 *
 * \param solve Executional scope identifying threaded background matrices locking limits.
 */
void adb_solve_stop(struct adb_solve *solve)
{
	solve->exit = 1;
}

/**
 * \brief Return dynamic completion ratio tracking current matrix exploration boundaries.
 * 
 * Computes fraction representing depth mapped over cumulative valid haystack target arrays.
 *
 * \param solve Internal execution limits structures determining thread ranges.
 * \return Numeric percentage fractional representations mapping 0.0 baseline across 1.0 success completions.
 */
float adb_solve_get_progress(struct adb_solve *solve)
{
	return (float)solve->haystack.num_objects / solve->progress;
}

/**
 * \brief Configure initial execution context mapping dynamic properties defining a processing instance.
 *
 * \param db Baseline central parent database structure referencing core indices and schemas.
 * \param table_id Core catalog ID tracing internal lookup references.
 * \return Allocated dynamically populated operational workspace structures ready for object configurations.
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
	solve->constraint.max_ra = 2.0 * M_PI;
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

/**
 * \brief Dismantle operational execution states recovering memory matrices correctly isolating subcomponents.
 *
 * \param solve Fully populated parent tree environment structure defining thread conditions safely mapped out.
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
