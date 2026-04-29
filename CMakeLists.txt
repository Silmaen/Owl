#
#
#
cmake_minimum_required(VERSION 3.24)
set(PROJECT_PREFIX OWL)
set(PROJECT_PREFIX_LOWER owl)
set(ENGINE_NAME OwlEngine)
project(Owl VERSION 0.1.1 LANGUAGES C CXX)
#
# ---=== Options ===---
#
option(${PROJECT_PREFIX}_BUILD_SHARED "If the Library must be build as shared library" ON)
option(${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY "Use release version of third party libraries" ON)
option(${PROJECT_PREFIX}_DEFINE_VULKAN_LAYERS "Copy vulkan layers in the binary directory" OFF)

option(${PROJECT_PREFIX}_BUILD_NEST "If wanted to generate Owl Nest" ON)

option(${PROJECT_PREFIX}_TESTING "Enable the unit tests" ON)
option(${PROJECT_PREFIX}_ENABLE_COVERAGE "Run code coverage during test run" OFF)
option(${PROJECT_PREFIX}_ENABLE_STACKTRACE "Use stacktrace within the memory tracker (slow down the code)" OFF)
option(${PROJECT_PREFIX}_ENABLE_PROFILING "Output profiling information during execution" OFF)

#
# ---=== Base Configs ===---

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")
include(BaseConfig)

# When third-party packages are consumed as Release builds, map Debug builds to the Release
# imported-config so both link resolution AND `$<TARGET_RUNTIME_DLLS:...>` pick the same variant.
# Set at directory scope so it propagates to every `add_subdirectory`.  Without this, Windows
# Debug builds linked against `libd.lib` but the post-build copy brought `lib.dll`, causing the
# loader to fail with `STATUS_DLL_NOT_FOUND` (0xc0000135) on every test binary.
if (${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY)
    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
endif ()

#
# ---=== Bundle help pages ===---
# Copy the Markdown documentation pages from `doc/pages/` plus the canonical
# README/CHANGELOG/CONTRIBUTING into `engine_assets/help/`, and generate a
# small `index.yml` describing each page (id, title, category). The runtime
# `HelpPanel` reads this directory; the existing `engine_assets/` install
# rule embeds it into packaged builds automatically.
include(HelpAssets)
owl_bundle_help_assets()

#
# ---=== The Program ===---
#
add_subdirectory(source)

#
# ---=== TESTING PROCEDURES ===---
#
if (${PROJECT_PREFIX}_TESTING AND NOT ${PROJECT_PREFIX}_ENABLE_CLANG_TIDY)
    enable_testing()
    add_subdirectory(test)
endif ()

#
# ---=== CLEANING PROCEDURES ===---
#
add_custom_target(clean_bin
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
        COMMENT "Cleaning the binary directory ${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(clean_bin PROPERTIES FOLDER "Utils")


# ----------------------------------------------------------------------------------------------------------------------
# Include CPack package
if (${PROJECT_PREFIX}_PACKAGING)
    include(CPackConfig.cmake)
    include(CPack)
endif ()
# ----------------------------------------------------------------------------------------------------------------------
