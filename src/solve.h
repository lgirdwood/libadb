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
 *  Copyright (C) 2008 - 2014 Liam Girdwood
 */

#ifndef __ADB_SOLVE_H
#define __ADB_SOLVE_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include "debug.h"
#include "table.h"
#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include <libastrodb/solve.h>

/*! \defgroup solve Solve
 *
 * \brief Astronomical plate solving routines.
 *
 * Engine for matching unrecognized star fields to known catalogs. It utilizes
 * geometric asterism hashing (distances, parities, and magnitudes) across 
 * quad-patterns to compute accurate equatorial coordinates (RA/DEC) from 
 * raw plate focal geometries.
 */

/* Turns on debug output for each solve stage */
// #define DEBUG

#define MIN_PLATE_OBJECTS 4
#define MAX_POTENTAL_MATCHES 256
#define MAX_ACTUAL_MATCHES 16
#define MAX_RT_SOLUTIONS 32
#define DELTA_MAG_COEFF 0.5
#define DELTA_DIST_COEFF 1.0
#define DELTA_PA_COEFF 1.0

/*! \struct tdata
 * \ingroup solve
 */
struct tdata {
	double pattern_min;
	double pattern_max;
	double plate_actual;
};

/*! \struct needle_object
 * \ingroup solve
 */
struct needle_object {
	struct adb_pobject *pobject;
	struct tdata distance;
	struct tdata pa;
	struct tdata pa_flip;
	struct tdata mag;
};

/*! \struct needle_pattern
 * \ingroup solve
 */
struct needle_pattern {
	// struct needle_object primary;
	/* in order of brightness */
	struct needle_object secondary[ADB_NUM_TARGETS - 1];
};

/*! \struct target_solve_mag
 * \ingroup solve
 */
struct target_solve_mag {
	/* plate object magnitude bounds */
	int start_pos[MIN_PLATE_OBJECTS - 1];
	int end_pos[MIN_PLATE_OBJECTS - 1];
};

/*! \struct solve_tolerance
 * \ingroup solve
 */
struct solve_tolerance {
	/* tuning coefficients */
	double dist;
	double mag;
	double pa;
};

/*! \struct solve_constraint
 * \ingroup solve
 */
struct solve_constraint {
	/* solve constraints of plate/ccd */
	double min_ra;
	double max_ra;
	double min_dec;
	double max_dec;
	double min_mag;
	double max_mag;
	double min_fov;
	double max_fov;
	double min_area;
	double max_area;
	double min_JD;
	double max_JD;
	double min_pobjects;
	double max_pobjects;
};

/*! \struct adb_reference_object
 * \ingroup solve
 */
struct adb_reference_object {
	const struct adb_object *object;
	struct adb_pobject pobject;
	int id;

	/* estimated magnitude */
	float mag_mean;
	float mag_sigma;

	/* estimated position */
	double dist_mean;
	double posn_sigma;

	int clip_mag;
	int clip_posn;
};

/*! \struct adb_solve_solution
 * \ingroup solve
 */
struct adb_solve_solution {
	struct adb_solve *solve;

	/* objects and plate objects used to find solution */
	const struct adb_object *object[ADB_NUM_TARGETS];
	struct adb_pobject soln_pobject[ADB_NUM_TARGETS];

	/* plate objects to solve */
	int num_pobjects;
	struct adb_pobject *pobjects;

	/* source object storage */
	struct adb_source_objects source;
	struct adb_object_set *set;
	struct adb_db *db;

	/* solution delta to db */
	struct solve_tolerance delta;
	double divergance;
	double rad_per_pix;
	int flip;

	/* solved objects from current table */
	struct adb_solve_object *solve_object;
	int num_solved_objects;
	int num_unsolved_objects;
	int total_objects;

	/* reference objects - total solved objects - can come from any table */
	struct adb_reference_object *ref;
	int num_ref_objects;
};

/* solver runtime data */
/*! \struct solve_runtime
 * \ingroup solve
 */
struct solve_runtime {
	struct adb_solve *solve;

	/* potential matches on magnitude */
	struct target_solve_mag pot_magnitude;

	/* potential matches after magnitude and distance checks */
	struct adb_solve_solution pot_distance[MAX_POTENTAL_MATCHES];
	int num_pot_distance;
	int num_pot_distance_checked;

