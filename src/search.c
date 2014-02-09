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
 *  Copyright (C) 2008, 2012 Liam Girdwood
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libastrodb/adbstdio.h>
#include <libastrodb/astrodb.h>
#include <libastrodb/table.h>
#include <libastrodb/db.h>
#include <libastrodb/readme.h>

#define MAX_PARAMS			128
#define PARAM_OPER			0
#define PARAM_COMP			1
#define OP_OP				0
#define OP_COMP				1

#define SRCH_MAX_BRANCH_TESTS		32


/*! \defgroup search Search
 *
 * Searching
 *
 * Reverse Polish Notation is used to define the search parameters. 
 */

struct adb_search_branch;

/*! \typedef struct astrodb_search
 * \ingroup search
 * \brief Search object
 */
struct astrodb_search {
	struct astrodb_db *db;
	struct astrodb_table *table;

	int test_count;         		/*!< number of search test_count */
	int hit_count;			/*!< number of search hit_count */

	struct adb_search_branch *root;	/*!< root search test */

	struct adb_search_branch *branch_orphan[SRCH_MAX_BRANCH_TESTS];	/*!< orphaned operator_t children */
	int branch_orphan_count;

	struct adb_search_branch *test_orphan[SRCH_MAX_BRANCH_TESTS];/*!< orphaned comparator_t children */
	int test_orphan_count;

	struct adb_search_branch *start_branch;
	int search_test_count;		/*!< number of search nodes */

	const struct astrodb_object **objects;	/*!< search result objects */
};

/* compares <data> with <value> using <comparator_t> */
typedef int (*comparator_t)(void *data, void *value);

/* operation on  nodes / lists */
typedef int (*operator_t)(const struct astrodb_object *object,
		struct adb_search_branch *, int num);

struct adb_search_branch {
	comparator_t compare;	/*!< comparator_t func */
	int offset;				/*!< data offset */
	void *value;

	operator_t operator;		/*!< operator_t on list */
	unsigned int type;

	struct adb_search_branch *branch[SRCH_MAX_BRANCH_TESTS];
	int test_count;
};

/* comparators (type)_(operator)_comp */
static int int_lt_comp(void *data, void *value)
{
	return *(int *) data < *(int *) value;
}

static int int_gt_comp(void *data, void *value)
{
	return *(int *) data > *(int *) value;
}

static int int_eq_comp(void *data, void *value)
{
	return *(int *) data == *(int *) value;
}

static int int_ne_comp(void *data, void *value)
{
	return *(int *) data != *(int *) value;
}

static int float_lt_comp(void *data, void *value)
{
	return *(float *) data < *(float *) value;
}

static int float_gt_comp(void *data, void *value)
{
	return *(float *) data > *(float *) value;
}

static int float_eq_comp(void *data, void *value)
{
	return *(float *) data == *(float *) value;
}

static int float_ne_comp(void *data, void *value)
{
	return *(float *) data != *(float *) value;
}

static int double_lt_comp(void *data, void *value)
{
	return *(double *) data < *(double *) value;
}

static int double_gt_comp(void *data, void *value)
{
	return *(double *) data > *(double *) value;
}

static int double_eq_comp(void *data, void *value)
{
	return *(double *) data == *(double *) value;
}

static int double_ne_comp(void *data, void *value)
{
	return *(double *) data != *(double *) value;
}

static int string_lt_comp(void *data, void *value)
{
	return (strcmp(data, value) > 0);
}

static int string_gt_comp(void *data, void *value)
{	
	return (strcmp(data, value) < 0);
}

static int string_eq_comp(void *data, void *value)
{
	return !strcmp(data, value);
}

static int string_ne_comp(void *data, void *value)
{
	return strcmp(data, value);
}

// TODO: implement regcomp
static int string_eq_wildcard_comp(void *data, void *value)
{
	int i = 0;
	char *ptr = value;
	
	while (*ptr != '*') {
		i++;
		ptr++;
	}

	return !strncmp(data, value, i);
}

/*  test list operators, operate on list of struct adb_search_branch's */
static int test_AND_comp(const struct astrodb_object *object,
		struct adb_search_branch *branch, int num)
{
	struct adb_search_branch *test = branch->branch[--num];
	char *ptr = (char*) object;

