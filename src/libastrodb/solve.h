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
 *  Copyright (C) 2005 - 2014 Liam Girdwood
 */

#ifndef __LIBADB_SOLVE_H
#define __LIBADB_SOLVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct adb_db;
struct adb_object;
struct adb_object_set;

/******************** Table Solver ********************************************/

/**
 * \defgroup solve Plate Solver
 * \brief Astrometric plate solving functionality
 */

/**
 * \brief Maximum number of targets solver can use
 * \ingroup solve
 */
#define ADB_NUM_TARGETS 16

/*! \struct adb_pobject
 * \brief Plate object representing an extracted star/source from an image
 * \ingroup solve
 */
struct adb_pobject {
	int x; /*!< x coordinate of the object in pixels */
	int y; /*!< y coordinate of the object in pixels */
	unsigned int adu; /*!< analogue to digital units (flux/intensity) */
	unsigned int
		extended; /*!< is extended object (e.g., galaxy/nebula) rather than a point source */
};

/*! \enum adb_constraint
 * \brief Constraints that can be applied to the solver search
 * \ingroup solve
 */
enum adb_constraint {
	ADB_CONSTRAINT_MAG, /*!< min and maximum magnitude */
	ADB_CONSTRAINT_FOV, /*!< min and max image FOV in degrees */
	ADB_CONSTRAINT_RA, /*!< min and max image center RA in degrees */
	ADB_CONSTRAINT_DEC, /*!< min and max image DEC in degrees */
	ADB_CONSTRAINT_AREA, /*!< min and max search area size in degrees */
	ADB_CONSTRAINT_JD, /*!< min and max search date in Julian Days */
	ADB_CONSTRAINT_POBJECTS, /*!< min and max plate objects used to solve */
	/* TODO add others */
};

/*! \enum adb_find
 * \brief Flags for solving behavior - can be OR-ed together
 * \ingroup solve
 */
enum adb_find {
	ADB_FIND_ALL = 1 << 0, /*!< find all possible solutions */
	ADB_FIND_FIRST = 1 << 1, /*!< find first valid solution and stop */
	ADB_FIND_PLANETS = 1 << 2, /*!< include planets in search */
	ADB_FIND_ASTEROIDS = 1 << 3, /*!< include asteroids in search */
	ADB_FIND_PROPER_MOTION = 1
							 << 4, /*!< apply Proper Motion (PM) in solution */
};

/*! \enum adb_plate_bounds
 * \brief Plate bounding box corners for coordinate transformations
 * \ingroup solve
 */
enum adb_plate_bounds {
	ADB_BOUND_TOP_RIGHT, /*!< Top right corner of the plate */
	ADB_BOUND_TOP_LEFT, /*!< Top left corner of the plate */
	ADB_BOUND_BOTTOM_RIGHT, /*!< Bottom right corner of the plate */
	ADB_BOUND_BOTTOM_LEFT, /*!< Bottom left corner of the plate */
	ADB_BOUND_CENTRE, /*!< Center of the plate */
};

/*! \struct adb_source_objects
 * \brief Source objects wrapper
 * \ingroup solve
 */
struct adb_source_objects {
	const struct adb_object **objects; /*!< array of object pointers */
	int num_objects; /*!< number of objects in the array */
};

/*! \struct adb_solve_object
 * \brief Solved object mapping a catalog object to a plate object
 * \ingroup solve
 */
struct adb_solve_object {
	const struct adb_object *object; /*!< the catalog object */
	struct adb_pobject pobject; /*!< the plate object */

	/* object properties from image */
	double ra; /*!< Right Ascension in degrees */
	double dec; /*!< Declination in degrees */
	float mag; /*!< measured magnitude */

	/* estimated magnitude */
	float mean; /*!< mean magnitude */
	float sigma; /*!< magnitude standard deviation (sigma) */
};

/*! \struct adb_solve_solution
 * \brief Solver solution representing a successful or candidate plate solve
 * \ingroup solve
 */
struct adb_solve_solution;

/*! \struct adb_solve
 * \brief Solver context containing the state of the solving process
 * \ingroup solve
 */
struct adb_solve;

/**
 * \brief Creates a new solver context
 * \ingroup solve
 * \param db Pointer to the database context
 * \param table_id Table identifier to use for solving
 * \return Pointer to the newly created solver context, or NULL on error
 */
struct adb_solve *adb_solve_new(struct adb_db *db, int table_id);

/**
 * \brief Free a solver and its internal resources
 * \ingroup solve
 * \param solve The solver context to free
 */
void adb_solve_free(struct adb_solve *solve);