	/* potential matches after magnitude, distance and PA */
	struct adb_solve_solution pot_pa[MAX_POTENTAL_MATCHES];
	int num_pot_pa;

	/* target cluster */
	struct needle_object soln_target[MIN_PLATE_OBJECTS];

#ifdef DEBUG
	int debug;
#endif
};

/*! \struct adb_solve_plate
 * \ingroup solve
 */
struct adb_solve_plate {
	/* plate object solve window */
	int window_start;
	int window_end;

	/* plate width and height in pixels */
	int width;
	int height;

	double ra;
	double dec;

	/* detected plate objects */
	struct adb_pobject object[ADB_NUM_TARGETS];
	int num_objects;
};

/*! \struct adb_solve
 * \ingroup solve
 */
struct adb_solve {
	struct adb_db *db;
	struct adb_table *table;
	pthread_mutex_t mutex;

	struct solve_constraint constraint;

	/* target cluster */
	struct needle_pattern target;

	/* source object set */
	struct adb_source_objects haystack;

	struct solve_tolerance tolerance;

	/* potential solutions from all runtimes */
	struct adb_solve_solution solution[MAX_RT_SOLUTIONS];
	int num_solutions;

	/* plate properties */
	struct adb_solve_plate plate;

	/* loop exit */
	int exit;
	int progress;
};

#ifdef DEBUG
static const struct adb_object *dobj[4] = { NULL, NULL, NULL, NULL };
static const struct adb_object *sobj[4] = { NULL, NULL, NULL, NULL };

/* debug object designations for solution */
static const char *dnames[] = {
	"3992-746-1",
	"3992-942-1",
	"3992-193-1",
	"3996-1436-1",
};

/* debug object names for single objects */
static const char *snames[] = {
	"3992-648-1",
	"3992-645-1",
	"3992-349-1",
	"3992-882-1",
};

static void debug_init(struct adb_object_set *set)
{
	int i, found;

	fprintf(stdout, "!! enabling solve debug\n");
	adb_set_hash_key(set, ADB_FIELD_DESIGNATION);

	/* search for solution objects */
	for (i = 0; i < adb_size(dnames); i++) {
		found =
			adb_set_get_object(set, dnames[i], ADB_FIELD_DESIGNATION, &dobj[i]);
		if (found <= 0)
			fprintf(stderr, "can't find %s\n", dnames[i]);
	}

	/* search for single objects */
	for (i = 0; i < adb_size(snames); i++) {
		found =
			adb_set_get_object(set, snames[i], ADB_FIELD_DESIGNATION, &sobj[i]);
		if (found <= 0)
			fprintf(stderr, "can't find %s\n", snames[i]);
		else
			fprintf(stdout, "found %s mag %f\n", sobj[i]->designation,
					sobj[i]->mag);
	}
}

#define DOBJ_CHECK(stage, object)                                       \
	do {                                                                \
		if (object == dobj[stage - 1] && runtime->debug == stage - 1) { \
			runtime->debug = stage;                                     \
		} else {                                                        \
			if (runtime->debug == stage)                                \
				runtime->debug = stage - 1;                             \
		}                                                               \
	} while (0);
#define DOBJ_CHECK_DIST(stage, object1, object2, dist, min, max, num, i)       \
	do {                                                                       \
		if (stage < runtime->debug)                                            \
			fprintf(                                                           \
				stdout,                                                        \
				"pass %d:object %s (%3.3f) --> %s (%3.3f) dist %f min %f max " \
				"%f tests %d no %d\n",                                         \
				stage, object1->designation, object1->mag,                     \
				object2->designation, object2->mag, dist, min, max, num, i);   \
	} while (0)
#define DOBJ_LIST(stage, object1, object2, dist, i)                           \
	do {                                                                      \
		if (stage <= runtime->debug)                                          \
			fprintf(                                                          \
				stdout,                                                       \
				" check %d:object %s (%3.3f) --> %s (%3.3f) dist %f no %d\n", \
				stage, object1->designation, object1->mag,                    \
				object2->designation, object2->mag, dist, i);                 \
	} while (0)
#define DOBJ_PA_CHECK(object0, object1, object2, delta, min, max)              \
	do {                                                                       \
		if (object0 == dobj[0])                                                \
			fprintf(stdout,                                                    \
					"check %s --> (%s) <-- %s is %3.3f min %3.3f max %3.3f\n", \
					object1->designation, object0->designation,                \
					object2->designation, delta * R2D, min * R2D, max * R2D);  \
	} while (0)
