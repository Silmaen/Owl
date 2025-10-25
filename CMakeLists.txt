#
#
#
cmake_minimum_required(VERSION 3.24)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")
set(PROJECT_PREFIX OWL)
set(PROJECT_PREFIX_LOWER owl)
set(ENGINE_NAME OwlEngine)
project(Owl VERSION 0.0.2 LANGUAGES C CXX)
#
# ---=== Options ===---
#
option(${PROJECT_PREFIX}_BUILD_SHARED "If the Library must be build as shared library" ON)
option(${PROJECT_PREFIX}_SKIP_DEPMANAGER "Don't use depmanager" OFF)
option(${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY "Use release version of third party libraries" ON)
option(${PROJECT_PREFIX}_DEFINE_VULKAN_LAYERS "Copy vulkan layers in the binary directory" OFF)

option(${PROJECT_PREFIX}_DOC_ONLY "To only generate documentation" OFF)
option(${PROJECT_PREFIX}_BUILD_NEST "If wanted to generate Owl Nest" ON)
option(${PROJECT_PREFIX}_BUILD_SANDBOX "If wanted to generate Sandbox" ON)
option(${PROJECT_PREFIX}_BUILD_DRONE "If wanted to generate OWl Drone" ON)
option(${PROJECT_PREFIX}_BUILD_CAST "If wanted to generate OWl Cast" ON)

option(${PROJECT_PREFIX}_TESTING "Enable the unit tests" ON)
option(${PROJECT_PREFIX}_ENABLE_COVERAGE "Run code coverage during test run" OFF)
option(${PROJECT_PREFIX}_ENABLE_STACKTRACE "Use stacktrace within the memory tracker (slow down the code)" OFF)
option(${PROJECT_PREFIX}_ENABLE_PROFILING "Output profiling information during execution" OFF)

#
# ---=== Base Configs ===---
#
include(BaseConfig)

if (NOT ${PROJECT_PREFIX}_DOC_ONLY)
    #
    # ---=== Sanitizers ===---
    #
    include(Sanitizers)

    #
    # ---=== The Program ===---
    #
    add_subdirectory(source)

    #
    # ---=== TESTING PROCEDURES ===---
    #
    if (${PROJECT_PREFIX}_TESTING AND NOT ${PROJECT_PREFIX}_ENABLE_CLANG_TIDY)
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
    set_target_properties(clean_bin PROPERTIES FOLDER "Utils")
endif ()


# ----------------------------------------------------------------------------------------------------------------------
# Include CPack package
if (${PROJECT_PREFIX}_PACKAGING)
    include(CPackConfig.cmake)
    include(CPack)
endif ()
# ----------------------------------------------------------------------------------------------------------------------
