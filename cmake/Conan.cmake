#
# Third-party dependencies from Conan 2 (OWL_DEPENDENCY_PROVIDER=conan).
#
# Runs `conan install` on the root `conanfile.py` at configure time, the way `Depmanager.cmake` loads its
# environment, then puts the generated CMakeDeps files on CMAKE_PREFIX_PATH. Conan itself comes from the
# Poetry environment (dev group).
#
set(${PROJECT_PREFIX}_CONAN_PROFILE "" CACHE STRING
        "Conan profile for host and build (default: conan/profiles/<os>-<compiler>)")
set(${PROJECT_PREFIX}_CONAN_HOME "" CACHE PATH "CONAN_HOME used for the install (empty: Conan's default)")
set(${PROJECT_PREFIX}_CONAN_BUILD "missing" CACHE STRING "Value of conan install --build")
set(${PROJECT_PREFIX}_CONAN_LOCKFILE "${CMAKE_SOURCE_DIR}/conan.lock" CACHE FILEPATH
        "Conan lockfile pinning every recipe revision (empty: resolve the versions of conanfile.py)")
# OFF when Conan drives CMake itself (`conan create`, see conanfile.py): the dependencies are then already
# installed and conan_toolchain.cmake puts them on CMAKE_PREFIX_PATH.
option(${PROJECT_PREFIX}_CONAN_INSTALL "Run conan install at configure time" ON)