#define SOBJ_CHECK(object)                                                 \
	do {                                                                   \
		if (object == sobj[0] || object == sobj[1] || object == sobj[2] || \
			object == sobj[3])                                             \
			runtime->debug = 1;                                            \
		else                                                               \
			runtime->debug = 0;                                            \
	} while (0);
#define SOBJ_CHECK_DIST(object, dist, min, max, num)                         \
	do {                                                                     \
		if (runtime->debug)                                                  \
			fprintf(stdout, "%d: object %s (%3.3f) dist %f min %f max %f\n", \
					num, object->designation, object->mag, dist, min, max);  \
	} while (0)
#define SOBJ_FOUND(object)                                        \
	do {                                                          \
		if (runtime->debug)                                       \
			fprintf(stdout, " *found %s\n", object->designation); \
		if (object == sobj[0])                                    \
			sobj[0] = NULL;                                       \
		if (object == sobj[1])                                    \
			sobj[1] = NULL;                                       \
		if (object == sobj[2])                                    \
			sobj[2] = NULL;                                       \
		if (object == sobj[3])                                    \
			sobj[3] = NULL;                                       \
	} while (0);
#define SOBJ_MAG(min, max) fprintf(stdout, "mag min %f max %f\n", min, max);
#define SOBJ_CHECK_SET(object)                                             \
	do {                                                                   \
		if (object == sobj[0] || object == sobj[1] || object == sobj[2] || \
			object == sobj[3])                                             \
			fprintf(stdout, "set found: %s\n", object->designation);       \
	} while (0);
#else
#define debug_init(set) \
	while (0) {         \
	}
#define DOBJ_CHECK(stage, object)
#define DOBJ_CHECK_DIST(stage, object1, object2, dist, min, max, num, i)
#define DOBJ_LIST(stage, object1, object2, dist, i)
#define DOBJ_PA_CHECK(object0, object1, object2, delta, min, max)
#define SOBJ_CHECK(object)
#define SOBJ_CHECK_DIST(object, dist, min, max, num)
#define SOBJ_FOUND(object)
#define SOBJ_MAG(min, max)
#define SOBJ_CHECK_SET(object)
#endif

static inline double dmax(double a, double b)
{
	return a > b ? a : b;
}

static inline double dmin(double a, double b)
{
	return a < b ? a : b;
}

static inline double tri_max(double a, double b, double c)
{
	return dmax(dmax(a, b), c);
}

static inline double tri_avg(double a, double b, double c)
{
	return (a + b + c) / 3.0;
}

static inline double quad_max(double a, double b, double c, double d)
{
	return dmax(dmax(a, b), dmax(c, d));
}

static inline double tri_diff(double a, double b, double c)
{
	return tri_max(a, b, c);
}

static inline double quad_diff(double a, double b, double c, double d)
{
	return quad_max(a, b, c, d);
}

static inline float quad_avg(float a, float b, float c, float d)
{
	return (a + b + c + d) / 4.0;
}

/* put PA in correct quadrant */
static inline double equ_quad(double pa)
{
	double pa_ = pa;

	if (pa > M_PI * 2.0)
		pa_ = pa - (M_PI * 2.0);
	if (pa < 0.0)
		pa_ = pa + (M_PI * 2.0);
	return pa_;
}

/**
 * \brief Get plate distance squared between primary and secondary objects
 * \ingroup solve
 * \param primary Pointer to primary plate object
 * \param secondary Pointer to secondary plate object
 * \return Distance squared in pixels
 */
double distance_get_plate(struct adb_pobject *primary,
						  struct adb_pobject *secondary);

/**
 * \brief Get distance in between two adb_source_objects
 * \ingroup solve
 * \param o1 Pointer to first source object
 * \param o2 Pointer to second source object
 * \return Distance in radians
 */
double distance_get_equ(const struct adb_object *o1,
						const struct adb_object *o2);

/**
 * \brief Get position angle in radians relative to plate north
 * \ingroup solve
 * \param primary Pointer to primary plate object
 * \param secondary Pointer to secondary plate object
 * \return Position angle in radians
 */
double pa_get_plate(struct adb_pobject *primary, struct adb_pobject *secondary);

/**
 * \brief Get position angle in radians
 * \ingroup solve
 * \param o1 Pointer to first source object
 * \param o2 Pointer to second source object
 * \return Position angle in radians
 */
