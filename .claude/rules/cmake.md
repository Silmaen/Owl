---
paths:
  - "**/CMakeLists.txt"
  - "**/*.cmake"
  - "CMakePresets*.json"
---

# CMake Conventions

## Variables

- All project options use `${PROJECT_PREFIX}_` prefix (expands to `OWL_`)
- Lowercase variant: `${PROJECT_PREFIX_LOWER}_` (expands to `owl_`)
- Engine library name: `${ENGINE_NAME}` (expands to `OwlEngine`)

## Source Discovery

**Always use `file(GLOB_RECURSE ...)`** for sources and headers. Never maintain explicit file lists:
```cmake
file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
```
Adding a new `.cpp` or `.h` file requires no CMakeLists.txt modification.

## Linking Dependencies

**Never call `find_package()` directly** for Conan-managed packages. Use the wrapper (it maps the Conan
names that differ, see `cmake/Conan.cmake`):
```cmake
owl_target_link_libraries(TargetName PRIVATE|PUBLIC|INTERFACE ModuleName REQUIRED)
```

Options:
- `MODULE_TARGET X::Y` — when the CMake target name differs from the package name
- `REQUIRED`, `QUIET`, `CONFIG` — forwarded to `find_package()`

Examples:
```cmake
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE box2d REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glfw3 MODULE_TARGET glfw REQUIRED)
owl_target_link_libraries(${ENGINE_NAME} PUBLIC EnTT REQUIRED)
```

## Adding a New Dependency

1. Add `self.requires(...)` to `conanfile.py` (and its linkage in `default_options`), then relock `conan.lock`
2. Add `owl_target_link_libraries()` call in the appropriate `CMakeLists.txt`
3. Dependencies are auto-fetched during `cmake --preset` configure step (see `.claude/rules/dependencies.md`)

## Application Targets

Follow the existing pattern:
```cmake
set(OWL_PROJECT ${CMAKE_PROJECT_NAME}MyApp)
file(GLOB_RECURSE SRCS sources/*.cpp)
file(GLOB_RECURSE HDRS sources/*.h)
add_executable(${OWL_PROJECT} ${SRCS} ${HDRS})
set_target_properties(${OWL_PROJECT} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    FOLDER "App")
target_include_directories(${OWL_PROJECT} PRIVATE sources)
target_link_libraries(${OWL_PROJECT} PRIVATE ${ENGINE_NAME})
target_compile_definitions(${OWL_PROJECT} PRIVATE OWL_ASSETS_LOCATION="source/myapp/assets")
target_import_so_files(${OWL_PROJECT})  # Linux shared lib copy
```

## Test Targets

Tests are auto-discovered from `test/` subdirectories. See testing rules.

## Build Types

- Settings propagate via `${CMAKE_PROJECT_NAME}_Base` INTERFACE library
- Never add compile flags to individual targets — add to Base if global
- `OWL_DEBUG` / `OWL_RELEASE` compile definitions are set automatically

## Platform Handling

- Use `${PROJECT_PREFIX}_PLATFORM_WINDOWS` and `${PROJECT_PREFIX}_PLATFORM_LINUX` guards
- Linux shared lib copy: `target_import_so_files(target)`
- Windows DLL copy: post-build `copy_if_different` with `$<TARGET_RUNTIME_DLLS:target>`
- Exports: `libOwlEngine.so` is built with hidden visibility and `--exclude-libs,ALL` (the only per-target
  flags, set in `source/owl/CMakeLists.txt`); a symbol used outside the engine needs `OWL_API` on Linux too

## Options (`OWL_*`)

The one reference table is *CMake Options* in `doc/pages/building.md`: declare every new option with `option()` (or
a `CACHE` variable) and add its row there in the same change. Third parties are Release in every build type
(`OWL_USE_RELEASE_THIRD_PARTY`, through `CMAKE_MAP_IMPORTED_CONFIG_DEBUG` in the top-level `CMakeLists.txt`).