/**
 * \brief Set the magnitude delta tolerance for the solver
 * \ingroup solve
 * \param solve The solver context
 * \param delta_mag The magnitude tolerance (delta)
 * \return 0 on success, or an error code
 */
int adb_solve_set_magnitude_delta(struct adb_solve *solve, double delta_mag);

/**
 * \brief Set the distance delta tolerance for the solver
 * \ingroup solve
 * \param solve The solver context
 * \param delta_pixels The distance tolerance in pixels
 * \return 0 on success, or an error code
 */
int adb_solve_set_distance_delta(struct adb_solve *solve, double delta_pixels);

/**
 * \brief Set the position angle (PA) delta tolerance for the solver
 * \ingroup solve
 * \param solve The solver context
 * \param delta_degrees The position angle tolerance in degrees
 * \return 0 on success, or an error code
 */
int adb_solve_set_pa_delta(struct adb_solve *solve, double delta_degrees);

/**
 * \brief Add a plate object (extracted source/star) to the solver
 * \ingroup solve
 * \param solve The solver context
 * \param pobject Pointer to the plate object to add
 * \return 0 on success, or an error code
 */
int adb_solve_add_plate_object(struct adb_solve *solve,
							   struct adb_pobject *pobject);

/**
 * \brief Set a specific solver constraint (like FOV limits)
 * \ingroup solve
 * \param solve The solver context
 * \param type The constraint type from adb_constraint enum
 * \param min The minimum value for the constraint
 * \param max The maximum value for the constraint
 * \return 0 on success, or an error code
 */
int adb_solve_constraint(struct adb_solve *solve, enum adb_constraint type,
						 double min, double max);

/**
 * \brief Execute the solve process
 * \ingroup solve
 * \param solve The solver context
 * \param set Target object set to use for solving (can be NULL)
 * \param find Bitmask of adb_find flags controlling the search behavior
 * \return 0 on success, or an error code (e.g. if no solution is found)
 */
int adb_solve(struct adb_solve *solve, struct adb_object_set *set,
			  enum adb_find find);

/**
 * \brief Get a specific solution from the solver after running adb_solve
 * \ingroup solve
 * \param solve The solver context
 * \param solution The index of the solution to retrieve
 * \return Pointer to the requested solution, or NULL if the index is invalid
 */
struct adb_solve_solution *adb_solve_get_solution(struct adb_solve *solve,
												  unsigned int solution);

/**
 * \brief Retrieve all matched objects for a valid solution
 * \ingroup solve
 * \param solution The solver solution to retrieve objects from
 * \return 0 on success, or an error code
 */
int adb_solution_get_objects(struct adb_solve_solution *solution);

/**
 * \brief Retrieve extended object information from a valid solution
 * \ingroup solve
 * \param solution The solver solution
 * \return 0 on success, or an error code
 */
int adb_solution_get_objects_extended(struct adb_solve_solution *solution);

/**
 * \brief Add additional plate objects to an existing solution
 * \ingroup solve
 * \param solution The solver solution
 * \param pobjects Array of plate objects to add
 * \param num_pobjects Number of plate objects in the array
 * \return 0 on success, or an error code
 */
int adb_solution_add_pobjects(struct adb_solve_solution *solution,
							  struct adb_pobject *pobjects, int num_pobjects);

/**
 * \brief Set specific object search limits for a solution
 * \ingroup solve
 * \param solution The solver solution
 * \param fov Field of view limit in degrees
 * \param mag_limit Magnitude limit
 * \param table_id Table identifier to use
 * \return 0 on success, or an error code
 */
int adb_solution_set_search_limits(struct adb_solve_solution *solution,
								   double fov, double mag_limit, int table_id);

/**
 * \brief Get the total number of plate objects registered in a solution
 * \ingroup solve
 * \param solve The solver context
 * \param solution The solver solution
 * \return The number of plate objects, or -1 on error
 */
int adb_solve_get_pobject_count(struct adb_solve *solve,
								struct adb_solve_solution *solution);

/**
 * \brief Calculate the divergence (error metric) of a matched solution
 * \ingroup solve
 * \param solution The solver solution
 * \return The calculated divergence value
 */
double adb_solution_divergence(struct adb_solve_solution *solution);

/**
 * \brief Get a specific solved object from a solution by index
 * \ingroup solve
 * \param solution The solver solution
 * \param index The index of the object to retrieve
 * \return Pointer to the solved object, or NULL if index is out of bounds
 */
struct adb_solve_object *
adb_solution_get_object(struct adb_solve_solution *solution, int index);

/**
 * \brief Get a specific solved object from a solution based on pixel coordinates
 * \ingroup solve
 * \param solution The solver solution
 * \param x The x pixel coordinate to search near
 * \param y The y pixel coordinate to search near
 * \return Pointer to the found solved object, or NULL if not found
 */
