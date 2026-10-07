# Conan migration and dependencies {#page-design-conan-migration}

[TOC]

Design page for the v0.3.0 dependency work, summarised in the [Roadmap](../roadmap.md) and
[Foundations](foundations.md).

## Goal

Anyone can build Owl from public infrastructure. DepManager (packages served by the maintainer's server) is replaced by
**Conan 2**, preferring **ConanCenter** recipes; in-house recipes are avoided and listed honestly when they remain.
DepManager and `owl_engine.py` are removed in phase 0; consumers use the packaged archive (CPack) until v1.0.0
publishes the engine as a Conan package (`conan create .` already works, see below).

## Place in the release

This is **Phase 0** of v0.3.0 ([Foundations](foundations.md)): it runs first, before the correctness work, so the
riskiest items (missing recipes, versions absent from ConanCenter, breaking upgrades) surface early. The reduction of
public dependencies comes later, in phase D, once the chain is stable.

## Current state (2026-10-07)

Conan 2 is the only provider. Every preset builds and passes its tests on Conan in CI (Ubuntu 26.04 image, binaries
shared through the `owl-cache` server, table below); DepManager, `depmanager.yml`, `cmake/Depmanager.cmake`
and `owl_engine.py` are gone.

```bash
docker/run.sh cmake --preset <preset>
```

- `conanfile.py` (root) is both the dependency list and the OwlEngine package recipe. `cmake/Conan.cmake` runs
  `conan install` at configure time into
  `output/build/<preset>/conan/` (CMakeDeps files), then puts that folder on `CMAKE_PREFIX_PATH`.
- Conan 2 comes from Poetry (dev group). Options: `OWL_CONAN_PROFILE` (default `conan/profiles/<os>-<compiler>`),
  `OWL_CONAN_HOME` (a dedicated cache), `OWL_CONAN_BUILD` (`--build`, default `missing`), `OWL_CONAN_LOCKFILE`
  (default `conan.lock`, empty to resolve freely), `OWL_CONAN_INSTALL` (OFF when Conan drives CMake itself).
- The recipe options follow the CMake ones: `shared` (`OWL_BUILD_SHARED`), `testing` (`OWL_TESTING`, gtest as a
  test requirement) and `nest` (`OWL_BUILD_NEST`, imgui-color-text-edit and md4c).
- `owl_target_link_libraries()` stays the only entry point: it maps the two names that differ (`stb_image` →
  `stb::stb`, `TinyGLTF` → `TinyGLTF::TinyGLTF`) and builds the imgui backends; no other `CMakeLists.txt` changed.
- ConanCenter's imgui ships its backends and `imgui_stdlib` as sources (`res/bindings`, `res/misc/cpp`):
  `owl_imgui_bindings` compiles them (GLFW, OpenGL 2/3, Vulkan) with the headers staged under `backends/`. They are
  linked through `$<BUILD_INTERFACE:…>`: compiled into the engine, never exported.
- ConanCenter's lunasvg exports `include/` while its header sits in `include/lunasvg/`: the subdirectory is added
  (`OWL_CONAN_INCLUDE_SUBDIR_lunasvg`).
- ConanCenter has no binary for Clang 22 nor GCC 14: the first configure of each compiler builds the packages from
  source (about 50), later ones reuse the cache.

### Presets

State of the CI on 2026-10-07 (TeamCity, Ubuntu 26.04 image for Linux, MSYS2 MinGW64 with GCC 16.2 / Clang 22 on the
Windows agents):

| Configuration                            | Presets                                                          | State                                    |
|------------------------------------------|------------------------------------------------------------------|------------------------------------------|
| Build Linux x64 / Clang, GCC             | `linux-clang-debug` / `-release`, `linux-gcc-debug` / `-release` | Green, release + Doxygen on Clang        |
| Include Check                            | `linux-include-check`                                            | Green (strict libc++, no PCH)            |
| Clang-Tidy, Static Analyzer              | `linux-clang-tidy`                                               | Green; third-party sources not analysed  |
| Sanitizers Address, Thread, Undefined    | `linux-sanitizer-*`                                              | Green                                    |
| Build Linux arm64 / Clang, GCC (nightly) | `linux-*` on the arm64 agent                                     | Green once the dependency cache was warm |
| Build Windows x64 / Clang, GCC           | `windows-*`                                                      | Green                                    |
| Packages (nightly)                       | `package-engine-*`, `package-app-nest-*`                         | Ship the Conan shared libraries          |

### Profiles and build types

- `conan/profiles/linux-clang` and `conan/profiles/linux-gcc`: compiler detected from the toolchain, libstdc++,
  C++23, Ninja. The compiler picks the profile; there is no profile per build type or per sanitizer.
- Third parties are always Release (`OWL_USE_RELEASE_THIRD_PARTY`, ON by default), as with DepManager: a Debug preset
  maps them through `CMAKE_MAP_IMPORTED_CONFIG_DEBUG`, which now keeps an empty entry so CMake's configuration-less
  targets (FindOpenGL) still resolve. With the option OFF, `conan install` asks for the preset's build type.
- One binary set per compiler: Release, Debug, coverage, clang-tidy and the sanitizers of one compiler share it.

Every profile replaces the build tools the recipes ask in their own ranges by one version, the highest resolved
(`[replace_tool_requires]`: `cmake/4.4.3`, `pkgconf/2.5.1`), so each agent builds and caches one copy of each. CMake 4
refuses a `cmake_minimum_required` below 3.5, still declared by a few upstream projects: the profiles set
`CMAKE_POLICY_VERSION_MINIMUM=3.5` in `[buildenv]`.

### Sanitizers: dependencies not instrumented

Decision: the dependencies are **not** built with the sanitizers; only Owl's code is (`Owl_Base` flags).

- ASan, UBSan and LSan work with uninstrumented libraries: allocation interception covers every heap, and Owl's own
  code, where the bugs are looked for, is instrumented. This is what the DepManager presets already did.
- Instrumented dependencies would need a `compiler.sanitizer` settings extension and one full binary set per sanitizer
  (four times about 50 packages from source) for findings inside third-party code, outside Owl's scope.
- ThreadSanitizer is the exception to keep in mind: races inside an uninstrumented library (OpenAL's mixer thread,
  spdlog) go unseen and may show as false positives at the boundary; a suppression file is the remedy if it happens.
  MemorySanitizer (not a preset) would require instrumented dependencies and libc++: out of scope.

### Lockfile (`conan.lock`)

`conan.lock` pins every recipe revision (ConanCenter and local) for both Linux profiles; `cmake/Conan.cmake` passes it
to `conan install`, so a ConanCenter recipe update never changes a build silently. To update it after a change of
`conanfile.py` or of a local recipe, or to pick newer revisions:

```bash
docker/run.sh poetry run conan lock create . --profile:all conan/profiles/linux-clang -o "&:testing=True" -o "&:nest=True" --lockfile-out conan.lock
docker/run.sh poetry run conan lock create . --profile:all conan/profiles/linux-gcc -o "&:testing=True" -o "&:nest=True" --lockfile conan.lock --lockfile-out conan.lock
```

The first command re-resolves from scratch; the second adds what the GCC profile resolves differently. A new profile
(Windows, arm64) is added the same way, with `--lockfile conan.lock`. Run them with the same `CONAN_HOME` as the build
(`-DOWL_CONAN_HOME`), where `cmake/Conan.cmake` registered the `owl-local` remote. A local recipe's revision is a hash
of its content: regenerating without changing a recipe only moves its timestamp, a diff not worth committing.

### Binary cache (`owl-cache`)

ConanCenter has no binary for our compilers, so a cold agent builds every dependency (about 1 h 45 on Windows). An
optional Conan server serves as a shared binary cache:

- `OWL_CONAN_CACHE_URL` (CMake or environment) registers it as the `owl-cache` remote, after `owl-local` and before
  ConanCenter. Credentials come from Conan's own `CONAN_LOGIN_USERNAME_OWL_CACHE` / `CONAN_PASSWORD_OWL_CACHE`.
- At configure time `cmake/Conan.cmake` logs in and lists `zlib/*` on it; any failure disables the remote with a
  warning, and the install falls back to ConanCenter. Without a URL the remote is removed.
- With `OWL_CONAN_CACHE_UPLOAD=ON`, every binary of the local cache (latest package revisions) is uploaded (`conan list "*:*#latest"`, then
  `conan upload --list`), also after a failed install, so the packages built before the failure are kept; the
  server skips what it holds, and a failed upload is only a warning.
- TeamCity sets all four from the `conan_server`, `conan_user` and `conan_password` project parameters, so every CI
  build reads and fills the cache.

### Shared libraries next to the binaries

`target_import_so_files()` (`cmake/importSharedLibs.py`) copies every shared library that `ldd` resolves under the
Conan package cache (`<conan home>/p`, found with `conan config home`) next to the binary, as it does for DepManager's
`.edm` cache: `libOwlEngine.so`, `OwlNest` and `OwlRunner` find glfw, imgui, OpenAL Soft, libsndfile, the Vulkan loader
and Slang through `$ORIGIN`, and the `package-*` presets, which install `bin/`, ship them. Slang also loads files that
`ldd` cannot see (`libslang-glslang-*.so`, `libslang-glsl-module-*.so`, `slang-standard-module-*/`): they are copied
with it.

### OwlEngine package (`conan create`)

```bash
docker/run.sh poetry run conan create . --profile:all conan/profiles/linux-clang --lockfile conan.lock --lockfile-partial --build=missing
```

- `owlengine/<version>`, version read from `project(VERSION)` in `CMakeLists.txt` (single source, as in
  `owl_engine.py` and `version_test.cpp` now). Engine only: no editor, no tests, `OWL_PACKAGING` +
  `OWL_PACKAGE_ENGINE`, Conan drives CMake (`OWL_CONAN_INSTALL=OFF`, `conan_toolchain.cmake`).
- Consumers use the CMake config the engine installs (`lib/cmake/OwlEngine/`, `Owl::OwlEngine`, the one OwlDrone and
  the CPack archives use): the recipe sets `cmake_find_mode` to `none`, so `test_package` checks that file and not a
  Conan-generated one.
- `test_package/`: `find_package(OwlEngine CONFIG REQUIRED)`, a program including `<owl.h>`, linked to
  `Owl::OwlEngine`, compiled without any Owl flag and run (audit G-03).
- Install tree fixes it needed (audit PR-08): headers installed under `include/` and not `include/public/` (G-02);
  `Owl_Base` (`-Werror -Weverything`, internal definitions) linked as `$<BUILD_INTERFACE:…>` and no longer installed,
  the export keeping only `OWL_BUILD_SHARED`, `OWL_PLATFORM_<OS>` and `cxx_std_23` (G-01); `CMAKE_INSTALL_PREFIX` of
  the presets no longer overridden (G-06); `core/external/spdlog.h` moved to `private/` (A-19); version read from
  `CMakeLists.txt` by both recipes and the version test (G-19, F-06).
- yaml-cpp becomes a public dependency (`find_dependency(yaml-cpp)`, `transitive_headers`): `renderer/RenderLayer.h`
  includes it and `<owl.h>` reaches it. It leaves the public headers with PR-27 (G-07).
- Shared only: a static OwlEngine exports `OwlEnginePrivate` and every private dependency; the recipe refuses
  `shared=False` until that is designed (static consumers through CMakeDeps, or the private dependencies exported).
- `libOwlEngine.so` keeps `$ORIGIN` as its only RPATH, so GNU ld finds its own shared dependencies (glfw, OpenAL Soft,
  libsndfile, Slang, Vulkan loader) only through `LD_LIBRARY_PATH`: `test_package` builds in Conan's run environment.
  A Conan consumer does the same (or links with lld, which does not resolve them); to revisit with PR-27.
- Not wired into the `Package` CI action nor published to a remote: that is v1.0.0 (PR-09 remainder).

### Windows (MinGW)

The Windows configurations moved to Conan first: the agent's MSYS2 moved to GCC 16 and the prebuilt DepManager packages
no longer matched it (GCC tests died at load time with `0xc0000139`, the Clang link hit duplicate `std::__unicode`
symbols). With Conan every dependency is built by the agent's own compiler.

- Profiles `conan/profiles/windows-clang` and `windows-gcc` (MSYS2 MinGW64, libstdc++, C++23), picked by
  `cmake/Conan.cmake` from the compiler of the `windows-*` presets. `windows-gcc` names MinGW's `gcc.exe` by full
  path: in the MSYS2 bash of autotools recipes a bare `gcc` is MSYS's own POSIX compiler.
- Host settings and options go through a generated profile (cmd.exe splits `&:shared=…` at `&`).
- `slang` recipe: the MSVC import library is also shipped as `libslang.dll.a`, the name MinGW linkers search.
- `libmp3lame` local recipe: MinGW Clang builds with autotools (ConanCenter takes every Windows Clang for clang-cl).
- ImGuizmo and plutovg are shared, and the imgui backends and `imgui_stdlib` compiled into the engine drop their API
  macro: a shared imgui declares them `dllimport`.
- Shared libraries: Windows has no RPATH, so `conan install` runs the `runtime_deploy` deployer into `<build>/bin`.
- Lockfile: `conan.lock` is resolved with the Linux profiles; on Windows `--lockfile-partial` lets the Windows-only
  requirements resolve. Extend the lockfile from a Windows agent before dropping `--lockfile-partial`.

## Inventory

Checked on 2026-10-05 with `conan search -r conancenter` (Conan 2.33). "Owl (Conan)" is the version Owl uses;
"DepManager" the one `depmanager.yml` pinned when it was removed.

| Dependency            | DepManager     | Owl (Conan)     | Source                | Linkage | Note                                                                           |
|-----------------------|----------------|-----------------|-----------------------|---------|--------------------------------------------------------------------------------|
| box2d                 | 3.1.1          | 3.1.1           | ConanCenter           | static  | Latest on ConanCenter                                                          |
| cpptrace              | 1.0.4          | 1.0.4           | ConanCenter           | static  | Pulls libdwarf 2.1.0 (DepManager: 2.2.0)                                       |
| debugbreak            | 1.0            | —               | removed               | —       | Replaced by `OWL_DEBUG_BREAK()` in `core/Assert.h`                             |
| entt                  | 3.15.0         | 4.0.0           | local recipe          | header  | ConanCenter stops at 3.16.0; its recipe, C++20 for 4.x                         |
| freetype              | 2.13.3         | 2.13.2          | ConanCenter           | static  | Transitive, pinned by msdfgen 1.12 (2.14.3 on ConanCenter)                     |
| glad                  | 2.0.4          | 2.0.8           | ConanCenter           | static  | Generated at build time: GL 4.6 compatibility                                  |
| glfw                  | 3.4.0          | 3.4             | ConanCenter           | shared  |                                                                                |
| googletest            | 1.17.0         | 1.18.0          | ConanCenter (`gtest`) | static  |                                                                                |
| imgui                 | 1.92.7-docking | 1.92.9b-docking | ConanCenter           | shared  | Backends compiled by Owl (see above)                                           |
| imgui_color_text_edit | 1.92.7         | cci.20260417    | local recipe          | static  | Absent; DepManager's commit (v1.92.9 rewrote the cursor API)                   |
| imguizmo              | 1.92.7         | 1.10            | local recipe          | static  | ConanCenter's cci.20231114 does not build with imgui 1.92                      |
| libpng                | 1.6.50         | 1.6.58          | ConanCenter           | static  | Transitive (msdf-atlas-gen)                                                    |
| libsndfile            | 1.2.2          | 1.2.2           | ConanCenter           | shared  | LGPL: kept shared                                                              |
| lua                   | 5.5.0          | 5.5.0           | ConanCenter           | static  |                                                                                |
| lunasvg               | 3.5.0          | 3.5.0           | ConanCenter           | static  | Pulls plutovg 1.3.2                                                            |
| magic_enum            | 0.9.7          | 0.9.8           | ConanCenter           | header  |                                                                                |
| md4c                  | 0.5.2          | 0.5.2           | ConanCenter           | static  |                                                                                |
| msdf-atlas-gen        | 1.3            | 1.4             | local recipe          | static  | ConanCenter packages the tool only; this one builds the library                |
| msdfgen               | 1.12.1         | 1.13            | local recipe          | static  | ConanCenter stops at 1.12: its recipe, unchanged                               |
| nfd (extended)        | 1.2.1          | 1.4.1           | local recipe          | static  | Absent from ConanCenter; GTK 3 + wayland-client (system)                       |
| openal                | 1.24.3         | 1.25.2          | local recipe          | shared  | `openal-soft`, ConanCenter stops at 1.24.3; LGPL: shared                       |
| slang                 | (Vulkan SDK)   | 2026.19         | local recipe          | shared  | Upstream release binaries (DepManager: 2026.1)                                 |
| spdlog                | 1.16.0         | 1.17.0          | ConanCenter           | static  | `use_std_fmt`: no fmt dependency                                               |
| spirv-cross           | (Vulkan SDK)   | 1.4.357.0       | ConanCenter           | static  | Only the core/glsl/cpp/reflect components                                      |
| stb_image             | 2.28           | cci.20240531    | ConanCenter (`stb`)   | header  |                                                                                |
| taskflow              | 4.0.0          | 4.1.0           | local recipe          | header  | ConanCenter stops at 4.0.0; its recipe, unchanged                              |
| tinygltf              | 2.9.6          | 2.9.7           | ConanCenter           | header  | Pulls nlohmann_json                                                            |
| tinyobjloader         | 2.0.0-rc13     | 2.0.0-rc13      | local recipe          | static  | ConanCenter stops at rc10: its recipe, without the rc10 patch (upstream since) |
| tinyxml2              | 11.0.0         | —               | removed               | —       | Declared, never used (10.0.0 still comes in through msdfgen-ext)               |
| ufbx                  | 0.20.1         | 0.23.1          | local recipe          | static  | Absent from ConanCenter (single source file)                                   |
| vulkan                | 1.4.341 (SDK)  | 1.4.357.0       | ConanCenter           | shared  | vulkan-headers, vulkan-loader, vulkan-utility-libraries                        |
| yaml-cpp              | 0.8.0          | 0.9.0           | ConanCenter           | static  |                                                                                |
| zeus                  | 1.3.1          | —               | removed               | —       | Dead fallback branch of `core/expected.h`                                      |
| zlib                  | 1.3.1          | 1.3.2           | ConanCenter           | static  | Transitive                                                                     |
| zstd                  | 1.5.7          | 1.5.7           | ConanCenter           | static  |                                                                                |

## Local recipes (`conan/recipes/`)

Served as the `owl-local` remote (`local-recipes-index`, the ConanCenter layout: `<name>/config.yml` +
`<name>/all/conanfile.py` + `conandata.yml`), registered first (it wins over ConanCenter for its names) and
refreshed (`--update=<name>`) by `cmake/Conan.cmake`. Each downloads the
public upstream archive (sha256 pinned) and adds at most a few-line `CMakeLists.txt`, so each can be proposed to
ConanCenter as is.

| Recipe                               | Why it is local                                                                                                 |
|--------------------------------------|-----------------------------------------------------------------------------------------------------------------|
| `slang` 2026.19                      | Absent; building it pulls LLVM-sized dependencies, so the recipe repackages the upstream release binaries       |
| `ufbx` 0.23.1                        | Absent; one `.c` file, upstream ships no build system                                                           |
| `imgui-color-text-edit` cci.20260417 | Absent (goossens fork); pinned to the DepManager commit until the v1.92.9 cursor API is adopted                 |
| `imguizmo` 1.10                      | ConanCenter's only recent version (cci.20231114) calls `BeginChildFrame`, removed in imgui 1.92                 |
| `nativefiledialog-extended` 1.4.1    | Absent; the GitHub archive lacks the `wayland-protocols` submodule, fetched as a second source                  |
| `msdf-atlas-gen` 1.4                 | ConanCenter's recipe packages the command-line tool only; this one builds the library on ConanCenter's msdfgen  |
| `libmp3lame` 3.100                   | ConanCenter's recipe, except MinGW Clang builds with autotools: upstream takes every Windows Clang for clang-cl |
| `entt` 4.0.0                         | ConanCenter stops at 3.16.0: its recipe, requiring C++20 from 4.x                                               |
| `taskflow` 4.1.0                     | ConanCenter stops at 4.0.0: its recipe, unchanged                                                               |
| `openal-soft` 1.25.2                 | ConanCenter stops at 1.24.3: its recipe, `-Werror=function-effects` off (libstdc++ is not `nonblocking`)        |
| `msdfgen` 1.13                       | ConanCenter stops at 1.12 (needed by msdf-atlas-gen 1.4): its recipe, unchanged                                 |
| `tinyobjloader` 2.0.0-rc13           | ConanCenter stops at rc10: its recipe, unchanged                                                                |

## What remains

Phase 0, before anything else of v0.3.0:

- EnTT 4.0.0 and Taskflow 4.1.0 are in, through local recipes until ConanCenter has them
- The lagging direct versions (G-08) are in too, through local recipes: OpenAL Soft 1.25.2, msdfgen 1.13,
  msdf-atlas-gen 1.4, tinyobjloader rc13. Left: a lockfile update report in CI (`conan graph outdated`, whose
  `cci.*` hits are false positives); the transitive versions follow their recipes

Later:

- Vulkan validation layers (`OWL_ENABLE_VULKAN_LAYERS`) from `vulkan-validationlayers`
- Propose the twelve local recipes (or their new versions) to ConanCenter, msdf-atlas-gen as a library option, the
  libmp3lame clang-cl fix
- imgui-color-text-edit v1.92.9: port `CodeEditorDocument` to the `DocPos` cursor API, then bump the recipe
- v1.0.0: the OwlEngine Conan package published and run by the `Package` action (PR-09), a static variant, YAML
  out of the public headers (PR-27)

## Fewer public dependencies (phase D)

Target: `find_package(OwlEngine)` exposes **EnTT** only, plus **imgui** through the optional `Owl::Gui` target.

- spdlog behind an `owl::log` facade
- yaml-cpp out of the public headers (A-03, G-07); rapidyaml as a private replacement to evaluate (fiche 5)
- To evaluate: miniaudio instead of OpenAL Soft + libsndfile; a single image loader instead of stb_image + libpng
