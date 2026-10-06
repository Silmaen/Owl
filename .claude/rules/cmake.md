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

**Never call `find_package()` directly** for depmanager- or Conan-managed packages. Use the wrapper (it
maps the Conan names that differ, see `cmake/Conan.cmake`):
```cmake
owl_target_link_libraries(TargetName PRIVATE|PUBLIC|INTERFACE ModuleName REQUIRED ${THIRD_PARTY_RELEASE})
```

Options:
- `FORCE_RELEASE` (via `${THIRD_PARTY_RELEASE}`) — use release build in debug mode
- `MODULE_TARGET X::Y` — when the CMake target name differs from the package name
- `REQUIRED`, `QUIET`, `CONFIG` — forwarded to `find_package()`

Examples:
```cmake
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE box2d REQUIRED ${THIRD_PARTY_RELEASE})
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glfw3 MODULE_TARGET glfw REQUIRED ${THIRD_PARTY_RELEASE})
owl_target_link_libraries(${ENGINE_NAME} PUBLIC EnTT REQUIRED ${THIRD_PARTY_RELEASE})
```

## Adding a New Dependency

1. Add entry to `depmanager.yml` with `version` and `kind` (static/shared/header)
2. Add `owl_target_link_libraries()` call in the appropriate `CMakeLists.txt`
3. Dependencies are auto-fetched during `cmake --preset` configure step

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

## Options (`OWL_*`)

| Option                                    | Default    | Description                                                                                                                                   |
|-------------------------------------------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| `OWL_BUILD_SHARED`                        | ON         | Build engine as shared library                                                                                                                |
| `OWL_BUILD_NEST`                          | ON         | Build Owl Nest editor                                                                                                                         |
| `OWL_TESTING`                             | ON         | Enable unit tests                                                                                                                             |
| `OWL_ENABLE_COVERAGE`                     | OFF        | Code coverage (auto-enabled in debug presets)                                                                                                 |
| `OWL_ENABLE_MEMORY_TRACKER`               | OFF        | Install global `new`/`delete` overrides so `TrackerAPI` records every allocation (always on in Debug; opt-in in Release for leak diagnostics) |
| `OWL_ENABLE_STACKTRACE`                   | OFF        | Memory tracker stacktrace (implies `OWL_ENABLE_MEMORY_TRACKER`; performance impact)                                                           |
| `OWL_ENABLE_PROFILING`                    | OFF        | Profiling output                                                                                                                              |
| `OWL_USE_RELEASE_THIRD_PARTY`             | ON         | Use release builds of third-party libraries                                                                                                   |
| `OWL_ENABLE_VULKAN_LAYERS`                | OFF        | Copy Vulkan layers to binary directory                                                                                                        |
| `OWL_ENABLE_CLANG_TIDY`                   | OFF        | Enable clang-tidy static analysis                                                                                                             |
| `OWL_ENABLE_ADDRESS_SANITIZER`            | OFF        | AddressSanitizer (CI presets)                                                                                                                 |
| `OWL_ENABLE_THREAD_SANITIZER`             | OFF        | ThreadSanitizer (CI presets)                                                                                                                  |
| `OWL_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER` | OFF        | UBSanitizer (CI presets)                                                                                                                      |
| `OWL_ENABLE_MEMORY_SANITIZER`             | OFF        | MemorySanitizer (Clang-only, CI presets)                                                                                                      |
| `OWL_ENABLE_DOCUMENTATION`                | OFF        | Enable Doxygen documentation generation                                                                                                       |
| `OWL_PACKAGING`                           | OFF        | Enable packaging mode                                                                                                                         |
| `OWL_BENCHMARK`                           | OFF        | Build the `owl_bench` micro-benchmark harness (`bench/`, see `bench/README.md`)                                                               |
| `OWL_INCLUDE_CHECK`                       | OFF        | Add `owl_include_check`: every header and source compiled alone, no PCH, strict libc++ (Clang only, `linux-include-check` preset)             |
| `OWL_DEPENDENCY_PROVIDER`                 | depmanager | Third-party provider: `depmanager` or `conan` (see `.claude/rules/dependencies.md`)                                                           |
| `OWL_TEST_SHUFFLE`                        | OFF        | Run every test binary with `--gtest_shuffle` (ON in the sanitizer presets; seed via `GTEST_RANDOM_SEED`)                                      |
