# Contributing to Owl

Thank you for your interest in contributing to Owl!

The contributor guide is [doc/pages/contributing.md](doc/pages/contributing.md): setup, code style, documentation,
tests, dependencies and the checks to run before a pull request. In short:

1. Fork, clone, and branch from `main` (`Feature/<topic>`)
2. Build and test inside the build image (see [Building](doc/pages/building.md)):
   ```bash
   docker/run.sh cmake --preset linux-clang-release -S .
   docker/run.sh cmake --build output/build/linux-clang-release
   docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure
   ```
3. Pass the Code Style gate (`docker/run.sh poetry run python ci_action.py CodeStyle linux-clang-release`) and,
   when C++ changed, the `ClangTidy` action
4. Open a pull request against `main` that says what changes and why

Security issues follow `SECURITY.md`. Contributions are licensed under the [MIT License](LICENSE).