struct adb_solve_object *
adb_solution_get_object_at(struct adb_solve_solution *solution, int x, int y);

/**
 * \brief Set generic image properties on the solver context
 * \ingroup solve
 * \param solve The solver context
 * \param width The width of the input image in pixels
 * \param height The height of the input image in pixels
 * \param ra The Right Ascension of the image center in degrees
 * \param dec The Declination of the image center in degrees
 */
void adb_solve_image_set_properties(struct adb_solve *solve, int width,
									int height, double ra, double dec);

/**
 * \brief Get the calculated pixel size (scale) from a successful solution
 * \ingroup solve
 * \param solution The solver solution
 * \return The pixel scale (typically arcseconds per pixel or degrees depending on context)
 */
double adb_solution_get_pixel_size(struct adb_solve_solution *solution);

/**
 * \brief Get the equatorial coordinates of specific plate boundaries
 * \ingroup solve
 * \param solution The solver solution
 * \param bounds The specific boundary (corner/center) to query
 * \param ra Pointer to store the resulting Right Ascension in degrees
 * \param dec Pointer to store the resulting Declination in degrees
 */
void adb_solution_get_plate_equ_bounds(struct adb_solve_solution *solution,
									   enum adb_plate_bounds bounds, double *ra,
									   double *dec);

/**
 * \brief Convert image plate pixel coordinates to equatorial coordinates
 * \ingroup solve
 * \param solution The solver solution containing transformation logic
 * \param x The x pixel coordinate on the plate
 * \param y The y pixel coordinate on the plate
 * \param ra Pointer to store the resulting Right Ascension in degrees
 * \param dec Pointer to store the resulting Declination in degrees
 */
void adb_solution_plate_to_equ_position(struct adb_solve_solution *solution,
										int x, int y, double *ra, double *dec);

/**
 * \brief Convert equatorial coordinates to image plate pixel coordinates
 * \ingroup solve
 * \param solution The solver solution containing transformation logic
 * \param ra The Right Ascension in degrees
 * \param dec The Declination in degrees
 * \param x Pointer to store the resulting x pixel coordinate
 * \param y Pointer to store the resulting y pixel coordinate
 */
void adb_solution_equ_to_plate_position(struct adb_solve_solution *solution,
										double ra, double dec, double *x,
										double *y);

/**
 * \brief Fast approximate conversion of plate pixel to equatorial coordinates
 * \ingroup solve
 * \param solution The solver solution
 * \param x The x pixel coordinate on the plate
 * \param y The y pixel coordinate on the plate
 * \param ra Pointer to store the resulting Right Ascension in degrees
 * \param dec Pointer to store the resulting Declination in degrees
 */
void adb_solution_plate_to_equ_position_fast(
	struct adb_solve_solution *solution, int x, int y, double *ra, double *dec);

/**
 * \brief Fast approximate conversion of equatorial to plate pixel coordinates
 * \ingroup solve
 * \param solution The solver solution
 * \param ra The Right Ascension in degrees
 * \param dec The Declination in degrees
 * \param x Pointer to store the resulting x pixel coordinate
 * \param y Pointer to store the resulting y pixel coordinate
 */
void adb_solution_equ_to_plate_position_fast(struct adb_solve_solution *solution,
											 double ra, double dec, double *x,
											 double *y);

/**
 * \brief Calculate photometry properties for matches in a solution
 * \ingroup solve
 * \param solution The solver solution
 * \return 0 on success, or an error code
 */
int adb_solution_calc_photometry(struct adb_solve_solution *solution);

/**
 * \brief Calculate detailed astrometry metrics for matches in a solution
 * \ingroup solve
 * \param solution The solver solution
 * \return 0 on success, or an error code
 */
int adb_solution_calc_astrometry(struct adb_solve_solution *solution);

/**
 * \brief Request the solver to stop the current long-running operation
 * \ingroup solve
 * \param solve The solver context
 */
void adb_solve_stop(struct adb_solve *solve);

/**
 * \brief Get the progress of the current generalized solving operation
 * \ingroup solve
 * \param solve The solver context
 * \return A float between 0.0 and 1.0 representing completion progress
 */
float adb_solve_get_progress(struct adb_solve *solve);

/**
 * \brief Get the progress of solving for a specific solution trajectory
 * \ingroup solve
 * \param solution The solver solution
 * \return A float between 0.0 and 1.0 representing completion progress
 */
float adb_solution_get_progress(struct adb_solve_solution *solution);

#ifdef __cplusplus
};
#endif

#endif
