#
#
#
cmake_minimum_required(VERSION 3.24)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")
set(PRJPREFIX OWL)
set(PRJPREFIX_LOWER owl)
set(ENGINE_NAME OwlEngine)
project(Owl VERSION 0.0.2 LANGUAGES C CXX)
#
# ---=== Options ===---
#
option(${PRJPREFIX}_BUILD_SHARED "If the Library must be build as shared library" ON)
option(${PRJPREFIX}_SKIP_DEPMANAGER "Don't use depmanager" OFF)
option(${PRJPREFIX}_DEFINE_VULKAN_LAYERS "Copy vulkan layers in the binary directory" ON)

option(${PRJPREFIX}_DOC_ONLY "To only generate documentation" OFF)
option(${PRJPREFIX}_BUILD_NEST "If wanted to generate Owl Nest" ON)
option(${PRJPREFIX}_BUILD_SANDBOX "If wanted to generate Sandbox" ON)
option(${PRJPREFIX}_BUILD_DRONE "If wanted to generate OWl Drone" ON)
option(${PRJPREFIX}_BUILD_CAST "If wanted to generate OWl Cast" ON)

option(${PRJPREFIX}_TESTING "Enable the unit tests" ON)
option(${PRJPREFIX}_ENABLE_COVERAGE "Run code coverage during test run" OFF)
option(${PRJPREFIX}_ENABLE_STACKTRACE "Use stacktrace within the memory tracker (slow down the code)" OFF)
option(${PRJPREFIX}_ENABLE_PROFILING "Output profiling informations during execution" OFF)

#
# ---=== Base Configs ===---
#
include(BaseConfig)

if (NOT ${PRJPREFIX}_DOC_ONLY)
    #
    # ---=== Sanitizers ===---
    #
    include(SaniTests)

    #
    # ---=== The Program ===---
    #
    add_subdirectory(source)

    #
    # ---=== TESTING PROCEDURES ===---
    #
    if (${PRJPREFIX}_TESTING AND NOT ${PRJPREFIX}_ENABLE_CLANG_TIDY)
        enable_testing()
        add_subdirectory(test)
    endif ()

    #
    # ---=== CLEANING PROCEDURES ===---
    #
    add_custom_target(clean_bin
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
            COMMENT "Cleaning the binary directory ${CMAKE_BINARY_DIR}/bin"
    )
endif ()


# ----------------------------------------------------------------------------------------------------------------------
# Include CPack package
if (${PRJPREFIX}_PACKAGING)
    include(CPackConfig.cmake)
    include(CPack)
endif ()
# ----------------------------------------------------------------------------------------------------------------------
