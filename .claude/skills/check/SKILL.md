---
name: check
description: Pre-handoff gate for Owl changes — build, tests, CodeStyle and clang-tidy inside Docker
---

# Check the current change before handing it back

Run what the diff requires, in this order, stopping at the first failure:

1. `git diff --stat main...HEAD` plus `git status` to know what changed (C++, Python, docs, shaders).
2. C++ or CMake changed: `build` skill on `linux-clang-release`, then `test` skill (full suite).
3. Always: `docker/run.sh poetry run python ci_action.py CodeStyle linux-clang-release`.
4. C++ changed: `docker/run.sh cmake --preset linux-clang-tidy -S .` if not configured, then
   `docker/run.sh cmake --build output/build/linux-clang-tidy`, then
   `docker/run.sh poetry run python ci_action.py ClangTidy linux-clang-tidy -- --diff_base=main`.
5. Docs or public API changed: `docker/run.sh poetry run python ci_action.py Documentation linux-clang-release`.
6. Check that `doc/pages/changelog.md` `[Unreleased]` and `doc/pages/roadmap.md` reflect the change.

Report each step as passed / failed / skipped (with the reason), quoting the first error of a failure.
