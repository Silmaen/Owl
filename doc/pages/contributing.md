# Contributing {#page-contributing}

[TOC]

This page is the contributor guide: setup, conventions and the checks a change must pass. The root
`CONTRIBUTING.md` only points here.

## Getting started

1. Fork the repository and clone your fork
2. Build it once (see [Building](building.md)): every command runs inside the build image through `docker/run.sh`
3. Create a branch from `main`: `Feature/<topic>` for a change meant to merge, `Experiment/<topic>` for a trial

```bash
git switch -c Feature/my-feature main
docker/run.sh cmake --preset linux-clang-release -S .
docker/run.sh cmake --build output/build/linux-clang-release
docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure
```

## Code Style

The project uses `.clang-format` (LLVM-based) and `.clang-tidy` for automated formatting
and static analysis.

### Formatting Rules

- **Indentation**: tabs (not spaces), 120-character column limit
- **Trailing return types**: use `-> Type` for non-void functions
- **Early returns**: prefer early returns to reduce nesting

### Naming Conventions

| Element           | Convention  | Example      |
|-------------------|-------------|--------------|
| Member variables  | `m_` prefix | `m_width`    |
| Input parameters  | `i` prefix  | `iFilename`  |
| Output parameters | `o` prefix  | `oResult`    |
| In/out parameters | `io` prefix | `ioBuffer`   |
| Local variables   | camelCase   | `frameCount` |

### Smart Pointers

Use Owl aliases instead of `std` types:

| Owl Alias       | Standard Equivalent     |
|-----------------|-------------------------|
| `shared<T>`     | `std::shared_ptr<T>`    |
| `mkShared<T>()` | `std::make_shared<T>()` |
| `uniq<T>`       | `std::unique_ptr<T>`    |
| `mkUniq<T>()`   | `std::make_unique<T>()` |
| `weak<T>`       | `std::weak_ptr<T>`      |

### Enumerations

Use `enum struct` (scoped enumerations) with an explicit underlying type for all new enums.

## Documentation

All public API types and functions have Doxygen doc-comments: `/** ... */` blocks with `@brief`, `@param` and
`@return`, `///` for a one-line member or enum value. Implementation files carry only their file header.

The user documentation lives in `doc/pages/` and changes in the same pull request as the code it describes:

- `doc/pages/*.md` and `doc/pages/design/*.md`, plus the root `README.md` and `CONTRIBUTING.md`, feed both Doxygen
  and the in-editor help panel (the root `CHANGELOG.md` / `ROADMAP.md` summaries feed Doxygen only). The bundle
  step (`cmake/HelpAssets.cmake`) strips Doxygen anchors (`{#page-name}`) and `[TOC]` lines, and rewrites
  `(../images/foo.svg)` to `(images/foo.svg)`, so those source-side conventions remain valid for Doxygen builds.
- Mermaid diagrams (` ```mermaid ` fences) render natively on GitHub and through mermaid.js in the Doxygen HTML
  output. The in-editor help panel does **not** execute JavaScript: mermaid blocks render as plain code blocks
  there. Prefer mermaid for architecture / flow / sequence diagrams; the SVGs in `doc/images/` remain valid for
  static figures.
- Every symbol, file or `OWL_*` option a page cites between backticks must exist: the Code Style gate checks it
  (see [Continuous Integration](continuous_integration.md)).
- `doc/pages/changelog.md` gets one line under `[Unreleased]` per change, and `doc/pages/roadmap.md` the badge of
  the item.

## Adding Tests

Tests use Google Test. Test files are auto-discovered from `test/<category>_tests/` directories.
To add a new test:

1. Create a `.cpp` file in the appropriate `test/<category>_tests/` directory
2. Include `<gtest/gtest.h>` and the headers under test
3. Write `TEST` or `TEST_F` cases
4. Configure again (the sources are globbed) and build -- no CMakeLists.txt changes needed

Test executables follow the naming pattern `owl_<folder>_unit_test`.

## Adding Dependencies

Dependencies come from Conan 2 via `conanfile.py`, pinned by `conan.lock`.

1. Add `self.requires("<name>/<version>")` to `conanfile.py` with an exact version (ConanCenter first, otherwise a
   minimal recipe in `conan/recipes/`), its linkage in `default_options`, then regenerate `conan.lock`
2. In the target's `CMakeLists.txt`, use:
   ```cmake
   owl_target_link_libraries(<target> <PRIVATE|PUBLIC|INTERFACE> <module> REQUIRED)
   ```
3. Do **not** call `find_package()` directly for Conan-managed dependencies

See [Building](building.md) for the full build setup.

## Logging

Use the engine logging macros:

| Level    | Engine Macro        | Client Macro   |
|----------|---------------------|----------------|
| Trace    | `OWL_CORE_TRACE`    | `OWL_TRACE`    |
| Info     | `OWL_CORE_INFO`     | `OWL_INFO`     |
| Warning  | `OWL_CORE_WARN`     | `OWL_WARN`     |
| Error    | `OWL_CORE_ERROR`    | `OWL_ERROR`    |
| Critical | `OWL_CORE_CRITICAL` | `OWL_CRITICAL` |

## Before a pull request

1. The build has no warning (`-Werror` is on) and the whole test suite passes
2. The Code Style gate passes:
   `docker/run.sh poetry run python ci_action.py CodeStyle linux-clang-release`
3. When C++ changed, clang-tidy passes on what the branch touches (the `linux-clang-tidy` preset only exports the
   compile commands; the CI action runs clang-tidy):
   ```bash
   docker/run.sh poetry run python ci_action.py Build linux-clang-tidy
   docker/run.sh poetry run python ci_action.py ClangTidy linux-clang-tidy -- --diff_base=main
   ```
4. Commits are short and focused; the pull request targets `main` and says what changes and why

The CI then runs clang-tidy, the sanitizers and the full test suite — see
[Continuous Integration](continuous_integration.md) for the build matrix and which configurations run on draft
vs. ready PRs vs. `main` only.

## Reporting Issues

- Use GitHub Issues for bug reports and feature requests
- Include steps to reproduce, expected behaviour, and actual behaviour
- Attach logs or screenshots when relevant
- Security issues follow the root `SECURITY.md` instead

## License

By contributing, you agree that your contributions will be licensed under the MIT License (the root `LICENSE`).
