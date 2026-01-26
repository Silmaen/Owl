#
# base library
#

# List or source files
file(GLOB_RECURSE SRCS
        *.cpp
)
file(GLOB_RECURSE HDRS_PRIVATE
        private/*.h
)
add_library(${ENGINE_NAME}Private INTERFACE)
target_include_directories(${ENGINE_NAME}Private INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private>)
target_compile_definitions(${ENGINE_NAME}Private INTERFACE ${PROJECT_PREFIX}_ENGINE_BUILD)
if (${PROJECT_PREFIX}_PACKAGE_ENGINE)
    target_compile_definitions(${ENGINE_NAME}Private INTERFACE ${PROJECT_PREFIX}_PACKAGE_ENGINE)
endif ()
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
file(GLOB_RECURSE HDRS_PUBLIC
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
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES
        ${HDRS_PUBLIC}
)
set_target_properties(${ENGINE_NAME} PROPERTIES FOLDER "Engine")

target_include_directories(${ENGINE_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(${ENGINE_NAME} PUBLIC ${CMAKE_PROJECT_NAME}_Base)
target_link_libraries(${ENGINE_NAME} PRIVATE ${ENGINE_NAME}Private)

# ---------========= PRIVATE third party =========---------
if (${PROJECT_PREFIX}_USE_RELEASE_THIRD_PARTY)
    set(THIRD_PARTY_RELEASE FORCE_RELEASE)
endif ()
# box2d
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE box2d REQUIRED ${THIRD_PARTY_RELEASE})

# cpptrace
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE cpptrace REQUIRED ${THIRD_PARTY_RELEASE})

# debugbreak
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE debugbreak REQUIRED ${THIRD_PARTY_RELEASE})

# glad
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glad 2.0 REQUIRED ${THIRD_PARTY_RELEASE})

# GLFW
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE glfw3 MODULE_TARGET glfw REQUIRED ${THIRD_PARTY_RELEASE})

# ImGuizmo
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE imguizmo REQUIRED ${THIRD_PARTY_RELEASE})

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

# yaml
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE yaml-cpp REQUIRED ${THIRD_PARTY_RELEASE})

# zeus
owl_target_link_libraries(${ENGINE_NAME}Private INTERFACE zeus REQUIRED ${THIRD_PARTY_RELEASE} MODULE_TARGET "zeus::expected")

# TBB (optionnal)
find_package(TBB QUIET)
if (TBB_FOUND)
    target_link_libraries(${ENGINE_NAME}Private INTERFACE TBB::tbb)
    target_compile_definitions(${ENGINE_NAME}Private INTERFACE ${PROJECT_PREFIX}_ENGINE_USE_TBB)
endif ()

# ---------========= PUBLIC third party =========---------

# Entt
owl_target_link_libraries(${ENGINE_NAME} PUBLIC EnTT REQUIRED ${THIRD_PARTY_RELEASE})

# ImGui
owl_target_link_libraries(${ENGINE_NAME} PUBLIC imgui REQUIRED ${THIRD_PARTY_RELEASE})


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
        set(shader_libs
                libshaderc_shared
                SPIRV
                vulkan-1
        )
        foreach (l_lib ${shader_libs})
            add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${shaderc_DIR}/../../bin/${l_lib}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                    COMMAND_EXPAND_LISTS
                    COMMENT "Copy the vulkan shared dll"
            )
        endforeach ()
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
if (${PROJECT_PREFIX}_PACKAGE_ENGINE)
    foreach (config IN ITEMS Debug Release)
        install(TARGETS ${ENGINE_NAME} EXPORT OwlTargets${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_PROCESSOR}-${config}
                CONFIGURATIONS ${config}
                LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                ARCHIVE DESTINATION ${${PROJECT_PREFIX}_INSTALL_LIB}/${config}
                RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                FILE_SET HEADERS DESTINATION include
                COMPONENT Engine
        )
        install(TARGETS ${CMAKE_PROJECT_NAME}_Base EXPORT OwlTargets${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_PROCESSOR}-${config}
                CONFIGURATIONS ${config}
                COMPONENT Engine)
        install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/
                DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/${config}
                CONFIGURATIONS ${config}
                COMPONENT Engine
        )
    endforeach ()
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_assets/
            DESTINATION assets
            COMPONENT Engine
    )
else ()
    install(TARGETS ${ENGINE_NAME}
            LIBRARY DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            RUNTIME DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
            FRAMEWORK DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/
            DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}
    )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_assets/
            DESTINATION ${${PROJECT_PREFIX}_INSTALL_BIN}/assets
            COMPONENT Engine
    )
endif ()