double pa_get_equ(const struct adb_object *o1, const struct adb_object *o2);

/**
 * \brief Get ratio of magnitude from primary to secondary plate objects
 * \ingroup solve
 * \param primary Pointer to primary plate object
 * \param secondary Pointer to secondary plate object
 * \return Magnitude ratio
 */
double mag_get_plate_diff(struct adb_pobject *primary,
						  struct adb_pobject *secondary);

/**
 * \brief Calculate the average difference between plate ADU values and solution
 * objects. Use this as basis for calculating magnitudes based on plate ADU.
 * \ingroup solve
 * \param solve Pointer to solver context
 * \param solution Pointer to solution context
 * \param primary Pointer to primary plate object
 * \return Calculated magnitude
 */
float mag_get_plate(struct adb_solve *solve,
					struct adb_solve_solution *solution,
					struct adb_pobject *primary);

/**
 * \brief Compare pattern objects magnitude against source objects
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param primary Pointer to primary source object
 * \param idx Index of object
 * \return 0 if successful, negative error otherwise
 */
int mag_solve_object(struct solve_runtime *runtime,
					 const struct adb_object *primary, int idx);

/**
 * \brief Compare single pattern objects magnitude against source objects
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param solution Pointer to solution context
 * \param pobject Pointer to plate object
 * \return 0 if successful, negative error otherwise
 */
int mag_solve_single_object(struct solve_runtime *runtime,
							struct adb_solve_solution *solution,
							struct adb_pobject *pobject);

/**
 * \brief Compare magnitude of objects for sorting
 * \ingroup solve
 * \param o1 Pointer to first object
 * \param o2 Pointer to second object
 * \return Comparison result (-1, 0, or 1)
 */
int mag_object_cmp(const void *o1, const void *o2);

int pa_solve_object(struct solve_runtime *runtime,
					const struct adb_object *primary, int idx);

int pa_solve_single_object(struct solve_runtime *runtime,
						   struct adb_solve_solution *solution);

/**
 * \brief Check magnitude matched objects on pattern distance
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param primary Pointer to primary source object
 * \return 0 if successful, negative error otherwise
 */
int distance_solve_object(struct solve_runtime *runtime,
						  const struct adb_object *primary);

/**
 * \brief Check magnitude matched single object on pattern distance
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param solution Pointer to solution context
 * \return 0 if successful, negative error otherwise
 */
int distance_solve_single_object(struct solve_runtime *runtime,
								 struct adb_solve_solution *solution);

/**
 * \brief Check magnitude matched single object on extended pattern distance
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param solution Pointer to solution context
 * \return 0 if successful, negative error otherwise
 */
int distance_solve_single_object_extended(struct solve_runtime *runtime,
										  struct adb_solve_solution *solution);

/**
 * \brief Calculate equatorial RA/DEC from plate X/Y position
 * \ingroup solve
 * \param solution Pointer to solution match
 * \param primary Pointer to plate object
 * \param ra_ Pointer to store resulting RA
 * \param dec_ Pointer to store resulting DEC
 */
void posn_plate_to_equ(struct adb_solve_solution *solution,
					   struct adb_pobject *primary, double *ra_, double *dec_);
/**
 * \brief Calculate plate X/Y from equatorial RA/DEC
 * \ingroup solve
 * \param solution Pointer to solution match
 * \param ra Input RA
 * \param dec Input DEC
 * \param x_ Pointer to store resulting Plate X
 * \param y_ Pointer to store resulting Plate Y
 */
void posn_equ_to_plate(struct adb_solve_solution *solution, double ra,
					   double dec, double *x_, double *y_);

/**
 * \brief Add matching objects i,j,k to list of potentials on distance
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param primary Pointer to primary source object
 * \param source Pointer to source objects pool
 * \param i Index i
 * \param j Index j
 * \param k Index k
 * \param delta Computed distance delta
 * \param rad_per_pix Radians per pixel conversion factor
 */
void target_add_match_on_distance(struct solve_runtime *runtime,
								  const struct adb_object *primary,
								  struct adb_source_objects *source, int i,
								  int j, int k, double delta,
								  double rad_per_pix);

/**
 * \brief Add single object to list of potentials on distance
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param primary Pointer to primary source object
 * \param source Pointer to source objects pool
 * \param delta Computed distance delta
 * \param flip Boolean indicating mapping parity flip
 */
