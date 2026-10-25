---
name: coverage
description: Run code coverage analysis for the Owl project (GCC or Clang debug preset, gcovr.cfg) inside Docker
---

# Generate code coverage

1. Pick the preset: `linux-gcc-debug` gives the figure the pull requests are gated on, `linux-clang-debug` the one of
   the nightly Clang build. Both are measured because GCC and Clang count lines differently; say which one you report.
2. Build and test the preset (`build` and `test` skills), then run:
   ```bash
   ./docker_run.sh poetry run python ci_action.py Coverage <preset>
   ```
3. Manual gcovr runs **always** use the project config: `poetry run gcovr --config gcovr.cfg …` — never pass
   filters on the command line. With Clang, add `--gcov-executable "llvm-cov gcov"`.
4. Report the global line/branch coverage, the five least-covered files touched by the current work, and the
   HTML report path.
