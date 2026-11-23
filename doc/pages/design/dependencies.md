# Dependencies (Conan 2) {#page-design-dependencies}

[TOC]

Every third-party library comes from **Conan 2**, ConanCenter first; the few in-house recipes are listed below. User
side: [Building](../building.md).

## How Conan is wired

```bash
./docker_run.sh cmake --preset <preset>
```

- `conanfile.py` (root) is both the dependency list and the OwlEngine package recipe. `cmake/Conan.cmake` runs
  `conan install` at configure time into
  `output/build/<preset>/conan/` (CMakeDeps files), then puts that folder on `CMAKE_PREFIX_PATH`.
- Conan 2 comes from Poetry (dev group). Options: `OWL_CONAN_PROFILE` (default `conan/profiles/<os>-<compiler>`),
  `OWL_CONAN_HOME` (a dedicated cache), `OWL_CONAN_BUILD` (`--build`, default `missing`), `OWL_CONAN_LOCKFILE`
  (default `conan.lock`, empty to resolve freely), `OWL_CONAN_INSTALL` (OFF when Conan drives CMake itself).
- The recipe options follow the CMake ones: `shared` (`OWL_BUILD_SHARED`), `testing` (`OWL_TESTING`, gtest as a
  test requirement), `nest` (`OWL_BUILD_NEST`, the editor-only packages), `tracy` (`OWL_PROFILER=tracy`, the Tracy
  client built `on_demand`, see [Profiling](../profiling.md)) and one per engine module (`render`, `physics`, `audio`,
  `script`, `gui`, the `OWL_MODULE_*` options).
- `owl_target_link_libraries()` stays the only entry point: it maps the two names that differ (`stb_image` →
  `stb::stb`, `TinyGLTF` → `TinyGLTF::TinyGLTF`) and builds the imgui backends.
- ConanCenter's imgui ships its backends and `imgui_stdlib` as sources (`res/bindings`, `res/misc/cpp`):
  `owl_imgui_bindings` compiles them (GLFW, OpenGL 2/3, Vulkan) with the headers staged under `backends/`. They are
  linked through `$<BUILD_INTERFACE:…>`: compiled into the engine, never exported.
- ConanCenter's lunasvg exports `include/` while its header sits in `include/lunasvg/`: the subdirectory is added
  (`OWL_CONAN_INCLUDE_SUBDIR_lunasvg`).
- ConanCenter has no binary for the image's compilers: the first configure of each compiler builds the packages from
  source (about 50), later ones reuse the cache or the binary cache below.

### Profiles and build types

- `conan/profiles/linux-clang` and `conan/profiles/linux-gcc` (`linux-clang-armv8` for the arm64 cross build):
  compiler detected from the toolchain, libstdc++, C++23, Ninja. The compiler picks the profile; there is no profile per build type or per sanitizer.
- Third parties are always Release (`OWL_USE_RELEASE_THIRD_PARTY`, ON by default): a Debug preset
  maps them through `CMAKE_MAP_IMPORTED_CONFIG_DEBUG`, which keeps an empty entry so CMake's configuration-less
  targets (FindOpenGL) still resolve. With the option OFF, `conan install` asks for the preset's build type.
- One binary set per compiler: Release, Debug, coverage, clang-tidy and the sanitizers of one compiler share it.

Every profile replaces the build tools the recipes ask in their own ranges by one version, the highest resolved
(`[replace_tool_requires]`: `cmake/4.4.4`, `pkgconf/2.5.1`), so each agent builds and caches one copy of each. CMake 4
refuses a `cmake_minimum_required` below 3.5, still declared by a few upstream projects: the profiles set
`CMAKE_POLICY_VERSION_MINIMUM=3.5` in `[buildenv]`.

### Sanitizers: dependencies not instrumented

The dependencies are **not** built with the sanitizers; only Owl Engine's code is (`Owl_Base` flags).

- ASan, UBSan and LSan work with uninstrumented libraries: allocation interception covers every heap, and Owl Engine's own
  code, where the bugs are looked for, is instrumented.
