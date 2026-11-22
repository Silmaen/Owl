#
#
# load custom Environment variables
include(Environment)
# Wire ccache/sccache as compiler launcher when available
include(CompilerCache)
# Setup Poetry, and sync python dependencies
include(Poetry)
# Third-party dependencies from Conan 2 (conanfile.py, see doc/pages/design/dependencies.md)
include(Conan)
# Load Utility functions
include(OwlUtils)
#
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# No C++ module in the project: without this, CMake scans every translation unit for imports before compiling it.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
# An executable is not linked again when only a shared library it uses changed: the loader resolves the new one.
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(${CMAKE_PROJECT_NAME}_Base INTERFACE)
add_library(${CMAKE_PROJECT_NAME}_BaseTest INTERFACE)
add_dependencies(${CMAKE_PROJECT_NAME}_BaseTest ${CMAKE_PROJECT_NAME}_Base)

#
# ---=== Supported OS ===---
#
#
# Operating System
#
# -- host
pretty_platform_str(${CMAKE_HOST_SYSTEM_NAME} ${PROJECT_PREFIX}_HOST_PLATFORM_STR)
set(${PROJECT_PREFIX}_HOST_PLATFORM_VER_STR "${CMAKE_HOST_SYSTEM_VERSION}")
# -- target
if (NOT ${PROJECT_PREFIX}_PLATFORM_STR)
    pretty_platform_str(${CMAKE_SYSTEM_NAME} ${PROJECT_PREFIX}_PLATFORM_STR)
endif ()
if (${PROJECT_PREFIX}_PLATFORM_STR MATCHES "windows")
    set(${PROJECT_PREFIX}_PLATFORM_WINDOWS ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PLATFORM_WINDOWS)
elseif (${PROJECT_PREFIX}_PLATFORM_STR MATCHES "android")
    set(${PROJECT_PREFIX}_PLATFORM_ANDROID ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PLATFORM_ANDROID)
elseif (${PROJECT_PREFIX}_PLATFORM_STR MATCHES "linux")
    set(${PROJECT_PREFIX}_PLATFORM_LINUX ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PLATFORM_LINUX)
elseif (${PROJECT_PREFIX}_PLATFORM_STR MATCHES "emscripten")
    set(${PROJECT_PREFIX}_PLATFORM_STR "web")
    set(${PROJECT_PREFIX}_PLATFORM_WEB ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PLATFORM_WEB)
else ()
    message(FATAL_ERROR "Unsupported Target Operating System '${${PROJECT_PREFIX}_PLATFORM_STR}'")
endif ()
set(${PROJECT_PREFIX}_PLATFORM_VER_STR "${CMAKE_SYSTEM_VERSION}")
#
# CPU Architecture
#
# -- host
pretty_architecture_str(${CMAKE_HOST_SYSTEM_PROCESSOR} ${PROJECT_PREFIX}_HOST_ARCH_STR)
# -- target (CMAKE_SYSTEM_PROCESSOR: the toolchain's when cross compiling, else the build machine's)
if (NOT ${PROJECT_PREFIX}_ARCH_STR)
    if (EMSCRIPTEN)
        # Emscripten.cmake names the processor x86 for the projects testing it.
        set(${PROJECT_PREFIX}_ARCH_STR "wasm32")
    else ()
        pretty_architecture_str(${CMAKE_SYSTEM_PROCESSOR} ${PROJECT_PREFIX}_ARCH_STR)
    endif ()
endif ()
if (${PROJECT_PREFIX}_ARCH_STR STREQUAL "x64")
    set(${PROJECT_PREFIX}_PLATFORM_X64 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_X64)
elseif (${PROJECT_PREFIX}_ARCH_STR STREQUAL "arm64")
    set(${PROJECT_PREFIX}_PLATFORM_ARM64 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_ARM64)
elseif (${PROJECT_PREFIX}_ARCH_STR STREQUAL "wasm32")
    set(${PROJECT_PREFIX}_PLATFORM_WASM32 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_WASM32)
else ()
    message(FATAL_ERROR "Unsupported Target architecture '${${PROJECT_PREFIX}_ARCH_STR}'")