	if (num > 0)
		return  test->compare(ptr + test->offset, test->value) &&
				test_AND_comp(object, branch, num);

	/* last one */
	return test->compare(ptr + test->offset, test->value);
}

static int test_OR_comp(const struct astrodb_object *object,
		struct adb_search_branch *branch, int num)
{
	struct adb_search_branch *test = branch->branch[--num];
	char *ptr = (char*) object;

	if (num > 0)
		return test->compare(ptr + test->offset, test->value)
			|| test_OR_comp(object, branch, num);

	/* last one */
	return test->compare(ptr + test->offset, test->value);
}

/*  test list operators, operate on list of struct adb_search_branch's */
static int test_AND_oper(const struct astrodb_object *object,
		struct adb_search_branch *branch, int num)
{
	struct adb_search_branch *test = branch->branch[--num];

	if (num > 0)
		return test->operator(object, test, test->test_count) && 
			test_AND_oper(object, branch, num);

	/* last one */
	return test->operator(object, test, test->test_count);
}

static int test_OR_oper(const struct astrodb_object *object,
		struct adb_search_branch *branch, int num)
{
	struct adb_search_branch *test = branch->branch[--num];

	if (num > 0)
		return test->operator(object, test, test->test_count) ||
			test_OR_oper(object, branch, num);
	/* last one */
	return test->operator(object, test, test->test_count);
}

static comparator_t get_comparator(enum astrodb_comparator comp,
	astrodb_ctype ctype)
{
	switch (ctype) {
	case ADB_CTYPE_INT:
	case ADB_CTYPE_SHORT:
		switch (comp) {
		case ADB_COMP_LT:
			return int_lt_comp;
		case ADB_COMP_GT:
			return int_gt_comp;
		case ADB_COMP_EQ:
			return int_eq_comp;
		case ADB_COMP_NE:
			return int_ne_comp;
		}
		break;
	case ADB_CTYPE_FLOAT:
		switch (comp) {
		case ADB_COMP_LT:
			return float_lt_comp;
		case ADB_COMP_GT:
			return float_gt_comp;
		case ADB_COMP_EQ:
			return float_eq_comp;
		case ADB_COMP_NE:
			return float_ne_comp;
		}
		break;
	case ADB_CTYPE_DOUBLE:
		switch (comp) {
		case ADB_COMP_LT:
			return double_lt_comp;
		case ADB_COMP_GT:
			return double_gt_comp;
		case ADB_COMP_EQ:
			return double_eq_comp;
		case ADB_COMP_NE:
			return double_ne_comp;
		}
		break;
	case ADB_CTYPE_STRING:
		switch (comp) {
		case ADB_COMP_LT:
			return string_lt_comp;
		case ADB_COMP_GT:
			return string_gt_comp;
		case ADB_COMP_EQ:
			return string_eq_comp;
		case ADB_COMP_NE:
			return string_ne_comp;
		}
		break;
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_DOUBLE_MPC:
		return NULL;
	}
	return NULL;
}

/*! \fn astrodb_search* astrodb_search_new(astrodb_table *table)
 * \param table dataset
 * \returns astrodb_search object on success or NULL on failure
 *
 * Creates an new search object
 */
struct astrodb_search *astrodb_search_new(struct astrodb_db *db, int table_id)
{
	struct astrodb_table *table = &db->table[table_id];
	struct astrodb_search *search;

	if (table_id < 0 || table_id > db->table_count)
		return NULL;

	search = calloc(1, sizeof(struct astrodb_search));
	if (search == NULL)
		return NULL;

	search->db = db;
	search->table = &db->table[table_id];

	search->objects =
		calloc(table->object.count, sizeof(struct astrodb_object *));
	if (search->objects == NULL) {
		free(search);
		return NULL;
	}

	return search;
}

static inline void free_branch(struct adb_search_branch *branch)
{
	int i;

	for (i = 0; i < branch->test_count; i++)
		free_branch(branch->branch[i]);

	free(branch->value);
	free(branch);
}

/*! \fn void astrodb_search_free(astrodb_search* search);
 * \param search Search
 *
 * Free's a search and it resources
 */
