#
#  Main Executable
#
set(OWL_PROJECT ${CMAKE_PROJECT_NAME}Sandbox)

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

target_import_so_files(${OWL_PROJECT})
# ----==== third party ====----
# ----==== third party ====----

if (${PROJECT_PREFIX}_BUILD_SHARED AND WIN32)
    add_custom_command(TARGET ${OWL_PROJECT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:${OWL_PROJECT}>" "$<TARGET_FILE_DIR:${OWL_PROJECT}>"
            COMMAND_EXPAND_LISTS
    )
endif ()

if (${PROJECT_PREFIX}_PACKAGE_ENGINE)
    foreach (config IN ITEMS Debug Release)
        install(TARGETS ${OWL_PROJECT}
                CONFIGURATIONS ${config}
                LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                ARCHIVE DESTINATION ${${PROJECT_PREFIX}_INSTALL_LIB}/${config}
                RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                COMPONENT Engine
        )
    endforeach ()
else ()
    install(TARGETS ${OWL_PROJECT}
            LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
    )
endif ()
