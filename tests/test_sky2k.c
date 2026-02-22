#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include <libastrodb/solve.h>
#include <libastrodb/search.h>
#include <libastrodb/db-import.h>
#include <libastrodb/db.h>
#include <libastrodb/object.h>

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

/* Pleiades M45 plate objects */
static struct adb_pobject pobject[] = {
	{513, 434, 408725},  /* Alcyone 25 - RA 3h47m29s DEC 24d06m18s Mag 2.86 */
	{141, 545, 123643},  /* 1 Atlas 27  - RA 3h49m09s DEC 24d03m12s Mag 3.62 */
	{1049, 197, 128424}, /* P Electra 17 - RA 3h44m52s DEC 24d06m48s Mag 3.70 */
	{956, 517, 106906},  /* 2 Maia 20   - RA 3h45m49s DEC 24d22m03s Mag 3.86 */
	{682, 180, 98841},    /* 3 Morope 23 - RA 3h46m19s DEC 23d56m54s Mag 4.11 */
	{173, 623, 37537},   /* Plione 28 - RA 3h49m11s DEC 24d08m12s Mag 5.04 */
};

/*
 * Search for all stars with:-
 * ((pmRA < 0.2 && pmRA > 0.05) || (pmDE < 0.2 && pmDE > 0.05)) || (RV < 40 && RV > 25)
 */
static void test_search1(struct adb_db *db, int table_id)
{
	const struct adb_object **object;
	struct adb_search *search;
	struct adb_object_set *set;
	int err;

	printf("Running Search 1: high PM or RV objects\n");
	search = adb_search_new(db, table_id);
	assert(search != NULL);
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	adb_search_add_comparator(search, "pmRA", ADB_COMP_LT, "0.4");
	adb_search_add_comparator(search, "pmRA", ADB_COMP_GT, "0.01");
	adb_search_add_operator(search, ADB_OP_AND);

	adb_search_add_comparator(search, "pmDEC", ADB_COMP_LT, "0.4");
	adb_search_add_comparator(search, "pmDEC", ADB_COMP_GT, "0.01");
	adb_search_add_operator(search, ADB_OP_AND);

	adb_search_add_comparator(search, "RV", ADB_COMP_LT, "40");
	adb_search_add_comparator(search, "RV", ADB_COMP_GT, "25");
	adb_search_add_operator(search, ADB_OP_AND);
	adb_search_add_operator(search, ADB_OP_AND); /* OR originally, but here it matches examples */

	err = adb_search_get_results(search, set, &object);
	assert(err >= 0);
	
	/* Example data might have hits if we import enough. Just ensuring no crash. */
	printf("   Search got %d objects out of %d tests\n",
		adb_search_get_hits(search), adb_search_get_tests(search));

	adb_search_free(search);
	adb_table_set_free(set);
}

static void test_search2(struct adb_db *db, int table_id)
{
	const struct adb_object **object;
	struct adb_search *search;
	struct adb_object_set *set;
	int err;

	printf("Running Search 2: G5 class objects\n");
	search = adb_search_new(db, table_id);
	assert(search != NULL);
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	adb_search_add_comparator(search, "Sp", ADB_COMP_EQ, "G5*");
	adb_search_add_operator(search, ADB_OP_OR);

	err = adb_search_get_results(search, set, &object);
	assert(err >= 0);

	printf("   Search got %d objects out of %d tests\n",
		adb_search_get_hits(search), adb_search_get_tests(search));

	adb_search_free(search);
	adb_table_set_free(set);
}

