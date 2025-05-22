#
#  Main Executable
#
set(OWL_PROJECT ${CMAKE_PROJECT_NAME}Cast)

file(GLOB_RECURSE SRCS
        sources/*.cpp
)
file(GLOB_RECURSE HDRS
        sources/*.h
)
add_executable(${OWL_PROJECT}
        ${SRCS} ${HDRS})
set_target_properties(${OWL_PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

target_include_directories(${OWL_PROJECT} PRIVATE sources)

target_link_libraries(${OWL_PROJECT} PRIVATE
        ${ENGINE_NAME}
)

target_compile_definitions(${OWL_PROJECT} PRIVATE OWL_ASSETS_LOCATION="source/owlcast/assets")

target_import_so_files(${OWL_PROJECT})
# ----==== third party ====----

# magic_enum
find_package(magic_enum REQUIRED)
target_link_libraries(${OWL_PROJECT} PRIVATE magic_enum::magic_enum)

# ----==== third party ====----

if (${PROJECT_PREFIX}_BUILD_SHARED AND WIN32)
    add_custom_command(TARGET ${OWL_PROJECT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:${OWL_PROJECT}>" "$<TARGET_FILE_DIR:${OWL_PROJECT}>"
            COMMAND_EXPAND_LISTS
    )
endif ()

add_custom_command(TARGET ${OWL_PROJECT} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/imgui.ini" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imgui.ini"
        VERBATIM)

install(TARGETS ${OWL_PROJECT}
        LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
        DESTINATION assets
)
