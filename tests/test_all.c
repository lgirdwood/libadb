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

  total++;
  passed += run_test("NGC Unit Test", TEST_NGC_PATH);

  total++;
  passed += run_test("Sky2K Unit Test", TEST_SKY2K_PATH);

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
