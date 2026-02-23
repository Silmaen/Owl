# Contributing {#contributing}

[TOC]

This page describes conventions and workflows for contributing to Owl.

## Code Style

The project uses `.clang-format` (LLVM-based) and `.clang-tidy` for automated formatting
and static analysis.

### Formatting Rules

- **Indentation**: tabs (not spaces), 120-character column limit
- **Trailing return types**: use `-> Type` for non-void functions
- **Early returns**: prefer early returns to reduce nesting

### Naming Conventions

| Element            | Convention        | Example              |
|--------------------|-------------------|----------------------|
| Member variables   | `m_` prefix       | `m_width`            |
| Input parameters   | `i` prefix        | `iFilename`          |
| Output parameters  | `o` prefix        | `oResult`            |
| In/out parameters  | `io` prefix       | `ioBuffer`           |
| Local variables    | camelCase          | `frameCount`         |

### Smart Pointers

Use Owl aliases instead of `std` types:

| Owl Alias          | Standard Equivalent              |
|--------------------|----------------------------------|
| `shared<T>`        | `std::shared_ptr<T>`             |
| `mkShared<T>()`    | `std::make_shared<T>()`          |

### Enumerations

Use `enum struct` (scoped enumerations) for all new enums.

## Documentation

All public API types and functions should have Doxygen doc-comments. Use `///` or
`/** ... */` style. The engine uses the `@param`, `@return`, and `@brief` Doxygen commands.

## Adding Tests

Tests use Google Test. Test files are auto-discovered from `test/<category>_tests/` directories.
To add a new test:

1. Create a `.cpp` file in the appropriate `test/<category>_tests/` directory
2. Include `<gtest/gtest.h>` and the headers under test
3. Write `TEST` or `TEST_F` cases
4. Build and run -- no CMakeLists.txt changes needed

Test executables follow the naming pattern `owl_<folder>_unit_test`.

## Adding Dependencies

Dependencies are managed by [DepManager](https://github.com/Silmaen/DepManager) via
`depmanager.yml`.

1. Add an entry to `depmanager.yml` with explicit `version` and `kind` (`static`, `shared`, or omit for header-only)
2. In the target's `CMakeLists.txt`, use:
   ```cmake
   owl_target_link_libraries(<target> <PRIVATE|PUBLIC|INTERFACE> <module> REQUIRED)
   ```
3. Do **not** call `find_package()` directly for DepManager-managed dependencies

See @ref building for the full build setup.

## Logging

Use the engine logging macros:

| Level     | Engine Macro           | Client Macro      |
|-----------|------------------------|--------------------|
| Trace     | `OWL_CORE_TRACE`       | `OWL_TRACE`       |
| Info      | `OWL_CORE_INFO`        | `OWL_INFO`        |
| Warning   | `OWL_CORE_WARN`        | `OWL_WARN`        |
| Error     | `OWL_CORE_ERROR`       | `OWL_ERROR`       |
| Critical  | `OWL_CORE_CRITICAL`    | `OWL_CRITICAL`    |

## Git Workflow

- Create feature branches from `main`
- Keep commits focused and well-described
- Run tests locally before pushing: `ctest --test-dir output/build/<preset> --output-on-failure`
- CI will run clang-tidy, sanitizers, and the full test suite on pull requests
