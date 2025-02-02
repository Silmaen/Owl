#
#
#
include(OwlUtils)
#
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(${CMAKE_PROJECT_NAME}_Base INTERFACE)
add_library(${CMAKE_PROJECT_NAME}_BaseTest INTERFACE)
add_dependencies(${CMAKE_PROJECT_NAME}_BaseTest ${CMAKE_PROJECT_NAME}_Base)
find_package(Python REQUIRED)

#
# ---=== Supported OS ===---
#
#
# Operating System
#
# -- host
pretty_platform_str(${CMAKE_HOST_SYSTEM_NAME} ${PRJPREFIX}_HOST_PLATFORM_STR)
set(${PRJPREFIX}_HOST_PLATFORM_VER_STR "${CMAKE_HOST_SYSTEM_VERSION}")
# -- target
if (NOT ${PRJPREFIX}_PLATFORM_STR)
    pretty_platform_str(${CMAKE_SYSTEM_NAME} ${PRJPREFIX}_PLATFORM_STR)
endif ()
if (${PRJPREFIX}_PLATFORM_STR MATCHES "windows")
    set(${PRJPREFIX}_PLATFORM_WINDOWS ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_PLATFORM_WINDOWS)
elseif (${PRJPREFIX}_PLATFORM_STR MATCHES "linux")
    set(${PRJPREFIX}_PLATFORM_LINUX ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_PLATFORM_LINUX)
else ()
    message(FATAL_ERROR "Unsupported Target Operating System '${${PRJPREFIX}_PLATFORM_STR}'")
endif ()
set(${PRJPREFIX}_PLATFORM_VER_STR "${CMAKE_SYSTEM_VERSION}")
#
# CPU Architecture
#
# -- host
pretty_architecture_str(${CMAKE_HOST_SYSTEM_PROCESSOR} ${PRJPREFIX}_HOST_ARCH_STR)
# -- target
if (NOT ${PRJPREFIX}_ARCH_STR)
    set(${PRJPREFIX}_ARCH_STR ${${PRJPREFIX}_HOST_ARCH_STR})
endif ()
if (${PRJPREFIX}_ARCH_STR STREQUAL "x64")
    set(${PRJPREFIX}_PLATFORM_X64 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_X64)
elseif (${PRJPREFIX}_ARCH_STR STREQUAL "arm64")
    set(${PRJPREFIX}_PLATFORM_ARM64 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_ARM64)
else ()
    message(FATAL_ERROR "Unsupported Target architecture '${${PRJPREFIX}_ARCH_STR}'")
endif ()
#
# ---=== Supported Compiler ===----
#
set(${PRJPREFIX}_GNU_MINIMAL 11)
set(${PRJPREFIX}_CLANG_MINIMAL 14)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PRJPREFIX}_GNU_MINIMAL})
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PRJPREFIX}_GNU_MINIMAL}")
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
    set(${PRJPREFIX}_COMPILER_GCC ON)
    set(${PRJPREFIX}_COMPILER_STR "gcc")
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PRJPREFIX}_CLANG_MINIMAL})
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PRJPREFIX}_CLANG_MINIMAL}")
    endif ()
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
            -Werror -Weverything -pedantic
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-c++20-compat
            -Wno-padded
            -Wno-exit-time-destructors
    )
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
            -Wno-global-constructors # Ony in gtest -> only for tests
    )
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 18)
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
                -Wno-switch-default
        )
    endif ()
    set(${PRJPREFIX}_COMPILER_CLANG ON)
    set(${PRJPREFIX}_COMPILER_STR "clang")
else ()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()

# Cross compilation
if (NOT (${${PRJPREFIX}_HOST_PLATFORM_STR} STREQUAL ${${PRJPREFIX}_PLATFORM_STR} AND
        ${${PRJPREFIX}_HOST_ARCH_STR} STREQUAL ${${PRJPREFIX}_ARCH_STR}))
    set(${PRJPREFIX}_CROSS_COMPILATION ON)
endif ()

# sum up the system
print_system_n_target_infos()
#
# Third parties
#
if (NOT ${PRJPREFIX}_SKIP_DEPMANAGER)
    include(Depmanager)
endif ()

#
# --== Properties ==--
#
get_property(${PRJPREFIX}_IS_GENERATOR_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if (${${PRJPREFIX}_IS_GENERATOR_MULTI_CONFIG})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
else ()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/output/install)

set(${PRJPREFIX}_INSTALL_BIN "bin/${${PRJPREFIX}_PLATFORM_STR}_${${PRJPREFIX}_ARCH_STR}")
set(${PRJPREFIX}_INSTALL_LIB "lib/${${PRJPREFIX}_PLATFORM_STR}_${${PRJPREFIX}_ARCH_STR}")

add_custom_target(${CMAKE_PROJECT_NAME}_SuperBase)
if (${PRJPREFIX}_PLATFORM_WINDOWS)
    if (MINGW)
        cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH COMPILER_PATH)
        message(STATUS "MinGW environment detected: add dependence to dlls from ${COMPILER_PATH}")
        set(REQUIRED_LIBS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        foreach (lib IN ITEMS ${REQUIRED_LIBS})
            if (NOT EXISTS ${COMPILER_PATH}/${lib})
                message(WARNING "Required Dll not found: ${COMPILER_PATH}/${lib}")
            else ()
                message(STATUS "Adding Dll: ${COMPILER_PATH}/${lib}")
                add_custom_command(TARGET ${CMAKE_PROJECT_NAME}_SuperBase POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_PATH}/${lib} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
                        COMMENT "Copying ${lib} into ${${CMAKE_RUNTIME_OUTPUT_DIRECTORY}}")
            endif ()
        endforeach ()
    endif ()
elseif (${PRJPREFIX}_PLATFORM_LINUX)
    set(CMAKE_PLATFORM_USES_PATH_WHEN_NO_SONAME OFF)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath='$ORIGIN' -Wl,--disable-new-dtags")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath='$ORIGIN' -Wl,--disable-new-dtags")
endif ()
add_dependencies(${CMAKE_PROJECT_NAME}_Base ${CMAKE_PROJECT_NAME}_SuperBase)

target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PRJPREFIX}_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PRJPREFIX}_MINOR=${CMAKE_PROJECT_VERSION_MINOR}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PRJPREFIX}_PATCH=${CMAKE_PROJECT_VERSION_PATCH}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_AUTHOR="Silmaen")

if (${${PRJPREFIX}_IS_GENERATOR_MULTI_CONFIG})
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_$<IF:$<CONFIG:Debug>,DEBUG,RELEASE>)
else ()
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_DEBUG)
    else ()
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_RELEASE)
    endif ()
endif ()

#
# Debugging options
#
if (${PRJPREFIX}_ENABLE_STACKTRACE)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_STACKTRACE)
endif ()
if (${PRJPREFIX}_ENABLE_PROFILING)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_STACKTRACE)
endif ()
if (${PRJPREFIX}_ENABLE_RENDERER_VERBOSE_CAPABILITIES)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_RENDERER_VERBOSE_CAPABILITIES)
endif ()
if (${PRJPREFIX}_ENABLE_SHADER_REFLECT_RESOURCES)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_SHADER_REFLECT_RESOURCES)
endif ()


if (${PRJPREFIX}_ENABLE_COVERAGE)
    include(cmake/CoverageConfig.cmake)
endif ()

include(cmake/DocumentationConfig.cmake)

if (NOT ${PRJPREFIX}_PACKAGING)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_DEVELOPMENT)
endif ()
