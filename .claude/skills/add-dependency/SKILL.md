---
name: add-dependency
description: Add a new C++ dependency to the Owl project via Conan 2
---

# Add a new C++ dependency

Arguments: `<package-name>` and optionally `<version>` and `<linkage>` (static/shared/header).

## Steps

1. Check if the package is already in `conanfile.py`, then on ConanCenter:
   `docker/run.sh poetry run conan search <name> -r conancenter`. Never vendor sources. A package missing there
   gets a minimal local recipe in `conan/recipes/<name>/` (see `.claude/rules/dependencies.md`).
2. Add `self.requires("<name>/<version>")` in `conanfile.py` (`requirements()`; editor-only under
   `if self.options.nest`), and its linkage in `default_options` (`"<name>/*:shared": True|False`) when it is not
   header-only.
   - If the version is not specified, ask the user.
   - If the linkage is not specified, ask the user (static, shared, or header-only).
3. Regenerate `conan.lock` (`conan lock create` commands of `doc/pages/design/conan-migration.md`).
4. Add the `owl_target_link_libraries()` call in the appropriate `CMakeLists.txt`:
   ```cmake
   owl_target_link_libraries(<target> PRIVATE <package_name> REQUIRED)
   ```
   - Engine internal deps: add to `source/owl/CMakeLists.txt` with `${ENGINE_NAME}Private INTERFACE`
   - Engine public deps: only EnTT (plus imgui through `Owl::Gui`); a new one is the maintainer's call and needs
     `find_dependency()` in `cmake/config/OwlEngineConfig.cmake.in` and `transitive_headers` in `conanfile.py`
   - App deps: add to `source/<app>/CMakeLists.txt` with `${OWL_PROJECT} PRIVATE`
   - A CMake package or target name that differs from `<package_name>` gets an `OWL_CONAN_PACKAGE_<Module>` /
     `OWL_CONAN_TARGET_<Module>` entry in `cmake/Conan.cmake`.
5. Test the configure step:
   ```bash
   docker/run.sh cmake --preset linux-clang-release -S .
   ```
   Third-party headers that raise warnings get a wrapper in `source/owl/private/core/external/`.
6. Add the dependency to the inventory of `doc/pages/design/conan-migration.md`, then report success/failure.