void astrodb_search_free(struct astrodb_search *search)
{
	free(search->objects);
	free_branch(search->start_branch);
	free(search);
}

/*! \fn int astrodb_search_add_operator(astrodb_search* search, astrodb_operator op);
 * \param search Search
 * \param op Operator
 * \returns 0 on success
 *
 * Add a test operation in RPN to the search
 */
int astrodb_search_add_operator(struct astrodb_search *search,
				enum astrodb_operator op)
{
	struct adb_search_branch *branch;
	int i;
	
	/* cannot have lone operator_t */
	if (search->root == NULL && 
		search->branch_orphan_count == 0 &&
		search->test_orphan_count == 0) {
		return -EINVAL;
	}
	
	branch = calloc(1, sizeof(struct adb_search_branch));
	if (branch == NULL)
		return -ENOMEM;

	adb_debug(search->db, ADB_LOG_SEARCH, "new %s branch with %d test and %d branch orphans\n",
		op == ADB_OP_AND ? "AND" : "OR",
		search->test_orphan_count, search->branch_orphan_count);

	/* we either have a parent of a compare or op
	 * or a sibling of an op 
	 *
	 * comparator_t parent = test_orphan != NULL, branch_orphan = NULL
	 * operator_t parent = test_orphan = NULL, branch_orphan != NULL
	 */
	
	if (search->test_orphan_count > 0) {
		/* comparator_t parent */
		switch (op) {
		case ADB_OP_AND:
			branch->operator = test_AND_comp;
			break;
		case ADB_OP_OR:
			branch->operator = test_OR_comp;
			break;
		}
		branch->type = OP_COMP;

		for (i = 0; i < search->test_orphan_count; i++) {
			branch->branch[i] = search->test_orphan[i];
			adb_debug(search->db, ADB_LOG_SEARCH, "added test for offset %d index %d to branch\n",
				branch->branch[i]->offset, i);
		}

		branch->test_count = search->test_orphan_count;
		search->test_orphan_count = 0;

	} else {
		/* operator_t parent */
		switch (op) {
		case ADB_OP_AND:
			branch->operator = test_AND_oper;
			break;
		case ADB_OP_OR:
			branch->operator = test_OR_oper;
			break;
		}

		for (i = 0; i < search->branch_orphan_count; i++) {
			branch->branch[i] = search->branch_orphan[i];
			adb_debug(search->db, ADB_LOG_SEARCH, "added %s branch at index %d\n",
				search->branch_orphan[i]->operator == test_AND_oper ? "AND" : "OR", i);
		}

		branch->test_count = search->branch_orphan_count;
		search->branch_orphan_count = 0;
		branch->type = OP_OP;
	}
	
	for (i = search->search_test_count; i > 0; i--)
		search->branch_orphan[i] = search->branch_orphan[i - 1];

	/* we are an orphan ourselves */
	search->branch_orphan[0] = branch;
	search->start_branch = branch;
	search->branch_orphan_count++; 
	adb_debug(search->db, ADB_LOG_SEARCH, "added %s branch as root index, total %d\n",
		search->branch_orphan[0]->operator == test_AND_oper ? "AND" : "OR",
		search->search_test_count);
	search->search_test_count++;
	return 0;
}

/*! \fn int astrodb_search_add_comparator(astrodb_search* search, char* field, astrodb_comparator compare, char* value);
 * \param search Search
 * \param field Field name
 * \param compare Comparator
 * \param value Compare value
 *
 * Add a comparator_t in RPN to the search
 */
