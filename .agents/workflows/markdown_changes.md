---
description: How to modify markdown files
---

# Modifying Markdown Files

When making changes to markdown files (`.md`) in the repository, you must follow this workflow to ensure documentation quality:

1. **Make Changes**: Edit the markdown files as required to fulfill the request.
2. **Auto-Format**: Automatically fix any simple formatting issues using the markdown linter.
   // turbo

   ```bash
   npx markdownlint-cli "**/*.md" --fix
   ```

3. **Run Linter**: Run the markdown linter to verify formatting compliance.
   // turbo

   ```bash
   npx markdownlint-cli "**/*.md"
   ```

4. **Fix Remaining Errors**: If the linter reports any errors, you must manually fix them and re-run the linter until it completely passes cleanly.
5. **Mandatory Testing**: Run all C and Python tests to verify the repository is in a healthy state, as per standard agent instructions.
6. **Clean Workspace**: Check `git status` and run `git clean -fdx` or manually remove any generated test artifacts (like `.egg-info` blocks) that shouldn't be committed.
7. **Commit Changes**: You must ALWAYS ask the user for permission before running any `git commit` commands.