- Instrumented dependencies would need a `compiler.sanitizer` settings extension and one full binary set per sanitizer
  (four times about 50 packages from source) for findings inside third-party code, outside Owl Engine's scope.
- ThreadSanitizer is the exception to keep in mind: races inside an uninstrumented library (OpenAL's mixer thread,
  spdlog) go unseen and may show as false positives at the boundary; a suppression file is the remedy.
  MemorySanitizer (not a preset) would require instrumented dependencies and libc++: out of scope.

### Lockfile (`conan.lock`)

`conan.lock` pins every recipe revision (ConanCenter and local) for both Linux profiles; `cmake/Conan.cmake` passes it
to `conan install`, so a ConanCenter recipe update never changes a build silently. To update it after a change of
`conanfile.py` or of a local recipe, or to pick newer revisions:

```bash
./docker_run.sh poetry run conan lock create . --profile:all conan/profiles/linux-clang -o "&:testing=True" -o "&:nest=True" -o "&:tracy=True" --lockfile="" --lockfile-out conan.lock --update
./docker_run.sh poetry run conan lock create . --profile:all conan/profiles/linux-gcc -o "&:testing=True" -o "&:nest=True" -o "&:tracy=True" --lockfile conan.lock --lockfile-out conan.lock
```

The first command re-resolves from scratch (`--lockfile=""`: Conan otherwise reads the `conan.lock` of the current folder; `--update`: the newest version of each range on the remotes, not the cached one); the second adds what the GCC profile resolves differently. A new profile
(Windows, arm64) is added the same way, with `--lockfile conan.lock`. Run them with the same `CONAN_HOME` as the build
(`-DOWL_CONAN_HOME`), where `cmake/Conan.cmake` registered the `owl-local` remote. A local recipe's revision is a hash
of its content: regenerating without changing a recipe only moves its timestamp, a diff not worth committing.

### Binary cache (`owl-cache`)

ConanCenter has no binary for our compilers, so a cold agent builds every dependency. An
optional Conan server serves as a shared binary cache:

- `OWL_CONAN_CACHE_URL` (CMake or environment) registers it as the `owl-cache` remote, after `owl-local` and before
  ConanCenter. Credentials come from Conan's own `CONAN_LOGIN_USERNAME_OWL_CACHE` / `CONAN_PASSWORD_OWL_CACHE`.
- At configure time `cmake/Conan.cmake` logs in and lists `zlib/*` on it; any failure disables the remote with a
  warning, and the install falls back to ConanCenter. Without a URL the remote is removed.
- With `OWL_CONAN_CACHE_UPLOAD=ON`, every binary of the local cache (latest package revisions) is uploaded (`conan list "*:*#latest"`, then
  `conan upload --list`), also after a failed install, so the packages built before the failure are kept; the
  server skips what it holds, and a failed upload is only a warning.
- GitLab CI sets them from masked project variables (`OWL_CONAN_CACHE_UPLOAD` in `ci/gitlab/templates.yml`), so every
  CI build reads and fills the cache.

### Shared libraries next to the binaries

`target_import_so_files()` (`cmake/importSharedLibs.py`) copies every shared library that `ldd` resolves under the
Conan package cache (`<conan home>/p`, found with `conan config home`) next to the binary: `libOwlEngine.so`,
`OwlNest` and `OwlRunner` find glfw, imgui, OpenAL Soft, libsndfile, the Vulkan loader and Slang through `$ORIGIN`,
and `cpack` ships them. Slang also loads files that `ldd` cannot see (`libslang-glslang-*.so`,
`libslang-glsl-module-*.so`, `slang-standard-module-*/`): they are copied with it.

### OwlEngine package (`conan create`)

```bash
./docker_run.sh poetry run conan create . --profile:all conan/profiles/linux-clang --lockfile conan.lock --lockfile-partial --build=missing
```

