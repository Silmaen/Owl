# Conan migration and dependencies {#page-design-conan-migration}

[TOC]

Design page for the v0.3.0 dependency work, summarised in the [Roadmap](../roadmap.md) and
[Foundations](foundations.md).

## Goal

Anyone can build Owl from public infrastructure. DepManager (packages served by the maintainer's server) is replaced by
**Conan 2**, preferring **ConanCenter** recipes; in-house recipes are avoided and listed honestly when they remain.
The engine itself is published as a Conan package. DepManager and `owl_engine.py` are removed at the end of the cycle.

## Place in the release

This is **Phase 0** of v0.3.0 ([Foundations](foundations.md)): it runs first, before the correctness work, so the
riskiest items (missing recipes, versions absent from ConanCenter, breaking upgrades) surface early. The reduction of
public dependencies comes later, in phase D, once the chain is stable.

## Current state (2026-10-05)

Every Linux preset configures and builds with every dependency from Conan 2, and passes its tests (UBSan: renderer suite
times out at 3600 s; table below).
DepManager stays the default provider: the switch is the maintainer's decision.

```bash
docker/run.sh cmake --preset <preset> -DOWL_DEPENDENCY_PROVIDER=conan
```

- `OWL_DEPENDENCY_PROVIDER` (`depmanager` by default, or `conan`) picks the provider in `cmake/BaseConfig.cmake`.
- `conanfile.py` (root) is both the dependency list and the OwlEngine package recipe. `cmake/Conan.cmake` runs
  `conan install` at configure time, as `cmake/Depmanager.cmake` loads its environment, into
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

### Presets in Conan mode

Measured on 2026-10-05 in the build image (`docker/run.sh`, Clang 22.1, GCC 14.2), with a dedicated Conan cache.

| Preset                               | Configure | Build | Tests                               | Note                                                         |
|--------------------------------------|-----------|-------|-------------------------------------|--------------------------------------------------------------|
| `linux-clang-release`                | yes       | yes   | 16 / 16                             | Reference                                                    |
| `linux-clang-debug`                  | yes       | yes   | 16 / 16                             | Coverage flags on                                            |
| `linux-gcc-release`                  | yes       | yes   | 16 / 16                             | `linux-gcc` profile, packages built once from source         |
| `linux-gcc-debug`                    | yes       | yes   | 16 / 16                             | Coverage flags on                                            |
| `linux-clang-tidy`                   | yes       | yes   | — (no tests)                        | clang-tidy clean; the imgui backends are not analysed        |
| `linux-sanitizer-address`            | yes       | yes   | 16 / 16                             |                                                              |
| `linux-sanitizer-leak`               | yes       | yes   | 16 / 16                             |                                                              |
| `linux-sanitizer-thread`             | yes       | yes   | 16 / 16 with `docker/run.sh --perf` | TSan must disable ASLR: needs `seccomp=unconfined`, as in CI |
| `linux-sanitizer-undefined-behavior` | yes       | yes   | 15 / 16, renderer timeout           | Renderer suite > 3600 s under UBSan; no UBSan report         |
| `package-app-nest-linux`             | yes       | yes   | CPack archive                       | Ships the Conan `.so` and the Slang modules                  |
| `package-engine-linux`               | —         | —     | —                                   | Not run; `conan create` covers the engine package            |
| `windows-*`, `windows-clang-tidy`    | —         | —     | —                                   | Prepared (profiles, graph resolves); first agent run pending |
| Linux arm64                          | —         | —     | —                                   | Not run here (x86_64 host)                                   |

### Profiles and build types

- `conan/profiles/linux-clang` and `conan/profiles/linux-gcc`: compiler detected from the toolchain, libstdc++,
  C++23, Ninja. The compiler picks the profile; there is no profile per build type or per sanitizer.
- Third parties are always Release (`OWL_USE_RELEASE_THIRD_PARTY`, ON by default), as with DepManager: a Debug preset
  maps them through `CMAKE_MAP_IMPORTED_CONFIG_DEBUG`, which now keeps an empty entry so CMake's configuration-less
  targets (FindOpenGL) still resolve. With the option OFF, `conan install` asks for the preset's build type.
- One binary set per compiler: Release, Debug, coverage, clang-tidy and the sanitizers of one compiler share it.

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
- Not yet wired into the `Package` CI action (PR-09 remainder), nor published to a remote.

### Windows (MinGW)

Prepared, not yet run on a Windows agent (the build image has no MinGW toolchain):

- Profiles `conan/profiles/windows-clang` and `windows-gcc` (MSYS2 MinGW64, libstdc++, C++23), picked by
  `cmake/Conan.cmake` from the compiler of the `windows-*` presets. `conan graph info` resolves the whole graph with
  both profiles (68 and 70 packages, no invalid configuration), checked from Linux with `-pr:b linux-clang`.
- `slang` recipe: the MSVC import library is also shipped as `libslang.dll.a`, the name MinGW linkers search; both
  `lld` and GNU `ld` read the MSVC short import format.
- Shared libraries: Windows has no RPATH, so `conan install` runs the `runtime_deploy` deployer into
  `<build>/bin`, next to the executables.
- Lockfile: `conan.lock` is resolved with the Linux profiles; on Windows `--lockfile-partial` lets the Windows-only
  requirements (e.g. `jwasm` as a build requirement with GCC) resolve. Extend the lockfile from a Windows agent
  (`conan lock create … --lockfile conan.lock --lockfile-out conan.lock`) before dropping `--lockfile-partial`.
- Trying it: every TeamCity configuration exposes `env.OWL_DEPENDENCY_PROVIDER` (default `depmanager`); a custom run
  with `conan` on *Windows x64 / Clang* exercises the whole chain on the agent. Locally:
  `OWL_DEPENDENCY_PROVIDER=conan cmake --preset windows-clang-debug`.
- Expected first-run cost: most ConanCenter packages have no MinGW binary and build from source (`--build=missing`),
  then stay in the agent's Conan cache.
- This also fixes the duplicate `std::__unicode` symbols seen with DepManager's prebuilt `spdlog` (built by another
  toolchain): with Conan, every dependency is built by the agent's own compiler.