void target_add_single_match_on_distance(struct solve_runtime *runtime,
										 const struct adb_object *primary,
										 struct adb_source_objects *source,
										 double delta, int flip);

/**
 * \brief Solve distance for extended single object
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param solution Pointer to solution match
 * \return status 0 for success
 */
int distance_solve_single_object_extended(struct solve_runtime *runtime,
										  struct adb_solve_solution *solution);

/**
 * \brief Add extended single match to potentials targeting
 * \ingroup solve
 * \param runtime Pointer to solve runtime
 * \param primary Pointer to primary source object
 * \param source Pointer to source objects
 * \param delta Computed delta distance
 * \param flip Boolean flip flag
 */
void target_add_single_match_extended(struct solve_runtime *runtime,
									  const struct adb_object *primary,
									  struct adb_source_objects *source,
									  double delta, int flip);

/**
 * \brief Get a set of source objects to check the pattern against
 * \ingroup solve
 * \param solve Pointer to solve config
 * \param set Pointer to object set
 * \param source Pointer to source output
 * \return status 0 for success
 */
int target_prepare_source_objects(struct adb_solve *solve,
								  struct adb_object_set *set,
								  struct adb_source_objects *source);

/**
 * \brief Add reference object to target solution
 * \ingroup solve
 * \param soln Pointer to target solution
 * \param id Source object id
 * \param object Pointer to original object
 * \param pobject Pointer to matched plate object
 * \return status 0 for success
 */
int target_add_ref_object(struct adb_solve_solution *soln, int id,
						  const struct adb_object *object,
						  struct adb_pobject *pobject);

/**
 * \brief Create target pattern based on solve configs
 * \ingroup solve
 * \param solve Pointer to root solve context
 */
void target_create_pattern(struct adb_solve *solve);

/**
 * \brief Create a pattern of plate targets and sort by magnitude
 * \ingroup solve
 * \param solve Pointer to solve context
 * \param pobject Pointer to initial plate object
 * \param solution Pointer to matching destination solution
 * \param runtime Pointer to solve runtime iteration
 */
void target_create_single(struct adb_solve *solve, struct adb_pobject *pobject,
						  struct adb_solve_solution *solution,
						  struct solve_runtime *runtime);

/**
 * \brief Helper to compute magnitude plate coefficients
 * \ingroup solve
 * \param solution Target configuration
 */
void mag_calc_plate_coefficients(struct adb_solve_solution *solution);

/**
 * \brief Calculate the magnitude of all unsolved plate objects
 * \ingroup solve
 * \param solution Target configuration
 */
void mag_calc_unsolved_plate(struct adb_solve_solution *solution);

/**
 * \brief Calculate the magnitude, mag delta mean and mag delta sigma
 * of a solved plate object
 * \ingroup solve
 * \param solution Target configuration
 */
void mag_calc_solved_plate(struct adb_solve_solution *solution);

/**
 * \brief Helper to clip and set up initial position plate equations
 * \ingroup solve
 * \param solution Target solution setup
 */
void posn_clip_plate_coefficients(struct adb_solve_solution *solution);

/**
 * \brief Calculate the position of unsolved plate objects
 * \ingroup solve
 * \param solution Target configuration setup
 */
void posn_calc_unsolved_plate(struct adb_solve_solution *solution);

/**
 * \brief Calculate the position of solved plate objects
 * \ingroup solve
 * \param solution Target configuration setup
 */
void posn_calc_solved_plate(struct adb_solve_solution *solution);

/**
 * \brief Rapid inline computation of plate X/Y from angular coordinates
 * \ingroup solve
 * \param solution Pointer to setup instance context
 * \param ra Right Ascension in radians
 * \param dec Declination in radians
 * \param x_ Storage pointer to output plate X
 * \param y_ Storage pointer to output plate Y
 */
void posn_equ_to_plate_fast(struct adb_solve_solution *solution, double ra,
							double dec, double *x_, double *y_);

/**
 * \brief Rapid inline computation of angular coordinates from plate position
 * \ingroup solve
 * \param solution Pointer to layout parameters
 * \param primary Pointer to incoming plate object representation
 * \param ra_ Angular ascension offset target
 * \param dec_ Angular declination offset target
 */
void posn_plate_to_equ_fast(struct adb_solve_solution *solution,
							struct adb_pobject *primary, double *ra_,
							double *dec_);

#endif

#endif
