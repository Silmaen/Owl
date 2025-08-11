# ----------------------------------------------------------------------------------------------------------------------
# Clang sanitizers options
if (${PROJECT_PREFIX}_COMPILER_CLANG)
    option(${PROJECT_PREFIX}_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(${PROJECT_PREFIX}_ENABLE_ADDRESS_SANITIZER "Enable address sanitizer" OFF)
    option(${PROJECT_PREFIX}_ENABLE_THREAD_SANITIZER "Enable thread sanitizer" OFF)
    option(${PROJECT_PREFIX}_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER "Enable undefined behavior sanitizer" OFF)
    option(${PROJECT_PREFIX}_ENABLE_LEAK_SANITIZER "Enable memory leaks sanitizer" OFF)
    option(${PROJECT_PREFIX}_ENABLE_MEMORY_SANITIZER "Enable memory sanitizer" OFF)
endif ()
# ----------------------------------------------------------------------------------------------------------------------

# ----------------------------------------------------------------------------------------------------------------------
# Clang-tidy
if (${PROJECT_PREFIX}_ENABLE_CLANG_TIDY)
    math(EXPR ${PROJECT_PREFIX}_SANITIZER_COUNT "${${PROJECT_PREFIX}_SANITIZER_COUNT} + 1")
    find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
    if (CLANG_TIDY_EXECUTABLE)
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXECUTABLE}")
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        set(CMAKE_CXX_SCAN_FOR_MODULES ON)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}" -extra-arg=-Wno-unknown-warning-option -p "${CMAKE_BINARY_DIR}")

        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_USE_CLANG_TIDY)
        message(STATUS "CLANG-TIDY activated.")
    else ()
        set(${PROJECT_PREFIX}_ENABLE_CLANG_TIDY OFF CACHE BOOL "No Clang tidy found" FORCE)
        message(WARNING "No clang-tidy found on the system, deactivating it.")
    endif ()
endif ()
# ----------------------------------------------------------------------------------------------------------------------

# ----------------------------------------------------------------------------------------------------------------------
# Clang sanitizer - address
# ----------------------------------------------------------------------------------------------------------------------
if (${PROJECT_PREFIX}_ENABLE_ADDRESS_SANITIZER)
    math(EXPR ${PROJECT_PREFIX}_SANITIZER_COUNT "${${PROJECT_PREFIX}_SANITIZER_COUNT} + 1")
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=address -fsanitize-recover=address -O0 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=address)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_ADDRESS_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER_CUSTOM_ALLOCATOR)
endif ()

# ----------------------------------------------------------------------------------------------------------------------
# Clang sanitizer - thread Sanitizer
# ----------------------------------------------------------------------------------------------------------------------
if (${PROJECT_PREFIX}_ENABLE_THREAD_SANITIZER)
    math(EXPR ${PROJECT_PREFIX}_SANITIZER_COUNT "${${PROJECT_PREFIX}_SANITIZER_COUNT} + 1")
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=thread -O0 -g3)
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=thread)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_THREAD_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER_CUSTOM_ALLOCATOR)
endif ()

# ----------------------------------------------------------------------------------------------------------------------
# Clang sanitizer - Undefined behavior
# ----------------------------------------------------------------------------------------------------------------------
if (${PROJECT_PREFIX}_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER)
    math(EXPR ${PROJECT_PREFIX}_SANITIZER_COUNT "${${PROJECT_PREFIX}_SANITIZER_COUNT} + 1")
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=undefined -fsanitize-recover=undefined -O0 -g3 -fno-omit-frame-pointer)
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=undefined)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_UNDEFINED_BEHAVIOR_SANITIZER)
endif ()

# ----------------------------------------------------------------------------------------------------------------------
# Clang sanitizer - leak Sanitizer
# ----------------------------------------------------------------------------------------------------------------------
if (${PROJECT_PREFIX}_ENABLE_LEAK_SANITIZER)
    math(EXPR ${PROJECT_PREFIX}_SANITIZER_COUNT "${${PROJECT_PREFIX}_SANITIZER_COUNT} + 1")
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=leak -fsanitize-recover=leak -O0 -g3 -fno-omit-frame-pointer)
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=leak)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_LEAK_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER_CUSTOM_ALLOCATOR)
endif ()

# ----------------------------------------------------------------------------------------------------------------------
# Compatibility check
if (${PROJECT_PREFIX}_SANITIZER_COUNT GREATER 1)
    message(FATAL_ERROR "You can only use code coverage/inspection tools one by one.")
endif ()

if (${PROJECT_PREFIX}_ENABLE_COVERAGE AND ${PROJECT_PREFIX}_SANITIZER_COUNT GREATER 0)
    message(FATAL_ERROR "You can only use code coverage/inspection tools one by one.")
endif ()
# ----------------------------------------------------------------------------------------------------------------------
