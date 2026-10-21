#
# CompilerCache.cmake — wire ccache (or sccache) as the C/C++ compiler launcher
# when available. Honours the OWL_USE_CCACHE option; silently no-ops when the
# binary is missing so dev machines without ccache keep building.
#
# CCACHE_DIR is read from the environment so the same configuration works for
# local devs (default `~/.cache/ccache`) and for TeamCity (the docker container
# is launched with `-e CCACHE_DIR=/tmp/cache_dir/ccache`, see CMakePresetsBase.json).
#

if (NOT ${PROJECT_PREFIX}_USE_CCACHE)
    return()
endif ()

find_program(${PROJECT_PREFIX}_CCACHE_PROGRAM
        NAMES ccache sccache
        DOC "Path to ccache or sccache, used as compiler launcher"
)

if (NOT ${PROJECT_PREFIX}_CCACHE_PROGRAM)
    message(STATUS "Owl: ccache/sccache not found — compiler launcher disabled")
    return()
endif ()

execute_process(
        COMMAND ${${PROJECT_PREFIX}_CCACHE_PROGRAM} --version
        OUTPUT_VARIABLE _OWL_CCACHE_VERSION_OUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" _OWL_CCACHE_VERSION "${_OWL_CCACHE_VERSION_OUT}")

# Without this sloppiness ccache refuses every compilation that uses a precompiled header
# (`could_not_use_precompiled_header`), nearly all of them here. ccache 4.8+ reads it on its command line, so no
# agent needs a config file.
set(_OWL_LAUNCHER "${${PROJECT_PREFIX}_CCACHE_PROGRAM}")
get_filename_component(_OWL_LAUNCHER_NAME "${${PROJECT_PREFIX}_CCACHE_PROGRAM}" NAME_WE)
if (_OWL_LAUNCHER_NAME STREQUAL "ccache" AND _OWL_CCACHE_VERSION VERSION_GREATER_EQUAL 4.8)
    list(APPEND _OWL_LAUNCHER "sloppiness=pch_defines,time_macros,include_file_mtime,include_file_ctime")
endif ()
# A tree configured with the bare launcher gets the new one.
foreach (_OWL_LANG C CXX)
    if ("${CMAKE_${_OWL_LANG}_COMPILER_LAUNCHER}" STREQUAL "${${PROJECT_PREFIX}_CCACHE_PROGRAM}")
        unset(CMAKE_${_OWL_LANG}_COMPILER_LAUNCHER CACHE)
    endif ()
    set(CMAKE_${_OWL_LANG}_COMPILER_LAUNCHER "${_OWL_LAUNCHER}" CACHE STRING "${_OWL_LANG} compiler launcher")
endforeach ()

if (DEFINED ENV{CCACHE_DIR})
    set(_OWL_CCACHE_DIR "$ENV{CCACHE_DIR}")
else ()
    set(_OWL_CCACHE_DIR "(default — \$HOME/.cache/ccache or \$XDG_CACHE_HOME/ccache)")
endif ()

message(STATUS "Owl: compiler launcher = ${CMAKE_CXX_COMPILER_LAUNCHER} (${_OWL_CCACHE_VERSION})")
message(STATUS "Owl: CCACHE_DIR        = ${_OWL_CCACHE_DIR}")

unset(_OWL_CCACHE_VERSION_OUT)
unset(_OWL_CCACHE_VERSION)
unset(_OWL_CCACHE_DIR)
unset(_OWL_LAUNCHER)
unset(_OWL_LAUNCHER_NAME)
unset(_OWL_LANG)