int astrodb_search_add_comparator(struct astrodb_search *search,
		const char *field, enum astrodb_comparator comp, const char *value)
{
	astrodb_ctype ctype;
	struct adb_search_branch *test;
	comparator_t search_comp;
	
	ctype = astrodb_table_get_field_type(search->db, search->table->id, field);
	if (ctype == ADB_CTYPE_NULL) {
		adb_error(search->db, "invalid C type for field %s\n", field);
		return -EINVAL;
	}

	search_comp = get_comparator(comp, ctype);
	if (search_comp == NULL) {
		adb_error(search->db, "failed to get comparator %d for C type %d\n",
			comp, ctype);
		return -EINVAL;
	}

	if (strstr(value, "*"))
		search_comp = string_eq_wildcard_comp;
	
	test = calloc(1, sizeof(struct adb_search_branch));
	if (test == NULL)
		return -ENOMEM;

	test->offset = astrodb_table_get_field_offset(search->db, search->table->id, field);
	test->compare = search_comp;

	adb_debug(search->db, ADB_LOG_SEARCH, "new test on field %s for value %s with type %d offset %d at %d comp %p\n\n",
			field, value, ctype, test->offset, search->test_orphan_count,  test->compare);

	switch (ctype) {
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_STRING:
	case ADB_CTYPE_DOUBLE_MPC:
		test->value = strdup(value);
		if (test->value == NULL)
			goto err;
		break;
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_INT:
		test->value = calloc(1, sizeof(int));
		if (test->value == NULL)
			goto err;
		*((int *) test->value) = strtol(value, NULL, 10);
		break;
	case ADB_CTYPE_FLOAT:
		test->value = calloc(1, sizeof(float));
		if (test->value == NULL)
			goto err;
		*((float *) test->value) = strtod(value, NULL);
		break;
	case ADB_CTYPE_DOUBLE:
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
		test->value = calloc(1, sizeof(double));
		if (test->value == NULL)
			goto err;
		*((double *) test->value) = strtod(value, NULL);
		break;
	}

	search->test_orphan[search->test_orphan_count++] = test;
	search->search_test_count++;
	search->start_branch = NULL;

	return 0;

err:
	return -ENOMEM;
}

/*! \fn int astrodb_search_add_custom_comparator(astrodb_search* search, astrodb_custom_comparator compare)
 * \param search Search
 * \param compare Custom comparator_t function
 * \returns 0 on success
 *
 * Add a custom search comparator_t. It should return 1 for a match and 0 for a
 * search miss.
 */
int astrodb_search_add_custom_comparator(struct astrodb_search *search,
					astrodb_custom_comparator comp)
{
	return -EINVAL;
}

static inline int check_object(struct adb_search_branch *branch,
		const struct astrodb_object *object)
{
	return branch->operator(object, branch, branch->test_count);
}

/*! \fn int astrodb_search_get_results(astrodb_search* search, astrodb_progress progress, astrodb_slist **result, unsigned int src) 
 * \param search Search
 * \param progress Progress callback
 * \param result Search results
 * \param src Object source
 *
 * Get search results.
 */
int astrodb_search_get_results(struct astrodb_search *search,
				struct astrodb_object_set *set,
				const struct astrodb_object **objects[])
{
	struct astrodb_table *table = search->table;
	int i, j, object_heads;

	if (search->branch_orphan_count > 0) {
		search->root = search->branch_orphan[0];
	} else if (search->test_orphan_count > 0) {
		adb_error(search->db, "unbalanced search - test orphans exist\n");
		return -EINVAL;
	}

	if (search->start_branch == NULL) {
		adb_error(search->db, "unbalanced search - no start branch\n");
		return -EINVAL;
	}

	/* operator_t is root */
	object_heads = astrodb_table_set_get_objects(set);

	if (object_heads <= 0)
		return object_heads;

	search->hit_count = 0;
	search->test_count = 0;

	/* search objects in each object head */
	for (i = 0; i < object_heads; i++) {

		const void *object = set->object_heads[i].objects;

		for (j = 0; j < set->object_heads[i].count; j++)  {

			if (check_object(search->start_branch, object))
				search->objects[search->hit_count++] = object;

			object += table->object.bytes;
			search->test_count++;
		}
	}

	adb_info(search->db, ADB_LOG_SEARCH, "search count %d clipped heads %d tested objects %d\n",
			set->count, object_heads, search->test_count);

	*objects = search->objects;
	return search->hit_count;
}

/*! \fn int astrodb_search_get_hits(astrodb_search* search);
 * \param search Search
 * \returns Hits
 *
 * Get the number of search hit_count.
 */
int astrodb_search_get_hits(struct astrodb_search *search)
{
	return search->hit_count;
}

/*! \fn int astrodb_search_get_tests(astrodb_search* search);
 * \param search Search
 * \returns Tests
 *
 * Get the number of search test_count
 */
int astrodb_search_get_tests(struct astrodb_search *search)
{
	return search->test_count;
}
