#
# Packaging configuration
#
if (${PROJECT_PREFIX}_PACKAGE_ENGINE)
    include(GNUInstallDirs)
    # Create config file for FindPackage
    foreach (config IN ITEMS Debug Release)
        install(EXPORT OwlTargets${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_PROCESSOR}-${config}
                FILE OwlTargets${CMAKE_SYSTEM_NAME}.cmake
                NAMESPACE Owl::
                CONFIGURATIONS ${config}
                DESTINATION cmake
                COMPONENT Engine)
    endforeach ()
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
            INSTALL_DESTINATION cmake)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/OwlEngineConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/OwlEngineConfigVersion.cmake
            DESTINATION cmake)
endif ()

set(CPACK_PACKAGE_NAME "Owl")
if (${PROJECT_PREFIX}_PACKAGE_NAME) # to be defined in presets
    set(CPACK_PACKAGE_NAME "${${PROJECT_PREFIX}_PACKAGE_NAME}")
endif ()
set(CPACK_PACKAGE_VENDOR "${${PROJECT_PREFIX}_AUTHOR}")
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
# Get the latest abbreviated commit hash of the working branch
if (NOT ${PROJECT_PREFIX}_GIT_HASH)
    execute_process(
            COMMAND git log -1 --format=%h
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE ${PROJECT_PREFIX}_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()
string(SUBSTRING ${${PROJECT_PREFIX}_GIT_HASH} 0 7 ${PROJECT_PREFIX}_GIT_HASH)
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
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${NOW}-${${PROJECT_PREFIX}_GIT_HASH}-${${PROJECT_PREFIX}_PLATFORM_STR}-${${PROJECT_PREFIX}_GLIBC_STR}-${${PROJECT_PREFIX}_ARCH_STR}")
else ()
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${NOW}-${${PROJECT_PREFIX}_GIT_HASH}-${${PROJECT_PREFIX}_PLATFORM_STR}-${${PROJECT_PREFIX}_ARCH_STR}")
endif ()
if (NOT ${PROJECT_PREFIX}_BUILD_SHARED)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-static")
else ()
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
endif ()

message(STATUS "PACKAGING Expected package Name: ${CPACK_PACKAGE_FILE_NAME}")

if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "ZIP")
else ()
    set(CPACK_GENERATOR "TGZ")
endif ()

message(STATUS "Using ${CPACK_GENERATOR} Package generator")
