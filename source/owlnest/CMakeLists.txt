

set(OWL_PROJECT ${CMAKE_PROJECT_NAME}Nest)

# ----==== app Runner ====----
file(GLOB_RECURSE SRCS_RUNNER CONFIGURE_DEPENDS
        runner/*.cpp
)
file(GLOB_RECURSE HDRS_RUNNER CONFIGURE_DEPENDS
        runner/*.h
)
#
#  Editor commands: undo stack, scene commands, the command registry and the editor session (headless one included),
#  without any UI. Shared by the editor, the runner (scenario `command:` steps) and the owlnest tests.
#
set(OWL_NEST_COMMANDS ${OWL_PROJECT}Commands)
file(GLOB_RECURSE SRCS_COMMANDS CONFIGURE_DEPENDS
        sources/commands/*.cpp
        sources/session/*.cpp
        sources/EntitySnapshot.cpp
        sources/Undo*.cpp
)
# The node-graph commands edit documents of the editor UI: they stay in OwlNestCore.
list(FILTER SRCS_COMMANDS EXCLUDE REGEX "NodeGraphCommands\\.cpp$")
add_library(${OWL_NEST_COMMANDS} STATIC ${SRCS_COMMANDS})
set_target_properties(${OWL_NEST_COMMANDS} PROPERTIES FOLDER "App")
target_include_directories(${OWL_NEST_COMMANDS} PUBLIC sources)
target_link_libraries(${OWL_NEST_COMMANDS} PUBLIC ${ENGINE_NAME})
owl_target_link_libraries(${OWL_NEST_COMMANDS} PUBLIC yaml-cpp REQUIRED ${THIRD_PARTY_RELEASE})

#
#  MCP server: JSON-RPC 2.0 dispatcher over an editor session (tools from the command registry, resources) and its
#  Streamable HTTP transport on 127.0.0.1 (cpp-httplib, no TLS), whose messages the main loop runs.
#
set(OWL_NEST_MCP ${OWL_PROJECT}Mcp)
file(GLOB_RECURSE SRCS_MCP CONFIGURE_DEPENDS
        sources/mcp/*.cpp
)
add_library(${OWL_NEST_MCP} STATIC ${SRCS_MCP})
set_target_properties(${OWL_NEST_MCP} PROPERTIES FOLDER "App")
target_link_libraries(${OWL_NEST_MCP} PUBLIC ${OWL_NEST_COMMANDS})
owl_target_link_libraries(${OWL_NEST_MCP} PUBLIC nlohmann_json REQUIRED)
# Public: the owlnest tests drive the server with its client.
owl_target_link_libraries(${OWL_NEST_MCP} PUBLIC httplib REQUIRED)
if (WIN32)
    # Token drawn from BCryptGenRandom.
    target_link_libraries(${OWL_NEST_MCP} PRIVATE bcrypt)
    # cpp-httplib requires Windows 10, MinGW defaults to an older target.
    target_compile_definitions(${OWL_NEST_MCP} PUBLIC _WIN32_WINNT=0x0A00)
endif ()

# Benchmarks without the editor: the command and MCP libraries only.
if (NOT ${PROJECT_PREFIX}_BUILD_NEST)
    return()
endif ()

set(RUNNER_NAME ${CMAKE_PROJECT_NAME}Runner)
add_executable(${RUNNER_NAME} ${SRCS_RUNNER} ${HDRS_RUNNER})
set_target_properties(${RUNNER_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(${RUNNER_NAME} PRIVATE sources)
target_compile_definitions(${RUNNER_NAME} PRIVATE OWL_ASSETS_LOCATION="source/owlnest/assets")
target_link_libraries(${RUNNER_NAME} PRIVATE ${ENGINE_NAME} ${OWL_NEST_COMMANDS})
set_target_properties(${RUNNER_NAME} PROPERTIES FOLDER "App")
target_import_so_files(${RUNNER_NAME})
owl_windows_application(${RUNNER_NAME} "Owl Runner, the game player of Owl Engine")

# yaml
owl_target_link_libraries(${RUNNER_NAME} PRIVATE yaml-cpp REQUIRED)
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
list(REMOVE_ITEM SRCS ${SRCS_COMMANDS} ${SRCS_MCP})
file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS
        sources/*.h
)
add_library(${OWL_NEST_CORE} STATIC ${SRCS} ${HDRS})
set_target_properties(${OWL_NEST_CORE} PROPERTIES FOLDER "App")
target_include_directories(${OWL_NEST_CORE} PUBLIC sources)
target_link_libraries(${OWL_NEST_CORE} PUBLIC Owl::Gui ${OWL_NEST_COMMANDS} ${OWL_NEST_MCP})
target_compile_definitions(${OWL_NEST_CORE} PUBLIC OWL_ASSETS_LOCATION="source/owlnest/assets")
# The help bundle of this build tree, used when no asset directory holds `help/` (a package installs it in assets/).
target_compile_definitions(${OWL_NEST_CORE} PUBLIC OWL_HELP_BUILD_DIR="${${PROJECT_PREFIX}_HELP_DIR}")

# ----==== third party ====----

# magic_enum
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC magic_enum REQUIRED)

# imgui_color_text_edit (syntax-highlighting text editor widget)
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC imgui_color_text_edit REQUIRED
        MODULE_TARGET "imgui_color_text_edit::imgui_color_text_edit")

# md4c (CommonMark + GFM tables/strikethrough/autolinks parser, used by the
# editor's MarkdownPreview / HelpPanel renderer).
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC md4c REQUIRED
        MODULE_TARGET "md4c::md4c")

# lunasvg (live SVG rendering for the code editor preview)
# `find_package(Threads)` is required *before* lunasvg because its transitive
# `plutovg::plutovg` import target references `Threads::Threads` in its link
# interface. The engine library only links lunasvg into an INTERFACE target so
# CMake never has to resolve that property; here we link it into a real
# target which forces full resolution.
find_package(Threads REQUIRED)
owl_target_link_libraries(${OWL_NEST_CORE} PUBLIC lunasvg REQUIRED)

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
owl_windows_application(${OWL_PROJECT} "Owl Nest, the editor of Owl Engine")

target_import_so_files(${OWL_PROJECT})

if (${PROJECT_PREFIX}_BUILD_SHARED AND WIN32)
    add_custom_command(TARGET ${OWL_PROJECT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:${OWL_PROJECT}>" "$<TARGET_FILE_DIR:${OWL_PROJECT}>"
            COMMAND_EXPAND_LISTS
    )
endif ()

install(TARGETS ${OWL_PROJECT} ${RUNNER_NAME}
        COMPONENT Nest
        LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
        FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
        DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets
        COMPONENT Nest
)
# The editor rasterizes its icons from the SVG sources at run time (theme colours): installed with the other icons.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets_sources/icons/
        DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets/icons
        COMPONENT Nest
        FILES_MATCHING
        PATTERN "*.svg"
        PATTERN "templates" EXCLUDE
)
# The feature showcase, opened from the File screen.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_demo/
        DESTINATION engine_demo
        COMPONENT Nest
)
# The sample games, beside the Engine Demo (one project folder each), listed by the File screen.
if (EXISTS ${CMAKE_SOURCE_DIR}/samples)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/samples/
            DESTINATION samples
            COMPONENT Nest
    )
endif ()