- `owlengine/<version>`, version read from `project(VERSION)` in `CMakeLists.txt` (single source, also for
  `version_test.cpp`). Engine only: no editor, no tests (the `Engine` install component), Conan drives CMake
  (`OWL_CONAN_INSTALL=OFF`, `conan_toolchain.cmake`).
- Consumers use the CMake config the engine installs (`lib/cmake/OwlEngine/`, `Owl::OwlEngine`, the one the CPack
  archives use): the recipe sets `cmake_find_mode` to `none`, so `test_package` checks that file and not a
  Conan-generated one.
- `test/package/`: `find_package(OwlEngine CONFIG REQUIRED)`, a program including `<owl.h>`, linked to
  `Owl::OwlEngine`, compiled without any Owl flag and run; a second one on `Owl::Gui`.
- Install tree: headers under `include/`; `Owl_Base` (`-Werror -Weverything`, internal definitions) linked as
  `$<BUILD_INTERFACE:…>` and not installed, the export keeping only `OWL_BUILD_SHARED`, `OWL_PLATFORM_<OS>` and
  `cxx_std_23`.
- Public dependencies: EnTT only for `Owl::OwlEngine`; imgui only through the optional `Owl::Gui` target
  (`find_package(OwlEngine COMPONENTS Gui)`, its own `OwlEngineGuiTargets.cmake`), for `<owlgui.h>` and the three
  `gui/` headers that include imgui. spdlog and yaml-cpp are private: the log macros of `core/Log.h` never include
  spdlog, and the render-stack configuration crosses the public API as YAML text (`RendererStackEntry::defaultConfig`,
  `RenderLayer::applyConfig`).
- Shared only: a static OwlEngine exports `OwlEnginePrivate` and every private dependency; the recipe refuses
  `shared=False`.
- `libOwlEngine.so` keeps `$ORIGIN` as its only RPATH, so GNU ld finds its own shared dependencies (glfw, OpenAL Soft,
  libsndfile, Slang, Vulkan loader) only through `LD_LIBRARY_PATH`: `test_package` builds in Conan's run environment.
  A Conan consumer does the same (or links with lld, which does not resolve them).
- Not wired into the `Package` CI action nor published to a remote (planned for v1.0.0, see the
  [Roadmap](../roadmap.md)).

### Windows (MinGW)

Every dependency is built by the Windows agent's own MSYS2 compiler.

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

Direct dependencies and the transitive ones worth a note; exact versions in `conanfile.py`, revisions in `conan.lock`.
The `DependencyReport` CI action lists what lags behind ConanCenter or upstream.