Remaining risks, to check on the first agent run: local recipes on Windows (`nativefiledialog-extended` Win32 branch,
`ufbx`, `msdf-atlas-gen`), Slang's runtime modules next to `slang.dll`, and `windres` for GLFW resources.

## Inventory

Checked on 2026-10-05 with `conan search -r conancenter` (Conan 2.33). "Owl (Conan)" is the version the Conan build
uses; "DepManager" the one still pinned in `depmanager.yml`.

| Dependency            | DepManager     | Owl (Conan)     | Source                | Linkage | Note                                                             |
|-----------------------|----------------|-----------------|-----------------------|---------|------------------------------------------------------------------|
| box2d                 | 3.1.1          | 3.1.1           | ConanCenter           | static  | Latest on ConanCenter                                            |
| cpptrace              | 1.0.4          | 1.0.4           | ConanCenter           | static  | Pulls libdwarf 2.1.0 (DepManager: 2.2.0)                         |
| debugbreak            | 1.0            | —               | removed               | —       | Replaced by `OWL_DEBUG_BREAK()` in `core/Assert.h`               |
| entt                  | 3.15.0         | 3.16.0          | ConanCenter           | header  | EnTT 4.0.0 not on ConanCenter yet                                |
| freetype              | 2.13.3         | 2.13.2          | ConanCenter           | static  | Transitive, pinned by msdfgen 1.12 (2.14.3 on ConanCenter)       |
| glad                  | 2.0.4          | 2.0.8           | ConanCenter           | static  | Generated at build time: GL 4.6 compatibility                    |
| glfw                  | 3.4.0          | 3.4             | ConanCenter           | shared  |                                                                  |
| googletest            | 1.17.0         | 1.18.0          | ConanCenter (`gtest`) | static  |                                                                  |
| imgui                 | 1.92.7-docking | 1.92.9b-docking | ConanCenter           | shared  | Backends compiled by Owl (see above)                             |
| imgui_color_text_edit | 1.92.7         | cci.20260417    | local recipe          | static  | Absent; DepManager's commit (v1.92.9 rewrote the cursor API)     |
| imguizmo              | 1.92.7         | 1.10            | local recipe          | static  | ConanCenter's cci.20231114 does not build with imgui 1.92        |
| libpng                | 1.6.50         | 1.6.58          | ConanCenter           | static  | Transitive (msdf-atlas-gen)                                      |
| libsndfile            | 1.2.2          | 1.2.2           | ConanCenter           | shared  | LGPL: kept shared                                                |
| lua                   | 5.5.0          | 5.5.0           | ConanCenter           | static  |                                                                  |
| lunasvg               | 3.5.0          | 3.5.0           | ConanCenter           | static  | Pulls plutovg 1.3.2                                              |
| magic_enum            | 0.9.7          | 0.9.8           | ConanCenter           | header  |                                                                  |
| md4c                  | 0.5.2          | 0.5.2           | ConanCenter           | static  |                                                                  |
| msdf-atlas-gen        | 1.3            | 1.3             | local recipe          | static  | ConanCenter packages the tool only; 1.4 upstream                 |
| msdfgen               | 1.12.1         | 1.12            | ConanCenter           | static  | Pinned by msdf-atlas-gen 1.3; 1.13 not on ConanCenter            |
| nfd (extended)        | 1.2.1          | 1.4.1           | local recipe          | static  | Absent from ConanCenter; GTK 3 + wayland-client (system)         |
| openal                | 1.24.3         | 1.24.3          | ConanCenter           | shared  | `openal-soft`; 1.25 not on ConanCenter; LGPL: shared             |
| slang                 | (Vulkan SDK)   | 2026.19         | local recipe          | shared  | Upstream release binaries (DepManager: 2026.1)                   |
| spdlog                | 1.16.0         | 1.17.0          | ConanCenter           | static  | `use_std_fmt`: no fmt dependency                                 |
| spirv-cross           | (Vulkan SDK)   | 1.4.357.0       | ConanCenter           | static  | Only the core/glsl/cpp/reflect components                        |
| stb_image             | 2.28           | cci.20240531    | ConanCenter (`stb`)   | header  |                                                                  |
| taskflow              | 4.0.0          | 4.0.0           | ConanCenter           | header  | 4.1.0 not on ConanCenter yet                                     |
| tinygltf              | 2.9.6          | 2.9.7           | ConanCenter           | header  | Pulls nlohmann_json                                              |
| tinyobjloader         | 2.0.0-rc13     | 2.0.0-rc10      | ConanCenter           | static  | Step back to rc10, enough for `MeshLoader`                       |
| tinyxml2              | 11.0.0         | —               | removed               | —       | Declared, never used (10.0.0 still comes in through msdfgen-ext) |
| ufbx                  | 0.20.1         | 0.23.1          | local recipe          | static  | Absent from ConanCenter (single source file)                     |
| vulkan                | 1.4.341 (SDK)  | 1.4.357.0       | ConanCenter           | shared  | vulkan-headers, vulkan-loader, vulkan-utility-libraries          |
| yaml-cpp              | 0.8.0          | 0.9.0           | ConanCenter           | static  |                                                                  |
| zeus                  | 1.3.1          | —               | removed               | —       | Dead fallback branch of `core/expected.h`                        |
| zlib                  | 1.3.1          | 1.3.2           | ConanCenter           | static  | Transitive                                                       |
| zstd                  | 1.5.7          | 1.5.7           | ConanCenter           | static  |                                                                  |

