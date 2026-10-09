#
#
#
cmake_minimum_required(VERSION 3.24)
set(PROJECT_PREFIX OWL)
set(PROJECT_PREFIX_LOWER owl)
set(ENGINE_NAME OwlEngine)
project(Owl VERSION 0.3.0 LANGUAGES C CXX)
#
# ---=== Options ===---
#
option(${PROJECT_PREFIX}_BUILD_SHARED "If the Library must be build as shared library" ON)
option(${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY "Use release version of third party libraries" ON)
option(${PROJECT_PREFIX}_ENABLE_VULKAN_LAYERS "Copy vulkan layers in the binary directory" OFF)

option(${PROJECT_PREFIX}_BUILD_NEST "If wanted to generate Owl Nest" ON)

option(${PROJECT_PREFIX}_TESTING "Enable the unit tests" ON)
option(${PROJECT_PREFIX}_ENABLE_COVERAGE "Run code coverage during test run" OFF)
option(${PROJECT_PREFIX}_ENABLE_MEMORY_TRACKER "Install the global new/delete overrides so TrackerAPI records every allocation (off by default in every build type: it doubles the live allocation set and skews Debug timings)" OFF)
option(${PROJECT_PREFIX}_ENABLE_STACKTRACE "Use stacktrace within the memory tracker (slow down the code)" OFF)
set(${PROJECT_PREFIX}_PROFILER "none" CACHE STRING "Backend of the OWL_PROFILE_* macros: none, tracy or chrome")
set_property(CACHE ${PROJECT_PREFIX}_PROFILER PROPERTY STRINGS none tracy chrome)
option(${PROJECT_PREFIX}_ENABLE_PROFILING "Deprecated: same as OWL_PROFILER=chrome" OFF)
set(${PROJECT_PREFIX}_LOG_LEVEL "trace" CACHE STRING "Lowest log level compiled in: trace, debug, info, warning, error, critical or off")
set_property(CACHE ${PROJECT_PREFIX}_LOG_LEVEL PROPERTY STRINGS trace debug info warning error critical off)
option(${PROJECT_PREFIX}_USE_CCACHE "Use ccache (or sccache) as a compiler launcher when available" ON)
option(${PROJECT_PREFIX}_BENCHMARK "Build the micro-benchmark harness in bench/" OFF)
option(${PROJECT_PREFIX}_INCLUDE_CHECK "Add the owl_include_check target: every header and source compiled alone against strict libc++, without PCH (Clang only)" OFF)
option(${PROJECT_PREFIX}_FUZZING "Build the libFuzzer targets in fuzz/ (Clang only, instruments the engine)" OFF)
option(${PROJECT_PREFIX}_ENABLE_DOCUMENTATION "Require Doxygen (otherwise the documentation target exists only when Doxygen is found)" OFF)
option(${PROJECT_PREFIX}_HELP_FETCH_BADGES "Download the remote badge images of the help pages missing from the local cache (network)" OFF)
option(${PROJECT_PREFIX}_ENABLE_RENDERER_VERBOSE_CAPABILITIES "Log every capability of the graphics device at start-up" OFF)
option(${PROJECT_PREFIX}_ENABLE_SHADER_REFLECT_RESOURCES "Log the resources reflected from every compiled shader" OFF)

#
# ---=== Engine modules ===---
# The core is always built. A module turned OFF drops its third parties; its public API stays, backed by the Null
# backend or a no-op (see doc/pages/building.md, "Engine modules").
#
option(${PROJECT_PREFIX}_MODULE_RENDER "OpenGL and Vulkan backends, GLFW window and input, Slang compiler (OFF: Null backends, headless)" ON)
option(${PROJECT_PREFIX}_MODULE_PHYSICS "Box2D physics (OFF: PhysicCommand does nothing)" ON)
option(${PROJECT_PREFIX}_PHYSICS_AVX2 "Build Box2D with AVX2 on x86_64 (a CPU without AVX2 then runs without physics)" ON)
option(${PROJECT_PREFIX}_MODULE_AUDIO "OpenAL sound backend and libsndfile decoding (OFF: Null sound backend)" ON)
option(${PROJECT_PREFIX}_MODULE_SCRIPT "Lua scripting (OFF: scripts never load)" ON)
option(${PROJECT_PREFIX}_MODULE_GUI "ImGui layer and widgets, target Owl::Gui (needs OWL_MODULE_RENDER)" ON)
if (${PROJECT_PREFIX}_MODULE_GUI AND NOT ${PROJECT_PREFIX}_MODULE_RENDER)
    message(STATUS "OWL_MODULE_GUI needs OWL_MODULE_RENDER: Gui module disabled")
    set(${PROJECT_PREFIX}_MODULE_GUI OFF)
endif ()
set(${PROJECT_PREFIX}_ALL_MODULES ON)
foreach (_owl_module RENDER PHYSICS AUDIO SCRIPT GUI)
    if (NOT ${PROJECT_PREFIX}_MODULE_${_owl_module})
        set(${PROJECT_PREFIX}_ALL_MODULES OFF)
    endif ()
endforeach ()
unset(_owl_module)
# The editor, the runner, the benchmarks and the fuzzers exercise every module.
if (NOT ${PROJECT_PREFIX}_ALL_MODULES)
    foreach (_owl_tool BUILD_NEST BENCHMARK FUZZING)
        if (${PROJECT_PREFIX}_${_owl_tool})
            message(STATUS "OWL_${_owl_tool} needs every engine module: disabled")
            set(${PROJECT_PREFIX}_${_owl_tool} OFF)
        endif ()
    endforeach ()
    unset(_owl_tool)
endif ()

#
# ---=== Base Configs ===---

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")
include(BaseConfig)

# When third-party packages are consumed as Release builds, map Debug builds to the Release
# imported-config so both link resolution AND `$<TARGET_RUNTIME_DLLS:...>` pick the same variant.
# Set at directory scope so it propagates to every `add_subdirectory`.  Without this, Windows
# Debug builds linked against `libd.lib` but the post-build copy brought `lib.dll`, causing the
# loader to fail with `STATUS_DLL_NOT_FOUND` (0xc0000135) on every test binary.
# The empty entry keeps configuration-less imported targets usable (CMake's FindOpenGL ones, used in Conan mode).
if (${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY)
    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release "")
endif ()

#
# ---=== Bundle help pages ===---
# Copy the Markdown documentation pages from `doc/pages/` plus the canonical
# README/CONTRIBUTING into `<build>/help/`, and generate a small `index.yml`
# describing each page (id, title, category). The runtime `HelpPanel` reads it
# from the build tree, or from `assets/help/` where both CPack components
# install it.
include(HelpAssets)
owl_bundle_help_assets()
target_compile_definitions(${CMAKE_PROJECT_NAME}_BaseTest INTERFACE ${PROJECT_PREFIX}_HELP_BUILD_DIR="${${PROJECT_PREFIX}_HELP_DIR}")

#
# ---=== The Program ===---
#
add_subdirectory(source)

#
# ---=== TESTING PROCEDURES ===---
#
if (${PROJECT_PREFIX}_TESTING)
    enable_testing()
    add_subdirectory(test)
endif ()

#
# ---=== BENCHMARKS ===---
#
if (${PROJECT_PREFIX}_BENCHMARK)
    add_subdirectory(bench)
endif ()

#
# ---=== INCLUDE CHECK ===---
#
include(IncludeCheck)
if (${PROJECT_PREFIX}_INCLUDE_CHECK)
    owl_setup_include_check()
endif ()

#
# ---=== FUZZING ===---
#
if (${PROJECT_PREFIX}_FUZZING)
    add_subdirectory(fuzz)
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
include(CPackConfig.cmake)
include(CPack)
# ----------------------------------------------------------------------------------------------------------------------
