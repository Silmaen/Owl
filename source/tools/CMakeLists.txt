#
# Build tools, run during the build, never installed.
#

# ----==== Shader bake ====----
# Compiles the engine Slang shaders to SPIR-V into bin/assets, where the runner and the editor (dev tree and both
# CPack components) find them before their own cache: a known output is never compiled again at startup.
set(OWL_SHADER_BAKE ${CMAKE_PROJECT_NAME}ShaderBake)
file(GLOB_RECURSE SRCS_SHADER_BAKE CONFIGURE_DEPENDS shader_bake/*.cpp)
add_executable(${OWL_SHADER_BAKE} ${SRCS_SHADER_BAKE})
set_target_properties(${OWL_SHADER_BAKE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tools")
# The shader utilities are engine internals, as for the benchmarks.
target_link_libraries(${OWL_SHADER_BAKE} PRIVATE ${ENGINE_NAME} ${ENGINE_NAME}Private)
target_import_so_files(${OWL_SHADER_BAKE})

if (CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    message(STATUS "Shader bake: cross compilation without emulator, the shaders compile at first run.")
    return()
endif ()
set(_owl_baked_assets "${CMAKE_BINARY_DIR}/bin/assets")
file(GLOB_RECURSE OWL_SLANG_SHADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine_assets/shaders/*.slang)
set(_owl_bake_stamp "${CMAKE_BINARY_DIR}/shader_bake.stamp")
add_custom_command(OUTPUT ${_owl_bake_stamp}
        # The target name (not its file) gets the cross-compiling emulator prepended.
        COMMAND ${OWL_SHADER_BAKE} "${CMAKE_SOURCE_DIR}/engine_assets/shaders" "${_owl_baked_assets}"
        COMMAND ${CMAKE_COMMAND} -E touch ${_owl_bake_stamp}
        DEPENDS ${OWL_SHADER_BAKE} ${OWL_SLANG_SHADERS}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        COMMENT "Compile the engine shaders to SPIR-V"
        VERBATIM
)
add_custom_target(${OWL_SHADER_BAKE}Run ALL DEPENDS ${_owl_bake_stamp})
set_target_properties(${OWL_SHADER_BAKE}Run PROPERTIES FOLDER "Tools")
unset(_owl_bake_stamp)

# Next to the engine assets of each component; a game pack embeds them with the other shader files.
install(DIRECTORY ${_owl_baked_assets}/shaders/
        DESTINATION assets/shaders
        COMPONENT Engine
)
if (${PROJECT_PREFIX}_BUILD_NEST)
    install(DIRECTORY ${_owl_baked_assets}/shaders/
            DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets/shaders
            COMPONENT Nest
    )
endif ()
unset(_owl_baked_assets)
