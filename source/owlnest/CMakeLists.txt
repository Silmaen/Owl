

set(OWL_PROJECT ${CMAKE_PROJECT_NAME}Nest)

# ----==== app Runner ====----
file(GLOB_RECURSE SRCS_RUNNER CONFIGURE_DEPENDS
        runner/*.cpp
)
file(GLOB_RECURSE HDRS_RUNNER CONFIGURE_DEPENDS
        runner/*.h
)
set(RUNNER_NAME ${CMAKE_PROJECT_NAME}Runner)
add_executable(${RUNNER_NAME} ${SRCS_RUNNER} ${HDRS_RUNNER})
set_target_properties(${RUNNER_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(${RUNNER_NAME} PRIVATE sources)
target_compile_definitions(${RUNNER_NAME} PRIVATE OWL_ASSETS_LOCATION="source/owlnest/assets")
target_link_libraries(${RUNNER_NAME} PRIVATE ${ENGINE_NAME})
set_target_properties(${RUNNER_NAME} PROPERTIES FOLDER "App")
target_import_so_files(${RUNNER_NAME})

# yaml
owl_target_link_libraries(${RUNNER_NAME} PRIVATE yaml-cpp REQUIRED ${THIRD_PARTY_RELEASE})
# ----==== app Runner ====----

#
#  Editor core library: every editor source but the entry point, shared by the
#  OwlNest executable and the owlnest_tests unit tests.
#
set(OWL_NEST_CORE ${OWL_PROJECT}Core)
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
        sources/*.cpp
)
list(REMOVE_ITEM SRCS ${CMAKE_CURRENT_SOURCE_DIR}/sources/main.cpp)
file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS
        sources/*.h
)
add_library(${OWL_NEST_CORE} STATIC ${SRCS} ${HDRS})
set_target_properties(${OWL_NEST_CORE} PROPERTIES FOLDER "App")
target_include_directories(${OWL_NEST_CORE} PUBLIC sources)
target_link_libraries(${OWL_NEST_CORE} PUBLIC ${ENGINE_NAME})
target_compile_definitions(${OWL_NEST_CORE} PUBLIC OWL_ASSETS_LOCATION="source/owlnest/assets")

# ----==== third party ====----

# magic_enum
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC magic_enum REQUIRED ${THIRD_PARTY_RELEASE})

# ImGui
owl_target_link_libraries(${ENGINE_NAME} PRIVATE imgui REQUIRED ${THIRD_PARTY_RELEASE})

# imgui_color_text_edit (syntax-highlighting text editor widget)
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC imgui_color_text_edit REQUIRED ${THIRD_PARTY_RELEASE}
        MODULE_TARGET "imgui_color_text_edit::imgui_color_text_edit")

# md4c (CommonMark + GFM tables/strikethrough/autolinks parser, used by the
# editor's MarkdownPreview / HelpPanel renderer).
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC md4c REQUIRED ${THIRD_PARTY_RELEASE}
        MODULE_TARGET "md4c::md4c")

# lunasvg (live SVG rendering for the code editor preview)
# `find_package(Threads)` is required *before* lunasvg because its transitive
# `plutovg::plutovg` import target references `Threads::Threads` in its link
# interface. The engine library only links lunasvg into an INTERFACE target so
# CMake never has to resolve that property; here we link it into a real
# target which forces full resolution.
find_package(Threads REQUIRED)
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC lunasvg REQUIRED ${THIRD_PARTY_RELEASE})

# yaml
find_package(yaml-cpp REQUIRED)
target_link_libraries(${OWL_NEST_CORE} PUBLIC yaml-cpp::yaml-cpp)

# ----==== third party ====----

#
#  Main Executable
#
add_executable(${OWL_PROJECT} sources/main.cpp)
set_target_properties(${OWL_PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${OWL_PROJECT} PROPERTIES FOLDER "App")
target_link_libraries(${OWL_PROJECT} PRIVATE ${OWL_NEST_CORE})
add_dependencies(${OWL_PROJECT} ${CMAKE_PROJECT_NAME}Runner)

target_import_so_files(${OWL_PROJECT})

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

install(TARGETS ${OWL_PROJECT} ${RUNNER_NAME}
        COMPONENT Nest
        LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/theme.yml
        DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        COMPONENT Nest
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/imgui.ini
        DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        RENAME OwlNest_imgui.ini
        COMPONENT Nest
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
        DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets
        COMPONENT Nest
)
