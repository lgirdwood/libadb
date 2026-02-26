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

/**
 * \defgroup search Table Search
 * \brief Database search and querying functionality
 */

/*! \enum adb_operator
 * \brief Search logical operators used to combine multiple search criteria
 * \ingroup search
 */
enum adb_operator {
	ADB_OP_AND, /*!< Logical AND operator */
	ADB_OP_OR /*!< Logical OR operator */
};

/*! \enum adb_comparator
 * \brief Search field comparators used to evaluate individual criteria
 * \ingroup search
 */
enum adb_comparator {
	ADB_COMP_LT, /*!< Less than comparator (<) */
	ADB_COMP_GT, /*!< Greater than comparator (>) */
	ADB_COMP_EQ, /*!< Equal to comparator (==) */
	ADB_COMP_NE /*!< Not equal to comparator (!=) */
};

/*! \typedef adb_custom_comparator
 * \brief A custom object search comparator function pointer type
 * \ingroup search
 *
 * This function prototype allows users to define custom filtering logic
 * during search operations.
 *
 * \param object Pointer to the object currently being evaluated
 * \return 1 if the object matches the custom criteria, 0 otherwise
 */
typedef int (*adb_custom_comparator)(void *object);

/*! \struct adb_search
 * \brief Search context containing the state of the search query
 * \ingroup search
 *
 * This opaque structure holds the compiled query, operators, comparators,
 * and tracks search execution statistics (hits and tests).
 */

/**
 * \brief Creates an new search context object
 * \ingroup search
 *
 * Initializes a new search query designed to run against a specific table
 * within the database.
 *
 * \param db Pointer to the database context
 * \param table_id The identifier of the table to search
 * \return Pointer to the newly created search context, or NULL on error
 */
struct adb_search *adb_search_new(struct adb_db *db, int table_id);

/**
 * \brief Free's a search context and its resources
 * \ingroup search
 *
 * Cleans up memory used by a search query once it is no longer needed.
 *
 * \param search The search context to free
 */
void adb_search_free(struct adb_search *search);

/**
 * \brief Add a logical operation (in RPN) to the search
 * \ingroup search
 *
 * libastrodb search queries are constructed using Reverse Polish Notation (RPN).
 * This function pushes a logical operator (AND, OR) onto the query stack.
 *
 * \param search The search context
 * \param op The operator to add (from adb_operator)
 * \return 0 on success, or an error code
 */
int adb_search_add_operator(struct adb_search *search, enum adb_operator op);

/**
 * \brief Add a field comparator (in RPN) to the search
 * \ingroup search
 *
 * Pushes a complete evaluation statement (field, comparator, value) onto
 * the query stack. When the query executes, it evaluates this statement
 * for constraints.
 *
 * \param search The search context
 * \param field The name of the database table field to evaluate
 * \param comp The comparison operator to apply
 * \param value The value to compare the field against
 * \return 0 on success, or an error code
 */
int adb_search_add_comparator(struct adb_search *search, const char *field,
							  enum adb_comparator comp, const char *value);

/**
 * \brief Add a custom comparator callback (in RPN) to the search
 * \ingroup search
 *
 * Pushes a user-defined C function onto the query stack that will be called
 * to evaluate objects during the search process.
 *
 * \param search The search context
 * \param comp The custom comparator function pointer
 * \return 0 on success, or an error code
 */
int adb_search_add_custom_comparator(struct adb_search *search,
									 adb_custom_comparator comp);

/**
 * \brief Execute a search query and retrieve matching results
 * \ingroup search
 *
 * Runs the compiled RPN query on the provided object set and collects pointers
 * to objects that match all search criteria.
 *
 * \param search The search context containing the compiled query
 * \param set The target object set containing candidate objects to query against
 * \param objects Pointer to an array of object pointers that will be populated with results
 * \return The number of results (hits) found, or a negative error code
 */
int adb_search_get_results(struct adb_search *search,
						   struct adb_object_set *set,
						   const struct adb_object **objects[]);

/**
 * \brief Get the total number of search hits (successful matches)
 * \ingroup search
 *
 * Returns the number of objects that successfully passed all query criteria
 * during the most recent execution.
 *
 * \param search The search context
 * \return The number of successful hits
 */
int adb_search_get_hits(struct adb_search *search);

/**
 * \brief Get the total number of search tests (evaluations)
 * \ingroup search
 *
 * Returns the total number of objects that were evaluated against the query
 * criteria. This is useful for analyzing query performance.
 *
 * \param search The search context
 * \return The total number of evaluation tests performed
 */
int adb_search_get_tests(struct adb_search *search);

#ifdef __cplusplus
};
#endif

#endif
