---
paths:
  - "cmake/Conan.cmake"
  - "conanfile.py"
  - "conan/**"
  - "conan.lock"
---

# Dependencies (Conan 2)

Every third-party library comes from Conan 2 (`conanfile.py`), the only provider. **No vendoring**: never copy
external sources into the repo. Check ConanCenter and `conan/recipes/` before assuming a library is missing
(ImGuizmo already bundles GraphEditor, ImSequencer, ImCurveEdit, ImGradient, ImZoomSlider, ImLightRig).

## Rules

- `conanfile.py` (root) lists the dependencies; `cmake/Conan.cmake` runs `conan install` at configure time
  into `output/build/<preset>/conan/` and puts the CMakeDeps files on `CMAKE_PREFIX_PATH`.
- Conan comes from Poetry (dev group): `docker/run.sh poetry run conan …`, never a host install.
- ConanCenter first. A recipe missing there goes in `conan/recipes/<name>/` (CCI layout: `config.yml` +
  `all/conanfile.py` + `all/conandata.yml`), served as the `owl-local` local-recipes-index remote. Keep it
  minimal and list it in `doc/pages/design/conan-migration.md`.
- Pin exact versions in `conanfile.py`. Profiles are versioned in `conan/profiles/` (`<os>-<compiler>`, picked from
  the compiler). No profile per build type or sanitizer: third parties are Release and never instrumented.
- `conan.lock` pins every recipe revision and is always used. After touching `conanfile.py` or a local recipe,
  regenerate it with the `conan lock create` commands of the design page and commit it with the change.
- Link with `owl_target_link_libraries()` (`cmake/OwlUtils.cmake`), never a raw `find_package()`. It maps the few
  names that differ (`OWL_CONAN_PACKAGE_<Module>` / `OWL_CONAN_TARGET_<Module>` in `cmake/Conan.cmake`) and
  compiles the imgui backends from the package.
- Recipe options mirror CMake: `shared`, `testing` (gtest), `nest` (editor-only packages), `tracy`
  (`OWL_PROFILER=tracy`). An editor-only dependency goes under `if self.options.nest`.
- Removing a dependency: drop it from `conanfile.py`, every matching `owl_target_link_libraries()`, and relock.
- Shared libraries of the Conan cache are copied next to the binaries by `target_import_so_files()`.
- In a worktree sharing `fake_home`, pass `-DOWL_CONAN_HOME=/fhome/.conan2-owl` so Owl keeps its own cache.
- Optional binary cache: `OWL_CONAN_CACHE_URL` (remote `owl-cache`, `OWL_CONAN_CACHE_UPLOAD=ON` to fill it);
  unreachable means ConanCenter only.

## Commands

```bash
docker/run.sh cmake --preset linux-clang-release -DOWL_CONAN_HOME=/fhome/.conan2-owl
docker/run.sh poetry run conan search <name> -r conancenter
docker/run.sh poetry run conan create . --profile:all conan/profiles/linux-clang --lockfile conan.lock --lockfile-partial --build=missing
```

## OwlEngine for other projects

- Until v1.0.0, consumers (e.g. OwlDrone) use the `OwlEngine` archive (`cpack` in a release tree, `Engine`
  component): it installs `lib/`, `include/`, `assets/`, `lib/cmake/OwlEngine/`; `find_package(OwlEngine CONFIG
  REQUIRED)` + `Owl::OwlEngine`. Moving a public header breaks them.
- Public dependencies: EnTT only; imgui comes with the optional `Owl::Gui` (`COMPONENTS Gui`, `<owlgui.h>`). No
  public header includes another third-party header (yaml-cpp crosses the API as YAML text).
- `conan create` already packages OwlEngine (`owlengine/<version>`, shared only) and builds `test_package/` against
  the installed CMake config: a public header that includes an undeclared dependency, or a build flag leaking into
  the export, breaks it. Publishing it is a v1.0.0 item.
