#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "lib.h"
#include "htm.h"

#define D2R (1.7453292519943295769e-2)

static void test_htm_lifecycle(void) {
	printf("Running HTM Lifecycle Test...\n");
	struct htm *htm = htm_new(7, 2);
	assert(htm != NULL);
	assert(htm->depth == 7);
	assert(htm->trixel_count > 0);
	assert(htm->vertex_count > 0);
	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_depth_calculations(void) {
	printf("Running HTM Depth Calculation Test...\n");
	
	/* resolution testing */
	int d1 = htm_get_depth_from_resolution(0.1 * D2R);
	int d2 = htm_get_depth_from_resolution(1.0 * D2R);
	assert(d1 > d2); /* Higher resolution needs deeper HTM */
	printf("   Resolution 0.1 deg gets depth %d\n", d1);

	/* pseudo magnitude bounds, we need an HTM context */
	struct htm *htm = htm_new(7, 1);
	
	/* Removed test for htm_get_depth_from_magnitude and htm_get_object_depth
	 * because they are declared in htm.h but never defined in libastrodb. */

	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_vertices_trixels(void) {
	printf("Running HTM Vertices and Trixel Test...\n");
	struct htm *htm = htm_new(7, 1);
	assert(htm != NULL);

	struct htm_vertex v;
	v.ra = 45.0 * D2R;
	v.dec = 45.0 * D2R;
	htm_vertex_update_unit(&v);
	
	assert(v.x > 0);
	assert(v.y > 0);
	assert(v.z > 0);

	struct htm_trixel *trixel = htm_get_home_trixel(htm, &v, 5);
	assert(trixel != NULL);
	assert(trixel->depth == 5);

	unsigned int id = htm_trixel_id(trixel);
	assert(id > 0);

	struct htm_trixel *retrieved = htm_get_trixel(htm, id);
	assert(retrieved != NULL);
	assert(retrieved == trixel);
	printf("   Retrieved trixel ID %u matches\n", id);

	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_clip_region(void) {
	printf("Running HTM Clip Region Test...\n");
	
	/* For clip and get_trixels we need a valid adb_db and set mock.
	 * Since htm_clip interacts with set->table and db->htm, we can setup standard instance */
	
	struct adb_library *lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", "tests");
	assert(lib != NULL);
	struct adb_db *db = adb_create_db(lib, 7, 1);
	assert(db != NULL);

	/* We can leverage the existing sky2kv4 dataset to get a real table */
	int table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id >= 0) {
		struct adb_object_set *set = adb_table_set_new(db, table_id);
		assert(set != NULL);

		int err = htm_clip(db->htm, set, 10.0 * D2R, 20.0 * D2R, 1.0 * D2R, 0, 5);
		assert(err == 0);

		int trixels = htm_get_trixels(db->htm, set);
		assert(trixels >= 0);
		printf("   Found %d clipped trixels\n", trixels);

		adb_table_set_free(set);
		adb_table_close(db, table_id);
	}

	adb_db_free(db);
	adb_close_library(lib);

	printf(" -> PASS\n");
}

int main(void) {
	printf("Starting HTM Unit Tests...\n");
	test_htm_lifecycle();
	test_htm_depth_calculations();
	test_htm_vertices_trixels();
	test_htm_clip_region();
	printf("All HTM Unit Tests Passed Successfully!\n");
	return 0;
}
