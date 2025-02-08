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
pretty_platform_str(${CMAKE_HOST_SYSTEM_NAME} ${PROJECT_PREFIX}_HOST_PLATFORM_STR)
set(${PROJECT_PREFIX}_HOST_PLATFORM_VER_STR "${CMAKE_HOST_SYSTEM_VERSION}")
# -- target
if (NOT ${PROJECT_PREFIX}_PLATFORM_STR)
    pretty_platform_str(${CMAKE_SYSTEM_NAME} ${PROJECT_PREFIX}_PLATFORM_STR)
endif ()
if (${PROJECT_PREFIX}_PLATFORM_STR MATCHES "windows")
    set(${PROJECT_PREFIX}_PLATFORM_WINDOWS ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PLATFORM_WINDOWS)
elseif (${PROJECT_PREFIX}_PLATFORM_STR MATCHES "linux")
    set(${PROJECT_PREFIX}_PLATFORM_LINUX ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_PLATFORM_LINUX)
else ()
    message(FATAL_ERROR "Unsupported Target Operating System '${${PROJECT_PREFIX}_PLATFORM_STR}'")
endif ()
set(${PROJECT_PREFIX}_PLATFORM_VER_STR "${CMAKE_SYSTEM_VERSION}")
#
# CPU Architecture
#
# -- host
pretty_architecture_str(${CMAKE_HOST_SYSTEM_PROCESSOR} ${PROJECT_PREFIX}_HOST_ARCH_STR)
# -- target
if (NOT ${PROJECT_PREFIX}_ARCH_STR)
    set(${PROJECT_PREFIX}_ARCH_STR ${${PROJECT_PREFIX}_HOST_ARCH_STR})
endif ()
if (${PROJECT_PREFIX}_ARCH_STR STREQUAL "x64")
    set(${PROJECT_PREFIX}_PLATFORM_X64 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_X64)
elseif (${PROJECT_PREFIX}_ARCH_STR STREQUAL "arm64")
    set(${PROJECT_PREFIX}_PLATFORM_ARM64 ON)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_PLATFORM_ARM64)
else ()
    message(FATAL_ERROR "Unsupported Target architecture '${${PROJECT_PREFIX}_ARCH_STR}'")
endif ()
#
# ---=== Supported Compiler ===----
#
set(${PROJECT_PREFIX}_GNU_MINIMAL 11)
set(${PROJECT_PREFIX}_CLANG_MINIMAL 14)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PROJECT_PREFIX}_GNU_MINIMAL})
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PROJECT_PREFIX}_GNU_MINIMAL}")
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
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS ${${PROJECT_PREFIX}_CLANG_MINIMAL})
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} compiler version too old: ${CMAKE_CXX_COMPILER_VERSION}, need ${${PROJECT_PREFIX}_CLANG_MINIMAL}")
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
                -Wno-unsafe-buffer-usage
        )
    endif ()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 20)
        target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE
                -Wno-unsafe-buffer-usage-in-libc-call
        )
    endif ()
    set(${PROJECT_PREFIX}_COMPILER_CLANG ON)
    set(${PROJECT_PREFIX}_COMPILER_STR "clang")
else ()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()

# Cross compilation
if (NOT (${${PROJECT_PREFIX}_HOST_PLATFORM_STR} STREQUAL ${${PROJECT_PREFIX}_PLATFORM_STR} AND
        ${${PROJECT_PREFIX}_HOST_ARCH_STR} STREQUAL ${${PROJECT_PREFIX}_ARCH_STR}))
    set(${PROJECT_PREFIX}_CROSS_COMPILATION ON)
endif ()

# sum up the system
print_system_n_target_infos()
#
# Third parties
#
if (NOT ${PROJECT_PREFIX}_SKIP_DEPMANAGER)
    include(Depmanager)
endif ()

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

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/output/install)

set(${PROJECT_PREFIX}_INSTALL_BIN "bin/${${PROJECT_PREFIX}_PLATFORM_STR}_${${PROJECT_PREFIX}_ARCH_STR}")
set(${PROJECT_PREFIX}_INSTALL_LIB "lib/${${PROJECT_PREFIX}_PLATFORM_STR}_${${PROJECT_PREFIX}_ARCH_STR}")

add_custom_target(${CMAKE_PROJECT_NAME}_SuperBase)
if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
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
elseif (${PROJECT_PREFIX}_PLATFORM_LINUX)
    set(CMAKE_PLATFORM_USES_PATH_WHEN_NO_SONAME OFF)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath='$ORIGIN' -Wl,--disable-new-dtags")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath='$ORIGIN' -Wl,--disable-new-dtags")
endif ()
add_dependencies(${CMAKE_PROJECT_NAME}_Base ${CMAKE_PROJECT_NAME}_SuperBase)

target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PROJECT_PREFIX}_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PROJECT_PREFIX}_MINOR=${CMAKE_PROJECT_VERSION_MINOR}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE "${PROJECT_PREFIX}_PATCH=${CMAKE_PROJECT_VERSION_PATCH}")
target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_AUTHOR="Silmaen")

if (${${PROJECT_PREFIX}_IS_GENERATOR_MULTI_CONFIG})
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_$<IF:$<CONFIG:Debug>,DEBUG,RELEASE>)
else ()
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_DEBUG)
    else ()
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_RELEASE)
    endif ()
endif ()

#
# Debugging options
#
if (${PROJECT_PREFIX}_ENABLE_STACKTRACE)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_STACKTRACE)
endif ()
if (${PROJECT_PREFIX}_ENABLE_PROFILING)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_STACKTRACE)
endif ()
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

if (NOT ${PROJECT_PREFIX}_PACKAGING)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE OWL_DEVELOPMENT)
endif ()

include(cmake/CheckCapabilities.cmake)
