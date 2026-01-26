# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Owl is a C++23 game engine with multiple graphics backends (OpenGL 4.5, Vulkan 1.3, Null), input backends (GLFW, Null), and sound backends (OpenAL, Null). It uses an Entity-Component-System architecture (EnTT) and includes a scene editor (Owl Nest).

## Build Commands

**Prerequisites:** CMake 3.24+, Ninja, Python 3.12+ with Poetry (for CI/dependency tooling), `depmanager` for fetching dependencies.

### Configure and build (direct CMake with presets):
```bash
cmake --preset linux-gcc-release -S .
cmake --build output/build/linux-gcc-release
```

### Available presets:
- `linux-gcc-release`, `linux-gcc-debug`
- `linux-clang-release`, `linux-clang-debug`
- CI-only: `linux-clang-tidy`, `linux-sanitizer-address`, `linux-sanitizer-thread`, `linux-sanitizer-undefined-behavior`, `linux-sanitizer-leak`

### Run tests:
```bash
ctest --test-dir output/build/<preset> --output-on-failure
```

### CI system (Python wrapper):
```bash
poetry run python ci_action.py Build <preset>
poetry run python ci_action.py Test <preset>
poetry run python ci_action.py Coverage <preset>
poetry run python ci_action.py Clean <preset>
poetry run python ci_action.py Documentation <preset>
```

## Python Environment

- This project uses **Poetry** for Python dependency management (`pyproject.toml`).
- **Always** use `poetry run` to execute Python commands (e.g., `poetry run python ci_action.py ...`).
- To install/sync dependencies: `poetry sync --no-root`
- Never use the system Python directly; always go through Poetry's virtualenv.

### Build output locations:
- Binaries: `output/build/<preset>/bin/`
- Libraries: `output/build/<preset>/lib/`
- Install: `output/install/<preset>/`

## Architecture

### Engine library (`source/owl/`)
- `public/` — Public API headers organized by module: `core/`, `data/`, `debug/`, `event/`, `gui/`, `input/`, `io/`, `math/`, `physic/`, `renderer/`, `scene/`, `sound/`, `window/`
- `private/` — Implementation files mirroring the public structure
- Builds as `OwlEngine` (shared by default, controlled by `OWL_BUILD_SHARED`)

### Applications (`source/`)
- `owlnest/` — Scene editor (two executables: editor + runner)
- `owldrone/` — Drone navigator
- `owlcast/` — Cast application
- `sandbox/` — Testing/prototyping app

### Tests (`test/`)
- Google Test framework, 14 test categories matching engine modules
- Each category builds as `owl_<category>_unit_test`
- Test helper utilities in `test/test_helper/`

### CI system (`ci/`)
- Python-based CI orchestration, entry point: `ci_action.py`
- Actions in `ci/actions/` (Build, Test, Coverage, Documentation, Package, Clean)
- All actions extend `ci.actions.base.action.BaseAction`
- Utilities in `ci/utils/` (preset parsing, cmake discovery, command execution)

### Dependencies
- Managed by [DepManager](https://github.com/Silmaen/DepManager) via `depmanager.yml`
- Dependencies auto-download during CMake configure step
- Versions are pinned explicitly in `depmanager.yml`

## Code Style

Enforced by `.clang-format` (LLVM-based) and `.clang-tidy`. Key conventions:

- **Tabs** for indentation, 120-character column limit
- **Member variables:** `m_` prefix
- **Parameters:** `i` prefix (input), `o` prefix (output), `io` prefix (input/output)
- **Local variables:** camelCase
- Use `shared<T>` / `mkShared<T>()` instead of `std::shared_ptr<T>` / `std::make_shared<T>()`
- Trailing return type syntax (`-> Type`) for non-void functions
- Early returns to reduce nesting
- Logging: `OWL_CORE_ERROR`, `OWL_CORE_WARN`, `OWL_CORE_INFO`

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `OWL_BUILD_SHARED` | ON | Build engine as shared library |
| `OWL_BUILD_NEST` | ON | Build Owl Nest editor |
| `OWL_BUILD_SANDBOX` | ON | Build sandbox app |
| `OWL_BUILD_DRONE` | ON | Build drone app |
| `OWL_BUILD_CAST` | ON | Build cast app |
| `OWL_TESTING` | ON | Enable unit tests |
| `OWL_ENABLE_COVERAGE` | OFF | Code coverage (auto-enabled in debug presets) |
| `OWL_ENABLE_STACKTRACE` | OFF | Memory tracker stacktrace |
| `OWL_ENABLE_PROFILING` | OFF | Profiling output |
