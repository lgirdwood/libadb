#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libastrodb/db-import.h>
#include <libastrodb/db.h>
#include <libastrodb/object.h>

static void test_query_all_objects(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count, heads;

	printf("Running Query 1: Get all objects\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, 0.0, 16.0);
	heads = adb_set_get_objects(set);
	count = adb_set_get_count(set);
    
	printf(" -> Found %d list heads and %d objects.\n", heads, count);
	
	/* Validate expected counts exactly as the original test_ngc.c did */
	assert(heads == 2706);
	assert(count == 7765);
    
	adb_table_set_free(set);
}

static void test_query_bright_objects(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count;

	printf("Running Query 2: Get bright objects (mag < 10.0)\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	/* Contrain by magnitudes up to 10.0 */
	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, 0.0, 10.0);
	adb_set_get_objects(set);
	count = adb_set_get_count(set);

	printf(" -> Found %d bright objects.\n", count);
	/* Ensure filtering happened */
	assert(count > 0 && count < 7765);
    
	adb_table_set_free(set);
}

static void test_query_specific_region(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count;

	printf("Running Query 3: Get objects in a specific region (RA 11h, DEC 10 deg)\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	/* Center at RA=11h(2.87rad), DEC=10deg(0.17rad), radius=10deg(0.17rad) */
	adb_table_set_constraints(set, 2.87, 0.17, 0.17, 0.0, 16.0);
	adb_set_get_objects(set);
	count = adb_set_get_count(set);

	printf(" -> Found %d objects in region.\n", count);
	assert(count > 0 && count < 7765);
    
	adb_table_set_free(set);
}

static void test_query_faint_objects(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count;

	printf("Running Query 4: Get very faint objects (mag 14.0 - 16.0)\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	/* Contrain by magnitude */
	adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, 14.0, 16.0);
	adb_set_get_objects(set);
	count = adb_set_get_count(set);

	printf(" -> Found %d faint objects.\n", count);
    assert(count > 0 && count < 7765);
	
	adb_table_set_free(set);
}

static void test_query_north_pole(struct adb_db *db, int table_id)
{
	struct adb_object_set *set;
	int count;

	printf("Running Query 5: Get objects near North Celestial Pole (DEC > 80 deg)\n");
	set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	/* Center at RA=0, DEC=90 deg (1.57 rad), radius=10 deg (0.17 rad) */
	adb_table_set_constraints(set, 0.0, 1.57, 0.17, 0.0, 16.0);
	adb_set_get_objects(set);
	count = adb_set_get_count(set);

	printf(" -> Found %d objects near North Pole.\n", count);
    assert(count > 0 && count < 7765);
    
	adb_table_set_free(set);
}

int ngc_query_test(const char *lib_dir) {
  struct adb_library *lib;
  struct adb_db *db;
  int ret = 0, table_id;

  /* set the remote CDS server and initialise local repository/cache */
  /* We use NULL for remote since we're using local tests dir */
  lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir);
  if (lib == NULL) {
    fprintf(stderr, "failed to open library\n");
    return -ENOMEM;
  }

  db = adb_create_db(lib, 5, 1);
  if (db == NULL) {
    fprintf(stderr, "failed to create db\n");
    ret = -ENOMEM;
    goto lib_err;
  }

  adb_set_msg_level(db, ADB_MSG_DEBUG);
  adb_set_log_level(db, ADB_LOG_ALL);

  /* use CDS catalog class VII, #118, dataset ngc2000 */
  table_id = adb_table_open(db, "VII", "118", "ngc2000");
  if (table_id < 0) {
    fprintf(stderr, "failed to create table\n");
    ret = table_id;
    goto table_err;
  }

  test_query_all_objects(db, table_id);
  test_query_bright_objects(db, table_id);
  test_query_specific_region(db, table_id);
  test_query_faint_objects(db, table_id);
  test_query_north_pole(db, table_id);

table_err:
  adb_table_close(db, table_id);
  adb_db_free(db);

lib_err:
  adb_close_library(lib);
  return ret;
}

int main(int argc, char *argv[]) {
  /* Use the tests directory relative to the current working directory */
  int ret = ngc_query_test("tests");

  if (ret != 0) {
    fprintf(stderr, "ngc_query_test failed with error %d\n", ret);
    return EXIT_FAILURE;
  }

  printf("ngc_query_test passed successfully.\n");
  return EXIT_SUCCESS;
}
