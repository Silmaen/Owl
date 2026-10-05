---
name: test
description: Run Owl unit tests (all, one category or one gtest filter) inside the Docker build image
---

# Run tests

1. Preset: the argument if given, else `linux-clang-release`; it must be built (run the `build` skill first
   if `output/build/<preset>/bin` is missing or sources changed).
2. All tests:
   ```bash
   docker/run.sh ctest --test-dir output/build/<preset> --output-on-failure -j8
   ```
3. One category or filter (run from the build dir: `getRootPath()` loops when CWD is the repo root):
   ```bash
   docker/run.sh bash -c 'cd output/build/<preset> && bin/owl_<category>_tests_unit_test --gtest_filter="<Suite>.<Test>"'
   ```
4. Tests that need a GPU or audio: prefix with `docker/run.sh --gui`.
5. Report passed / failed / skipped with failing test names and the assertion output; do not call a failure
   "flaky" without rerunning it.
