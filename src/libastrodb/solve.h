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

#define ADB_NUM_TARGETS	16

/*! \struct adb_pobject
 * \brief Plate object
 * \ingroup solve
 */
struct adb_pobject {
	int x;
	int y;
	unsigned int adu;
};

enum adb_constraint {
	ADB_CONSTRAINT_MAG,
	ADB_CONSTRAINT_FOV,
	ADB_CONSTRAINT_RA,
	ADB_CONSTRAINT_DEC,
	/* TODO add others */
};

enum adb_find {
	ADB_FIND_ALL,
	ADB_FIND_FIRST,
};

struct adb_source_objects {
	const struct adb_object **objects;
	int num_objects;
};

/*! \struct adb_solve_solution
 * \brief Solved object
 * \ingroup solve
 */
struct adb_solve_object {
	const struct adb_object *object;
	struct adb_pobject pobject;

	/* object properties from image */
	double ra;
	double dec;
	float mag;
};

/*! \struct adb_solve_solution
 * \brief Solver solution
 * \ingroup solve
 */
struct adb_solve_solution {
	/* in order of brightness */
	const struct adb_object *object[ADB_NUM_TARGETS];

	/* source object storage */
	struct adb_source_objects source;
	struct adb_object_set *set;

	/* solution delta to db */
	double delta_pa;
	double delta_distance;
	double delta_magnitude;
	double divergance;
	double rad_per_1kpix;
	int flip;

	/* solved objects */
	struct adb_solve_object *solve_object;
	int num_solved_objects;
};

struct adb_solve;

/*! \fn adb_search* adb_search_new(adb_table *table);
 * \brief Creates an new search object
 * \ingroup search
 */
struct adb_solve *adb_solve_new(struct adb_db *db, int table_id);

/*! \fn void adb_search_free(adb_search* search);
 * \brief Free's a search and it resources
 * \ingroup search
 */
void adb_solve_free(struct adb_solve *solve);

int adb_solve_set_magnitude_delta(struct adb_solve *solve,
		double delta_mag);

int adb_solve_set_distance_delta(struct adb_solve *solve,
		double delta_pixels);

int adb_solve_set_pa_delta(struct adb_solve *solve,
		double delta_degrees);

/*! \fn int adb_search_add_operator(adb_search* search, adb_operator op);
 * \brief Add an operation in RPN to the search
 * \ingroup search
 */
int adb_solve_add_plate_object(struct adb_solve *solve,
				struct adb_pobject *pobject);

/*! \fn int adb_search_add_comparator(adb_search* search, char* field,
					adb_comparator compare, char* value);
 * \brief Add a comparator_t in RPN to the search
 * \ingroup search
 */
int adb_solve_constraint(struct adb_solve *solve,
		enum adb_constraint type, double min, double max);


/*! \fn int adb_solve(struct adb_solve *solve,
				struct adb_object_set *set,
				struct adb_solve_objects **solve_objects[],
				double dist_coeff, double mag_coeff,
				double pa_coeff);
 * \brief Execute a search
 * \ingroup search
 */
int adb_solve(struct adb_solve *solve, struct adb_object_set *set,
		enum adb_find find);

int adb_solve_get_solutions(struct adb_solve *solve,
	unsigned int solution, struct adb_solve_solution **solutions);

/*! \fn int adb_solve_get_object(struct adb_solve *solve,
		struct adb_solve_objects *solve_objects,
		struct adb_pobject *pobject, const struct adb_object **object);
 * \brief Execute a search
 * \ingroup search
 */
int adb_solve_get_objects(struct adb_solve *solve,
	struct adb_solve_solution *solution,
	struct adb_pobject *pobjects, int num_pobjects);

int adb_solve_prep_solution(struct adb_solve *solve,
		unsigned int solution, double fov, double mag_limit);

#ifdef __cplusplus
};
#endif

#endif
