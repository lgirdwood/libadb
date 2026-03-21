---
description: How to modify C code files
---

# Modifying C Code

When making changes to C source (`.c`) or header (`.h`) files in the repository, you must follow this workflow to ensure code quality and consistent formatting:

1. **Make Changes**: Edit the C code files as required to fulfill the request.
2. **Auto-Format**: Automatically format the modified C files using `clang-format` to maintain style consistency.
   // turbo

   ```bash
   find src lntest examples libnova2 -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +
   ```

3. **Mandatory Testing**: After modifying or formatting C code, you MUST run all C tests (`ctest`) and Python tests (`python/run_tests.sh`) to verify the repository compiles successfully and remains in a healthy state.
4. **Clean Workspace**: Check `git status` and run `git clean -fdx` or manually remove any generated test artifacts that shouldn't be committed.
5. **Commit Changes**: You must ALWAYS ask the user for permission before running any `git commit` commands.
