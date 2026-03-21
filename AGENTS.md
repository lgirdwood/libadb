# Agent Instructions for libastrodb

## Building and Testing

### C Code

libastrodb uses CMake for building the C library.

**Building:**

```bash
# The build directory must always be outside the source directory
cmake -S . -B ../build -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --config Release
```

**Testing:**
Tests are executed using CTest.

```bash
cd ../build
ctest -C Release
```

### Python Code

Python unit tests test the python bindings using the locally built library.

**Testing:**
Tests are executed via the `pytest` CMake target which sets up the environment and runs unittest:

```bash
cmake --build ../build --target pytest
```

### Markdown Files

When modifying markdown (`.md`) files in the repository, you MUST ensure that they comply with the `markdownlint` standards.

**Linting and Auto-formatting:**

```bash
npx markdownlint-cli "**/*.md" --fix
```

## Task Completion Guidelines

Before completing any task, an agent MUST:

1. **Run All Tests**: Execute all related unit tests (both C and Python) as described in the sections above.
2. **Verify Tests Pass**: Ensure that 100% of the tests pass successfully. Do not finish the task or consider it accepted if there are any failing tests. You must debug and fix the implementation or test until they pass.
3. **Clean Temporary Files**: Check `git status` and delete any temporary or generated files from the workspace (such as `egg-info`, `__pycache__`, or `build` folders) before committing your final changes.

## Commit Message Guidelines

When making commits, ensure you strictly follow these rules:

- **Permission**: You must ALWAYS ask for the user's permission before executing any `git commit` commands.
- **Line Length**: The overall commit message must be at most 80 characters per line.
- **Subject**: The subject line must follow the format `feature: description`.
- **Body**: The commit message body must properly describe the change and why it was made.
- **Sign-off**: Every commit must include a sign-off line using the developer's name and email configured in git (e.g., retrieve via `git config user.name` and `git config user.email`).

### Example Commit Message

```text
core: fix memory leak in calculation layer

This change resolves an issue where memory allocated for the temporary
trajectory nodes was never freed. It ensures that the cleanup function
is always called before exiting the scope.

Signed-off-by: Developer Name <developer@example.com>
```