endif ()
#
# ---=== Supported Compiler ===----
#
set(${PROJECT_PREFIX}_GNU_MINIMAL_STRICT 11)
set(${PROJECT_PREFIX}_CLANG_MINIMAL_STRICT 15)
set(${PROJECT_PREFIX}_GNU_MINIMAL 13)
set(${PROJECT_PREFIX}_CLANG_MINIMAL 18)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PROJECT_PREFIX}_GNU_MINIMAL_STRICT})
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PROJECT_PREFIX}_GNU_MINIMAL}")
    endif ()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PROJECT_PREFIX}_GNU_MINIMAL})
        message(WARNING "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PROJECT_PREFIX}_GNU_MINIMAL}")
    endif ()
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
            -Werror -Wall -Wextra -pedantic
            -Wdeprecated
            -Wdeprecated-declarations
            -Wcast-align
            -Wcast-qual
            -Wno-mismatched-new-delete
    )
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE
            STBI_NO_SIMD)
    set(${PROJECT_PREFIX}_COMPILER_GCC ON)
    set(${PROJECT_PREFIX}_COMPILER_STR "gcc")
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PROJECT_PREFIX}_CLANG_MINIMAL_STRICT})
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PROJECT_PREFIX}_CLANG_MINIMAL_STRICT}")
    endif ()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PROJECT_PREFIX}_CLANG_MINIMAL})
        message(WARNING "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PROJECT_PREFIX}_CLANG_MINIMAL}")
    endif ()
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
            -Werror -Weverything -pedantic
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-c++20-compat
            -Wno-padded
            -Wno-exit-time-destructors
            -Wno-global-constructors # Only in gtest -> only for tests
            # No header mtime in the precompiled header: a fresh checkout would change it, and with it the ccache key
            # of every unit using it (ninja's dependencies already rebuild it when a header changes).
            "SHELL:-Xclang -fno-pch-timestamp"
    )
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 18)
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
                -Wno-switch-default
                -Wno-unsafe-buffer-usage
        )
    endif ()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 20)
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
                -Wno-unsafe-buffer-usage-in-libc-call
        )
    endif ()
    # Probed rather than versioned: the NDK's Clang says 21 without the warning.
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-Wnrvo ${PROJECT_PREFIX}_HAS_WARNING_NRVO)
    if (${PROJECT_PREFIX}_HAS_WARNING_NRVO)
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
                -Wno-nrvo
        )
    endif ()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 23)
        # Suggestions to mark every getter returning a member reference `[[clang::lifetimebound]]` (intra- and
        # cross-TU). Of the bug detectors, only the permissive set stays on: the strict-only members report a
        # `std::vector&` parameter as invalidated by its own `push_back`.
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
                -Wno-lifetime-safety-suggestions
                -Wno-lifetime-safety-invalidation
                -Wno-lifetime-safety-use-after-scope-moved
                -Wno-lifetime-safety-return-stack-addr-moved
                -Wno-lifetime-safety-dangling-field-moved
                -Wno-lifetime-safety-dangling-global-moved
        )
    endif ()
    set(${PROJECT_PREFIX}_COMPILER_CLANG ON)
    set(${PROJECT_PREFIX}_COMPILER_STR "clang")
else ()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()
# Bit-exact simulation on every target: no fused multiply-add (Clang fuses `a*b+c` on arm64, GCC in GNU mode),
# no fast maths, denormals kept (doc/pages/design/simulation-networking.md).
target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -ffp-contract=off -fno-fast-math)

# Cross compilation
if (NOT (${${PROJECT_PREFIX}_HOST_PLATFORM_STR} STREQUAL ${${PROJECT_PREFIX}_PLATFORM_STR} AND
        ${${PROJECT_PREFIX}_HOST_ARCH_STR} STREQUAL ${${PROJECT_PREFIX}_ARCH_STR}))
    set(${PROJECT_PREFIX}_CROSS_COMPILATION ON)
endif ()

# sum up the system
print_system_n_target_infos()

#
# --== Properties ==--
#
get_property(${PROJECT_PREFIX}_IS_GENERATOR_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if (${${PROJECT_PREFIX}_IS_GENERATOR_MULTI_CONFIG})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
else ()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

# Only a default: a preset, -DCMAKE_INSTALL_PREFIX or a package manager keeps its own prefix.
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/output/install CACHE PATH "Install prefix" FORCE)
endif ()

set(${PROJECT_PREFIX}_INSTALL_BIN "bin/${${PROJECT_PREFIX}_PLATFORM_STR}_${${PROJECT_PREFIX}_ARCH_STR}")
set(${PROJECT_PREFIX}_INSTALL_LIB "lib/${${PROJECT_PREFIX}_PLATFORM_STR}_${${PROJECT_PREFIX}_ARCH_STR}")

