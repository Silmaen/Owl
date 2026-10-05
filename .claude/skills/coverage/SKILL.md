---
name: coverage
description: Run code coverage analysis for the Owl project (Clang debug preset, gcovr.cfg) inside Docker
---

# Generate code coverage

1. Use `linux-clang-debug` (coverage flags are on in debug presets).
2. Run:
   ```bash
   docker/run.sh poetry run python ci_action.py Coverage linux-clang-debug
   ```
3. Manual gcovr runs **always** use the project config: `poetry run gcovr --config gcovr.cfg …` — never pass
   filters on the command line. With Clang, add `--gcov-executable "llvm-cov gcov"`.
4. Report the global line/branch coverage, the five least-covered files touched by the current work, and the
   HTML report path.
