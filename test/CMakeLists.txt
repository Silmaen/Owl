
file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

add_custom_target(All_Tests)
# googletest
find_package(GTest REQUIRED)

foreach (child ${children})
    if (${child} MATCHES "test_helper")
        continue()
    endif ()
    if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
        file(GLOB_RECURSE
                SRCS
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
            add_test(${PROJECT_PREFIX_LOWER}_${child}_UTests ${CMAKE_BINARY_DIR}/bin/${PROJECT_PREFIX_LOWER}_${child}_unit_test --gtest_output=xml:test/${child}_UTest_Report.xml)
            set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES TIMEOUT 3600)
            # Tests that touch shared process-level resources (freetype, GLFW init, OpenAL, msdfgen globals)
            # are serialized via RESOURCE_LOCK to avoid sporadic SEGFAULTs in parallel ctest runs.
            if (${child} STREQUAL "font_tests" OR ${child} STREQUAL "core_tests" OR
                ${child} STREQUAL "gui_tests" OR ${child} STREQUAL "renderer_tests" OR
                ${child} STREQUAL "sound_tests" OR ${child} STREQUAL "script_tests")
                set_tests_properties(${PROJECT_PREFIX_LOWER}_${child}_UTests PROPERTIES RESOURCE_LOCK owl_engine_globals)
            endif ()
            if (${PROJECT_PREFIX}_BUILD_SHARED AND WIN32)
                # Copy gtest + every runtime DLL the test binary's full link graph pulls in next to
                # the executable.  The engine's own post-build copies OwlEngine-side deps, but a
                # test may surface additional transitive DLLs that weren't on the engine's
                # `TARGET_RUNTIME_DLLS` evaluation (different linkage path).  Idempotent via
                # `copy_if_different`.
                add_custom_command(TARGET ${PROJECT_PREFIX_LOWER}_${child}_unit_test POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:GTest::gtest>" "${CMAKE_BINARY_DIR}/bin"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:GTest::gtest_main>" "${CMAKE_BINARY_DIR}/bin"
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
