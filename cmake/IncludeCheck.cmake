#
# IncludeCheck.cmake — "include what you use" gate for the standard library.
#
# A recent libstdc++ (MSYS2 MinGW) no longer drags <cstdint>, <mutex>, … in transitively, so a file
# that relies on such an accidental include compiles on Linux and breaks on Windows. The precompiled
# header hides the problem everywhere. This module reproduces the strict behaviour on Linux: every
# header is compiled alone in a generated translation unit, and every source file is compiled again,
# both against libc++ with `_LIBCPP_REMOVE_TRANSITIVE_INCLUDES` and without the precompiled header.
#
# Targets (all EXCLUDE_FROM_ALL, compile-only — nothing is linked):
#   owl_header_check   every header of the engine, the editor, the runner, the test helpers, the bench
#   owl_source_check   every .cpp of the same targets
#   owl_include_check  both
#
# Enabled by OWL_INCLUDE_CHECK (OFF by default); needs Clang and libc++.
#

if (NOT ${PROJECT_PREFIX}_INCLUDE_CHECK)
    return()
endif ()

if (NOT ${PROJECT_PREFIX}_COMPILER_CLANG)
    message(FATAL_ERROR "${PROJECT_PREFIX}_INCLUDE_CHECK needs Clang (libc++), current compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-stdlib=libc++")
check_cxx_source_compiles("#include <version>\n#ifndef _LIBCPP_VERSION\n#error not libc++\n#endif\nint main() { return 0; }"
        ${PROJECT_PREFIX}_HAS_LIBCXX)
unset(CMAKE_REQUIRED_FLAGS)
if (NOT ${PROJECT_PREFIX}_HAS_LIBCXX)
    message(FATAL_ERROR "${PROJECT_PREFIX}_INCLUDE_CHECK needs libc++ (-stdlib=libc++), not found for ${CMAKE_CXX_COMPILER}")
endif ()

set(${PROJECT_PREFIX}_INCLUDE_CHECK_DIR "${CMAKE_BINARY_DIR}/include_check")
add_custom_target(${PROJECT_PREFIX_LOWER}_header_check)
add_custom_target(${PROJECT_PREFIX_LOWER}_source_check)
add_custom_target(${PROJECT_PREFIX_LOWER}_include_check)
add_dependencies(${PROJECT_PREFIX_LOWER}_include_check
        ${PROJECT_PREFIX_LOWER}_header_check ${PROJECT_PREFIX_LOWER}_source_check)
set_target_properties(${PROJECT_PREFIX_LOWER}_header_check ${PROJECT_PREFIX_LOWER}_source_check
        ${PROJECT_PREFIX_LOWER}_include_check PROPERTIES FOLDER "Utils")

