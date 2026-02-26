#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "../src/private.h"
#include "../src/hash.h"
#include "../src/table.h"
#include "../src/lib.h"

static void test_hash_string(void)
{
	printf("   Testing hash_string()...\n");
	int mod = 100;

	/* The actual algorithm in hash_string:
	 * val = 0;
	 * while(*str) { if(*str >= '0' && *str <= 'z') val = ((val<<5) ^ (val>>27)) ^ *str; str++; }
	 * val = abs(val % mod);
	 */

	int hash1 = hash_string("alp Sco", 7, mod);
	int hash2 = hash_string("alp sco", 7, mod);
	int hash3 = hash_string("alp-Sco", 7, mod);
	int hash4 = hash_string("alp Sco ", 8, mod);
	(void)hash4;
	(void)hash3;
	(void)hash2;
	(void)hash1;

	/* Spaces and hyphens and trailing spaces are ignored or processed into the hash?
	 * Actually, space ' ' (ascii 32) and hyphen '-' (ascii 45) are NOT between '0' (48) and 'z' (122), 
	 * so they are ignored by the hash_string logic!
	 */
	assert(hash1 == hash3);
	assert(hash1 == hash4);

	/* Case sensitivity means these will be different hashes */
	assert(hash1 != hash2);

	/* Mod calculation */
	assert(hash1 >= 0 && hash1 < mod);
	printf("    -> PASS\n");
}

static void test_hash_int(void)
{
	printf("   Testing hash_int()...\n");
	int mod = 100;

	int hash1 = hash_int(12345, mod);
	int hash2 = hash_int(-12345, mod);
	(void)hash2;
	(void)hash1;

	assert(hash1 == 45); /* abs(12345 % 100) */
	assert(hash2 == 45); /* abs(-12345 % 100) */

	printf("    -> PASS\n");
}

static void test_hash_build(void)
{
	printf("   Testing hash_build_table()...\n");

	struct adb_library *lib =
		adb_open_library("cdsarc.u-strasbg.fr", "/pub/cats", "tests");
	assert(lib != NULL);

	struct adb_db *db = adb_create_db(lib, 7, 1);
	assert(db != NULL);

	int table_id = adb_table_open(db, "V", "109", "sky2kv4");
	assert(table_id >= 0);

	/* Build a hash map for "Name" */
	int hc = adb_table_hash_key(db, table_id, "Name");
	assert(hc >= 0);

	struct adb_table *table = &db->table[table_id];

	/* Extract a hash index to verify it works */
	int found_map = -1;
	for (int i = 0; i < table->hash.num; i++) {
		if (strcmp(table->hash.map[i].key, "Name") == 0) {
			found_map = i;
			break;
		}
	}
	assert(found_map >= 0);
	(void)hc;

	/* The table is opened, which usually builds the table hashes automatically inside adb_table_open.
	 * But we can manually examine it. 
	 */
	struct hash_map *map = &table->hash.map[found_map];
	(void)map;
	assert(map->type == ADB_CTYPE_STRING);

	/* Let's find an object by hashing manually */
	int obj_count = table->object.count;
	assert(obj_count > 0);

	int test_hash = hash_string("21alp Sco", 9, obj_count);
	(void)test_hash;
	assert(map->index[test_hash] != NULL);
	assert(map->index[test_hash]->count > 0);

	printf("    -> PASS\n");

	adb_table_close(db, table_id);
	adb_db_free(db);
	adb_close_library(lib);
}

int main(void)
{
	printf("Starting Hash Unit Tests...\n");

	test_hash_string();
	test_hash_int();
	test_hash_build();

	printf("All Hash Unit Tests Passed Successfully!\n");
	return 0;
}
