
file(GLOB children CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

option(${PROJECT_PREFIX}_TEST_SHUFFLE "Run every gtest binary with --gtest_shuffle (seed from GTEST_RANDOM_SEED, else random and logged)" OFF)
option(${PROJECT_PREFIX}_RENDER_TESTS "Build the image tests (OwlRunner on lavapipe / llvmpipe, label render)" ON)
set(${PROJECT_PREFIX}_TEST_TIMEOUT 600 CACHE STRING "Per test binary ctest timeout in seconds (a hung binary costs this much)")
set(${PROJECT_PREFIX}_TEST_EXTRA_ARGS)
if (${PROJECT_PREFIX}_TEST_SHUFFLE)
    list(APPEND ${PROJECT_PREFIX}_TEST_EXTRA_ARGS --gtest_shuffle)
endif ()
# Any sanitizer report must fail the test, under ctest as on the CI.
set(${PROJECT_PREFIX}_TEST_ENVIRONMENT
        "ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:detect_stack_use_after_return=1:strict_init_order=1:check_initialization_order=1"
        "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
        "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tsan.supp")

add_custom_target(All_Tests)
# googletest
find_package(GTest REQUIRED)

foreach (child ${children})
    if (${child} MATCHES "test_helper")
        continue()
    endif ()
    if (${child} STREQUAL "render_tests" AND NOT ${PROJECT_PREFIX}_RENDER_TESTS)
        message(STATUS "OWL_RENDER_TESTS is OFF, skipping 'render_tests'")
        continue()
    endif ()
    if (${child} STREQUAL "owlnest_tests" AND NOT TARGET ${CMAKE_PROJECT_NAME}NestCore)
        message(STATUS "Owl Nest is not built, skipping 'owlnest_tests'")
        continue()
    endif ()
    if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
        file(GLOB_RECURSE
                SRCS CONFIGURE_DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/${child}/*.cpp
        )
        if ("${SRCS} " STREQUAL " ")
            message(STATUS "Test folder '${child}' is empty, skipping")
        else ()
            add_executable(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${SRCS})
            set_target_properties(${PROJECT_PREFIX_LOWER}_${child}_unit_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
            set_target_properties(${PROJECT_PREFIX_LOWER}_${child}_unit_test PROPERTIES FOLDER "UnitTests")
            target_include_directories(${PROJECT_PREFIX_LOWER}_${child}_unit_test PUBLIC test_helper)
            target_link_libraries(${PROJECT_PREFIX_LOWER}_${child}_unit_test GTest::gtest GTest::gtest_main)
            target_link_libraries(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${ENGINE_NAME})
            target_link_libraries(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${ENGINE_NAME}Private)
            target_link_libraries(${PROJECT_PREFIX_LOWER}_${child}_unit_test Owl_BaseTest)
            # owlnest_tests exercises the editor sources through the OwlNestCore static library.
            if (${child} STREQUAL "owlnest_tests")
                target_link_libraries(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${CMAKE_PROJECT_NAME}NestCore)
            endif ()
            # runner_tests compiles the runner's frame bench sources and runs the OwlRunner executable
            # (headless, null backend) when the editor apps are built.
            if (${child} STREQUAL "runner_tests")
                target_include_directories(${PROJECT_PREFIX_LOWER}_${child}_unit_test PRIVATE
                        ${CMAKE_SOURCE_DIR}/source/owlnest/runner)
                target_sources(${PROJECT_PREFIX_LOWER}_${child}_unit_test PRIVATE
                        ${CMAKE_SOURCE_DIR}/source/owlnest/runner/FrameBench.cpp
                        ${CMAKE_SOURCE_DIR}/source/owlnest/runner/FrameBenchStats.cpp)
                if (TARGET ${CMAKE_PROJECT_NAME}Runner)
                    add_dependencies(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${CMAKE_PROJECT_NAME}Runner)
                    target_compile_definitions(${PROJECT_PREFIX_LOWER}_${child}_unit_test PRIVATE
                            OWL_RUNNER_EXECUTABLE="$<TARGET_FILE:${CMAKE_PROJECT_NAME}Runner>")
                endif ()
            endif ()
            # render_tests run OwlRunner on lavapipe / llvmpipe and compare its captures to reference images; they need
            # a display, so ctest starts them under xvfb-run when it exists. Label `render` to run or skip them alone.
            set(TEST_LAUNCHER "")
            if (${child} STREQUAL "render_tests")
                target_compile_definitions(${PROJECT_PREFIX_LOWER}_${child}_unit_test PRIVATE
                        OWL_RENDER_TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/render_tests")
                if (TARGET ${CMAKE_PROJECT_NAME}Runner)
                    add_dependencies(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${CMAKE_PROJECT_NAME}Runner)
                    target_compile_definitions(${PROJECT_PREFIX_LOWER}_${child}_unit_test PRIVATE
                            OWL_RUNNER_EXECUTABLE="$<TARGET_FILE:${CMAKE_PROJECT_NAME}Runner>")
                endif ()
                find_program(XVFB_RUN_PROGRAM xvfb-run)
                if (XVFB_RUN_PROGRAM)
                    set(TEST_LAUNCHER ${XVFB_RUN_PROGRAM} -a -s "-screen 0 1280x1024x24")
                endif ()
            endif ()
            add_test(NAME ${PROJECT_PREFIX_LOWER}_${child}_UTests COMMAND ${TEST_LAUNCHER} ${CMAKE_BINARY_DIR}/bin/${PROJECT_PREFIX_LOWER}_${child}_unit_test --gtest_output=xml:test/${child}_UTest_Report.xml ${${PROJECT_PREFIX}_TEST_EXTRA_ARGS})
            set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES TIMEOUT ${${PROJECT_PREFIX}_TEST_TIMEOUT}
                    ENVIRONMENT "${${PROJECT_PREFIX}_TEST_ENVIRONMENT}")
            if (${child} STREQUAL "render_tests")
                set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES LABELS render)
            endif ()
            if (${child} STREQUAL "script_tests")
                # Hostile-script tests rely on the Lua quotas: a quota that never fires must fail, not hang.
                set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES TIMEOUT 300)
            endif ()
            # Tests that touch shared process-level resources (freetype, GLFW init, OpenAL, msdfgen globals)
            # are serialized via RESOURCE_LOCK to avoid sporadic SEGFAULTs in parallel ctest runs.
            if (${child} STREQUAL "font_tests" OR ${child} STREQUAL "core_tests" OR
                ${child} STREQUAL "gui_tests" OR ${child} STREQUAL "renderer_tests" OR
                ${child} STREQUAL "sound_tests" OR ${child} STREQUAL "script_tests")
                set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES RESOURCE_LOCK owl_engine_globals)
            endif ()
            # export_tests exports sample_project with OwlNest and plays it with the exported OwlRunner.
            if (${child} STREQUAL "export_tests")
                set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES LABELS "export")
                foreach (app_target ${CMAKE_PROJECT_NAME}Nest ${CMAKE_PROJECT_NAME}Runner)
                    if (TARGET ${app_target})
                        add_dependencies(${PROJECT_PREFIX_LOWER}_${child}_unit_test ${app_target})
                    endif ()
                endforeach ()
            endif ()
            if (${PROJECT_PREFIX}_BUILD_SHARED AND WIN32)
                # Copy every runtime DLL the test binary's full link graph pulls in next to the executable: a test
                # may surface transitive DLLs the engine's own post-build copy did not see (gtest is static).
                add_custom_command(TARGET ${PROJECT_PREFIX_LOWER}_${child}_unit_test POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:${PROJECT_PREFIX_LOWER}_${child}_unit_test>" "${CMAKE_BINARY_DIR}/bin"
                        COMMAND_EXPAND_LISTS
                )
            endif ()

            add_dependencies(All_Tests ${PROJECT_PREFIX_LOWER}_${child}_unit_test)
            add_custom_command(TARGET All_Tests POST_BUILD
                    COMMAND ${PROJECT_PREFIX_LOWER}_${child}_unit_test
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        endif ()
    endif ()
endforeach ()

# Wayland smoke test: the runner presents frames on a headless weston (Vulkan and OpenGL); skipped without weston.
if (${PROJECT_PREFIX}_PLATFORM_LINUX AND ${PROJECT_PREFIX}_RENDER_TESTS AND TARGET ${CMAKE_PROJECT_NAME}Runner)
    add_test(NAME ${PROJECT_PREFIX_LOWER}_wayland_smoke
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wayland_smoke.sh $<TARGET_FILE:${CMAKE_PROJECT_NAME}Runner>
            ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/wayland_smoke)
    set_tests_properties(${PROJECT_PREFIX_LOWER}_wayland_smoke PROPERTIES LABELS "render;wayland" SKIP_RETURN_CODE 77
            TIMEOUT 300 ENVIRONMENT "${${PROJECT_PREFIX}_TEST_ENVIRONMENT}")
endif ()

if (${PROJECT_PREFIX}_ENABLE_COVERAGE)

    set(COVERAGE_RESULT_DIR ${CMAKE_BINARY_DIR}/Coverage)
    file(MAKE_DIRECTORY ${COVERAGE_RESULT_DIR})
    if ("d${${PROJECT_PREFIX}_GCOV}" STREQUAL "d")
        if (${PROJECT_PREFIX}_COMPILER_GCC)
            set(COVERAGE_GCOV "gcov")
        elseif (${PROJECT_PREFIX}_COMPILER_CLANG)
            set(COVERAGE_GCOV "llvm gcov")
        endif ()
    else ()
        set(COVERAGE_GCOV "${${PROJECT_PREFIX}_GCOV}")
    endif ()
    set(CMD ${GCOVR} -r ${CMAKE_SOURCE_DIR} -o ${COVERAGE_RESULT_DIR}/index.html --gcov-executable ${COVERAGE_GCOV} .)

    add_custom_command(TARGET All_Tests POST_BUILD
            COMMAND echo "command: '${CMD}'")
    add_custom_command(TARGET All_Tests POST_BUILD
            COMMAND ${CMD}
            COMMAND_EXPAND_LISTS
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            USES_TERMINAL)
    add_custom_command(TARGET All_Tests POST_BUILD
            COMMAND echo "look at the coverage result: file:///${COVERAGE_RESULT_DIR}/index.html")
endif ()