if (${PROJECT_PREFIX}_CONAN_INSTALL)
    if (NOT ${PROJECT_PREFIX}_CONAN_PROFILE)
        if (${PROJECT_PREFIX}_COMPILER_CLANG OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(_owl_conan_compiler clang)
        else ()
            set(_owl_conan_compiler gcc)
        endif ()
        string(TOLOWER "${CMAKE_SYSTEM_NAME}" _owl_conan_os)
        set(${PROJECT_PREFIX}_CONAN_PROFILE "${CMAKE_SOURCE_DIR}/conan/profiles/${_owl_conan_os}-${_owl_conan_compiler}")
    endif ()
    if (NOT EXISTS "${${PROJECT_PREFIX}_CONAN_PROFILE}")
        message(FATAL_ERROR "Conan profile '${${PROJECT_PREFIX}_CONAN_PROFILE}' not found.")
    endif ()

    # Release third parties in a Debug build, as with DepManager (CMAKE_MAP_IMPORTED_CONFIG_DEBUG maps them).
    if (${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY OR NOT CMAKE_BUILD_TYPE)
        set(${PROJECT_PREFIX}_CONAN_BUILD_TYPE Release)
    else ()
        set(${PROJECT_PREFIX}_CONAN_BUILD_TYPE ${CMAKE_BUILD_TYPE})
    endif ()
    if (${PROJECT_PREFIX}_BUILD_SHARED)
        set(_owl_conan_shared True)
    else ()
        set(_owl_conan_shared False)
    endif ()
    if (${PROJECT_PREFIX}_TESTING)
        set(_owl_conan_testing True)
    else ()
        set(_owl_conan_testing False)
    endif ()
    if (${PROJECT_PREFIX}_BUILD_NEST)
        set(_owl_conan_nest True)
    else ()
        set(_owl_conan_nest False)
    endif ()

    set(_owl_conan_env)
    if (${PROJECT_PREFIX}_CONAN_HOME)
        set(_owl_conan_env ${CMAKE_COMMAND} -E env "CONAN_HOME=${${PROJECT_PREFIX}_CONAN_HOME}")
    endif ()
    set(_owl_conan ${_owl_conan_env} ${Poetry_PREFIX} conan)
    set(_owl_conan_output "${CMAKE_BINARY_DIR}/conan")

    execute_process(COMMAND ${_owl_conan} --version
            OUTPUT_VARIABLE _owl_conan_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _owl_conan_result)
    if (NOT _owl_conan_result EQUAL 0)
        message(FATAL_ERROR "Conan is not available through Poetry ('poetry sync --no-root' installs it).")
    endif ()
    message(STATUS "${_owl_conan_version}, profile ${${PROJECT_PREFIX}_CONAN_PROFILE}")

    # Recipes missing from ConanCenter (conan/recipes/), served as a local-recipes-index remote. It comes first, so a
    # local recipe wins over a ConanCenter one of the same name and version (msdf-atlas-gen).
    execute_process(COMMAND ${_owl_conan} remote add owl-local "${CMAKE_SOURCE_DIR}/conan"
            --type=local-recipes-index --force --index 0
            OUTPUT_QUIET
            RESULT_VARIABLE _owl_conan_result)
    if (NOT _owl_conan_result EQUAL 0)
        message(FATAL_ERROR "Unable to register the 'owl-local' Conan remote (${CMAKE_SOURCE_DIR}/conan).")
    endif ()

    # A local recipe edited in the repository must replace the revision already in the Conan cache.
    file(GLOB _owl_conan_local_recipes RELATIVE "${CMAKE_SOURCE_DIR}/conan/recipes" "${CMAKE_SOURCE_DIR}/conan/recipes/*")
    set(_owl_conan_update)
    foreach (_owl_recipe IN LISTS _owl_conan_local_recipes)
        list(APPEND _owl_conan_update "--update=${_owl_recipe}")
    endforeach ()

    set(_owl_conan_lock)
    if (${PROJECT_PREFIX}_CONAN_LOCKFILE)
        if (NOT EXISTS "${${PROJECT_PREFIX}_CONAN_LOCKFILE}")
            message(FATAL_ERROR "Conan lockfile '${${PROJECT_PREFIX}_CONAN_LOCKFILE}' not found.")
        endif ()
        set(_owl_conan_lock --lockfile "${${PROJECT_PREFIX}_CONAN_LOCKFILE}")
        if (WIN32)
            # conan.lock is resolved with the Linux profiles; Windows-only requirements are not in it yet.
            list(APPEND _owl_conan_lock --lockfile-partial)
        endif ()
        message(STATUS "Conan lockfile ${${PROJECT_PREFIX}_CONAN_LOCKFILE}")
    endif ()

    # Root of the Conan package cache: the shared libraries found there are copied next to the binaries
    # (target_import_so_files), so they run and package without the cache.
    execute_process(COMMAND ${_owl_conan} config home
            OUTPUT_VARIABLE _owl_conan_home
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _owl_conan_result)
    if (_owl_conan_result EQUAL 0)
        set(${PROJECT_PREFIX}_SHARED_LIB_ROOTS "${_owl_conan_home}/p" CACHE INTERNAL "")
    endif ()

    # Windows has no RPATH: copy every shared library of the dependencies next to the binaries.
    set(_owl_conan_deploy)
    if (WIN32)
        set(_owl_conan_deploy --deployer=runtime_deploy "--deployer-folder=${CMAKE_BINARY_DIR}/bin")
    endif ()

    message(STATUS "Conan install (build type ${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}) into ${_owl_conan_output}")
    execute_process(COMMAND ${_owl_conan} install "${CMAKE_SOURCE_DIR}"
            --output-folder "${_owl_conan_output}"
            --profile:all "${${PROJECT_PREFIX}_CONAN_PROFILE}"
            --settings:host "build_type=${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}"
            --options:host "&:shared=${_owl_conan_shared}"
            --options:host "&:testing=${_owl_conan_testing}"
            --options:host "&:nest=${_owl_conan_nest}"
            --build=${${PROJECT_PREFIX}_CONAN_BUILD}
            ${_owl_conan_update}
            ${_owl_conan_lock}
            ${_owl_conan_deploy}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE _owl_conan_result)
    if (NOT _owl_conan_result EQUAL 0)
        message(FATAL_ERROR "conan install failed (see the output above).")
    endif ()

    list(PREPEND CMAKE_PREFIX_PATH "${_owl_conan_output}")
    list(PREPEND CMAKE_MODULE_PATH "${_owl_conan_output}")
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
else ()
    set(${PROJECT_PREFIX}_CONAN_BUILD_TYPE ${CMAKE_BUILD_TYPE})
    if (NOT ${PROJECT_PREFIX}_CONAN_BUILD_TYPE)
        set(${PROJECT_PREFIX}_CONAN_BUILD_TYPE Release)
    endif ()
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
    message(STATUS "Conan install skipped: the dependencies come from conan_toolchain.cmake.")
endif ()

execute_process(COMMAND getconf GNU_LIBC_VERSION
        OUTPUT_VARIABLE _owl_glibc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
if (_owl_glibc MATCHES "glibc ([0-9]+\\.[0-9]+)")
    set(${PROJECT_PREFIX}_GLIBC_STR "glibc_${CMAKE_MATCH_1}")
endif ()

# Packages whose CMake name or target differs from the DepManager one used in the CMakeLists.
set(${PROJECT_PREFIX}_CONAN_PACKAGE_stb_image stb)
set(${PROJECT_PREFIX}_CONAN_TARGET_stb_image stb::stb)
set(${PROJECT_PREFIX}_CONAN_TARGET_TinyGLTF TinyGLTF::TinyGLTF)
# lunasvg: ConanCenter exports `include/`, the headers live in `include/lunasvg/` (DepManager exports the latter).
set(${PROJECT_PREFIX}_CONAN_INCLUDE_SUBDIR_lunasvg lunasvg)

unset(_owl_conan_compiler)
unset(_owl_conan_os)
unset(_owl_conan_shared)
unset(_owl_conan_testing)
unset(_owl_conan_nest)
unset(_owl_conan_env)
unset(_owl_conan_local_recipes)
unset(_owl_conan_update)
unset(_owl_recipe)
unset(_owl_conan_result)
unset(_owl_conan_version)
unset(_owl_conan_lock)
unset(_owl_conan_home)
unset(_owl_glibc)
message(STATUS "Third parties loaded from Conan.")
