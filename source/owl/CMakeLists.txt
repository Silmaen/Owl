#
# base library
#

# List or source files
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
        *.cpp
)
file(GLOB_RECURSE HDRS_PRIVATE CONFIGURE_DEPENDS
        private/*.h
)
# public headers (those in the package)
file(GLOB_RECURSE HDRS_PUBLIC CONFIGURE_DEPENDS
        public/*.h
)

# ---------========= Optional modules (OWL_MODULE_*) =========---------
# A module turned OFF leaves out its backend sources and third parties. Its public API stays (Null backend, or the
# no-op implementation of `<module>/disabled/`), except Gui whose headers leave the package. OWL_WITH_<MODULE> (0 or
# 1) tells the code, the engine's as the consumers'.
set(_owl_excluded "")
if (NOT ${PROJECT_PREFIX}_MODULE_RENDER)
    list(APPEND _owl_excluded "/private/renderer/gpu/(opengl|vulkan)/" "/private/(window|input)/glfw/"
            "/private/renderer/utils/shaderFileUtils\\.(cpp|h)$" "/private/core/external/(glfw3|opengl46|slang|lunasvg)\\.h$")
endif ()
if (${PROJECT_PREFIX}_MODULE_PHYSICS)
    list(APPEND _owl_excluded "/private/physics/disabled/")
else ()
    list(APPEND _owl_excluded "/private/physics/(PhysicCommand|SolverTaskPool)\\.(cpp|h)$")
endif ()
if (NOT ${PROJECT_PREFIX}_MODULE_AUDIO)
    list(APPEND _owl_excluded "/private/sound/openal/" "/private/core/external/openal\\.h$")
endif ()
if (${PROJECT_PREFIX}_MODULE_SCRIPT)
    list(APPEND _owl_excluded "/private/script/disabled/")
else ()
    list(APPEND _owl_excluded "/private/script/(LuaBindings|LuaEngine|ScriptEngine|ScriptInstance)\\.(cpp|h)$"
            "/private/core/external/lua\\.h$")
endif ()
if (NOT ${PROJECT_PREFIX}_MODULE_GUI)
    list(APPEND _owl_excluded "/(private|public)/gui/" "/public/owlgui\\.h$" "/private/core/external/imgui\\.h$")
endif ()
foreach (_owl_regex IN LISTS _owl_excluded)
    list(FILTER SRCS EXCLUDE REGEX "${_owl_regex}")
    list(FILTER HDRS_PRIVATE EXCLUDE REGEX "${_owl_regex}")
    list(FILTER HDRS_PUBLIC EXCLUDE REGEX "${_owl_regex}")
endforeach ()
unset(_owl_regex)
unset(_owl_excluded)
add_library(${ENGINE_NAME}Private INTERFACE)
target_include_directories(${ENGINE_NAME}Private INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private>)
target_compile_definitions(${ENGINE_NAME}Private INTERFACE ${PROJECT_PREFIX}_ENGINE_BUILD)
target_sources(${ENGINE_NAME}Private INTERFACE ${HDRS_PRIVATE})

# exclude precompiled headers from source
list(REMOVE_ITEM SRCS ${CMAKE_CURRENT_SOURCE_DIR}/private/${PROJECT_PREFIX_LOWER}pch.cpp)
list(REMOVE_ITEM HDRS_PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private/${PROJECT_PREFIX_LOWER}pch.h)
# precompiled headers
if (NOT ${PROJECT_PREFIX}_ENABLE_CLANG_TIDY)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/${PROJECT_PREFIX_LOWER}pch.h")
        target_precompile_headers(${ENGINE_NAME}Private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private/${PROJECT_PREFIX_LOWER}pch.h)
    endif ()
endif ()

if (${PROJECT_PREFIX}_BUILD_SHARED)
    add_library(${ENGINE_NAME} SHARED)
    target_compile_definitions(${ENGINE_NAME} PUBLIC ${PROJECT_PREFIX}_BUILD_SHARED)
    target_compile_definitions(${ENGINE_NAME} PRIVATE ${PROJECT_PREFIX}_BUILD_DLL)
else ()
    add_library(${ENGINE_NAME} STATIC)
    set_target_properties(${ENGINE_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()
target_sources(${ENGINE_NAME}
        PRIVATE
        ${SRCS}
        PUBLIC FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public FILES
        ${HDRS_PUBLIC}
)
set_target_properties(${ENGINE_NAME} PROPERTIES FOLDER "Engine")

target_include_directories(${ENGINE_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public>
        $<INSTALL_INTERFACE:include>
)
# Owl_Base carries the project's own warnings (-Werror -Weverything...) and build settings: in-tree consumers
# (editor, tests) inherit them, the installed package does not. It only exports what the public headers need.
target_link_libraries(${ENGINE_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_PROJECT_NAME}_Base>)
target_compile_features(${ENGINE_NAME} PUBLIC cxx_std_23)
foreach (_owl_module RENDER PHYSICS AUDIO SCRIPT GUI)
    if (${PROJECT_PREFIX}_MODULE_${_owl_module})
        target_compile_definitions(${ENGINE_NAME} PUBLIC ${PROJECT_PREFIX}_WITH_${_owl_module}=1)
    else ()
        target_compile_definitions(${ENGINE_NAME} PUBLIC ${PROJECT_PREFIX}_WITH_${_owl_module}=0)
    endif ()
endforeach ()
unset(_owl_module)
if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    target_compile_definitions(${ENGINE_NAME} INTERFACE $<INSTALL_INTERFACE:${PROJECT_PREFIX}_PLATFORM_WINDOWS>)
elseif (${PROJECT_PREFIX}_PLATFORM_LINUX)
    target_compile_definitions(${ENGINE_NAME} INTERFACE $<INSTALL_INTERFACE:${PROJECT_PREFIX}_PLATFORM_LINUX>)
endif ()
target_link_libraries(${ENGINE_NAME} PRIVATE ${ENGINE_NAME}Private)
if (${PROJECT_PREFIX}_PLATFORM_LINUX)
    # Export only the OWL_API symbols, as the Windows DLL does, and none of the static third-party libraries.
    set_target_properties(${ENGINE_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
    target_link_options(${ENGINE_NAME} PRIVATE "LINKER:--exclude-libs,ALL")
endif ()

# ---------========= PRIVATE third party =========---------
# core
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE cpptrace REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE magic_enum REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE msdf-atlas-gen REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE nfd REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE stb_image REQUIRED)
# libpng (PNG writing; already a dependency of msdf-atlas-gen)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE PNG REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE TinyGLTF REQUIRED MODULE_TARGET "tinygltf::tinygltf")
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE tinyobjloader REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE ufbx REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE spdlog REQUIRED)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE zstd REQUIRED MODULE_TARGET "zstd::libzstd_static")
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE Taskflow REQUIRED MODULE_TARGET "Taskflow::Taskflow")
# yaml-cpp: the public headers carry YAML as text (renderer/RenderStack.h), never yaml-cpp types (audit G-07)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE yaml-cpp REQUIRED)
# Tracy client (OWL_PROFILER=tracy): private, the OWL_PROFILE_* macros of the public headers never include it
if (${PROJECT_PREFIX}_PROFILER_BACKEND STREQUAL "tracy")
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE Tracy CONFIG REQUIRED MODULE_TARGET "Tracy::TracyClient")
endif ()

# render: GPU backends, GLFW, the window icon (lunasvg), Vulkan, SPIRV-Cross and Slang
if (${PROJECT_PREFIX}_MODULE_RENDER)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glad 2.0 REQUIRED)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glfw3 MODULE_TARGET glfw REQUIRED)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE lunasvg REQUIRED)
    owl_target_link_vulkan(${ENGINE_NAME})
    # Vulkan Memory Allocator (header only)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE VulkanMemoryAllocator REQUIRED MODULE_TARGET "GPUOpen::VulkanMemoryAllocator")
endif ()

# physics
if (${PROJECT_PREFIX}_MODULE_PHYSICS)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE box2d REQUIRED)
endif ()

# audio
if (${PROJECT_PREFIX}_MODULE_AUDIO)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE OpenAL CONFIG REQUIRED)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE SndFile REQUIRED MODULE_TARGET "SndFile::sndfile")
endif ()

# script
if (${PROJECT_PREFIX}_MODULE_SCRIPT)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE lua CONFIG REQUIRED)
endif ()

# gui: ImGui is private to the engine; the gui/ headers that include it come with Owl::Gui
if (${PROJECT_PREFIX}_MODULE_GUI)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE imguizmo REQUIRED)
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE imgui REQUIRED)
endif ()

# ---------========= PUBLIC third party =========---------

# Entt: the only public dependency of Owl::OwlEngine (scene/Scene.h, scene/Entity.h)
owl_target_link_libraries(${ENGINE_NAME} PUBLIC EnTT REQUIRED)

# Owl::Gui: the engine plus imgui, for the consumers of <owlgui.h> (the editor, a game's own tools)
if (${PROJECT_PREFIX}_MODULE_GUI)
    add_library(${ENGINE_NAME}Gui INTERFACE)
    add_library(Owl::Gui ALIAS ${ENGINE_NAME}Gui)
    set_target_properties(${ENGINE_NAME}Gui PROPERTIES EXPORT_NAME Gui)
    target_link_libraries(${ENGINE_NAME}Gui INTERFACE ${ENGINE_NAME} imgui::imgui)
    # Build tree only, as for the engine: imgui_stdlib and the backends (the editor uses imgui_stdlib).
    target_link_libraries(${ENGINE_NAME}Gui INTERFACE $<BUILD_INTERFACE:${PROJECT_PREFIX_LOWER}_imgui_bindings>)
endif ()


# additional windows libs
if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    target_link_libraries(${ENGINE_NAME} PUBLIC Mf Mfplat Mfreadwrite)

    if (${PROJECT_PREFIX}_BUILD_SHARED)
        add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:${ENGINE_NAME}>" "$<TARGET_FILE_DIR:${ENGINE_NAME}>"
                COMMAND_EXPAND_LISTS
                COMMENT "Copy the shared dll"
        )
    else ()
        if (${PROJECT_PREFIX}_MODULE_RENDER)
            set(vulkan_libs
                    vulkan-1
            )
            foreach (l_lib ${vulkan_libs})
                add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VulkanLoader_DIR}/../../bin/${l_lib}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                        COMMAND_EXPAND_LISTS
                        COMMENT "Copy the vulkan shared dll"
                )
            endforeach ()
            add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${slang_DIR}/../../bin/slang.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                    COMMAND_EXPAND_LISTS
                    COMMENT "Copy the slang shared dll"
            )
        endif ()
        if (${PROJECT_PREFIX}_MODULE_AUDIO)
            add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenAL_DIR}/../../../bin/OpenAL32$<$<CONFIG:Debug>:d>.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                    COMMAND_EXPAND_LISTS
                    COMMENT "Copy the OpenAL shared dll"
            )
        endif ()
    endif ()
elseif (${PROJECT_PREFIX}_PLATFORM_LINUX)
    target_import_so_files(${ENGINE_NAME})
endif ()
# ------------------------------------------------------------------------------
#    Installations
# ------------------------------------------------------------------------------
# Two CPack components from one tree: the Engine SDK (lib/, include/, assets/, CMake config) and, with the editor,
# the Nest application (everything in bin/<platform>_<arch>/, third-party shared libraries included).
install(TARGETS ${ENGINE_NAME}
        EXPORT OwlEngineTargets
        COMPONENT Engine
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        FRAMEWORK DESTINATION lib
        FILE_SET HEADERS DESTINATION include
)
if (${PROJECT_PREFIX}_MODULE_GUI)
    install(TARGETS ${ENGINE_NAME}Gui
            EXPORT OwlEngineGuiTargets
            COMPONENT Engine
    )
endif ()
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_assets/
        DESTINATION assets
        COMPONENT Engine
        # A bundle generated there by an older configure.
        PATTERN "help" EXCLUDE
)
install(DIRECTORY ${${PROJECT_PREFIX}_HELP_DIR}/
        DESTINATION assets/help
        COMPONENT Engine
)
if (${PROJECT_PREFIX}_BUILD_NEST)
    install(TARGETS ${ENGINE_NAME}
            COMPONENT Nest
            LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
    )
    # The tested tree also holds the tests and what their runs wrote (logs, configs, caches): copy only what ships.
    install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/
            DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            COMPONENT Nest
            FILES_MATCHING
            PATTERN "*.so*"
            PATTERN "*.dll"
            REGEX "/slang-standard-module[^/]*/"
            PATTERN "owl_*" EXCLUDE
            PATTERN "cache" EXCLUDE
            PATTERN "render_tests" EXCLUDE
    )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_assets/
            DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets
            COMPONENT Nest
            PATTERN "help" EXCLUDE
    )
    install(DIRECTORY ${${PROJECT_PREFIX}_HELP_DIR}/
            DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets/help
            COMPONENT Nest
    )
endif ()
