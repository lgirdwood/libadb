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

#define ADB_NUM_TARGETS 16

/*! \struct adb_pobject
 * \brief Plate object
 * \ingroup solve
 */
struct adb_pobject {
  int x;                 /*!< x coordinate */
  int y;                 /*!< y coordinate */
  unsigned int adu;      /*!< analogue to digital units */
  unsigned int extended; /*!< is extended object */
};

enum adb_constraint {
  ADB_CONSTRAINT_MAG,      /*!< min and maximum magnitude */
  ADB_CONSTRAINT_FOV,      /*!< min and max image FOV */
  ADB_CONSTRAINT_RA,       /*!< min and max image center RA */
  ADB_CONSTRAINT_DEC,      /*!< min and max image DEC */
  ADB_CONSTRAINT_AREA,     /*!< min and max search area size in degrees */
  ADB_CONSTRAINT_JD,       /*!< min and max search data in JD */
  ADB_CONSTRAINT_POBJECTS, /*!< min and max plate objects used to solve */
                           /* TODO add others */
};

/* flags for solving - can be OR-ed together */
enum adb_find {
  ADB_FIND_ALL = 1 << 0,           /*!< find all solutions */
  ADB_FIND_FIRST = 1 << 1,         /*!< find first solutions */
  ADB_FIND_PLANETS = 1 << 2,       /*!< include planets in search */
  ADB_FIND_ASTEROIDS = 1 << 3,     /*!< include asteroids in search */
  ADB_FIND_PROPER_MOTION = 1 << 4, /*!< apply PM in solution */
};

enum adb_plate_bounds {
  ADB_BOUND_TOP_RIGHT,
  ADB_BOUND_TOP_LEFT,
  ADB_BOUND_BOTTOM_RIGHT,
  ADB_BOUND_BOTTOM_LEFT,
  ADB_BOUND_CENTRE,
};

/*! \struct adb_source_objects
 * \brief Source objects
 */
struct adb_source_objects {
  const struct adb_object **objects; /*!< objects array */
  int num_objects;                   /*!< number of objects */
};

/*! \struct adb_solve_object
 * \brief Solved object
 * \ingroup solve
 */
struct adb_solve_object {
  const struct adb_object *object; /*!< the catalog object */
  struct adb_pobject pobject;      /*!< the plate object */

  /* object properties from image */
  double ra;  /*!< Right Ascension */
  double dec; /*!< Declination */
  float mag;  /*!< magnitude */

  /* estimated magnitude */
  float mean;  /*!< mean mag */
  float sigma; /*!< mag sigma */
};

/*! \struct adb_solve_solution
 * \brief Solver solution
 * \ingroup solve
 */

struct adb_solve_solution;
struct adb_solve;

/**
 * \brief Creates a new solver context
 * \ingroup solve
 */
struct adb_solve *adb_solve_new(struct adb_db *db, int table_id);

/**
 * \brief Free a solver and its resources
 * \ingroup solve
 */
void adb_solve_free(struct adb_solve *solve);

int adb_solve_set_magnitude_delta(struct adb_solve *solve, double delta_mag);

int adb_solve_set_distance_delta(struct adb_solve *solve, double delta_pixels);

int adb_solve_set_pa_delta(struct adb_solve *solve, double delta_degrees);

/**
 * \brief Add a plate object to the solver
 * \ingroup solve
 */
int adb_solve_add_plate_object(struct adb_solve *solve,
                               struct adb_pobject *pobject);

/**
 * \brief Set a solver constraint
 * \ingroup solve
 */
int adb_solve_constraint(struct adb_solve *solve, enum adb_constraint type,
                         double min, double max);

/**
 * \brief Execute the solve process
 * \ingroup solve
 */
int adb_solve(struct adb_solve *solve, struct adb_object_set *set,
              enum adb_find find);

struct adb_solve_solution *adb_solve_get_solution(struct adb_solve *solve,
                                                  unsigned int solution);

/**
 * \brief Retrieve objects from a valid solution
 * \ingroup solve
 */
int adb_solution_get_objects(struct adb_solve_solution *solution);

int adb_solution_get_objects_extended(struct adb_solve_solution *solution);

int adb_solution_add_pobjects(struct adb_solve_solution *solution,
                              struct adb_pobject *pobjects, int num_pobjects);

int adb_solution_set_search_limits(struct adb_solve_solution *solution,
                                   double fov, double mag_limit, int table_id);

int adb_solve_get_pobject_count(struct adb_solve *solve,
                                struct adb_solve_solution *solution);

double adb_solution_divergence(struct adb_solve_solution *solution);

struct adb_solve_object *
adb_solution_get_object(struct adb_solve_solution *solution, int index);

struct adb_solve_object *
adb_solution_get_object_at(struct adb_solve_solution *solution, int x, int y);

void adb_solve_image_set_properties(struct adb_solve *solve, int width,
                                    int height, double ra, double dec);

double adb_solution_get_pixel_size(struct adb_solve_solution *solution);

void adb_solution_get_plate_equ_bounds(struct adb_solve_solution *solution,
                                       enum adb_plate_bounds bounds, double *ra,
                                       double *dec);

void adb_solution_plate_to_equ_position(struct adb_solve_solution *solution,
                                        int x, int y, double *ra, double *dec);

void adb_solution_equ_to_plate_position(struct adb_solve_solution *solution,
                                        double ra, double dec, double *x,
                                        double *y);

void adb_solution_plate_to_equ_position_fast(
    struct adb_solve_solution *solution, int x, int y, double *ra, double *dec);

void adb_solution_equ_to_plate_position_fast(
    struct adb_solve_solution *solution, double ra, double dec, double *x,
    double *y);

int adb_solution_calc_photometry(struct adb_solve_solution *solution);

int adb_solution_calc_astrometry(struct adb_solve_solution *solution);

void adb_solve_stop(struct adb_solve *solve);

float adb_solve_get_progress(struct adb_solve *solve);

float adb_solution_get_progress(struct adb_solve_solution *solution);

#ifdef __cplusplus
};
#endif

#endif
