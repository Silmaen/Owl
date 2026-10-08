#
# Packaging configuration
#
# Packaging configuration: every tree can run `cpack`, which writes one archive per component, `OwlEngine-…` (the
# SDK) and, when the editor is built, `OwlNest-…` (the application).
include(GNUInstallDirs)
# Install CMake export targets
install(EXPORT OwlEngineTargets
        FILE OwlEngineTargets.cmake
        NAMESPACE Owl::
        DESTINATION lib/cmake/OwlEngine
        COMPONENT Engine)
# Owl::Gui in a file of its own: OwlEngineConfig.cmake loads it (and imgui) only for the Gui component.
install(EXPORT OwlEngineGuiTargets
        FILE OwlEngineGuiTargets.cmake
        NAMESPACE Owl::
        DESTINATION lib/cmake/OwlEngine
        COMPONENT Engine)
# Create file for use of find_package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/OwlEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/config/OwlEngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/OwlEngineConfig.cmake
        INSTALL_DESTINATION lib/cmake/OwlEngine)
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/OwlEngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/OwlEngineConfigVersion.cmake
        DESTINATION lib/cmake/OwlEngine
        COMPONENT Engine)

set(CPACK_PACKAGE_NAME "Owl")
set(CPACK_PACKAGE_VENDOR "${${PROJECT_PREFIX}_AUTHOR}")
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
# Get the latest abbreviated commit hash of the working branch.
#
# Git may legitimately fail here (shallow clones, out-of-tree builds, and — the case that
# motivated this fallback — a checkout made on a Windows agent mounted into a Linux container,
# where `.git/objects/info/alternates` points to a Windows path that can't be resolved).  When
# that happens we fall back to TeamCity's `BUILD_VCS_NUMBER` environment variable; if that is
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
        if (DEFINED ENV{BUILD_VCS_NUMBER} AND NOT "$ENV{BUILD_VCS_NUMBER}" STREQUAL "")
            set(${PROJECT_PREFIX}_GIT_HASH "$ENV{BUILD_VCS_NUMBER}")
        else ()
            set(${PROJECT_PREFIX}_GIT_HASH "unknown")
        endif ()
    endif ()
endif ()
# Quoting the expansion guards against an empty value collapsing into zero arguments.
string(SUBSTRING "${${PROJECT_PREFIX}_GIT_HASH}" 0 7 ${PROJECT_PREFIX}_GIT_HASH)
if (${PROJECT_PREFIX}_PACK_TIME)
    string(TIMESTAMP NOW "-%Y%m%d_%H%M")
endif ()

if (${PROJECT_PREFIX}_DISTRIBUTION)
    set(${PROJECT_PREFIX}_OS_VERSION "${${PROJECT_PREFIX}_DISTRIBUTION}")
else ()

    if (${PROJECT_PREFIX}_PLATFORM_ANDROID)
        set(${PROJECT_PREFIX}_OS_VERSION "${CMAKE_SYSTEM_VERSION}")
    elseif (${PROJECT_PREFIX}_PLATFORM_IOS OR ${PROJECT_PREFIX}_PLATFORM_MACOS)
        set(${PROJECT_PREFIX}_OS_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif ()
endif ()

if (${PROJECT_PREFIX}_GLIBC_STR)
    set(_owl_package_suffix "${CPACK_PACKAGE_VERSION}${NOW}-${${PROJECT_PREFIX}_GIT_HASH}-${${PROJECT_PREFIX}_PLATFORM_STR}-${${PROJECT_PREFIX}_GLIBC_STR}-${${PROJECT_PREFIX}_ARCH_STR}")
else ()
    set(_owl_package_suffix "${CPACK_PACKAGE_VERSION}${NOW}-${${PROJECT_PREFIX}_GIT_HASH}-${${PROJECT_PREFIX}_PLATFORM_STR}-${${PROJECT_PREFIX}_ARCH_STR}")
endif ()
if (NOT ${PROJECT_PREFIX}_BUILD_SHARED)
    set(_owl_package_suffix "${_owl_package_suffix}-static")
endif ()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${_owl_package_suffix}")

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL Engine)
set(CPACK_ARCHIVE_ENGINE_FILE_NAME "OwlEngine-${_owl_package_suffix}")
if (${PROJECT_PREFIX}_BUILD_NEST)
    list(APPEND CPACK_COMPONENTS_ALL Nest)
    set(CPACK_ARCHIVE_NEST_FILE_NAME "OwlNest-${_owl_package_suffix}")
endif ()
message(STATUS "PACKAGING Expected packages: ${CPACK_ARCHIVE_ENGINE_FILE_NAME} ${CPACK_ARCHIVE_NEST_FILE_NAME}")

if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "ZIP")
else ()
    set(CPACK_GENERATOR "TGZ")
endif ()

message(STATUS "Using ${CPACK_GENERATOR} Package generator")
