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

#ifndef __LIBADB_SEARCH_H
#define __LIBADB_SEARCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct adb_db;
struct adb_search;
struct adb_object;
struct adb_object_set;

/******************** Table Search ********************************************/

/*! \typedef enum adb_operator
 * \ingroup search
 *
 * Search operators
 */
enum adb_operator {
	ADB_OP_AND,				/*!< AND */
	ADB_OP_OR				/*!< OR */
};

/*! \typedef enum adb_comparator
 * \ingroup search
 *
 * Search field comparators
 */
enum adb_comparator {
	ADB_COMP_LT,			/*!< less than */
	ADB_COMP_GT, 			/*!< greater than */
	ADB_COMP_EQ,			/*!< equal to */
	ADB_COMP_NE			/*!< not equal to */
};

/*! \typedef int (*adb_custom_comparator)(void* object);
 *
 * A customised object search comarator.
 */
typedef int (*adb_custom_comparator)(void *object);

/*! \fn adb_search* adb_search_new(adb_table *table);
 * \brief Creates an new search object
 * \ingroup search
 */
struct adb_search *adb_search_new(struct adb_db *db, int table_id);

/*! \fn void adb_search_free(adb_search* search);
 * \brief Free's a search and it resources
 * \ingroup search
 */
void adb_search_free(struct adb_search *search);

/*! \fn int adb_search_add_operator(adb_search* search, adb_operator op);
 * \brief Add an operation in RPN to the search
 * \ingroup search
 */
int adb_search_add_operator(struct adb_search *search,
				enum adb_operator op);

/*! \fn int adb_search_add_comparator(adb_search* search, char* field,
					adb_comparator compare, char* value);
 * \brief Add a comparator_t in RPN to the search
 * \ingroup search
 */
int adb_search_add_comparator(struct adb_search *search,
		const char* field, enum adb_comparator comp, const char* value);

/*! \fn int adb_search_add_custom_comparator(adb_search* search,
						adb_custom_comparator compare);
 * \brief Add a comparator_t in RPN to the search
 * \ingroup search
 */
int adb_search_add_custom_comparator(struct adb_search *search,
					adb_custom_comparator comp);

/*! \fn int adb_search_get_results(adb_search* search, adb_progress progress,
					adb_slist **result, unsigned int src);
 * \brief Execute a search
 * \ingroup search
 */
int adb_search_get_results(struct adb_search *search,
				struct adb_object_set *set,
				const struct adb_object **objects[]);

/*! \fn int adb_search_get_hits(adb_search* search);
 * \brief Get the number of search hit_count.
 * \ingroup search
 */
int adb_search_get_hits(struct adb_search *search);

/*! \fn int adb_search_get_tests(adb_search* search);
 * \brief Get the number of search test_count
 * \ingroup search
 */
int adb_search_get_tests(struct adb_search *search);

#ifdef __cplusplus
};
#endif

#endif
