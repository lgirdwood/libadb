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

static void test_htm_lifecycle(void)
{
	printf("Running HTM Lifecycle Test...\n");
	struct htm *htm = htm_new(7, 2);
	assert(htm != NULL);
	assert(htm->depth == 7);
	assert(htm->trixel_count > 0);
	assert(htm->vertex_count > 0);
	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_depth_calculations(void)
{
	printf("Running HTM Depth Calculation Test...\n");

	/* resolution testing */
	int d1 = htm_get_depth_from_resolution(0.1 * D2R);
	int d2 = htm_get_depth_from_resolution(1.0 * D2R);
	(void)d2;
	assert(d1 > d2); /* Higher resolution needs deeper HTM */
	printf("   Resolution 0.1 deg gets depth %d\n", d1);

	/* pseudo magnitude bounds, we need an HTM context */
	struct htm *htm = htm_new(7, 1);

	/* Removed test for htm_get_depth_from_magnitude and htm_get_object_depth
	 * because they are declared in htm.h but never defined in libastrodb. */

	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_vertices_trixels(void)
{
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
	(void)retrieved;
	assert(retrieved != NULL);
	assert(retrieved == trixel);
	printf("   Retrieved trixel ID %u matches\n", id);

	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_clip_region(void)
{
	printf("Running HTM Clip Region Test...\n");

	/* For clip and get_trixels we need a valid adb_db and set mock.
	 * Since htm_clip interacts with set->table and db->htm, we can setup standard instance */

	struct adb_library *lib =
		adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", "tests");
	assert(lib != NULL);
	struct adb_db *db = adb_create_db(lib, 7, 1);
	assert(db != NULL);

	/* We can leverage the existing sky2kv4 dataset to get a real table */
	int table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id >= 0) {
		struct adb_object_set *set = adb_table_set_new(db, table_id);
		assert(set != NULL);

		int err =
			htm_clip(db->htm, set, 10.0 * D2R, 20.0 * D2R, 1.0 * D2R, 0, 5);
		(void)err;
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

static void test_htm_invalid_trixel(void)
{
	printf("Running HTM Invalid Trixel Test...\n");

	struct adb_library *lib =
		adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", "tests");
	assert(lib != NULL);
	struct adb_db *db = adb_create_db(lib, 7, 1);
	assert(db != NULL);

	int table_id = adb_table_open(db, "V", "109", "sky2kv4");
	if (table_id >= 0) {
		struct adb_object_set *set = adb_table_set_new(db, table_id);
		assert(set != NULL);

		/* Test the problematic case: RA is slightly negative */
		double ra = -0.056 * D2R;
		double dec = 0.0 * D2R;
		int err =
			htm_clip(db->htm, set, ra, dec, 1.0 * D2R, 0, 5);
		/* The bug was that slight negative RA caused invalid trixel. It should now normalize and succeed. */
		assert(err == 0);
		(void)err;

		adb_table_set_free(set);
		adb_table_close(db, table_id);
	}

	adb_db_free(db);
	adb_close_library(lib);

	printf(" -> PASS\n");
}

static void test_htm_all_quadrants(void)
{
	printf("Running HTM All Quadrants Test...\n");
	struct htm *htm = htm_new(7, 1);
	assert(htm != NULL);

	/* Test representative points in each of the 8 root trixels.
	 * The HTM octahedron maps to:
	 *   N0: RA 0-6h (0-90°)    DEC > 0
	 *   N1: RA 6-12h (90-180°) DEC > 0
	 *   N2: RA 12-18h (180-270°) DEC > 0
	 *   N3: RA 18-24h (270-360°) DEC > 0
	 *   S0: RA 0-6h   DEC < 0
	 *   S1: RA 6-12h  DEC < 0
	 *   S2: RA 12-18h DEC < 0
	 *   S3: RA 18-24h DEC < 0
	 */
	struct {
		double ra_deg, dec_deg;
		int exp_hemi;   /* 0=N, 1=S */
		int exp_quad;
		const char *name;
	} tests[] = {
		{  45.0,  45.0, 0, 0, "N0 (RA=3h,  DEC=+45)" },
		{ 135.0,  45.0, 0, 1, "N1 (RA=9h,  DEC=+45)" },
		{ 225.0,  45.0, 0, 2, "N2 (RA=15h, DEC=+45)" },
		{ 315.0,  45.0, 0, 3, "N3 (RA=21h, DEC=+45)" },
		{  45.0, -45.0, 1, 0, "S0 (RA=3h,  DEC=-45)" },
		{ 135.0, -45.0, 1, 1, "S1 (RA=9h,  DEC=-45)" },
		{ 225.0, -45.0, 1, 2, "S2 (RA=15h, DEC=-45)" },
		{ 315.0, -45.0, 1, 3, "S3 (RA=21h, DEC=-45)" },
	};

	int i;
	for (i = 0; i < 8; i++) {
		struct htm_vertex v;
		v.ra = tests[i].ra_deg * D2R;
		v.dec = tests[i].dec_deg * D2R;

		struct htm_trixel *t = htm_get_home_trixel(htm, &v, 0);
		printf("   %s: trixel=%p hemi=%s quad=%d\n",
			   tests[i].name, (void *)t,
			   t ? (t->hemisphere ? "S" : "N") : "NULL",
			   t ? t->quadrant : -1);

		assert(t != NULL);
		assert(t->hemisphere == tests[i].exp_hemi);
		assert(t->quadrant == tests[i].exp_quad);
	}

	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_nan_rejection(void)
{
	printf("Running HTM NaN Rejection Test...\n");
	struct htm *htm = htm_new(7, 1);
	assert(htm != NULL);

	struct htm_vertex v;
	struct htm_trixel *t;
	struct htm_trixel *results[2];
	int count;

	/* NaN RA */
	v.ra = NAN;
	v.dec = 45.0 * D2R;
	t = htm_get_home_trixel(htm, &v, 5);
	assert(t == NULL);
	(void)t;
	printf("   NaN RA correctly rejected\n");

	/* NaN DEC */
	v.ra = 45.0 * D2R;
	v.dec = NAN;
	t = htm_get_home_trixel(htm, &v, 5);
	assert(t == NULL);
	(void)t;
	printf("   NaN DEC correctly rejected\n");

	/* Both NaN */
	v.ra = NAN;
	v.dec = NAN;
	count = htm_get_home_trixels(htm, &v, 5, results, 2);
	assert(count == 0);
	(void)count;
	printf("   Both NaN correctly rejected (multi)\n");

	htm_free(htm);
	printf(" -> PASS\n");
}

static void test_htm_boundary_trixels(void)
{
	printf("Running HTM Boundary Trixels Test...\n");
	struct htm *htm = htm_new(7, 1);
	assert(htm != NULL);

	struct htm_trixel *results[8];
	struct htm_vertex v;
	int count;

	/* A point on the equator (DEC=0) sits on the N/S boundary.
	 * Due to the INSIDE_UP_LIMIT tolerance, it should be accepted
	 * by both the northern and southern root trixels. */
	v.ra = 45.0 * D2R;
	v.dec = 0.0;
	count = htm_get_home_trixels(htm, &v, 0, results, 8);
	printf("   DEC=0 boundary: found %d trixels\n", count);
	assert(count >= 2);

	/* Verify we got trixels from both hemispheres */
	int has_north = 0, has_south = 0;
	int i;
	for (i = 0; i < count; i++) {
		printf("     trixel %d: hemi=%s quad=%d\n", i,
			   results[i]->hemisphere ? "S" : "N", results[i]->quadrant);
		if (results[i]->hemisphere == 0)
			has_north = 1;
		else
			has_south = 1;
	}
	assert(has_north && has_south);
	(void)has_north;
	(void)has_south;
	printf("   Both hemispheres represented\n");

	/* Interior point should return exactly 1 */
	v.ra = 45.0 * D2R;
	v.dec = 45.0 * D2R;
	count = htm_get_home_trixels(htm, &v, 0, results, 8);
	printf("   Interior point: found %d trixel(s)\n", count);
	assert(count == 1);

	htm_free(htm);
	printf(" -> PASS\n");
}

int main(void)
{
	printf("Starting HTM Unit Tests...\n");
	test_htm_lifecycle();
	test_htm_depth_calculations();
	test_htm_vertices_trixels();
	test_htm_clip_region();
	test_htm_invalid_trixel();
	test_htm_all_quadrants();
	test_htm_nan_rejection();
	test_htm_boundary_trixels();
	printf("All HTM Unit Tests Passed Successfully!\n");
	return 0;
}
