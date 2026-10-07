#
# Third-party dependencies from Conan 2, the only provider.
#
# Runs `conan install` on the root `conanfile.py` at configure time, then puts the generated CMakeDeps files on
# CMAKE_PREFIX_PATH. Conan itself comes from the Poetry environment (dev group).
#
set(${PROJECT_PREFIX}_CONAN_PROFILE "" CACHE STRING
        "Conan profile for host and build (default: conan/profiles/<os>-<compiler>)")
set(${PROJECT_PREFIX}_CONAN_HOME "" CACHE PATH "CONAN_HOME used for the install (empty: Conan's default)")
set(${PROJECT_PREFIX}_CONAN_BUILD "missing" CACHE STRING "Value of conan install --build")
set(${PROJECT_PREFIX}_CONAN_CACHE_URL "" CACHE STRING
        "Conan server used as a binary cache (empty: environment variable of the same name, else none)")
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

    # Release third parties in a Debug build (CMAKE_MAP_IMPORTED_CONFIG_DEBUG maps them).
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
    # Re-adding it wipes its exported recipes, so only when it is missing or points elsewhere.
    execute_process(COMMAND ${_owl_conan} remote list --format=json
            OUTPUT_VARIABLE _owl_conan_remotes
            ERROR_QUIET
            RESULT_VARIABLE _owl_conan_result)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/conan" _owl_conan_local_url)
    set(_owl_conan_local_known OFF)
    if (_owl_conan_result EQUAL 0)
        string(JSON _owl_conan_count ERROR_VARIABLE _owl_conan_json_error LENGTH "${_owl_conan_remotes}")
        if (NOT _owl_conan_json_error AND _owl_conan_count GREATER 0)
            math(EXPR _owl_conan_last "${_owl_conan_count} - 1")
            foreach (_owl_conan_index RANGE ${_owl_conan_last})
                string(JSON _owl_conan_name GET "${_owl_conan_remotes}" ${_owl_conan_index} name)
                string(JSON _owl_conan_url GET "${_owl_conan_remotes}" ${_owl_conan_index} url)
                string(JSON _owl_conan_enabled GET "${_owl_conan_remotes}" ${_owl_conan_index} enabled)
                file(TO_CMAKE_PATH "${_owl_conan_url}" _owl_conan_url)
                if (_owl_conan_index EQUAL 0 AND _owl_conan_name STREQUAL "owl-local"
                        AND _owl_conan_url STREQUAL _owl_conan_local_url AND _owl_conan_enabled)
                    set(_owl_conan_local_known ON)
                endif ()
            endforeach ()
        endif ()
    endif ()
    set(_owl_conan_result 0)
    if (NOT _owl_conan_local_known)
        execute_process(COMMAND ${_owl_conan} remote add owl-local "${CMAKE_SOURCE_DIR}/conan"
                --type=local-recipes-index --force --index 0
                OUTPUT_QUIET
                RESULT_VARIABLE _owl_conan_result)
    endif ()
    if (NOT _owl_conan_result EQUAL 0)
        message(FATAL_ERROR "Unable to register the 'owl-local' Conan remote (${CMAKE_SOURCE_DIR}/conan).")
    endif ()

    # Optional binary cache (OWL_CONAN_CACHE_URL), searched after the local recipes and before ConanCenter.
    # Credentials come from Conan's own CONAN_LOGIN_USERNAME_OWL_CACHE / CONAN_PASSWORD_OWL_CACHE. When the
    # server is unset or does not answer, the remote is dropped and the install falls back to ConanCenter.
    if (NOT ${PROJECT_PREFIX}_CONAN_CACHE_URL AND DEFINED ENV{${PROJECT_PREFIX}_CONAN_CACHE_URL})
        set(${PROJECT_PREFIX}_CONAN_CACHE_URL "$ENV{${PROJECT_PREFIX}_CONAN_CACHE_URL}")
    endif ()
    if (NOT DEFINED ${PROJECT_PREFIX}_CONAN_CACHE_UPLOAD AND DEFINED ENV{${PROJECT_PREFIX}_CONAN_CACHE_UPLOAD})
        set(${PROJECT_PREFIX}_CONAN_CACHE_UPLOAD "$ENV{${PROJECT_PREFIX}_CONAN_CACHE_UPLOAD}")
    endif ()
    set(_owl_conan_cache OFF)
    if (${PROJECT_PREFIX}_CONAN_CACHE_URL)
        execute_process(COMMAND ${_owl_conan} remote add owl-cache "${${PROJECT_PREFIX}_CONAN_CACHE_URL}"
                --force --index 1
                OUTPUT_QUIET ERROR_QUIET
                RESULT_VARIABLE _owl_conan_result)
        if (_owl_conan_result EQUAL 0)
            execute_process(COMMAND ${_owl_conan} remote enable owl-cache OUTPUT_QUIET ERROR_QUIET)
            if (DEFINED ENV{CONAN_LOGIN_USERNAME_OWL_CACHE})
                execute_process(COMMAND ${_owl_conan} remote login owl-cache -cc core:non_interactive=True
                        TIMEOUT 60
                        OUTPUT_QUIET ERROR_QUIET
                        RESULT_VARIABLE _owl_conan_result)
            endif ()
        endif ()
        # `conan list -r` exits 0 on a connection or permission error and reports it in the JSON instead.
        if (_owl_conan_result EQUAL 0)
            execute_process(COMMAND ${_owl_conan} list "zlib/*" -r owl-cache --format=json
                    TIMEOUT 60
                    OUTPUT_VARIABLE _owl_conan_probe
                    ERROR_QUIET
                    RESULT_VARIABLE _owl_conan_result)
            if (_owl_conan_result EQUAL 0)
                # ERROR_VARIABLE is NOTFOUND (false) exactly when the `error` member exists.
                string(JSON _owl_conan_error ERROR_VARIABLE _owl_conan_no_error GET "${_owl_conan_probe}" owl-cache error)
                if (NOT _owl_conan_no_error OR NOT _owl_conan_probe MATCHES "owl-cache")
                    set(_owl_conan_result 1)
                endif ()
            endif ()
        endif ()
        if (_owl_conan_result EQUAL 0)
            set(_owl_conan_cache ON)
            message(STATUS "Conan binary cache ${${PROJECT_PREFIX}_CONAN_CACHE_URL}")
        else ()
            execute_process(COMMAND ${_owl_conan} remote disable owl-cache OUTPUT_QUIET ERROR_QUIET)
            message(WARNING "Conan binary cache ${${PROJECT_PREFIX}_CONAN_CACHE_URL} unreachable, using ConanCenter only.")
        endif ()
    else ()
        execute_process(COMMAND ${_owl_conan} remote remove owl-cache OUTPUT_QUIET ERROR_QUIET)
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

    # Host settings and options go through a generated profile rather than the command line: on
    # Windows `poetry` is a `.cmd` shim, and cmd.exe splits an argument like `&:shared=True` at `&`.
    file(MAKE_DIRECTORY "${_owl_conan_output}")
    set(_owl_conan_host_profile "${_owl_conan_output}/owl-host.profile")
    file(TO_CMAKE_PATH "${${PROJECT_PREFIX}_CONAN_PROFILE}" _owl_conan_base_profile)
    file(WRITE "${_owl_conan_host_profile}"
            "include(${_owl_conan_base_profile})\n\n"
            "[settings]\nbuild_type=${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}\n\n"
            "[options]\n"
            "&:shared=${_owl_conan_shared}\n"
            "&:testing=${_owl_conan_testing}\n"
            "&:nest=${_owl_conan_nest}\n\n"
            # Recipes running a Python generator at build time (glad needs jinja2) get the interpreter running
            # Conan, which has jinja2, not the first Python on PATH (MSYS2's on Windows, without it). Conan
            # renders profiles as Jinja templates, with `os` (hence `os.sys`) in scope.
            "{% set owl_python = os.sys.executable | replace(os.sep, '/') %}\n"
            "[conf]\n"
            "tools.cmake.cmaketoolchain:extra_variables={'Python_EXECUTABLE': '{{ owl_python }}', "
            "'Python3_EXECUTABLE': '{{ owl_python }}'}\n")
    file(REMOVE "${_owl_conan_output}/conan_toolchain.cmake")

    # The graph, resolved before the install, names the binaries this build may push to the binary cache.
    set(_owl_conan_upload OFF)
    if (_owl_conan_cache AND ${PROJECT_PREFIX}_CONAN_CACHE_UPLOAD)
        execute_process(COMMAND ${_owl_conan} graph info "${CMAKE_SOURCE_DIR}"
                --profile:host "${_owl_conan_host_profile}"
                --profile:build "${${PROJECT_PREFIX}_CONAN_PROFILE}"
                --build=${${PROJECT_PREFIX}_CONAN_BUILD}
                ${_owl_conan_lock}
                --format=json
                OUTPUT_FILE "${_owl_conan_output}/graph.json"
                ERROR_QUIET
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE _owl_conan_result)
        if (_owl_conan_result EQUAL 0)
            set(_owl_conan_upload ON)
        else ()
            message(WARNING "Conan binary cache: dependency graph unavailable, nothing will be pushed.")
        endif ()
    endif ()

    message(STATUS "Conan install (build type ${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}) into ${_owl_conan_output}")
    execute_process(COMMAND ${_owl_conan} install "${CMAKE_SOURCE_DIR}"
            --output-folder "${_owl_conan_output}"
            --profile:host "${_owl_conan_host_profile}"
            --profile:build "${${PROJECT_PREFIX}_CONAN_PROFILE}"
            --build=${${PROJECT_PREFIX}_CONAN_BUILD}
            ${_owl_conan_update}
            ${_owl_conan_lock}
            ${_owl_conan_deploy}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE _owl_conan_install_result)

    # Push the binaries of the graph not taken from a remote, also after a failed install, so the packages built
    # before the failure are not built again. Only Owl's: the agents' cache is shared with other projects.
    if (_owl_conan_upload)
        execute_process(COMMAND ${_owl_conan} list "*#*:*#latest" --format=json
                OUTPUT_FILE "${_owl_conan_output}/local.json"
                ERROR_QUIET
                RESULT_VARIABLE _owl_conan_result)
        if (_owl_conan_result EQUAL 0)
            execute_process(COMMAND ${Poetry_PREFIX} python "${CMAKE_SOURCE_DIR}/conan/upload_list.py"
                    "${_owl_conan_output}/graph.json" "${_owl_conan_output}/local.json"
                    "${_owl_conan_output}/upload.json"
                    RESULT_VARIABLE _owl_conan_result)
        endif ()
        if (_owl_conan_result EQUAL 0)
            execute_process(COMMAND ${_owl_conan} upload "--list=${_owl_conan_output}/upload.json" -r owl-cache -c
                    RESULT_VARIABLE _owl_conan_result)
        endif ()
        if (NOT _owl_conan_result EQUAL 0)
            message(WARNING "Conan binary cache: upload failed, the next build will build those packages again.")
        endif ()
    endif ()

    # The exit code alone is not trusted: a shim can lose it. No toolchain means no dependencies.
    if (NOT _owl_conan_install_result EQUAL 0 OR NOT EXISTS "${_owl_conan_output}/conan_toolchain.cmake")
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

# Packages whose CMake name or target differs from the module name used in the CMakeLists.
set(${PROJECT_PREFIX}_CONAN_PACKAGE_stb_image stb)
set(${PROJECT_PREFIX}_CONAN_TARGET_stb_image stb::stb)
set(${PROJECT_PREFIX}_CONAN_TARGET_TinyGLTF TinyGLTF::TinyGLTF)
# lunasvg: ConanCenter exports `include/`, the engine includes the headers as if `include/lunasvg/` were exported.
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
unset(_owl_conan_cache)
unset(_owl_conan_install_result)
unset(_owl_conan_upload)
unset(_owl_conan_remotes)
unset(_owl_conan_local_url)
unset(_owl_conan_local_known)
unset(_owl_conan_count)
unset(_owl_conan_json_error)
unset(_owl_conan_last)
unset(_owl_conan_index)
unset(_owl_conan_name)
unset(_owl_conan_url)
unset(_owl_conan_enabled)
unset(_owl_conan_probe)
unset(_owl_conan_error)
unset(_owl_conan_no_error)
unset(_owl_glibc)
message(STATUS "Third parties loaded from Conan.")
