
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
            add_executable(${PRJPREFIX_LOWER}_${child}_unit_test ${SRCS})
            set_target_properties(${PRJPREFIX_LOWER}_${child}_unit_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
            target_include_directories(${PRJPREFIX_LOWER}_${child}_unit_test PUBLIC test_helper)
            target_link_libraries(${PRJPREFIX_LOWER}_${child}_unit_test GTest::gtest GTest::gtest_main)
            target_link_libraries(${PRJPREFIX_LOWER}_${child}_unit_test ${ENGINE_NAME})
            target_link_libraries(${PRJPREFIX_LOWER}_${child}_unit_test Owl_BaseTest)
            add_test(${PRJPREFIX_LOWER}_${child}_UTests ${CMAKE_BINARY_DIR}/bin/${PRJPREFIX_LOWER}_${child}_unit_test --gtest_output=xml:test/${child}_UTest_Report.xml)
            set_tests_properties(${PRJPREFIX_LOWER}_${child}_UTests PROPERTIES TIMEOUT 3600)
            if (${PRJPREFIX}_BUILD_SHARED AND WIN32)
                add_custom_command(TARGET ${PRJPREFIX_LOWER}_${child}_unit_test POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GTest_DIR}/../../../bin/libgtest$<$<CONFIG:Debug>:d>.dll" "${CMAKE_BINARY_DIR}/bin"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GTest_DIR}/../../../bin/libgtest_main$<$<CONFIG:Debug>:d>.dll" "${CMAKE_BINARY_DIR}/bin"
                        COMMAND_EXPAND_LISTS
                )
            endif ()

            add_dependencies(All_Tests ${PRJPREFIX_LOWER}_${child}_unit_test)
            add_custom_command(TARGET All_Tests POST_BUILD
                    COMMAND ${PRJPREFIX_LOWER}_${child}_unit_test
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        endif ()
    endif ()
endforeach ()

if (${PRJPREFIX}_ENABLE_COVERAGE)

    set(COVERAGE_RESULT_DIR ${CMAKE_BINARY_DIR}/Coverage)
    file(MAKE_DIRECTORY ${COVERAGE_RESULT_DIR})
    set(COVERAGE_TITLE "Owl: tests coverage")
    set(COVERAGE_HTML --html --html-details-syntax-highlighting --html-nested --html-title ${COVERAGE_TITLE} --source-encoding utf-8)
    set(COVERAGE_FILTER_PLATFORM -e\".*windows.*\" -e\".*linux.*\")
    set(COVERAGE_FILTER_PRJ -e\".*test.*\" -e\".*owldrone.*\" -e\".*owlnest.*\" -e\".*sandbox.*\" -e\".*autogen.*\")
    set(COVERAGE_FILTER_RDR -e\".*vulkan.*\" -e\".*opengl.*\")
    set(COVERAGE_FILTER ${COVERAGE_FILTER_PLATFORM} ${COVERAGE_FILTER_PRJ} ${COVERAGE_FILTER_RDR})
    set(COVERAGE_OPTION -pds --txt-metric branch --exclude-unreachable-branches --exclude-noncode-lines --exclude-throw-branches -j 4 --exclude-lines-by-pattern \".*---UNCOVER---.*\")
    if ("d${${PRJPREFIX}_GCOV}" STREQUAL "d")
        set(COVERAGE_GCOV)
    else ()
        set(COVERAGE_GCOV "--gcov-executable \"llvm-cov gcov\" --gcov-ignore-parse-errors")
    endif ()
    set(CMD ${GCOVR} ${COVERAGE_OPTION} -r ${CMAKE_SOURCE_DIR} -o ${COVERAGE_RESULT_DIR}/index.html ${COVERAGE_GCOV} ${COVERAGE_HTML} ${COVERAGE_FILTER} .)

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