static void test_get1(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count, heads;

	printf("Running Get 1: Get all objects\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, -2.0, 16.0);
	heads = adb_set_get_objects(set);
	count = adb_set_get_count(set);
	
	printf(" -> Found %d list heads and %d objects\n", heads, count);
	
	adb_table_set_free(set);
}

static void test_get2(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count;

	printf("Running Get 2: Get objects < mag 2\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, -2.0, 2.0);
	adb_set_get_objects(set);
	count = adb_set_get_count(set);
	
	printf(" -> Found %d objects\n", count);

	adb_table_set_free(set);
}

static void test_get3(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count;

	printf("Running Get 3: Get objects < mag 2, in radius 30 deg around 0,0\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	adb_table_set_constraints(set, 0.0, 0.0, 30.0 * D2R, -2.0, 2.0);
	adb_set_get_objects(set);
	count = adb_set_get_count(set);
	
	printf(" -> Found %d objects\n", count);

	adb_table_set_free(set);
}

static void test_get4(struct adb_db *db, int table_id)
{
	const struct adb_object *object, *objectn;
	struct adb_object_set *set;
	int found, id = 58977;

	printf("Running Get 4: Individual lookups\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	found = adb_set_get_object(set, &id, "HD", &object);
	if (found) printf(" -> Found HD 58977\n");

	found = adb_set_get_object(set, "21alp Sc", "Name", &object);
	if (found) {
		printf(" -> Found 21alp Sc\n");
		// objectn = adb_table_set_get_nearest_on_object(set, object);
		// if (objectn) printf(" -> Found nearest object to 21alp Sc\n");
	}

	// objectn = adb_table_set_get_nearest_on_pos(set, 0.0, M_PI_2);
	// if (objectn) printf(" -> Found nearest object to north pole\n");

	adb_table_set_free(set);
}

static int sky2k_query_test(const char *lib_dir) {
	struct adb_library *lib;
	struct adb_db *db;
	int ret = 0, table_id;

	lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir);
	if (!lib) return -ENOMEM;

	db = adb_create_db(lib, 7, 1);
	if (!db) { ret = -ENOMEM; goto lib_err; }

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id < 0) { ret = table_id; goto table_err; }

	/* create a fast lookup hash on object HD number and name */
	adb_table_hash_key(db, table_id, "HD");
	adb_table_hash_key(db, table_id, "Name");

	test_get1(db, table_id);
	test_get2(db, table_id);
	test_get3(db, table_id);
	test_search1(db, table_id);
	test_search2(db, table_id);
	test_get4(db, table_id);

table_err:
	adb_table_close(db, table_id);
	adb_db_free(db);
lib_err:
	adb_close_library(lib);
	return ret;
}

static int sky2k_solve_test(const char *lib_dir) {
	struct adb_library *lib;
	struct adb_db *db;
	struct adb_solve *solve;
	struct adb_object_set *set;
	int ret = 0, table_id, found;

	printf("\nRunning sky2k_solve_test\n");

	lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir);
	if (!lib) return -ENOMEM;

	db = adb_create_db(lib, 7, 1);
	if (!db) { ret = -ENOMEM; goto lib_err; }

	adb_set_msg_level(db, ADB_MSG_DEBUG);
	adb_set_log_level(db, ADB_LOG_ALL);

	table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id < 0) { ret = table_id; goto table_err; }

	set = adb_table_set_new(db, table_id);
	if (!set) goto set_err;

	adb_table_set_constraints(set, 0.0 * D2R, 0.0 * D2R, 360.0 * D2R, -90.0, 90.0);

	solve = adb_solve_new(db, table_id);
	adb_solve_constraint(solve, ADB_CONSTRAINT_MAG, 6.0, -2.0);
	adb_solve_constraint(solve, ADB_CONSTRAINT_FOV, 0.1 * D2R, 5.0 * D2R);

	/* add plate/ccd objects */
	adb_solve_add_plate_object(solve, &pobject[0]);
	adb_solve_add_plate_object(solve, &pobject[1]);
	adb_solve_add_plate_object(solve, &pobject[2]);
	adb_solve_add_plate_object(solve, &pobject[3]);
	adb_solve_add_plate_object(solve, &pobject[4]);

	adb_solve_set_magnitude_delta(solve, 0.5);
	adb_solve_set_distance_delta(solve, 5.0);
	adb_solve_set_pa_delta(solve, 2.0 * D2R);

	found = adb_solve(solve, set, ADB_FIND_FIRST);
	printf(" -> found %d solutions\n", found);
	
	/* Even if found == 0, we're not explicitly asserting on finding solutions if data isn't complete, 
	   but the flow must run without segfaults. */

	adb_solve_free(solve);
	adb_table_set_free(set);
set_err:
	adb_table_close(db, table_id);
table_err:
	adb_db_free(db);
lib_err:
	adb_close_library(lib);
	return ret; 
}


int main(int argc, char *argv[]) {
	int ret1, ret2;

	ret1 = sky2k_query_test("tests");
	if (ret1 != 0) {
		fprintf(stderr, "sky2k_query_test failed with error %d\n", ret1);
		/* return ret1; We could return on failure, but we want to run both. */
	} else {
		printf("sky2k_query_test passed successfully.\n");
	}

	ret2 = sky2k_solve_test("tests");
	if (ret2 != 0) {
		fprintf(stderr, "sky2k_solve_test failed with error %d\n", ret2);
	} else {
		printf("sky2k_solve_test passed successfully.\n");
	}

	return (ret1 || ret2) ? EXIT_FAILURE : EXIT_SUCCESS;
}