| Dependency            | Owl             | Source                | Linkage | Note                                                                           |
|-----------------------|-----------------|-----------------------|---------|--------------------------------------------------------------------------------|
| cpp-httplib           | 0.65.0          | ConanCenter           | header  | MCP server, WebSocket server; TLS (OpenSSL) in the Owl Nest targets only       |
| cpptrace              | 1.0.4           | ConanCenter           | static  | Pulls libdwarf 2.1.0                                                           |
| enet                  | 1.3.18          | ConanCenter           | static  | `network` module: reliable UDP of `network::Transport`, behind its interface   |
| entt                  | 4.0.0           | ConanCenter           | header  | Header-only, public (the only one)                                             |
| fast_float            | 8.2.10          | ConanCenter           | header  | Pulled by rapidyaml, pinned directly                                           |
| freetype              | 2.15.0          | ConanCenter           | static  | Transitive, pinned by the msdfgen recipe; no brotli, no bzip2                  |
| glad                  | 2.0.8           | ConanCenter           | static  | Generated at build time: GL 4.6 compatibility                                  |
| glfw                  | 3.5.1           | local recipe          | shared  | ConanCenter: X11 only, or its own libwayland; both backends, system Wayland    |
| googletest            | 1.18.0          | ConanCenter (`gtest`) | static  |                                                                                |
| imgui                 | 1.92.9b-docking | ConanCenter           | shared  | Backends compiled by Owl (see above)                                           |
| imgui_color_text_edit | 1.93.0          | local recipe          | static  | Absent; upstream tag, `DocPos` cursor API                                      |
| imguizmo              | 1.10            | local recipe          | static  | ConanCenter's cci.20231114 does not build with imgui 1.92                      |
| joltphysics           | 5.7.0           | local recipe          | static  | Latest upstream; cross-platform deterministic, SSE4.2 on x86_64, no FMA        |
| ktx                   | 4.4.2           | local recipe          | static  | KTX2 of the cook: Basis Universal, BC7 / ASTC transcode; tools off             |
| libpng                | 1.6.59          | ConanCenter           | static  | Transitive (msdf-atlas-gen)                                                    |
| libsndfile            | 1.2.2           | ConanCenter           | shared  | LGPL: kept shared; flac 1.5.0 and libalsa 1.2.16.1 forced (override)           |
| luau                  | 0.747           | local recipe          | static  | ConanCenter stops at 0.700; Lua errors as C++ exceptions                       |
| lunasvg               | 3.5.0           | ConanCenter           | static  | Pulls plutovg 1.3.3                                                            |
| magic_enum            | 0.9.8           | ConanCenter           | header  |                                                                                |
| md4c                  | 0.6.0           | ConanCenter           | static  |                                                                                |
| msdf-atlas-gen        | 1.4             | local recipe          | static  | ConanCenter packages the tool only; this one builds the library                |
| msdfgen               | 1.13.1          | local recipe          | static  | ConanCenter stops at 1.12: its recipe, unchanged                               |
| nfd (extended)        | 1.4.1           | local recipe          | static  | Absent from ConanCenter; `portal` on Linux: D-Bus + wayland-client (system)    |
| nlohmann_json         | 3.12.0          | ConanCenter           | header  | Owl Nest (`nest`): MCP dispatcher; tinygltf pulls the same version             |
| openal                | 1.25.2          | local recipe          | shared  | `openal-soft`, ConanCenter stops at 1.24.3; LGPL: shared                       |
| openssl               | 4.1.0           | ConanCenter           | static  | Owl Nest (`nest`): HTTPS of the runner downloads, SHA-256 of the archives      |
| rapidyaml             | 0.15.2          | ConanCenter           | static  | Read side of scenes, prefabs and snapshots; pulls c4core and fast_float        |
| slang                 | 2026.22         | local recipe          | shared  | Upstream release binaries                                                      |
| spdlog                | 1.17.0          | ConanCenter           | static  | `use_std_fmt`: no fmt dependency                                               |
| spirv-cross           | 1.4.357.0       | ConanCenter           | static  | Only the core/glsl/cpp/reflect components                                      |
| stb_image             | cci.20261021    | ConanCenter (`stb`)   | header  | Newer than the one tinygltf pins (force)                                       |
| taskflow              | 4.1.0           | local recipe          | header  | ConanCenter stops at 4.0.0; its recipe, unchanged                              |
| tinygltf              | 2.9.7           | ConanCenter           | header  | Pulls nlohmann_json; read only, no stb_image_write                             |
| tracy                 | 0.14.1          | ConanCenter           | static  | Optional (`tracy` option), `on_demand`                                         |
| ufbx                  | 0.23.1          | local recipe          | static  | Absent from ConanCenter (single source file); FBX and OBJ                      |
| vulkan                | 1.4.357.0       | ConanCenter           | shared  | vulkan-headers, vulkan-loader, vulkan-utility-libraries                        |
| vulkan-memory-alloc.  | 3.3.0           | ConanCenter           | header  | VMA: every Vulkan buffer and image is a sub-allocation                         |
| xxhash                | 0.8.4           | ConanCenter           | static  | XXH3-128 keys of the asset cook cache                                          |
| yaml-cpp              | 0.9.0           | local recipe          | static  | ConanCenter's recipe, doubles written right on 32-bit targets (patch)          |
| zlib                  | 1.4             | ConanCenter           | static  | Owl Nest (`nest`): unpacks the runner archives (tar.gz); also transitive       |
| zstd                  | 1.5.7           | ConanCenter           | static  |                                                                                |

