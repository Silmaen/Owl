
find_program(EDEPMANAGER depmanager)
if (${EDEPMANAGER} STREQUAL EDEPMANAGER-NOTFOUND)
    message(FATAL_ERROR "Dependency manager not found.")
else ()
    execute_process(COMMAND ${EDEPMANAGER} info cmakedir
            OUTPUT_VARIABLE depmanager_path)
    string(STRIP ${depmanager_path} depmanager_path)
    string(REPLACE "\\" "/" depmanager_path ${depmanager_path})
    list(PREPEND CMAKE_MODULE_PATH ${depmanager_path})
    include(DepManager)
endif ()

if (${${PRJPREFIX}_BUILD_SHARED})
    set(LOCAL_KIND shared)
else ()
    set(LOCAL_KIND static)
endif ()

if (${PRJPREFIX}_CROSS_COMPILATION)
    message(STATUS "Loading '${LOCAL_KIND}' third parties for ${${PRJPREFIX}_ARCH_STR}")
    dm_load_environment(KIND ${LOCAL_KIND} ARCH ${${PRJPREFIX}_ARCH_STR})
else ()
    message(STATUS "Loading '${LOCAL_KIND}' third parties for native")
    dm_load_environment(KIND ${LOCAL_KIND})
endif ()

dm_get_glibc(GLIBC_VERSION)
if (${GLIBC_VERSION})
    set(${PRJPREFIX}_GLIBC_STR "glibc_${GLIBC_VERSION}")
endif ()

message(STATUS "Checking Third party dependencies")
execute_process(COMMAND python -u ${CMAKE_SOURCE_DIR}/ci/DependencyCheck.py)
