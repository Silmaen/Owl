

set(OWL_PROJECT ${CMAKE_PROJECT_NAME}Nest)

# ----==== app Runner ====----
file(GLOB_RECURSE SRCS_RUNNER
        runner/*.cpp
)
file(GLOB_RECURSE HDRS_RUNNER
        runner/*.h
)
set(RUNNER_NAME ${CMAKE_PROJECT_NAME}Runner)
add_executable(${RUNNER_NAME} ${SRCS_RUNNER} ${HDRS_RUNNER})
set_target_properties(${RUNNER_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(${RUNNER_NAME} PRIVATE sources)
target_compile_definitions(${RUNNER_NAME} PRIVATE OWL_ASSETS_LOCATION="source/owlnest/assets")
target_link_libraries(${RUNNER_NAME} PRIVATE ${ENGINE_NAME})
set_target_properties(${RUNNER_NAME} PROPERTIES FOLDER "App")

# yaml
owl_target_link_libraries(${RUNNER_NAME} PRIVATE yaml-cpp REQUIRED ${THIRD_PARTY_RELEASE})
# ----==== app Runner ====----

#
#  Main Executable
#
file(GLOB_RECURSE SRCS
        sources/*.cpp
)
file(GLOB_RECURSE HDRS
        sources/*.h
)
add_executable(${OWL_PROJECT}
        ${SRCS} ${HDRS})
set_target_properties(${OWL_PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${OWL_PROJECT} PROPERTIES FOLDER "App")

target_include_directories(${OWL_PROJECT} PRIVATE sources)

target_link_libraries(${OWL_PROJECT} PRIVATE
        ${ENGINE_NAME}
)
add_dependencies(${OWL_PROJECT} ${CMAKE_PROJECT_NAME}Runner)

target_compile_definitions(${OWL_PROJECT} PRIVATE OWL_ASSETS_LOCATION="source/owlnest/assets")

target_import_so_files(${OWL_PROJECT})
# ----==== third party ====----

# magic_enum
owl_target_link_libraries(${OWL_PROJECT} PRIVATE magic_enum REQUIRED ${THIRD_PARTY_RELEASE})

# yaml
find_package(yaml-cpp REQUIRED)
target_link_libraries(${OWL_PROJECT} PRIVATE yaml-cpp::yaml-cpp)

# ----==== third party ====----

if (${PROJECT_PREFIX}_BUILD_SHARED AND WIN32)
    add_custom_command(TARGET ${OWL_PROJECT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:${OWL_PROJECT}>" "$<TARGET_FILE_DIR:${OWL_PROJECT}>"
            COMMAND_EXPAND_LISTS
    )
endif ()

add_custom_command(TARGET ${OWL_PROJECT} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/imgui.ini" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/OwlNest_imgui.ini"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/theme.yml" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/theme.yml"
        VERBATIM)

install(TARGETS ${OWL_PROJECT}
        LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
)
install(TARGETS ${RUNNER_NAME}
        LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
        DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets
)
