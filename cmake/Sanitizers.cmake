# ----------------------------------------------------------------------------------------------------------------------
# Clang sanitizers options
if (${PROJECT_PREFIX}_COMPILER_CLANG)
    option(${PROJECT_PREFIX}_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(${PROJECT_PREFIX}_ENABLE_ADDRESS_SANITIZER "Enable address sanitizer" OFF)
    option(${PROJECT_PREFIX}_ENABLE_THREAD_SANITIZER "Enable thread sanitizer" OFF)
    option(${PROJECT_PREFIX}_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER "Enable undefined behavior sanitizer" OFF)
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
        # Deliberately NOT hooked into the compiler through CMAKE_CXX_CLANG_TIDY:
        # the `ClangTidy` CI action drives clang-tidy from compile_commands.json
        # once the build is done. That is what lets it analyse only the
        # translation units a pull request can change the verdict of --- the
        # compiler hook has no way to skip a file.
        #     poetry run python ci_action.py ClangTidy <preset>
        target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_USE_CLANG_TIDY)
        message(STATUS "CLANG-TIDY activated (run by the ClangTidy CI action, not by the compiler).")
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
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=address -fno-sanitize-recover=all -O0 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
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
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=undefined -fno-sanitize-recover=all -O0 -g3 -fno-omit-frame-pointer)
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=undefined)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_SANITIZER)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_UNDEFINED_BEHAVIOR_SANITIZER)
endif ()

# ----------------------------------------------------------------------------------------------------------------------
# libFuzzer coverage instrumentation (fuzz targets link -fsanitize=fuzzer themselves)
# ----------------------------------------------------------------------------------------------------------------------
if (${PROJECT_PREFIX}_FUZZING)
    if (NOT ${PROJECT_PREFIX}_COMPILER_CLANG)
        message(FATAL_ERROR "${PROJECT_PREFIX}_FUZZING requires Clang (libFuzzer).")
    endif ()
    target_compile_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=fuzzer-no-link -g)
    target_link_options(${CMAKE_PROJECT_NAME}_Base INTERFACE -fsanitize=fuzzer-no-link)
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PROJECT_PREFIX}_FUZZING)
endif ()

# ----------------------------------------------------------------------------------------------------------------------
# Compatibility check
# AddressSanitizer and UndefinedBehaviorSanitizer are designed to be combined; every other pair is rejected.
# LeakSanitizer has no dedicated option: on Linux it is part of AddressSanitizer (detect_leaks=1).
set(${PROJECT_PREFIX}_SANITIZER_ALLOWED_COUNT 1)
if (${PROJECT_PREFIX}_ENABLE_ADDRESS_SANITIZER AND ${PROJECT_PREFIX}_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER)
    set(${PROJECT_PREFIX}_SANITIZER_ALLOWED_COUNT 2)
endif ()
if (${PROJECT_PREFIX}_SANITIZER_COUNT GREATER ${PROJECT_PREFIX}_SANITIZER_ALLOWED_COUNT)
    message(FATAL_ERROR "You can only use code coverage/inspection tools one by one (except Address + Undefined behavior).")
endif ()

if (${PROJECT_PREFIX}_ENABLE_COVERAGE AND ${PROJECT_PREFIX}_SANITIZER_COUNT GREATER 0)
    message(FATAL_ERROR "You can only use code coverage/inspection tools one by one.")
endif ()
# ----------------------------------------------------------------------------------------------------------------------
