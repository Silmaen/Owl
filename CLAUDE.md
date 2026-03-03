# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Owl (v0.0.2) is a C++23 game engine with multiple graphics backends (OpenGL 4.5, Vulkan 1.3, Null), input backends (
GLFW, Null), and sound backends (OpenAL, Null). It uses an Entity-Component-System architecture (EnTT) and includes a
scene editor (Owl Nest). Supported platforms: Linux (x64/arm64) and Windows (x64, MinGW).

## Build Commands

**Prerequisites:** CMake 3.24+, Ninja, GCC 13+ or Clang 18+, Python 3.12+ with Poetry (for CI/dependency tooling),
`depmanager` for fetching dependencies.

### Configure and build (direct CMake with presets):

```bash
cmake --preset linux-gcc-release -S .
cmake --build output/build/linux-gcc-release
```

### Available presets:

- Linux: `linux-gcc-release`, `linux-gcc-debug`, `linux-clang-release`, `linux-clang-debug`
- Windows (MinGW): `windows-gcc-release`, `windows-gcc-debug`, `windows-clang-release`, `windows-clang-debug`
- CI-only: `linux-clang-tidy`, `windows-clang-tidy`, `linux-sanitizer-address`, `linux-sanitizer-thread`,
  `linux-sanitizer-undefined-behavior`, `linux-sanitizer-leak`
- Packaging: `package-engine-linux`, `package-engine-windows`, `package-app-nest-linux`, `package-app-nest-windows`,
  `package-app-drone-linux`, `package-app-drone-windows`

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

- This project uses **Poetry** for Python dependency management (`pyproject.toml`) incuding dev tools.
- **Always** use `poetry run` to execute Python commands (e.g., `poetry run python ci_action.py ...`).
- `pip` is forbidden, use **poetry** instead.
- To install/sync dependencies: `poetry sync --no-root`
- Never use the system Python directly; always go through Poetry's virtualenv.

### Build output locations:

- Binaries: `output/build/<preset>/bin/`
- Libraries: `output/build/<preset>/lib/`
- Install: `output/install/<preset>/`

## Architecture

### Engine library (`source/owl/`)

- `public/` — Public API headers organized by module: `core/`, `data/`, `debug/`, `event/`, `gui/`, `input/`, `io/`,
  `math/`, `physic/`, `renderer/`, `scene/`, `sound/`, `window/`
- `private/` — Implementation files mirroring the public structure
- Builds as `OwlEngine` (shared by default, controlled by `OWL_BUILD_SHARED`)

### Applications (`source/`)

- `owlnest/` — Scene editor (two executables: editor + runner)
- `owldrone/` — Drone navigator
- `owlcast/` — Cast application
- `sandbox/` — Testing/prototyping app

### Tests (`test/`)

- Google Test framework, 13 test categories: core, debug, event, font, gui, input, layer, math, mesh, physic, renderer,
  scene, sound
- Each category builds as `owl_<category>_unit_test`
- Test helper utilities in `test/test_helper/`

### CI system (`ci/`)

- Python-based CI orchestration, entry point: `ci_action.py`
- Actions in `ci/actions/`: Build, Test, Coverage, Documentation, Package, Clean, Help, DefineTeamCityVariables,
  PublishDoc, PublishPackage
- All actions extend `ci.actions.base.action.BaseAction`
- Utilities in `ci/utils/`: preset parsing, cmake discovery, command execution, logging, publishing, python helpers, TeamCity integration

### Engine assets (`engine_assets/`)

- Runtime assets bundled with the engine: fonts, shaders, textures, logo

### CMake modules (`cmake/`)

- Build configuration modules: `BaseConfig.cmake`, `CoverageConfig.cmake`, `Depmanager.cmake`,
  `DocumentationConfig.cmake`, `Environment.cmake`, `OwlUtils.cmake`, `Poetry.cmake`, `Python.cmake`,
  `Sanitizers.cmake`, `Vulkan.cmake`
- Preset definitions: `CMakePresetsBase.json`, `CMakePresetsLinux.json`, `CMakePresetsMinGW.json`,
  `CMakePresetsCI.json`, `CMakePresetsPackage.json`

### Shader Pipeline (Slang)

Shaders are written in **Slang** (`.slang` files), a single-source shading language compiled at runtime to SPIR-V for both OpenGL and Vulkan backends.