## Local recipes (`conan/recipes/`)

Served as the `owl-local` remote (`local-recipes-index`, the ConanCenter layout: `<name>/config.yml` +
`<name>/all/conanfile.py` + `conandata.yml`), registered first (it wins over ConanCenter for its names) and
refreshed (`--update=<name>`) by `cmake/Conan.cmake`. Each downloads the
public upstream archive (sha256 pinned) and adds at most a few-line `CMakeLists.txt`, so each can be proposed to
ConanCenter as is.

| Recipe                               | Why it is local                                                                                                |
|--------------------------------------|----------------------------------------------------------------------------------------------------------------|
| `slang` 2026.19                      | Absent; building it pulls LLVM-sized dependencies, so the recipe repackages the upstream release binaries      |
| `ufbx` 0.23.1                        | Absent; one `.c` file, upstream ships no build system                                                          |
| `imgui-color-text-edit` cci.20260417 | Absent (goossens fork); pinned to the DepManager commit until the v1.92.9 cursor API is adopted                |
| `imguizmo` 1.10                      | ConanCenter's only recent version (cci.20231114) calls `BeginChildFrame`, removed in imgui 1.92                |
| `nativefiledialog-extended` 1.4.1    | Absent; the GitHub archive lacks the `wayland-protocols` submodule, fetched as a second source                 |
| `msdf-atlas-gen` 1.3                 | ConanCenter's recipe packages the command-line tool only; this one builds the library on ConanCenter's msdfgen |

## What remains

Before the switch of the default:

- Windows MinGW presets on Conan (see above), on the Windows agents
- Linux arm64 (`linux-*` presets on the arm64 agent): the profiles detect the architecture, Slang has an arm64
  binary; to run once
- CI: the TeamCity configurations pass `-DOWL_DEPENDENCY_PROVIDER=conan` and keep a Conan cache per agent, the
  `Package` action runs `conan create` (PR-09), a lockfile update report (G-08)
- Vulkan validation layers (`OWL_ENABLE_VULKAN_LAYERS`) from `vulkan-validationlayers`

After it:

- DepManager, `depmanager.yml`, `cmake/Depmanager.cmake` and `owl_engine.py` removed
- Versions not on ConanCenter yet: EnTT 4.0.0, Taskflow 4.1.0, OpenAL Soft 1.25, msdfgen 1.13, msdf-atlas-gen 1.4,
  tinyobjloader rc13 — contribute them upstream (or bump the local recipes) rather than adding recipes
- Breaking upgrade EnTT 4 (before the open component registry, PR-37)
- Propose the six local recipes (or their new versions) to ConanCenter, msdf-atlas-gen as a library option
- imgui-color-text-edit v1.92.9: port `CodeEditorDocument` to the `DocPos` cursor API, then bump both providers
- Static OwlEngine package; YAML out of the public headers (PR-27)

## Fewer public dependencies (phase D)

Target: `find_package(OwlEngine)` exposes **EnTT** only, plus **imgui** through the optional `Owl::Gui` target.

- spdlog behind an `owl::log` facade
- yaml-cpp out of the public headers (A-03, G-07); rapidyaml as a private replacement to evaluate (fiche 5)
- To evaluate: miniaudio instead of OpenAL Soft + libsndfile; a single image loader instead of stb_image + libpng
