---
name: test
description: Run unit tests for the Owl project
---

# Run tests

1. If no preset is specified in the arguments, use the most recently built preset or ask the user. Check which build directories exist under `output/build/`.
2. Run all tests:
   ```bash
   ctest --test-dir output/build/<preset> --output-on-failure
   ```
3. If specific test categories are mentioned (e.g., "scene", "renderer"), run only that test:
   ```bash
   output/build/<preset>/bin/owl_<category>_tests_unit_test --gtest_output=xml:test_report.xml
   ```
4. If tests fail:
   - Show the failing test names and output
   - Analyze the error and suggest fixes
5. Report: total passed, failed, skipped.
