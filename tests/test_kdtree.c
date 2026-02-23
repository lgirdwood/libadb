#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "../src/private.h"
#include "../src/table.h"
#include "../src/lib.h"

#define D2R (1.7453292519943295769e-2)

static void test_kdtree_neighbors(void) {
	printf("Running KD-Tree Neighbors Test...\n");
	
	struct adb_library *lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", "tests");
	assert(lib != NULL);
	
	struct adb_db *db = adb_create_db(lib, 7, 1);
	assert(db != NULL);

	int table_id = adb_table_open(db, "V", "109", "sky2kv4");
	assert(table_id >= 0);

	/* create a fast lookup hash to seed an initial object */
	adb_table_hash_key(db, table_id, ADB_FIELD_DESIGNATION);

	struct adb_object_set *set = adb_table_set_new(db, table_id);
	assert(set != NULL);

	/* Unrestricted constraint to allow kd-tree searches across full domain */
	adb_table_set_constraints(set, 0.0 * D2R, 0.0 * D2R, 360.0 * D2R, -90.0, 90.0);
	/* Find seed directly from table hash */
	const struct adb_object *object = (const struct adb_object *)db->table[table_id].objects;
	printf("   Seed Object found: RA=%f, DEC=%f\n", adb_object_ra(object), adb_object_dec(object));

	/* TEST: Nearest to Object */
	const struct adb_object *nearest = adb_table_set_get_nearest_on_object(set, object);
	assert(nearest != NULL);
	assert(nearest != object);
	printf("   Nearest object: RA=%f, DEC=%f\n", adb_object_ra(nearest), adb_object_dec(nearest));

	/* TEST: Nearest to Coordinate Pole */
	const struct adb_object *nearest_pos = adb_table_set_get_nearest_on_pos(set, 0.0, M_PI_2);
	assert(nearest_pos != NULL);
	printf("   Nearest to Pole: RA=%f, DEC=%f\n", adb_object_ra(nearest_pos), adb_object_dec(nearest_pos));

	adb_table_set_free(set);
	adb_table_close(db, table_id);
	adb_db_free(db);
	adb_close_library(lib);
	
	printf(" -> PASS\n");
}

int main(void) {
	printf("Starting KD-Tree Unit Tests...\n");
	test_kdtree_neighbors();
	printf("All KD-Tree Unit Tests Passed Successfully!\n");
	return 0;
}
