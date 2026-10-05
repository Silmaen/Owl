---
paths:
  - "depmanager.yml"
  - "owl_engine.py"
  - "cmake/Depmanager.cmake"
  - "cmake/Conan.cmake"
  - "conanfile.py"
  - "conan/**"
---

# DepManager

Every third-party library comes from DepManager (`depmanager.yml`). **No vendoring**: never copy external
sources into the repo. Check the package catalogue before assuming a library is missing (e.g.
`imgui_color_text_edit` exists; ImGuizmo already bundles GraphEditor, ImSequencer, ImCurveEdit,
ImGradient, ImZoomSlider, ImLightRig).

## Rules

- Always `poetry run depmanager …` (inside `docker/run.sh`), never a bare `depmanager`.
- Pin exact versions; set `kind: static|shared` for every non header-only package.
- Link with `owl_target_link_libraries()` (`cmake/OwlUtils.cmake`), never a raw `find_package()`.
- Packages are fetched at configure time (`dm_load_environment()` in `cmake/Depmanager.cmake`).
- Removing a dependency: drop it from `depmanager.yml` and every matching `owl_target_link_libraries()`.

## Commands

```bash
docker/run.sh poetry run depmanager pack ls -p <name>:<version> -t <static|shared|header>
docker/run.sh poetry run depmanager pack ls <remote>
docker/run.sh poetry run depmanager pack pull -p <name>:<version> <remote>
docker/run.sh poetry run depmanager remote list
docker/run.sh poetry run depmanager build <recipe_dir> [-r] [-f]
```

## Conan 2 (migration in progress, `-DOWL_DEPENDENCY_PROVIDER=conan`)

DepManager stays the default until every preset builds on Conan (see `doc/pages/design/conan-migration.md`).

- `conanfile.py` (root) lists the dependencies; `cmake/Conan.cmake` runs `conan install` at configure time
  into `output/build/<preset>/conan/` and puts the CMakeDeps files on `CMAKE_PREFIX_PATH`.
- Conan comes from Poetry (dev group): `docker/run.sh poetry run conan …`, never a host install.
- ConanCenter first. A recipe missing there goes in `conan/recipes/<name>/` (CCI layout: `config.yml` +
  `all/conanfile.py` + `all/conandata.yml`), served as the `owl-local` local-recipes-index remote. Keep it
  minimal and list it in the design page.
- Profiles are versioned in `conan/profiles/` (`linux-clang`, `linux-gcc`, picked from the compiler). No profile
  per build type or sanitizer: third parties are Release and never instrumented.
- `conan.lock` pins every recipe revision and is always used. After touching `conanfile.py` or a local recipe,
  regenerate it with the two `conan lock create` commands of the design page and commit it with the change.
- `owl_target_link_libraries()` maps the few names that differ (`OWL_CONAN_PACKAGE_<Module>` /
  `OWL_CONAN_TARGET_<Module>` in `cmake/Conan.cmake`) and compiles the imgui backends from the package.
- Recipe options mirror CMake: `shared`, `testing` (gtest), `nest` (editor-only packages). An editor-only
  dependency goes under `if self.options.nest`.
- Shared libraries of the Conan cache are copied next to the binaries by `target_import_so_files()`.
- In a worktree sharing `fake_home`, pass `-DOWL_CONAN_HOME=/fhome/.conan2-owl` so Owl keeps its own cache.

```bash
docker/run.sh cmake --preset linux-clang-release -DOWL_DEPENDENCY_PROVIDER=conan -DOWL_CONAN_HOME=/fhome/.conan2-owl
docker/run.sh poetry run conan search <name> -r conancenter
docker/run.sh poetry run conan create . --profile:all conan/profiles/linux-clang --lockfile conan.lock --lockfile-partial --build=missing
```

`conan create` packages OwlEngine (`owlengine/<version>`, shared only) and builds `test_package/` against the
installed CMake config: a public header that includes an undeclared dependency, or a build flag leaking into
the export, breaks it.

## OwlEngine as a package (`owl_engine.py`)

- `poetry run depmanager build .` produces `owl_engine:<version>`, `shared` and `static` variants.
- Public deps declared in the recipe: EnTT, imgui, yaml-cpp (until PR-27). Version read from `CMakeLists.txt`.
  Editor and tests disabled, packaging mode on.
- Installs `lib/`, `include/`, `assets/`, `lib/cmake/OwlEngine/`.
- Consumers (e.g. OwlDrone): `find_package(OwlEngine CONFIG REQUIRED)` + `Owl::OwlEngine`. Moving a
  public header breaks them.
