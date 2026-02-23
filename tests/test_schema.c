#include <stdio.h>
#include <assert.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include <libastrodb/db-import.h>
#include "../src/schema.h"
#include "../src/table.h"
#include "../src/lib.h"

static void test_schema_lookups(void) {
	printf("   Testing Schema API lookups...\n");

	struct adb_library *lib = adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", "tests");
	assert(lib != NULL);
	
	struct adb_db *db = adb_create_db(lib, 7, 1);
	assert(db != NULL);

	int table_id = adb_table_open(db, "V", "109", "sky2kv4");
	assert(table_id >= 0);

	struct adb_table *table = &db->table[table_id];
	assert(table != NULL);

	/* test checking valid fields */
	printf("      Testing primary field retrieval...\n");
	int field_v_mag = schema_get_field(db, table, "V_mag");
	assert(field_v_mag >= 0);

	/* In sky2kv4, Name is often defined as an alternate property or designation */
	int field_designation = schema_get_field(db, table, ADB_FIELD_DESIGNATION);
	printf("      Designation primary idx: %d\n", field_designation);

	/* Test checking alt fields. Let's see if ADB_FIELD_NAME is an alt field */
	printf("      Testing alternative field retrieval...\n");
	/* Just ensure the function executes and doesn't crash on both valid and invalid lookups */
	int alt_name = schema_get_alt_field(db, table, "Name");
	printf("      Alt name idx: %d\n", alt_name);

	/* Check for an invalid field to ensure we get a negative error code safely */
	printf("      Testing missing field fetch error bounds...\n");
	int field_invalid = schema_get_field(db, table, "FieldThatDoesNotExist");
	assert(field_invalid < 0);

	adb_table_close(db, table_id);
	adb_db_free(db);
	adb_close_library(lib);

	printf("    -> PASS\n");
}

int main(void) {
	printf("Starting Schema Unit Tests...\n");
	
	test_schema_lookups();
	
	printf("All Schema Unit Tests Passed Successfully!\n");
	return 0;
}