#
# Create a compile-only object library that borrows the full (transitive) compile flags of iTarget, minus
# its precompiled header, and builds it against strict libc++.
#
function(owl_include_check_library iName iTarget)
    add_library(${iName} OBJECT EXCLUDE_FROM_ALL ${ARGN})
    target_include_directories(${iName} PRIVATE $<TARGET_PROPERTY:${iTarget},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${iName} PRIVATE $<TARGET_PROPERTY:${iTarget},COMPILE_DEFINITIONS>)
    # Warnings are the job of the normal build: the borrowed third-party include paths lose their
    # SYSTEM flag here, so only hard errors (the missing declarations) are kept.
    target_compile_options(${iName} PRIVATE $<TARGET_PROPERTY:${iTarget},COMPILE_OPTIONS>
            -stdlib=libc++ -D_LIBCPP_REMOVE_TRANSITIVE_INCLUDES -Wno-everything)
    set_target_properties(${iName} PROPERTIES
            DISABLE_PRECOMPILE_HEADERS ON
            CXX_STANDARD ${CMAKE_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON
            FOLDER "Utils/IncludeCheck")
endfunction()

#
# Compile every header matching the globs alone, with the flags of iTarget.
#
function(owl_add_header_check iTarget)
    if (NOT TARGET ${iTarget})
        return()
    endif ()
    file(GLOB_RECURSE l_headers CONFIGURE_DEPENDS ${ARGN})
    if ("${l_headers}" STREQUAL "")
        return()
    endif ()
    set(l_tus)
    foreach (l_header IN LISTS l_headers)
        file(RELATIVE_PATH l_rel "${CMAKE_SOURCE_DIR}" "${l_header}")
        string(REGEX REPLACE "\\.[^.]*$" ".cpp" l_tu "${${PROJECT_PREFIX}_INCLUDE_CHECK_DIR}/headers/${l_rel}")
        file(CONFIGURE OUTPUT "${l_tu}" CONTENT "#include \"${l_header}\"\n" @ONLY)
        list(APPEND l_tus "${l_tu}")
    endforeach ()
    owl_include_check_library(${PROJECT_PREFIX_LOWER}_header_check_${iTarget} ${iTarget} ${l_tus})
    add_dependencies(${PROJECT_PREFIX_LOWER}_header_check ${PROJECT_PREFIX_LOWER}_header_check_${iTarget})
endfunction()

#
# Compile every .cpp of iTarget again, with its own flags.
#
function(owl_add_source_check iTarget)
    if (NOT TARGET ${iTarget})
        return()
    endif ()
    get_target_property(l_sources ${iTarget} SOURCES)
    get_target_property(l_dir ${iTarget} SOURCE_DIR)
    set(l_cpps)
    foreach (l_source IN LISTS l_sources)
        if (NOT l_source MATCHES "\\.cpp$")
            continue()
        endif ()
        cmake_path(ABSOLUTE_PATH l_source BASE_DIRECTORY "${l_dir}")
        list(APPEND l_cpps "${l_source}")
    endforeach ()
    if ("${l_cpps}" STREQUAL "")
        return()
    endif ()
    owl_include_check_library(${PROJECT_PREFIX_LOWER}_source_check_${iTarget} ${iTarget} ${l_cpps})
    add_dependencies(${PROJECT_PREFIX_LOWER}_source_check ${PROJECT_PREFIX_LOWER}_source_check_${iTarget})
endfunction()

#
# Register the checks once every real target exists (called at the end of the top-level CMakeLists).
#
function(owl_setup_include_check)
    set(l_src "${CMAKE_SOURCE_DIR}/source")
    owl_add_header_check(${ENGINE_NAME} "${l_src}/owl/public/*.h" "${l_src}/owl/private/*.h")
    owl_add_header_check(${CMAKE_PROJECT_NAME}Nest "${l_src}/owlnest/sources/*.h")
    owl_add_header_check(${CMAKE_PROJECT_NAME}Runner "${l_src}/owlnest/runner/*.h")
    owl_add_header_check(${PROJECT_PREFIX_LOWER}_core_tests_unit_test "${CMAKE_SOURCE_DIR}/test/test_helper/*.h")
    owl_add_header_check(${PROJECT_PREFIX_LOWER}_bench "${CMAKE_SOURCE_DIR}/bench/*.h")

    set(l_targets ${ENGINE_NAME} ${CMAKE_PROJECT_NAME}Nest ${CMAKE_PROJECT_NAME}Runner ${PROJECT_PREFIX_LOWER}_bench)
    if (EXISTS "${CMAKE_SOURCE_DIR}/test" AND TARGET All_Tests)
        get_property(l_tests DIRECTORY "${CMAKE_SOURCE_DIR}/test" PROPERTY BUILDSYSTEM_TARGETS)
        list(FILTER l_tests INCLUDE REGEX "_unit_test$")
        list(APPEND l_targets ${l_tests})
    endif ()
    foreach (l_target IN LISTS l_targets)
        owl_add_source_check(${l_target})
    endforeach ()
endfunction()
