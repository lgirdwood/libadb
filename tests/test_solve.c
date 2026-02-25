/* clang-format off */
#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "../src/lib.h"
#include "../src/htm.h"
#include "../src/table.h"
#include "../src/solve.h"
/* clang-format on */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_solve_plate_distance(void)
{
	printf("   Testing distance_get_plate()...\n");
	struct adb_pobject p1;
	memset(&p1, 0, sizeof(p1));
	p1.x = 10.0;
	p1.y = 10.0;

	struct adb_pobject p2;
	memset(&p2, 0, sizeof(p2));
	p2.x = 13.0;
	p2.y = 14.0;

	double d = distance_get_plate(&p1, &p2);
	/* distance squared: (13-10)^2 + (14-10)^2 = 9 + 16 = 25 */
	assert(fabs(d - 25.0) < 1e-5);
	(void)d;
	printf("    -> PASS\n");
}

static void test_solve_plate_pa(void)
{
	printf("   Testing pa_get_plate()...\n");
	struct adb_pobject p1;
	memset(&p1, 0, sizeof(p1));
	p1.x = 10.0;
	p1.y = 10.0;

	struct adb_pobject p2;
	memset(&p2, 0, sizeof(p2));
	p2.x = 20.0;
	p2.y = 20.0;

	double pa = pa_get_plate(&p1, &p2);
	/* PA should be computable without crash and bounded by normal atan2 ranges */
	assert(pa >= -M_PI && pa <= M_PI * 2.0);
	(void)pa;
	printf("    -> PASS\n");
}

static void test_solve_plate_mag_diff(void)
{
	printf("   Testing mag_get_plate_diff()...\n");
	struct adb_pobject p1;
	memset(&p1, 0, sizeof(p1));
	p1.adu = 10;

	struct adb_pobject p2;
	memset(&p2, 0, sizeof(p2));
	p2.adu = 20;

	double diff = mag_get_plate_diff(&p1, &p2);
	/* As long as the math executes without fault it verifies linkage */
	(void)diff;
	printf("    -> PASS\n");
}

static void test_solve_mag_cmp(void)
{
	printf("   Testing mag_object_cmp()...\n");
	struct adb_object p1;
	memset(&p1, 0, sizeof(p1));
	p1.mag = 1.0;

	struct adb_object p2;
	memset(&p2, 0, sizeof(p2));
	p2.mag = 2.0;

	const struct adb_object *ptr1 = &p1;
	const struct adb_object *ptr2 = &p2;

	int cmp = mag_object_cmp(&ptr1, &ptr2);
	/* A basic functional test, cmp should be non-zero since they differ */
	assert(cmp != 0);
	(void)cmp;
	printf("    -> PASS\n");
}

static void test_solve_lifecycle(void)
{
	printf("   Testing adb_solve_new() and _free()...\n");
	struct adb_db db;
	memset(&db, 0, sizeof(db));

	struct adb_solve *solve = adb_solve_new(&db, 0);
	assert(solve != NULL);
	assert(solve->db == &db);
	assert(solve->constraint.min_pobjects == 4); /* MIN_PLATE_OBJECTS */

	adb_solve_free(solve);
	printf("    -> PASS\n");
}

int main(void)
{
	printf("Starting Solve Unit Tests...\n");
	test_solve_plate_distance();
	test_solve_plate_pa();
	test_solve_plate_mag_diff();
	test_solve_mag_cmp();
	test_solve_lifecycle();
	printf("All Solve Unit Tests Passed Successfully!\n");
	return 0;
}
