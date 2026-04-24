
set(EDEPMANAGER ${Poetry_PREFIX} depmanager)
string(REPLACE ";" " " EDMGR "${EDEPMANAGER}")
execute_process(COMMAND ${EDEPMANAGER} info version --raw
        OUTPUT_VARIABLE depmanager_version
        RESULT_VARIABLE depmanager_check_result)
if (NOT depmanager_check_result EQUAL 0)
    message(FATAL_ERROR "Depmanager command '${EDMGR}' is not installed or not available in the Python environment. Please install Depmanager to manage third party dependencies.")
else ()
    string(STRIP ${depmanager_version} depmanager_version)
    string(REPLACE "depmanager" "" depmanager_version ${depmanager_version})
    string(REPLACE "version" "" depmanager_version ${depmanager_version})
    string(STRIP ${depmanager_version} depmanager_version)
    message(STATUS "Depmanager found: '${EDMGR}' version ${depmanager_version}")

    execute_process(COMMAND ${EDEPMANAGER} info cmakedir --raw
            OUTPUT_VARIABLE depmanager_cmake_dir)
    string(STRIP ${depmanager_cmake_dir} depmanager_cmake_dir)
    string(REPLACE "\\" "/" depmanager_cmake_dir ${depmanager_cmake_dir})
    string(REPLACE "\n" "" depmanager_cmake_dir ${depmanager_cmake_dir})
    string(REPLACE "\r" "" depmanager_cmake_dir ${depmanager_cmake_dir})
    message(STATUS "Depmanager cmake modules path: ${depmanager_cmake_dir}")
    if (NOT EXISTS ${depmanager_cmake_dir}/DepManager.cmake)
        message(FATAL_ERROR "Depmanager cmake modules not found at: ${depmanager_cmake_dir}")
    endif ()

    # The upstream DepManager.cmake uses `find_program(DM_INTERNAL_COMMAND depmanager)` which
    # searches $PATH and can pick a stale system-wide binary (e.g. leftover `/usr/local/bin/depmanager`
    # whose shebang points at a system Python that no longer has the module installed). Resolve
    # the binary from the Poetry venv ourselves and pre-seed the cache so the upstream find_program
    # short-circuits.
    execute_process(COMMAND ${Poetry_PREFIX} python -c "import shutil, sys; p = shutil.which('depmanager'); print(p) if p else sys.exit(1)"
            OUTPUT_VARIABLE _owl_depmanager_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _owl_depmanager_find_result)
    if (_owl_depmanager_find_result EQUAL 0 AND _owl_depmanager_path)
        set(DM_INTERNAL_COMMAND "${_owl_depmanager_path}" CACHE FILEPATH "Path to depmanager binary" FORCE)
        message(STATUS "Depmanager binary pinned to Poetry venv: ${DM_INTERNAL_COMMAND}")
    endif ()
    unset(_owl_depmanager_path)
    unset(_owl_depmanager_find_result)

    list(PREPEND CMAKE_MODULE_PATH ${depmanager_cmake_dir})
    include(DepManager)
endif ()

if (${${PROJECT_PREFIX}_BUILD_SHARED})
    set(LOCAL_KIND shared)
else ()
    set(LOCAL_KIND static)
endif ()

if (${PROJECT_PREFIX}_CROSS_COMPILATION)
    message(STATUS "Loading '${LOCAL_KIND}' third parties for ${${PROJECT_PREFIX}_ARCH_STR}")
    dm_load_environment(KIND ${LOCAL_KIND} ARCH ${${PROJECT_PREFIX}_ARCH_STR})
else ()
    message(STATUS "Loading '${LOCAL_KIND}' third parties for native")
    dm_load_environment(KIND ${LOCAL_KIND})
endif ()

dm_get_glibc(GLIBC_VERSION)
if (${GLIBC_VERSION})
    set(${PROJECT_PREFIX}_GLIBC_STR "glibc_${GLIBC_VERSION}")
endif ()
message(STATUS "Third parties loaded.")
message(STATUS "-----------------------------------")
message(STATUS "CMAKE_PREFIX_PATH:")
foreach (path IN LISTS CMAKE_PREFIX_PATH)
    if (path STREQUAL "")
        continue ()
    endif ()
    message(STATUS "  - ${path}")
endforeach ()
message(STATUS "CMAKE_MODULE_PATH:")
foreach (path IN LISTS CMAKE_MODULE_PATH)
    if (path STREQUAL "")
        continue ()
    endif ()
    message(STATUS "  - ${path}")
endforeach ()
message(STATUS "-----------------------------------")
