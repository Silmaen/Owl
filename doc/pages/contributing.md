# Contributing {#page-contributing}

[TOC]

This page is the contributor guide: setup, conventions and the checks a change must pass. The root
`CONTRIBUTING.md` only points here.

## How Owl Engine is developed

Owl Engine is developed by one maintainer with Claude Code as a development tool, used under supervision: every
change is reviewed and owned by the maintainer, and goes through the same strict CI as any contribution (unit and
scenario tests on Linux x64/arm64 and Windows, sanitizers, clang-tidy, include checks and performance budgets).

## Getting started

1. Fork the repository and clone your fork
2. Build it once (see [Building](building.md)): every command runs inside the build image through `./docker_run.sh`
3. Create a branch from `main`: `Feature/<topic>` for a change meant to merge, `Experiment/<topic>` for a trial

```bash
git switch -c Feature/my-feature main
./docker_run.sh cmake --preset linux-clang-release -S .
./docker_run.sh cmake --build output/build/linux-clang-release
./docker_run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure
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

Every element of a public header (types, functions, special members, fields, enum values, typedefs, macros) has a
Doxygen doc-comment: `/** ... */` blocks with `@brief`, `@param` and `@return`, `///<` for a one-line field or enum
value. The `documentation` target extracts the public API only and fails on a missing one. Implementation files
carry only their file header.

The user documentation lives in `doc/pages/` and changes in the same pull request as the code it describes:

- `doc/help.yml` lists the pages of the help panel of Owl Nest, grouped by theme in sidebar order: every
  `doc/pages/*.md` and the root `README.md`. A new page takes its place there (the configure refuses an unlisted one).
  The bundle step (`cmake/HelpAssets.cmake`) strips the Doxygen anchors (`{#page-name}`) and `[TOC]` lines, rewrites
  the image paths and turns the links to pages outside the help into text, so those source-side conventions stay
  valid for Doxygen.
- The design pages (`doc/pages/design/*.md`) are plans: they are in the Doxygen documentation only, listed in
  `doc/pages/design/plans.md`. Help pages describe what exists, without versions or status badges.
- Diagrams are mermaid fences (a block opened by three backticks and the word `mermaid`): GitHub draws them, Doxygen
  through mermaid.js, the help panel as SVG files drawn at configure time by `tools/doc/mermaid_svg.py` (flowchart,
  sequence, state and class diagrams; another kind fails the configure).
- A fixed-size image is a raw `<img src="..." width="160">` tag (GitHub, Doxygen and the help panel honour the width).
- The Doxygen page tree follows `doc/help.yml`: the README is the main page and the parent of the first page of each
  group, which is the parent of the other pages of its group (`tools/doc/fix_md_links.py`). A page outside the help is
  attached to its parent by a `<!-- subpage: page.md -->` line, as the design pages to `doc/pages/design/plans.md`.
  Content already written in another page is linked, not copied.
- The Owl Nest screenshots in `doc/images/nest/` are made by `tools/doc/capture_nest.py`, headless inside the build
  image: a private Xvfb display, Owl Nest on lavapipe with the Engine Demo, one editor start per shot, set up through
  the reopened session, synthetic clicks and keys (XTest) and the MCP server, then grabbed, cropped and written as PNG
  of at most 400 KB. After a build, refresh them all, or some, with
  `./docker_run.sh poetry run python tools/doc/capture_nest.py [--only main_window,console]`; `--raw` and a folder keep the
  full frames. The shots, their gestures and their crops are listed at the top of the script: a moved button means
  new coordinates there. Look at every image before committing it.
- Every symbol, file or `OWL_*` option a page cites between backticks must exist: the Code Style gate checks it
  (see [Continuous Integration](continuous_integration.md)).
- `doc/pages/changelog.md` gets one line under `[Unreleased]` per change, and `doc/pages/roadmap.md` the badge of
  the item.

## Adding Tests

Tests use Google Test. Test files are auto-discovered from `test/unit/<category>_tests/` directories.
To add a new test:

1. Create a `.cpp` file in the appropriate `test/unit/<category>_tests/` directory
2. Include `<gtest/gtest.h>` and the headers under test
3. Write `TEST` or `TEST_F` cases
4. Configure again (the sources are globbed) and build -- no CMakeLists.txt changes needed

Each folder builds one executable, `owl_<folder>_unit_test` (`owl_scene_tests_unit_test`); scenario tests are
`test/unit/scenarios/*.owltest` files played headless by `OwlRunner --scenario` (see [Building](building.md#running-tests)).

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

## Bumping the version

The version is written once, in `project(Owl VERSION x.y.z)` of the root `CMakeLists.txt`. Everything else follows
at the next configure:

- the engine API `getVersionString()`, `getVersionCommit()` and `getVersionFullString()` (`core/Core.h`, the commit
  is the one the tree was configured from), hence the Owl Nest title bar and File screen, the start-up log of every
  application, the MCP server info, `game_info.yml` and the frame-bench reports
- Lua `engine.version()`, which fills the version label of the Engine Demo main menu
- the Engine Demo and the sample games, whose `owl_project.yml` has no `version`: a project without one exports the
  engine version (a game of its own keeps its `version`)
- the package names (`OwlNest-x.y.z-<commit>-…`), the Conan package version, the Windows version resources, the
  Doxygen `PROJECT_NUMBER` and the version the CI publishes

The changelog, the roadmap and the design pages are history: a release dates them by hand.

## Before a pull request

1. The build has no warning (`-Werror` is on) and the whole test suite passes
2. The Code Style gate passes:
   `./docker_run.sh poetry run python ci_action.py CodeStyle linux-clang-release`
3. When C++ changed, clang-tidy passes on what the branch touches (the `linux-clang-tidy` preset only exports the
   compile commands; the CI action runs clang-tidy):
   ```bash
   ./docker_run.sh poetry run python ci_action.py Build linux-clang-tidy
   ./docker_run.sh poetry run python ci_action.py ClangTidy linux-clang-tidy -- --diff_base=main
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