- **Source location:** `engine_assets/shaders/<renderer>/slang/<name>.slang`
- **Compilation:** `compileSlangToSpirv()` in `source/owl/private/renderer/utils/shaderFileUtils.cpp` — uses Slang C++ API to compile to SPIR-V with `BACKEND_VULKAN` or `BACKEND_OPENGL` preprocessor defines
- **Reflection:** `shaderReflect()` in the same file — uses spirv-cross to extract uniform buffers and sampled images from SPIR-V
- **Caching:** SPIR-V binaries cached as `.spv` files with hash-based validation (`computeShaderHash`, `isShaderCacheValid`)
- **Entry points:** `[shader("vertex")] vertexMain` and `[shader("fragment")] fragmentMain` per file
- **Key conventions:**
  - `column_major float4x4` for matrices uploaded from C++ (Slang defaults to row-major)
  - `[[vk::binding(N)]]` for explicit Vulkan descriptor bindings
  - `#ifdef BACKEND_VULKAN` for texture binding differences (binding 1 for Vulkan, binding 0 for OpenGL)
  - `NonUniformResourceIndex()` for texture array indexing (works on both backends)
  - No sRGB conversion in shaders (framebuffers use UNORM format without hardware sRGB)

### Dependencies

- Managed by [DepManager](https://github.com/Silmaen/DepManager) via `depmanager.yml` (30 external dependencies)
- Dependencies auto-download during CMake configure step
- Versions are pinned explicitly in `depmanager.yml`
- Key libraries: EnTT (ECS), ImGui/ImGuizmo (GUI), Box2D (physics), spdlog (logging), yaml-cpp (serialization), Vulkan
  SDK, GLFW, OpenAL, glad, freetype, msdfgen/msdf-atlas-gen (fonts), tinygltf/tinyobjloader/ufbx (mesh loading),
  magic_enum (enum reflection), cpptrace/libdwarf/debugbreak (debugging), zeus (math), nfd (file dialogs), tinyxml2
  (XML), libpng, zlib/zstd (compression), libsndfile (audio files), stb_image (image loading), googletest (testing),
  mavsdk (drone MAVLink SDK)

## DepManager (`dmgr`) Usage

DepManager is the C++ dependency manager for this project. It is installed as a Python package via Poetry and invoked
through `poetry run depmanager`. **Never install depmanager with `pip`; it is managed by Poetry.**

### Key Commands

All commands must be prefixed with `poetry run`:

```bash
# Check depmanager version
poetry run depmanager info version --raw

# Get the CMake modules directory (used by cmake/Depmanager.cmake)
poetry run depmanager info cmakedir --raw

# Get the base storage directory
poetry run depmanager info basedir --raw

# List local packages (with optional query filters)
poetry run depmanager pack ls
poetry run depmanager pack ls -p <name>:<version> -t <static|shared|header>

# List packages on a remote
poetry run depmanager pack ls <remote_name>

# Pull a specific package from a remote
poetry run depmanager pack pull -p <name>:<version> <remote_name>

# Push a local package to a remote
poetry run depmanager pack push -p <name>:<version> <remote_name>

# Clean local package cache
poetry run depmanager pack clean        # clean unused packages
poetry run depmanager pack clean -f     # full clean

# List configured remotes
poetry run depmanager remote list

# Add a remote server
poetry run depmanager remote add -n <name> -u <protocol>://<url[:port]>

# Sync with a remote (bidirectional)
poetry run depmanager remote sync <remote_name>

# Build packages from recipes
poetry run depmanager build <recipe_dir> [-r] [-f]
```

### Configuration File (`depmanager.yml`)

Located at the project root. Structure:

```yaml
remote:
  pull: true          # Allow downloading packages not found locally
  pull-newer: true    # Download newer remote versions (implies pull)
  server:             # Optional: configure a named remote inline
    name: "server_name"
    kind: "srvs"
    url: "https://example.com"

packages:
  <package_name>:
    version: "<version>"          # Exact version or range (e.g., ">=1.0.0")
    kind: "static|shared|header"  # Optional, defaults to environment kind
    optional: true                # Optional dependency (won't fail if missing)
```

### Rules for Working with DepManager

1. **Always use `poetry run depmanager`** — never call `depmanager` directly, as it must run in Poetry's virtualenv.
2. **Pin versions explicitly** in `depmanager.yml` — use exact versions (e.g., `3.1.1`), not ranges, to ensure
   reproducible builds.
3. **Specify `kind`** (`static`/`shared`) for each package that is not header-only — this controls the library type
   fetched by depmanager. Header-only libraries (e.g., `entt`, `magic_enum`, `stb_image`) can omit `kind`.
4. **Dependencies are fetched automatically** during CMake configure (`cmake --preset ...`) — the `cmake/Depmanager.cmake`
   module handles initialization, version checking, and environment loading via `dm_load_environment()`.
5. **Do not call `find_package()` directly** for depmanager-managed dependencies — use the `owl_target_link_libraries()`
   helper function defined in `cmake/OwlUtils.cmake` which wraps `find_package()` and `target_link_libraries()`.
6. **Adding a new dependency:**
   - Add the entry to `depmanager.yml` with `version` and `kind`
   - Use `owl_target_link_libraries(<target> <PRIVATE|PUBLIC|INTERFACE> <module> REQUIRED)` in the target's
     `CMakeLists.txt`
   - Optionally add `FORCE_RELEASE` to always link the release build of the dependency
7. **Removing a dependency:** remove the entry from `depmanager.yml` and all corresponding `owl_target_link_libraries()`
   calls in CMakeLists.txt files.
8. **The `remote` section** controls auto-download behavior — `pull: true` allows fetching missing packages from the
   configured remote server, `pull-newer: true` also updates to newer available versions.
9. **Cross-compilation** is supported — depmanager handles architecture-specific packages via the `ARCH` parameter in
   `dm_load_environment()`.
10. **Query predicates** use the format `-p <name>:<version>` with optional filters: `-t <type>`, `-o <os>`,
    `-a <arch>`, `-c <abi>`.

## Code Style

Enforced by `.clang-format` (LLVM-based) and `.clang-tidy`. Key conventions:

- **Tabs** for indentation, 120-character column limit
- **Member variables:** `m_` prefix
- **Parameters:** `i` prefix (input), `o` prefix (output), `io` prefix (input/output)
- **Local variables:** camelCase
- Use `shared<T>` / `mkShared<T>()` instead of `std::shared_ptr<T>` / `std::make_shared<T>()`
- Trailing return type syntax (`-> Type`) for non-void functions
- Early returns to reduce nesting
- Logging: `OWL_CORE_TRACE`, `OWL_CORE_INFO`, `OWL_CORE_WARN`, `OWL_CORE_ERROR`, `OWL_CORE_CRITICAL` (client equivalents
  without `_CORE`)

## CMake Options

| Option                                    | Default | Description                                    |
|-------------------------------------------|---------|------------------------------------------------|
| `OWL_BUILD_SHARED`                        | ON      | Build engine as shared library                 |
| `OWL_BUILD_NEST`                          | ON      | Build Owl Nest editor                          |
| `OWL_BUILD_SANDBOX`                       | ON      | Build sandbox app                              |
| `OWL_BUILD_DRONE`                         | ON      | Build drone app                                |
| `OWL_BUILD_CAST`                          | ON      | Build cast app                                 |
| `OWL_TESTING`                             | ON      | Enable unit tests                              |
| `OWL_ENABLE_COVERAGE`                     | OFF     | Code coverage (auto-enabled in debug presets)  |
| `OWL_ENABLE_STACKTRACE`                   | OFF     | Memory tracker stacktrace (performance impact) |
| `OWL_ENABLE_PROFILING`                    | OFF     | Profiling output                               |
| `OWL_USE_RELEASE_THIRD_PARTY`             | ON      | Use release builds of third-party libraries    |
| `OWL_DEFINE_VULKAN_LAYERS`                | OFF     | Copy Vulkan layers to binary directory         |
| `OWL_ENABLE_CLANG_TIDY`                   | OFF     | Enable clang-tidy static analysis              |
| `OWL_ENABLE_ADDRESS_SANITIZER`            | OFF     | AddressSanitizer (CI presets)                  |
| `OWL_ENABLE_THREAD_SANITIZER`             | OFF     | ThreadSanitizer (CI presets)                   |
| `OWL_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER` | OFF     | UBSanitizer (CI presets)                       |
| `OWL_ENABLE_LEAK_SANITIZER`               | OFF     | LeakSanitizer (CI presets)                     |
| `OWL_ENABLE_MEMORY_SANITIZER`             | OFF     | MemorySanitizer (Clang-only, CI presets)        |
| `OWL_ENABLE_DOCUMENTATION`                | OFF     | Enable Doxygen documentation generation        |
| `OWL_PACKAGING`                           | OFF     | Enable packaging mode                          |
