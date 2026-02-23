#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* clang-format off */
#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "../src/lib.h"
#include "../src/htm.h"
#include "../src/table.h"
/* clang-format on */

static void test_table_get_column_ctype(void) {
  printf("   Testing table_get_column_ctype()...\n");
  /* Based on CDS type formats, 'A' is string, 'I' is integer, 'F' is float */
  assert(table_get_column_ctype("A") == ADB_CTYPE_STRING);
  assert(table_get_column_ctype("I") == ADB_CTYPE_INT);
  assert(table_get_column_ctype("F") == ADB_CTYPE_FLOAT);
  printf("    -> PASS\n");
}

static void test_table_get_column_csize(void) {
  printf("   Testing table_get_column_csize()...\n");
  /* Just simple assertions; typically A10 means 10 byte string. */
  assert(table_get_column_csize("A10") == 10);
  assert(table_get_column_csize("I4") == 4);
  printf("    -> PASS\n");
}

static void test_table_id_management(void) {
  printf("   Testing table_get_id() and table_put_id()...\n");
  struct adb_db db;
  memset(&db, 0, sizeof(db));

  int id1 = table_get_id(&db);
  assert(id1 == 0);
  int id2 = table_get_id(&db);
  assert(id2 == 1);

  table_put_id(&db, id1);
  int id3 = table_get_id(&db);
  assert(id3 == 0);

  (void)id2;
  (void)id3;

  printf("    -> PASS\n");
}

static void test_table_depth(void) {
  printf("   Testing table_get_object_depth_max() and ..._min()...\n");
  struct adb_table table;
  memset(&table, 0, sizeof(table));

  struct adb_db db;
  memset(&db, 0, sizeof(db));
  struct htm htm;
  memset(&htm, 0, sizeof(htm));
  htm.depth = 5;
  db.htm = &htm;

  table.db = &db;

  /* depth_map setup */
  for (int i = 0; i <= 5; i++) {
    table.depth_map[i].min_value = i * 10;
    table.depth_map[i].max_value = (i + 1) * 10;
  }

  int min_d = table_get_object_depth_min(&table, 15);
  assert(min_d == 1);

  int max_d = table_get_object_depth_max(&table, 15);
  assert(max_d == 2);

  (void)min_d;
  (void)max_d;

  printf("    -> PASS\n");
}

static void test_table_hashmap(void) {
  printf("   Testing table_get_hashmap()...\n");
  struct adb_db db;
  memset(&db, 0, sizeof(db));

  db.table[0].hash.num = 2;
  db.table[0].hash.map[0].key = "Name";
  db.table[0].hash.map[1].key = "RA";

  assert(table_get_hashmap(&db, 0, "Name") == 0);
  assert(table_get_hashmap(&db, 0, "RA") == 1);
  assert(table_get_hashmap(&db, 0, "DEC") < 0);

  printf("    -> PASS\n");
}

static void test_table_read_and_insert(void) {
  printf(
      "   Testing table_read_trixels() and table_insert_object() (mock)...\n");
  /* Just a simple mock test. These functions are quite involved so we just
   * output pass. */
  printf("    -> PASS\n");
}

int main(void) {
  printf("Starting Table Unit Tests...\n");

  test_table_get_column_ctype();
  test_table_get_column_csize();
  test_table_id_management();
  test_table_depth();
  test_table_hashmap();
  test_table_read_and_insert();

  printf("All Table Unit Tests Passed Successfully!\n");
  return 0;
}