add_custom_target(${CMAKE_PROJECT_NAME}_SuperBase)
set_target_properties(${CMAKE_PROJECT_NAME}_SuperBase PROPERTIES FOLDER "Utils")
if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    if (MINGW)
        cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH COMPILER_PATH)
        message(STATUS "MinGW environment detected: add dependence to dlls from ${COMPILER_PATH}")
        set(REQUIRED_LIBS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libpng16-16.dll)
        foreach (lib IN ITEMS ${REQUIRED_LIBS})
            if (NOT EXISTS ${COMPILER_PATH}/${lib})
                message(WARNING "Required Dll not found: ${COMPILER_PATH}/${lib}")
            else ()
                message(STATUS "Adding Dll: ${COMPILER_PATH}/${lib}")
                add_custom_command(TARGET ${CMAKE_PROJECT_NAME}_SuperBase POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_PATH}/${lib} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
                        COMMENT "Copying ${lib} into ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
            endif ()
        endforeach ()
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -U_GLIBCXX_DEBUG)
        # PE/COFF section table is 16-bit by default (max 65535 sections). Heavy template
        # instantiations in Scene.cpp / SceneSerializer.cpp exceed that limit on
        # mingw-gcc 15+ and the linker reports "relocation truncated to fit" against
        # `.text$<symbol>` / `.pdata$<symbol>` / `.xdata$<symbol>`. The big-obj COFF
        # variant lifts the limit to ~4 billion sections; required to keep the project
        # link-clean as the engine grows. Apply globally to avoid whack-a-mole as more
        # template-heavy TUs land.
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -Wa,-mbig-obj)
    endif ()
elseif (${PROJECT_PREFIX}_PLATFORM_LINUX)
    set(CMAKE_PLATFORM_USES_PATH_WHEN_NO_SONAME OFF)
    # --as-needed (Ubuntu's gcc default, not clang's): Conan's xorg/system lists every libxcb-*, and a binary
    # recording all of them as NEEDED does not start on a desktop missing an unused one (libxcb-ewmh2).
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed -Wl,-rpath='$ORIGIN' -Wl,--disable-new-dtags")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed -Wl,-rpath='$ORIGIN' -Wl,--disable-new-dtags")
elseif (${PROJECT_PREFIX}_PLATFORM_WEB)
    # The task system needs threads: Web Workers on a shared memory, so the page must be cross-origin isolated.
    # Native WebAssembly exceptions, as the Conan packages (conan/profiles/emscripten-clang). musl defines `stderr` as
    # itself: every use expands a recursive macro. No shared library, so no object duplicated across them.
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -pthread -fwasm-exceptions -Wno-disabled-macro-expansion
            -Wno-unique-object-duplication)
    # Stacks of a desktop thread rather than Emscripten's 64 KB: the script compilers and Jolt recurse deeply.
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -pthread -fwasm-exceptions "SHELL:-s ALLOW_MEMORY_GROWTH=1"
            "SHELL:-s STACK_SIZE=8MB" "SHELL:-s DEFAULT_PTHREAD_STACK_SIZE=2MB")
endif ()
add_dependencies(${CMAKE_PROJECT_NAME}_Base ${CMAKE_PROJECT_NAME}_SuperBase)

# The version is project(VERSION) in CMakeLists.txt, the one place to edit: the engine reads it through
# getVersion*() (core/Core.h), CPack, Conan, Doxygen and the CI from there.
#
# OWL_GIT_HASH: abbreviated commit of the configured tree (package names, getVersionCommit()).
#
#
# Git may legitimately fail here (shallow clones, out-of-tree builds, and — the case that
# motivated this fallback — a checkout made on a Windows host mounted into a Linux container,
# where `.git/objects/info/alternates` points to a Windows path that can't be resolved).  When
# that happens we fall back to GitLab's `CI_COMMIT_SHA` environment variable; if that is
# also unavailable we settle for a literal so the downstream `SUBSTRING` stays well-formed.
if (NOT ${PROJECT_PREFIX}_GIT_HASH)
    execute_process(
            COMMAND git log -1 --format=%h
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE ${PROJECT_PREFIX}_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _owl_git_hash_result
            ERROR_QUIET
    )
    if (NOT _owl_git_hash_result EQUAL 0 OR "${${PROJECT_PREFIX}_GIT_HASH}" STREQUAL "")
        if (DEFINED ENV{CI_COMMIT_SHA} AND NOT "$ENV{CI_COMMIT_SHA}" STREQUAL "")
            set(${PROJECT_PREFIX}_GIT_HASH "$ENV{CI_COMMIT_SHA}")
        else ()
            set(${PROJECT_PREFIX}_GIT_HASH "unknown")
        endif ()
    endif ()
