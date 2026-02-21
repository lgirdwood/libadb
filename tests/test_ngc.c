#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libastrodb/db-import.h>
#include <libastrodb/db.h>
#include <libastrodb/object.h>

int ngc_query_test(const char *lib_dir) {
  struct adb_library *lib;
  struct adb_db *db;
  int ret = 0, table_id;
  struct adb_object_set *set;
  int count, heads;

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

  /* Search the whole sky */
  set = adb_table_set_new(db, table_id);
  if (!set) {
    ret = -ENOMEM;
    goto table_err;
  }

  adb_table_set_constraints(set, 0.0, 0.0, 2.0 * M_PI, 0.0, 16.0);

  heads = adb_set_get_objects(set);
  count = adb_set_get_count(set);

  printf("Found %d list heads and %d objects\n", heads, count);

  /* Validate expected counts */
  if (heads != 2706 || count != 7765) {
    fprintf(stderr,
            "Validation failed: expected 2706 list heads and 7765 objects, but "
            "got %d and %d\n",
            heads, count);
    ret = -1;
  }

  adb_table_set_free(set);

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
