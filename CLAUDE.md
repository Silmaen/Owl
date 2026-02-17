# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Owl (v0.0.2) is a C++23 game engine with multiple graphics backends (OpenGL 4.5, Vulkan 1.3, Null), input backends (GLFW, Null), and sound backends (OpenAL, Null). It uses an Entity-Component-System architecture (EnTT) and includes a scene editor (Owl Nest). Supported platforms: Linux (x64/arm64) and Windows (x64, MinGW).

## Build Commands

**Prerequisites:** CMake 3.24+, Ninja, Python 3.12+ with Poetry (for CI/dependency tooling), `depmanager` for fetching dependencies.

### Configure and build (direct CMake with presets):
```bash
cmake --preset linux-gcc-release -S .
cmake --build output/build/linux-gcc-release
```

### Available presets:
- Linux: `linux-gcc-release`, `linux-gcc-debug`, `linux-clang-release`, `linux-clang-debug`
- Windows (MinGW): `mingw-gcc-release`, `mingw-gcc-debug`, `mingw-clang-release`, `mingw-clang-debug`
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
- Google Test framework, 13 test categories: core, debug, event, font, gui, input, layer, math, mesh, physic, renderer, scene, sound
- Each category builds as `owl_<category>_unit_test`
- Test helper utilities in `test/test_helper/`

### CI system (`ci/`)
- Python-based CI orchestration, entry point: `ci_action.py`
- Actions in `ci/actions/`: Build, Test, Coverage, Documentation, Package, Clean, Help, DefineVariables, PublishDoc, PublishPackage
- All actions extend `ci.actions.base.action.BaseAction`
- Utilities in `ci/utils/`: preset parsing, cmake discovery, command execution, logging, publishing, python helpers, TeamCity integration

### Engine assets (`engine_assets/`)
- Runtime assets bundled with the engine: fonts, shaders, textures, logo

### CMake modules (`cmake/`)
- Build configuration modules: `BaseConfig.cmake`, `Depmanager.cmake`, `OwlUtils.cmake`, `Sanitizers.cmake`, `Vulkan.cmake`, `Poetry.cmake`, `CoverageConfig.cmake`
- Preset definitions: `CMakePresetsBase.json`, `CMakePresetsLinux.json`, `CMakePresetsMinGW.json`, `CMakePresetsCI.json`, `CMakePresetsPackage.json`

### Dependencies
- Managed by [DepManager](https://github.com/Silmaen/DepManager) via `depmanager.yml` (29 external dependencies)
- Dependencies auto-download during CMake configure step
- Versions are pinned explicitly in `depmanager.yml`
- Key libraries: EnTT (ECS), ImGui (GUI), Box2D (physics), spdlog (logging), yaml-cpp (serialization), Vulkan SDK, GLFW, OpenAL, glad, freetype, msdfgen/msdf-atlas-gen (fonts), tinygltf/tinyobjloader/ufbx (mesh loading)

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
| `OWL_ENABLE_STACKTRACE` | OFF | Memory tracker stacktrace (performance impact) |
| `OWL_ENABLE_PROFILING` | OFF | Profiling output |
| `OWL_USE_RELEASE_THIRD_PARTY` | ON | Use release builds of third-party libraries |
| `OWL_DEFINE_VULKAN_LAYERS` | OFF | Copy Vulkan layers to binary directory |
| `OWL_ENABLE_CLANG_TIDY` | OFF | Enable clang-tidy static analysis |
| `OWL_ENABLE_ADDRESS_SANITIZER` | OFF | AddressSanitizer (CI presets) |
| `OWL_ENABLE_THREAD_SANITIZER` | OFF | ThreadSanitizer (CI presets) |
| `OWL_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER` | OFF | UBSanitizer (CI presets) |
| `OWL_ENABLE_LEAK_SANITIZER` | OFF | LeakSanitizer (CI presets) |