endif ()
# Quoting the expansion guards against an empty value collapsing into zero arguments.
string(SUBSTRING "${${PROJECT_PREFIX}_GIT_HASH}" 0 7 ${PROJECT_PREFIX}_GIT_HASH)
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PROJECT_PREFIX}_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PROJECT_PREFIX}_MINOR=${CMAKE_PROJECT_VERSION_MINOR}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PROJECT_PREFIX}_PATCH=${CMAKE_PROJECT_VERSION_PATCH}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_AUTHOR="Silmaen")

if (${${PROJECT_PREFIX}_IS_GENERATOR_MULTI_CONFIG})
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_$<IF:$<CONFIG:Debug>,DEBUG_BUILD,RELEASE_BUILD>)
else ()
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_DEBUG_BUILD)
    else ()
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_RELEASE_BUILD)
    endif ()
endif ()

#
# Debugging options
#
if (${PROJECT_PREFIX}_PLATFORM_WEB AND ${PROJECT_PREFIX}_ENABLE_STACKTRACE)
    message(WARNING "No stack trace in WebAssembly: OWL_ENABLE_STACKTRACE turned OFF.")
    set(${PROJECT_PREFIX}_ENABLE_STACKTRACE OFF CACHE BOOL "" FORCE)
endif ()
if (${PROJECT_PREFIX}_ENABLE_MEMORY_TRACKER OR ${PROJECT_PREFIX}_ENABLE_STACKTRACE)
    # Stacktrace lives on top of the tracker — turning it on implies the tracker.
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_MEMORY_TRACKER)
endif ()
if (${PROJECT_PREFIX}_ENABLE_STACKTRACE)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_STACKTRACE)
endif ()
# Profiler behind the OWL_PROFILE_* macros (doc/pages/profiling.md); OWL_PROFILER_BACKEND is the one in effect.
set(${PROJECT_PREFIX}_PROFILER_BACKEND "${${PROJECT_PREFIX}_PROFILER}")
if (${PROJECT_PREFIX}_ENABLE_PROFILING AND ${PROJECT_PREFIX}_PROFILER_BACKEND STREQUAL "none")
    message(DEPRECATION "${PROJECT_PREFIX}_ENABLE_PROFILING is deprecated, use -D${PROJECT_PREFIX}_PROFILER=chrome.")
    set(${PROJECT_PREFIX}_PROFILER_BACKEND "chrome")
endif ()
if (${PROJECT_PREFIX}_PROFILER_BACKEND STREQUAL "tracy")
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PROFILER_TRACY)
elseif (${PROJECT_PREFIX}_PROFILER_BACKEND STREQUAL "chrome")
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PROFILER_CHROME)
elseif (NOT ${PROJECT_PREFIX}_PROFILER_BACKEND STREQUAL "none")
    message(FATAL_ERROR "Unknown ${PROJECT_PREFIX}_PROFILER '${${PROJECT_PREFIX}_PROFILER}' (none, tracy or chrome).")
endif ()
message(STATUS "Profiler: ${${PROJECT_PREFIX}_PROFILER_BACKEND}")
# Lowest log level compiled in: the OWL_*_TRACE ... macros below it expand to a discarded statement.
set(_owl_log_levels trace debug info warning error critical off)
list(FIND _owl_log_levels "${${PROJECT_PREFIX}_LOG_LEVEL}" _owl_log_level)
if (_owl_log_level LESS 0)
    message(FATAL_ERROR "Unknown ${PROJECT_PREFIX}_LOG_LEVEL '${${PROJECT_PREFIX}_LOG_LEVEL}' (${_owl_log_levels}).")
endif ()
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_LOG_COMPILED_LEVEL=${_owl_log_level})
unset(_owl_log_levels)
unset(_owl_log_level)
if (${PROJECT_PREFIX}_ENABLE_RENDERER_VERBOSE_CAPABILITIES)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_RENDERER_VERBOSE_CAPABILITIES)
endif ()
if (${PROJECT_PREFIX}_ENABLE_SHADER_REFLECT_RESOURCES)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SHADER_REFLECT_RESOURCES)
endif ()


if (${PROJECT_PREFIX}_ENABLE_COVERAGE)
    include(cmake/CoverageConfig.cmake)
endif ()

include(cmake/DocumentationConfig.cmake)

include(Sanitizers)
include(Vulkan)
