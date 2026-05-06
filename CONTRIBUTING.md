# Contributing to Owl

Thank you for your interest in contributing to Owl! This document provides guidelines
and information for contributors.

## Getting Started

1. Fork the repository and clone your fork
2. Install prerequisites (see [Building](doc/pages/building.md))
3. Create a feature branch from `main`

```bash
git checkout -b Feature/my-feature main
```

## Development Setup

```bash
# Install Python dependencies (Poetry required)
poetry sync --no-root

# Configure and build
cmake --preset linux-clang-release -S .
cmake --build output/build/linux-clang-release

# Run tests
ctest --test-dir output/build/linux-clang-release --output-on-failure
```

## Code Style

The project enforces style via `.clang-format` and `.clang-tidy`. Key conventions:

- **Tabs** for indentation, 120-character column limit
- **Trailing return types** for non-void functions: `auto foo() -> int`
- **Naming**: `m_` prefix for members, `i`/`o`/`io` prefix for parameters, camelCase for locals
- **Smart pointers**: use `shared<T>` / `mkShared<T>()` (not `std::shared_ptr`)
- **Enums**: use `enum struct` with explicit underlying type
- **Doxygen**: all public API must have doc-comments

See [Contributing Guide](doc/pages/contributing.md) for the full style reference.

## Pull Request Process

1. Ensure your code builds without warnings (`-Werror` is enabled)
2. Run the full test suite and verify all tests pass
3. Run clang-tidy: `cmake --preset linux-clang-tidy && cmake --build output/build/linux-clang-tidy`
4. Write clear, focused commit messages
5. Open a pull request against `main` with a description of what and why

## Documentation Conventions

`doc/pages/*.md` plus the canonical root files (`README.md`, `CHANGELOG.md`,
`CONTRIBUTING.md`) feed both Doxygen and the in-editor help panel. The bundle
step (`cmake/HelpAssets.cmake`) strips Doxygen anchors (`{#page-name}`),
`[TOC]` lines, and rewrites `(../images/foo.svg)` to `(images/foo.svg)`, so
those source-side conventions remain valid for Doxygen builds.

Mermaid diagrams (` ```mermaid ` fences) render natively on GitHub and through
mermaid.js in the Doxygen HTML output. The in-editor help panel does **not**
execute JavaScript: mermaid blocks render as plain code blocks there. Prefer
mermaid for architecture / flow / sequence diagrams; the SVGs in `doc/images/`
remain valid for static figures.

## Adding Tests

Tests use Google Test and are auto-discovered from `test/<category>_tests/` directories.
No CMakeLists.txt edits needed -- just add a `.cpp` file in the right folder.

## Adding Dependencies

Dependencies are managed by [DepManager](https://github.com/Silmaen/DepManager).
See [Building](doc/pages/building.md) for details.

## Reporting Issues

- Use GitHub Issues for bug reports and feature requests
- Include steps to reproduce, expected behaviour, and actual behaviour
- Attach logs or screenshots when relevant

## License

By contributing, you agree that your contributions will be licensed under the
[MIT License](LICENSE).
