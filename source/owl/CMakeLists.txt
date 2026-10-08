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

# public headers (those in the package)
file(GLOB_RECURSE HDRS_PUBLIC CONFIGURE_DEPENDS
        public/*.h

)

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
if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    target_compile_definitions(${ENGINE_NAME} INTERFACE $<INSTALL_INTERFACE:${PROJECT_PREFIX}_PLATFORM_WINDOWS>)
elseif (${PROJECT_PREFIX}_PLATFORM_LINUX)
    target_compile_definitions(${ENGINE_NAME} INTERFACE $<INSTALL_INTERFACE:${PROJECT_PREFIX}_PLATFORM_LINUX>)
endif ()
target_link_libraries(${ENGINE_NAME} PRIVATE ${ENGINE_NAME}Private)

# ---------========= PRIVATE third party =========---------
if (${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY)
    set(THIRD_PARTY_RELEASE FORCE_RELEASE)
endif ()
# box2d
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE box2d REQUIRED ${THIRD_PARTY_RELEASE})

# lua
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE lua CONFIG REQUIRED ${THIRD_PARTY_RELEASE})

# cpptrace
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE cpptrace REQUIRED ${THIRD_PARTY_RELEASE})

# glad
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glad 2.0 REQUIRED ${THIRD_PARTY_RELEASE})

# GLFW
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glfw3 MODULE_TARGET glfw REQUIRED ${THIRD_PARTY_RELEASE})

# ImGuizmo
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE imguizmo REQUIRED ${THIRD_PARTY_RELEASE})

# lunasvg
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE lunasvg REQUIRED ${THIRD_PARTY_RELEASE})

# magic_enum
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE magic_enum REQUIRED ${THIRD_PARTY_RELEASE})

# msdf-atlas-gen
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE msdf-atlas-gen REQUIRED ${THIRD_PARTY_RELEASE})

# NativeFile Dialog
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE nfd REQUIRED ${THIRD_PARTY_RELEASE})

# OpenAL
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE OpenAL CONFIG REQUIRED ${THIRD_PARTY_RELEASE})

# SndFile
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE SndFile REQUIRED ${THIRD_PARTY_RELEASE} MODULE_TARGET "SndFile::sndfile")

# stb_image
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE stb_image REQUIRED ${THIRD_PARTY_RELEASE})

# libpng (PNG writing; already a dependency of msdf-atlas-gen)
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE PNG REQUIRED ${THIRD_PARTY_RELEASE})

# tinygltf
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE TinyGLTF REQUIRED ${THIRD_PARTY_RELEASE} MODULE_TARGET "tinygltf::tinygltf")

# tinyobjloader
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE tinyobjloader REQUIRED ${THIRD_PARTY_RELEASE})

# ufbx
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE ufbx REQUIRED ${THIRD_PARTY_RELEASE})

# Vulkan
owl_target_link_vulkan(${ENGINE_NAME})

# spdlog
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE spdlog REQUIRED ${THIRD_PARTY_RELEASE})

# zstd
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE zstd REQUIRED ${THIRD_PARTY_RELEASE} MODULE_TARGET "zstd::libzstd_static")

# taskflow
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE Taskflow REQUIRED ${THIRD_PARTY_RELEASE} MODULE_TARGET "Taskflow::Taskflow")

# Tracy client (OWL_PROFILER=tracy): private, the OWL_PROFILE_* macros of the public headers never include it
if (${PROJECT_PREFIX}_PROFILER_BACKEND STREQUAL "tracy")
    owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE Tracy CONFIG REQUIRED ${THIRD_PARTY_RELEASE} MODULE_TARGET "Tracy::TracyClient")
endif ()

# ---------========= PUBLIC third party =========---------

# Entt
owl_target_link_libraries(${ENGINE_NAME} PUBLIC EnTT REQUIRED ${THIRD_PARTY_RELEASE})

# ImGui
owl_target_link_libraries(${ENGINE_NAME} PUBLIC imgui REQUIRED ${THIRD_PARTY_RELEASE})

# yaml-cpp: public until YAML leaves the public headers (renderer/RenderLayer.h, audit G-07 / PR-27)
owl_target_link_libraries(${ENGINE_NAME} PUBLIC yaml-cpp REQUIRED ${THIRD_PARTY_RELEASE})


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
        add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenAL_DIR}/../../../bin/OpenAL32$<$<CONFIG:Debug>:d>.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                COMMAND_EXPAND_LISTS
                COMMENT "Copy the OpenAL shared dll"
        )
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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_assets/
        DESTINATION assets
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
    )
endif ()
