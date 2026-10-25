---
name: check
description: Pre-handoff gate for Owl changes — build, tests, CodeStyle and clang-tidy inside Docker; `--quick` for the fast loop, `--fix` to apply the formatters first
---

# Check the current change before handing it back

Arguments: `--quick` runs the fast loop only (what a draft pull request gets from the CI), `--fix` applies the
formatters before the checks. Without `--quick`, run the full gate: it is what a ready pull request gets.

Run what the diff requires, in this order, stopping at the first failure:

1. `git diff --stat main...HEAD` plus `git status` to know what changed (C++, Python, docs, shaders, CI).
2. `--fix` only, on the changed files: `./docker_run.sh clang-format -i <C++ files>` and
   `./docker_run.sh poetry run ruff check --fix ci ci_action.py` then `./docker_run.sh poetry run ruff format ci ci_action.py`.
3. C++ or CMake changed: `build` skill on `linux-clang-release`, then `test` skill:
   - `--quick`: only the test categories the diff touches (`owl_<cat>_tests_unit_test`, or `ctest -R <cat>`);
   - otherwise the whole suite without the `slow` label, as the CI does.
4. Always: `./docker_run.sh poetry run python ci_action.py CodeStyle linux-clang-release`.
5. `--quick` stops here. Otherwise, C++ changed: `./docker_run.sh cmake --preset linux-clang-tidy -S .` if not
   configured, commit, then `./docker_run.sh poetry run python ci_action.py ClangTidy linux-clang-tidy -- --diff_base=main`.
6. Docs or public API changed: `./docker_run.sh poetry run python ci_action.py Documentation linux-clang-release`.
7. Check that `doc/pages/changelog.md` `[Unreleased]` and `doc/pages/roadmap.md` reflect the change.

Report each step as passed / failed / skipped (with the reason), quoting the first error of a failure.