## Local recipes (`conan/recipes/`)

Served as the `owl-local` remote (`local-recipes-index`, the ConanCenter layout: `<name>/config.yml` +
`<name>/all/conanfile.py` + `conandata.yml`), registered first (it wins over ConanCenter for its names) and
refreshed (`--update=<name>`) by `cmake/Conan.cmake`. Each downloads the
public upstream archive (sha256 pinned) and adds at most a few-line `CMakeLists.txt`, so each can be proposed to
ConanCenter as is.

| Recipe                            | Why it is local                                                                                                                                                                                                                                         |
|-----------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `slang` 2026.22                   | Absent; building it pulls LLVM-sized dependencies, so the recipe repackages the upstream release binaries                                                                                                                                               |
| `ufbx` 0.23.1                     | Absent; one `.c` file, upstream ships no build system                                                                                                                                                                                                   |
| `imgui-color-text-edit` 1.93.0    | Absent (goossens fork); the upstream release tag                                                                                                                                                                                                        |
| `imguizmo` 1.10                   | ConanCenter's only recent version (cci.20231114) calls `BeginChildFrame`, removed in imgui 1.92; the recipe also puts the thickness before the flags in the AddRect / AddPolyline calls of ImCurveEdit, GraphEditor and ImGradient (imgui 1.92.8 order) |
| `nativefiledialog-extended` 1.4.1 | Absent; the GitHub archive lacks the `wayland-protocols` submodule, fetched as a second source                                                                                                                                                          |
| `msdf-atlas-gen` 1.4              | ConanCenter's recipe packages the command-line tool only; this one builds the library on ConanCenter's msdfgen                                                                                                                                          |
| `libmp3lame` 3.100                | ConanCenter's recipe, except MinGW Clang builds with autotools: upstream takes every Windows Clang for clang-cl                                                                                                                                         |
| `taskflow` 4.1.0                  | ConanCenter stops at 4.0.0: its recipe, unchanged                                                                                                                                                                                                       |
| `luau` 0.747                      | ConanCenter stops at 0.700: a build of the upstream CMake, Lua errors raised as C++ exceptions (no `LUA_USE_LONGJMP`): an error unwinding a binding runs its destructors                                                                                |
| `openal-soft` 1.25.2              | ConanCenter stops at 1.24.3: its recipe on libalsa 1.2.16.1, `-Werror=function-effects` off (libstdc++)                                                                                                                                                 |
| `msdfgen` 1.13.1                  | ConanCenter stops at 1.12 (needed by msdf-atlas-gen 1.4): its recipe, on freetype 2.15.0 and tinyxml2 11.0.0                                                                                                                                            |
| `joltphysics` 5.7.0               | ConanCenter's recipe turns cross-platform determinism off and pushes `-mavx2 -mfma` to its consumers: deterministic, SSE4.2 on x86_64, no FMA, RTTI on, exceptions on the Web                                                                           |
| `glfw` 3.5.1                      | ConanCenter stops at 3.4 and builds Wayland on its own libwayland: both backends, system libraries dlopened                                                                                                                                             |
| `ktx` 4.4.2                       | ConanCenter's recipe plus a patch: the ETC1S encoder allocates the BasisLZ global data with `new[]` that `ktxTexture2_Destroy` frees with `free` (ASan)                                                                                                 |
| `yaml-cpp` 0.9.0                  | ConanCenter's recipe plus a patch: the 64-bit significand of a double went through a `size_t`, wrong on wasm32                                                                                                                                          |

## Planned

- Vulkan validation layers (`OWL_ENABLE_VULKAN_LAYERS`) from `vulkan-validationlayers`
- Propose the local recipes (or their new versions) to ConanCenter, msdf-atlas-gen as a library option, the
  libmp3lame clang-cl fix
- The OwlEngine Conan package published and run by the `Package` action, a static variant
- To evaluate: miniaudio instead of OpenAL Soft + libsndfile; a single image loader instead of stb_image + libpng
