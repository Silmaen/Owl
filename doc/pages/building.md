# Building Owl {#building}

[TOC]

This page explains how to configure, build, and test the Owl engine.

## Prerequisites

| Tool         | Version   | Notes                                       |
|--------------|-----------|---------------------------------------------|
| CMake        | 3.24+     | Build system generator                      |
| Ninja        |           | Recommended build backend                   |
| GCC          | 13+       | Or Clang 18+                                |
| Python       | 3.12+     | For CI tooling and DepManager               |
| Poetry       |           | Python dependency manager                   |
| DepManager   |           | C++ dependency manager (installed via Poetry)|

Install Python dependencies:

```bash
poetry sync --no-root
```

## Configure and Build

Owl uses CMake presets. To configure and build:

```bash
cmake --preset <preset> -S .
cmake --build output/build/<preset>
```

### Available Presets

#### Standard Presets

| Preset                   | OS      | Compiler | Config  |
|--------------------------|---------|----------|---------|
| `linux-gcc-release`      | Linux   | GCC      | Release |
| `linux-gcc-debug`        | Linux   | GCC      | Debug   |
| `linux-clang-release`    | Linux   | Clang    | Release |
| `linux-clang-debug`      | Linux   | Clang    | Debug   |
| `windows-gcc-release`    | Windows | MinGW GCC| Release |
| `windows-gcc-debug`      | Windows | MinGW GCC| Debug   |
| `windows-clang-release`  | Windows | MinGW Clang| Release |
| `windows-clang-debug`    | Windows | MinGW Clang| Debug   |

#### CI Presets

| Preset                                  | Purpose                        |
|-----------------------------------------|--------------------------------|
| `linux-clang-tidy`                      | Static analysis (clang-tidy)   |
| `windows-clang-tidy`                    | Static analysis (clang-tidy)   |
| `linux-sanitizer-address`               | AddressSanitizer               |
| `linux-sanitizer-thread`                | ThreadSanitizer                |
| `linux-sanitizer-undefined-behavior`    | UndefinedBehaviorSanitizer     |
| `linux-sanitizer-leak`                  | LeakSanitizer                  |

#### Packaging Presets

| Preset                        | Description                         |
|-------------------------------|-------------------------------------|
| `package-engine-linux`        | Package engine library for Linux    |
| `package-engine-windows`      | Package engine library for Windows  |
| `package-app-nest-linux`      | Package Owl Nest for Linux          |
| `package-app-nest-windows`    | Package Owl Nest for Windows        |
| `package-app-drone-linux`     | Package Owl Drone for Linux         |
| `package-app-drone-windows`   | Package Owl Drone for Windows       |

## Running Tests

Tests use Google Test and are enabled by default (`OWL_TESTING=ON`):

```bash
ctest --test-dir output/build/<preset> --output-on-failure
```

Test executables are named `owl_<category>_unit_test` with 13 categories: core, debug,
event, font, gui, input, layer, math, mesh, physic, renderer, scene, sound.

## CI System

The Python-based CI system wraps CMake operations. Always invoke through Poetry:

```bash
poetry run python ci_action.py Build <preset>
poetry run python ci_action.py Test <preset>
poetry run python ci_action.py Coverage <preset>
poetry run python ci_action.py Clean <preset>
poetry run python ci_action.py Documentation <preset>
```

## Build Output Locations

| Output     | Path                              |
|------------|-----------------------------------|
| Binaries   | `output/build/<preset>/bin/`      |
| Libraries  | `output/build/<preset>/lib/`      |
| Install    | `output/install/<preset>/`        |

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
| `OWL_ENABLE_ADDRESS_SANITIZER`            | OFF     | AddressSanitizer                               |
| `OWL_ENABLE_THREAD_SANITIZER`             | OFF     | ThreadSanitizer                                |
| `OWL_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER` | OFF     | UndefinedBehaviorSanitizer                     |
| `OWL_ENABLE_LEAK_SANITIZER`              | OFF     | LeakSanitizer                                  |
| `OWL_ENABLE_MEMORY_SANITIZER`             | OFF     | MemorySanitizer (Clang-only)                   |
| `OWL_ENABLE_DOCUMENTATION`                | OFF     | Enable Doxygen documentation generation        |
| `OWL_PACKAGING`                           | OFF     | Enable packaging mode                          |
