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
| Python     | 3.12+   | For CI tooling and Conan                      |
| Poetry     |         | Python dependency manager                     |
| Conan      | 2       | C++ dependency manager (installed via Poetry) |

Install Python dependencies:

```bash
poetry sync --no-root
```

### Third-party packages

Every dependency comes from [Conan 2](https://conan.io) and public infrastructure: ConanCenter, plus the few
recipes kept in `conan/recipes/` (served as the local `owl-local` remote). No server to set up:

```bash
poetry sync --no-root   # installs Conan 2 (dev group)
cmake --preset linux-clang-release
cmake --build output/build/linux-clang-release
```

The configure step runs `conan install` on `conanfile.py` with the profile of the compiler
(`conan/profiles/<os>-<compiler>`) and the versioned lockfile `conan.lock`, into `output/build/<preset>/conan/`. The
first run of each compiler builds the packages from source (ConanCenter has no binaries for these compilers); later
runs reuse the Conan cache. The shared libraries from the cache are copied next to the binaries.
`-DOWL_CONAN_HOME=<dir>` selects a dedicated cache, `-DOWL_CONAN_PROFILE` another profile, `-DOWL_CONAN_LOCKFILE=`
(empty) resolves without the lockfile, and `OWL_CONAN_CACHE_URL` adds a binary cache server (the CI uses one).

The engine is also a Conan package, checked by `test_package/` (a program built on `find_package(OwlEngine)`);
publishing it is a v1.0.0 item, until then other projects use the packaged archive (`package-engine-*` presets):

```bash
poetry run conan create . --profile:all conan/profiles/linux-clang --lockfile conan.lock --lockfile-partial --build=missing
```

Preset status, lockfile update command and the local recipes: [Conan migration](design/conan-migration.md).

### Troubleshooting a fresh checkout

If `cmake --preset …` fails in `conan install`, the output above the error names the package. A package
`not resolved` usually means `conan.lock` and `conanfile.py` disagree: regenerate the lockfile (design page). A
build failure of a third party on a new compiler is fixed in its profile or in a local recipe, never by vendoring.
`-DOWL_CONAN_HOME=<empty dir>` reproduces a fresh agent.

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
| `linux-sanitizer-address`            | AddressSanitizer (+ LSan)    |
| `linux-sanitizer-thread`             | ThreadSanitizer              |
| `linux-sanitizer-undefined-behavior` | UndefinedBehaviorSanitizer   |
| `linux-include-check`                | Strict-libc++ include check  |

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

The Python-based CI system wraps CMake operations. The same Python actions are invoked locally
and from the TeamCity build steps — see [Continuous Integration](continuous_integration.md) for
the full TC pipeline, build matrix, and trigger architecture.

Always invoke through Poetry:

```bash
poetry run python ci_action.py Build <preset>
poetry run python ci_action.py Test <preset>
poetry run python ci_action.py Coverage <preset>
poetry run python ci_action.py Clean <preset>
poetry run python ci_action.py Documentation <preset>
poetry run python ci_action.py ClangTidy <preset>
```

`ClangTidy` needs the preset built first — it reads `compile_commands.json` and
ninja's dependency database. On a pull request it analyses only the translation
units the diff can affect (the touched `.cpp` files plus every `.cpp` that
includes a touched header); elsewhere, all of them. See
[Clang-tidy scoping](continuous_integration.md#clang-tidy-scoping).

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
3. **Functional import test** — signature missing/mismatched, so
   `poetry run python -c "from cryptography.fernet import Fernet"` is run. If it fails,
   `OWL_CI_REFRESH_VENV=1` is exported; otherwise the venv is kept (covers legacy venvs that
   predate the marker).

`cmake/Poetry.cmake` consumes `OWL_CI_REFRESH_VENV` by running `poetry env remove --all` before
`poetry sync`, then re-writes the platform marker with the current signature. Happy-path
reruns pay a single file read; an arch switch pays the cost of one recreate (~30–60 s) and
self-heals.

To force a refresh manually (e.g. after a corrupt wheel): prepend `OWL_CI_REFRESH_VENV=1` to
the `poetry run python ci_action.py …` invocation, or to a direct `cmake --preset …` call.

## Running under Wayland or X11

On Linux the editor and the runner use GLFW with both display backends. By default (`auto`) they use Wayland when
`WAYLAND_DISPLAY` is set and X11 otherwise; force one with the `OWL_WINDOW_PLATFORM` environment variable or the
`windowPlatform` key of the application's `config.yml` (the variable wins):

```bash
OWL_WINDOW_PLATFORM=wayland output/build/linux-clang-release/bin/OwlNest   # native Wayland
OWL_WINDOW_PLATFORM=x11 output/build/linux-clang-release/bin/OwlNest       # X11 / XWayland: detached editor windows
docker/run.sh --platform=x11 --gpu=nvidia output/build/linux-clang-release/bin/OwlRunner
```

- `docker/run.sh --gui` mounts the Wayland socket, the X11 socket and `/dev/dri`; `--gpu=intel|nvidia` pins Vulkan,
  EGL and GLX to one GPU of a hybrid laptop (`nvidia` turns PRIME render offload on), `--platform=` sets
  `OWL_WINDOW_PLATFORM`.
- Under Wayland the editor keeps detached panels inside its main window (GLFW cannot position windows); use X11 for
  native detached windows.
- Under Wayland the icon comes from a hidden desktop entry written to `~/.local/share/applications/<app_id>.desktop`
  (`installDesktopEntry: false` in `config.yml` to opt out).
- A locked or hidden session presents no frame: X11 windows that wait for presentation (vsync on, or any NVIDIA
  PRIME window) crawl at about one frame per second until shown again (`docker/run.sh` warns when the session is
  locked).

See [Windowing and input](design/windowing-input.md) for the details and the GLFW limits.

## Build Output Locations

| Output    | Path                         |
|-----------|------------------------------|
| Binaries  | `output/build/<preset>/bin/` |
| Libraries | `output/build/<preset>/lib/` |
| Install   | `output/install/<preset>/`   |

## CMake Options

| Option                                    | Default    | Description                                             |
|-------------------------------------------|------------|---------------------------------------------------------|
| `OWL_BUILD_SHARED`                        | ON         | Build engine as shared library                          |
| `OWL_BUILD_NEST`                          | ON         | Build Owl Nest editor                                   |
| `OWL_TESTING`                             | ON         | Enable unit tests                                       |
| `OWL_ENABLE_COVERAGE`                     | OFF        | Code coverage (auto-enabled in debug presets)           |
| `OWL_ENABLE_STACKTRACE`                   | OFF        | Memory tracker stacktrace (performance impact)          |
| `OWL_ENABLE_PROFILING`                    | OFF        | Profiling output                                        |
| `OWL_USE_RELEASE_THIRD_PARTY`             | ON         | Use release builds of third-party libraries             |
| `OWL_ENABLE_VULKAN_LAYERS`                | OFF        | Copy Vulkan layers to binary directory                  |
| `OWL_ENABLE_CLANG_TIDY`                   | OFF        | Enable clang-tidy static analysis                       |
| `OWL_ENABLE_ADDRESS_SANITIZER`            | OFF        | AddressSanitizer                                        |
| `OWL_ENABLE_THREAD_SANITIZER`             | OFF        | ThreadSanitizer                                         |
| `OWL_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER` | OFF        | UndefinedBehaviorSanitizer                              |
| `OWL_ENABLE_MEMORY_SANITIZER`             | OFF        | MemorySanitizer (Clang-only)                            |
| `OWL_ENABLE_DOCUMENTATION`                | OFF        | Enable Doxygen documentation generation                 |
| `OWL_PACKAGING`                           | OFF        | Enable packaging mode                                   |
| `OWL_CONAN_PROFILE`                       | (auto)     | Conan profile, default `conan/profiles/<os>-<compiler>` |
| `OWL_CONAN_HOME`                          | (empty)    | `CONAN_HOME` for the install (empty: Conan's default)   |
| `OWL_CONAN_BUILD`                         | missing    | Value of `conan install --build`                        |
| `OWL_FUZZING`                             | OFF        | libFuzzer targets in `fuzz/` (Clang-only)               |
