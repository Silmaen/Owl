# Building Owl {#page-building}

[TOC]

This page explains how to configure, build, and test the Owl engine.

## Prerequisites

### Software

| Tool       | Version | Notes                                         |
|------------|---------|-----------------------------------------------|
| CMake      | 3.24+   | Build system generator                        |
| Ninja      |         | Recommended build backend                     |
| Clang      | 22+     | Or GCC 14+                                    |
| Python     | 3.12+   | For CI tooling and DepManager                 |
| Poetry     |         | Python dependency manager                     |
| DepManager |         | C++ dependency manager (installed via Poetry) |

Install Python dependencies:

```bash
poetry sync --no-root
```

### Third-party packages

Owl pulls every native dependency through DepManager, which fetches pre-built packages from a
remote server during `cmake --preset`. Before the first configure you must:

1. Stand up a [DepManager server](https://github.com/Silmaen/DepManagerServer) (or point Owl at
   an existing one) and register it via `poetry run depmanager remote add ...`.
2. Populate that server with the libraries pinned in `depmanager.yml` (Box2D, EnTT, GLFW,
   ImGui, ImGuizmo, msdfgen, Vulkan SDK, OpenAL, libsndfile, Lua, zstd, Taskflow…).

If you don't have packages built yet, the [OwlDependencies](https://github.com/Silmaen/OwlDependencies)
repository ships ready-to-use build recipes for every dependency Owl depends on; build them locally
with `poetry run depmanager build <recipe-dir>` and push them to your server. Once the server is
populated, the next CMake configure auto-downloads everything Owl needs.

## Configure and Build

Owl uses CMake presets. To configure and build:

```bash
cmake --preset <preset> -S .
cmake --build output/build/<preset>
```

### Available Presets

#### Standard Presets

| Preset                  | OS      | Compiler    | Config  |
|-------------------------|---------|-------------|---------|
| `linux-gcc-release`     | Linux   | GCC         | Release |
| `linux-gcc-debug`       | Linux   | GCC         | Debug   |
| `linux-clang-release`   | Linux   | Clang       | Release |
| `linux-clang-debug`     | Linux   | Clang       | Debug   |
| `windows-gcc-release`   | Windows | MinGW GCC   | Release |
| `windows-gcc-debug`     | Windows | MinGW GCC   | Debug   |
| `windows-clang-release` | Windows | MinGW Clang | Release |
| `windows-clang-debug`   | Windows | MinGW Clang | Debug   |

#### CI Presets

| Preset                               | Purpose                      |
|--------------------------------------|------------------------------|
| `linux-clang-tidy`                   | Static analysis (clang-tidy) |
| `windows-clang-tidy`                 | Static analysis (clang-tidy) |
| `linux-sanitizer-address`            | AddressSanitizer             |
| `linux-sanitizer-thread`             | ThreadSanitizer              |
| `linux-sanitizer-undefined-behavior` | UndefinedBehaviorSanitizer   |
| `linux-sanitizer-leak`               | LeakSanitizer                |

#### Packaging Presets

| Preset                     | Description                        |
|----------------------------|------------------------------------|
| `package-engine-linux`     | Package engine library for Linux   |
| `package-engine-windows`   | Package engine library for Windows |
| `package-app-nest-linux`   | Package Owl Nest for Linux         |
| `package-app-nest-windows` | Package Owl Nest for Windows       |

## Running Tests

Tests use Google Test and are enabled by default (`OWL_TESTING=ON`):

```bash
ctest --test-dir output/build/<preset> --output-on-failure
```

Test executables are named `owl_<category>_unit_test` with 15 categories: core, debug,
event, font, gui, input, io, layer, math, mesh, physic, renderer, scene, script, sound.

## CI System

The Python-based CI system wraps CMake operations. Always invoke through Poetry:

```bash
poetry run python ci_action.py Build <preset>
poetry run python ci_action.py Test <preset>
poetry run python ci_action.py Coverage <preset>
poetry run python ci_action.py Clean <preset>
poetry run python ci_action.py Documentation <preset>
```

### Multi-architecture CI (ARM64 + x86_64)

CI agents running different architectures on a shared `$HOME` (or bind-mounted workspace) would
otherwise collide on a single Poetry venv path and load wheels compiled for the wrong arch —
observed as `ImportError: cryptography/_rust.abi3.so: cannot open shared object file` on ARM64
after an x86_64 run.

`ci_action.py` runs `ci.utils.venv.needs_refresh()` on every invocation (not gated on TeamCity
— TC's Docker jobs don't propagate `TEAMCITY_VERSION` into the container, so an env-var gate
silently skips the protection). The check uses three stacked signals, cheapest first:

1. **No venv yet** → nothing to refresh (`poetry sync` will create one).
2. **Platform signature matches** — `<arch>-<os>-<impl>-<pyver>` read from a marker file inside
   the venv, written by `cmake/Poetry.cmake` after each successful sync. Match → keep venv.
3. **Functional import test** — signature missing/mismatched, so `poetry run python -c "from
   cryptography.fernet import Fernet"` is run. If it fails, `OWL_CI_REFRESH_VENV=1` is exported;
   otherwise the venv is kept (covers legacy venvs that predate the marker).

`cmake/Poetry.cmake` consumes `OWL_CI_REFRESH_VENV` by running `poetry env remove --all` before
`poetry sync`, then re-writes the platform marker with the current signature. Happy-path
reruns pay a single file read; an arch switch pays the cost of one recreate (~30–60 s) and
self-heals.

To force a refresh manually (e.g. after a corrupt wheel): prepend `OWL_CI_REFRESH_VENV=1` to
the `poetry run python ci_action.py …` invocation, or to a direct `cmake --preset …` call.

## Build Output Locations

| Output    | Path                         |
|-----------|------------------------------|
| Binaries  | `output/build/<preset>/bin/` |
| Libraries | `output/build/<preset>/lib/` |
| Install   | `output/install/<preset>/`   |

## CMake Options

| Option                                    | Default | Description                                    |
|-------------------------------------------|---------|------------------------------------------------|
| `OWL_BUILD_SHARED`                        | ON      | Build engine as shared library                 |
| `OWL_BUILD_NEST`                          | ON      | Build Owl Nest editor                          |
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
| `OWL_ENABLE_LEAK_SANITIZER`               | OFF     | LeakSanitizer                                  |
| `OWL_ENABLE_MEMORY_SANITIZER`             | OFF     | MemorySanitizer (Clang-only)                   |
| `OWL_ENABLE_DOCUMENTATION`                | OFF     | Enable Doxygen documentation generation        |
| `OWL_PACKAGING`                           | OFF     | Enable packaging mode                          |
