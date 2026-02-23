#include <stdio.h>
#include <stdlib.h>

int run_test(const char *name, const char *cmd) {
  printf("====================================================================="
         "=\n");
  printf("Running %s...\n", name);
  printf("Command: %s\n", cmd);
  printf("====================================================================="
         "=\n");

  int ret = system(cmd);

  printf("---------------------------------------------------------------------"
         "-\n");
  if (ret == 0) {
    printf("[\033[32mPASS\033[0m] %s\n\n", name);
    return 1;
  } else {
    printf("[\033[31mFAIL\033[0m] %s (exit code %d)\n\n", name, ret);
    return 0;
  }
}

int main(void) {
  int total = 0;
  int passed = 0;

  printf("\nStarting libastrodb Integration Test Suite...\n\n");

#ifndef TEST_NGC_PATH
#define TEST_NGC_PATH "./test_ngc"
#endif

#ifndef TEST_SKY2K_PATH
#define TEST_SKY2K_PATH "./test_sky2k"
#endif

#ifndef TEST_HTM_PATH
#define TEST_HTM_PATH "./test_htm"
#endif

#ifndef TEST_KDTREE_PATH
#define TEST_KDTREE_PATH "./test_kdtree"
#endif

#ifndef TEST_HASH_PATH
#define TEST_HASH_PATH "./test_hash"
#endif

#ifndef TEST_IMPORT_PATH
#define TEST_IMPORT_PATH "./test_import"
#endif

#ifndef TEST_SCHEMA_PATH
#define TEST_SCHEMA_PATH "./test_schema"
#endif

#ifndef TEST_TABLE_PATH
#define TEST_TABLE_PATH "./test_table"
#endif

#ifndef TEST_SOLVE_PATH
#define TEST_SOLVE_PATH "./test_solve"
#endif

  total++;
  passed += run_test("NGC Unit Test", TEST_NGC_PATH);

  total++;
  passed += run_test("Sky2K Unit Test", TEST_SKY2K_PATH);

  total++;
  passed += run_test("HTM Unit Test", TEST_HTM_PATH);

  total++;
  passed += run_test("KD-Tree Unit Test", TEST_KDTREE_PATH);

  total++;
  passed += run_test("Hash Unit Test", TEST_HASH_PATH);

  total++;
  passed += run_test("Import Unit Test", TEST_IMPORT_PATH);

  total++;
  passed += run_test("Schema Unit Test", TEST_SCHEMA_PATH);

  total++;
  passed += run_test("Table Unit Test", TEST_TABLE_PATH);

  total++;
  passed += run_test("Solve Unit Test", TEST_SOLVE_PATH);

  printf("====================================================================="
         "=\n");
  printf("Test Summary: %d/%d tests passed.\n", passed, total);
  printf("====================================================================="
         "=\n");

  if (passed == total) {
    printf("All tests passed successfully!\n\n");
    return EXIT_SUCCESS;
  } else {
    printf("Some tests failed! Please check the logs above.\n\n");
    return EXIT_FAILURE;
  }
}
