---
description: How to add a new feature
---

# Adding a New Feature

When adding a new feature to the project, you must follow these steps to ensure quality and reliability:

1. **Understand Requirements**: Review the feature request and identify the specific requirements and affected components.
2. **Implement Feature**: Write the code to implement the new feature.
3. **Implementation**: Write the code to implement the feature following project coding standards.
4. **Lint Markdown**: If you modify any markdown files, you MUST run the markdown linter (`npx markdownlint-cli "**/*.md" --fix`) as described in `markdown_changes.md`.
5. **Run Unit Tests**: Execute the test suite to verify your changes. You must run the tests.
6. **Verify Tests Pass**: The unit test must ALWAYS be run and pass for a feature to be accepted. Ensure 100% of the relevant tests are passing.
7. **Iterate if Necessary**: If any unit test fails, debug and fix the implementation or test until all tests pass successfully. Do not finish the task or consider the feature accepted until this condition is met.
8. **Commit Changes**: Once all tests pass, YOU MUST explicitly ask the user for permission before executing any `git commit` commands.
