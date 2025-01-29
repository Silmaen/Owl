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
target_compile_definitions(${ENGINE_NAME}Private INTERFACE ${PRJPREFIX}_ENGINE_BUILD)
target_sources(${ENGINE_NAME}Private INTERFACE ${HDRS_PRIVATE})

# exclude precompiled headers from source
list(REMOVE_ITEM SRCS ${CMAKE_CURRENT_SOURCE_DIR}/private/${PRJPREFIX_LOWER}pch.cpp)
list(REMOVE_ITEM HDRS_PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private/${PRJPREFIX_LOWER}pch.h)
# precompiled headers
if (NOT ${PRJPREFIX}_ENABLE_CLANG_TIDY)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/${PRJPREFIX_LOWER}pch.h")
        target_precompile_headers(${ENGINE_NAME}Private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private/${PRJPREFIX_LOWER}pch.h)
    endif ()
endif ()

# public headers (those in the package)
file(GLOB_RECURSE HDRS_PUBLIC
        public/*.h

)

if (${PRJPREFIX}_BUILD_SHARED)
    add_library(${ENGINE_NAME} SHARED)
    target_compile_definitions(${ENGINE_NAME} PUBLIC ${PRJPREFIX}_BUILD_SHARED)
    target_compile_definitions(${ENGINE_NAME} PRIVATE ${PRJPREFIX}_BUILD_DLL)
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

target_include_directories(${ENGINE_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(${ENGINE_NAME} PUBLIC ${CMAKE_PROJECT_NAME}_Base)
target_link_libraries(${ENGINE_NAME} PRIVATE ${ENGINE_NAME}Private)

# ---------========= PRIVATE third party =========---------

# box2d
find_package(box2d REQUIRED)
message(STATUS "Found box2d version ${box2d_VERSION} @ ${box2d_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE box2d::box2d)

# cpptrace
find_package(cpptrace REQUIRED)
message(STATUS "Found cpptrace version ${cpptrace_VERSION} @ ${cpptrace_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE cpptrace::cpptrace)

# debugbreak
find_package(debugbreak REQUIRED)
message(STATUS "Found debugbreak version ${debugbreak_VERSION} @ ${debugbreak_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE debugbreak::debugbreak)

# glad
find_package(glad 2.0 REQUIRED)
message(STATUS "Found glad version ${glad_VERSION} @ ${glad_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE glad::glad)

# GLFW
find_package(glfw3 REQUIRED)
message(STATUS "Found glfw3 version ${glfw3_VERSION} @ ${glfw3_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE glfw)

# ImGuizmo
find_package(imguizmo REQUIRED)
message(STATUS "Found imguizmo version ${imguizmo_VERSION} @ ${imguizmo_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE imguizmo::imguizmo)

# magic_enum
find_package(magic_enum REQUIRED)
message(STATUS "Found magic_enum version ${magic_enum_VERSION} @ ${magic_enum_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE magic_enum::magic_enum)

# msdf-atlas-gen
find_package(msdf-atlas-gen REQUIRED)
message(STATUS "Found msdf-atlas-gen version ${msdf-atlas-gen_VERSION} @ ${msdf-atlas-gen_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE msdf-atlas-gen::msdf-atlas-gen)

# NativeFile Dialog
find_package(nfd REQUIRED)
message(STATUS "Found nfd version ${nfd_VERSION} @ ${nfd_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE nfd::nfd)

# OpenAL
find_package(OpenAL CONFIG REQUIRED)
message(STATUS "Found OpenAL version ${OpenAL_VERSION} @ ${OpenAL_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE OpenAL::OpenAL)

# SndFile
find_package(SndFile CONFIG REQUIRED)
message(STATUS "Found SndFile version ${SndFile_VERSION} @ ${SndFile_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE SndFile::sndfile)

# stb_image
find_package(stb_image REQUIRED)
message(STATUS "Found stb_image version ${stb_image_VERSION} @ ${stb_image_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE stb_image::stb_image)

# Vulkan
# ----------------------
find_package(VulkanHeaders REQUIRED)
find_package(VulkanLoader REQUIRED)
get_filename_component(VULKAN_DIR ${VulkanLoader_DIR} DIRECTORY)
get_filename_component(VULKAN_DIR ${VULKAN_DIR} DIRECTORY)
message(STATUS "Found Vulkan version ${VulkanLoader_VERSION} @ ${VULKAN_DIR}")
target_link_libraries(${ENGINE_NAME} PRIVATE Vulkan::Loader)
find_package(VulkanUtilityLibraries REQUIRED)
message(STATUS "Found VulkanUtilityLibrary version ${VulkanLoader_VERSION} @ ${VULKAN_DIR}")
target_link_libraries(${ENGINE_NAME} PRIVATE Vulkan::LayerSettings)
target_link_libraries(${ENGINE_NAME} PRIVATE Vulkan::UtilityHeaders)
# Vulkan layers
if (${PRJPREFIX}_DEFINE_VULKAN_LAYERS)
    if (${PRJPREFIX}_PLATFORM_WINDOWS)
        set(${PRJPREFIX}_VULKAN_LAYER_PATH ${VULKAN_DIR}/bin)
        set(${PRJPREFIX}_VULKAN_LAYER_BIN_PATH ${VULKAN_DIR}/bin)
    else ()
        set(${PRJPREFIX}_VULKAN_LAYER_PATH ${VULKAN_DIR}/share/vulkan/explicit_layer.d)
        set(${PRJPREFIX}_VULKAN_LAYER_BIN_PATH ${VULKAN_DIR}/lib)
    endif ()
    target_compile_definitions(${ENGINE_NAME} PUBLIC
            $<BUILD_INTERFACE:${PRJPREFIX}_VULKAN_LAYER_PATH="${${PRJPREFIX}_VULKAN_LAYER_PATH}">
            $<BUILD_INTERFACE:${PRJPREFIX}_VULKAN_LAYER_BIN_PATH="${${PRJPREFIX}_VULKAN_LAYER_BIN_PATH}">)
endif ()
# Spir-V cross
if (${PRJPREFIX}_BUILD_SHARED AND WIN32)
    find_package(spirv_cross_c_shared REQUIRED)
    message(STATUS "Found spirv_cross_c_shared version ${spirv_cross_c_shared_VERSION} @ ${spirv_cross_c_shared_DIR}")
    target_link_libraries(${ENGINE_NAME}Private INTERFACE spirv-cross-c-shared)
else ()
    find_package(spirv_cross_core REQUIRED)
    find_package(spirv_cross_glsl REQUIRED)
    find_package(spirv_cross_hlsl REQUIRED)
    find_package(spirv_cross_msl REQUIRED)
    find_package(spirv_cross_cpp REQUIRED)
    find_package(spirv_cross_reflect REQUIRED)
    find_package(spirv_cross_c REQUIRED)
    message(STATUS "Found spirv_cross_c version ${spirv_cross_c_VERSION} @ ${spirv_cross_c_DIR}")
    target_link_libraries(${ENGINE_NAME}Private INTERFACE spirv-cross-cpp)
endif ()

# shaderc
find_package(SPIRV-Tools REQUIRED)
find_package(SPIRV-Tools-opt REQUIRED)
find_package(glslang REQUIRED)
find_package(shaderc REQUIRED)
target_link_libraries(${ENGINE_NAME}Private INTERFACE shaderc::shaderc_shared)
# ----------------------

# yaml
find_package(yaml-cpp REQUIRED)
message(STATUS "Found yaml-cpp version ${yaml-cpp_VERSION} @ ${yaml-cpp_DIR}")
target_link_libraries(${ENGINE_NAME}Private INTERFACE yaml-cpp::yaml-cpp)

# ---------========= PUBLIC third party =========---------

# Entt
find_package(EnTT REQUIRED)
message(STATUS "Found EnTT version ${EnTT_VERSION} @ ${EnTT_DIR}")
target_link_libraries(${ENGINE_NAME} PUBLIC EnTT::EnTT)

# fmt
find_package(fmt REQUIRED)
message(STATUS "Found fmt version ${fmt_VERSION} @ ${fmt_DIR}")
target_link_libraries(${ENGINE_NAME} PUBLIC fmt::fmt)

# ImGui
find_package(imgui REQUIRED)
message(STATUS "Found imgui version ${imgui_VERSION} @ ${imgui_DIR}")
target_link_libraries(${ENGINE_NAME} PUBLIC imgui::imgui)

# spdlog
find_package(spdlog REQUIRED)
message(STATUS "Found Spdlog version ${spdlog_VERSION} @ ${spdlog_DIR}")
target_link_libraries(${ENGINE_NAME} PUBLIC spdlog::spdlog)


# additional windows libs
if (${PRJPREFIX}_PLATFORM_WINDOWS)
    target_link_libraries(${ENGINE_NAME} PUBLIC Mf Mfplat Mfreadwrite)

    if (${PRJPREFIX}_BUILD_SHARED)
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
elseif (${PRJPREFIX}_PLATFORM_LINUX)
    target_import_so_files(${ENGINE_NAME})
endif ()
# ------------------------------------------------------------------------------
#    Installations
# ------------------------------------------------------------------------------
if (${PRJPREFIX}_PACKAGE_ENGINE)
    foreach (config IN ITEMS Debug Release)
        install(TARGETS ${ENGINE_NAME} EXPORT OwlTargets${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_PROCESSOR}-${config}
                CONFIGURATIONS ${config}
                LIBRARY DESTINATION ${${PRJPREFIX}_INSTALL_BIN}/${config}
                ARCHIVE DESTINATION ${${PRJPREFIX}_INSTALL_LIB}/${config}
                RUNTIME DESTINATION ${${PRJPREFIX}_INSTALL_BIN}/${config}
                FRAMEWORK DESTINATION ${${PRJPREFIX}_INSTALL_BIN}/${config}
                FILE_SET HEADERS DESTINATION include
                COMPONENT Engine
        )
        install(TARGETS ${CMAKE_PROJECT_NAME}_Base EXPORT OwlTargets${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_PROCESSOR}-${config}
                CONFIGURATIONS ${config}
                COMPONENT Engine)
        install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/
                DESTINATION ${${PRJPREFIX}_INSTALL_BIN}/${config}
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
            LIBRARY DESTINATION ${${PRJPREFIX}_INSTALL_BIN}
            RUNTIME DESTINATION ${${PRJPREFIX}_INSTALL_BIN}
            FRAMEWORK DESTINATION ${${PRJPREFIX}_INSTALL_BIN}
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/
            DESTINATION ${${PRJPREFIX}_INSTALL_BIN}
    )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine_assets/
            DESTINATION ${${PRJPREFIX}_INSTALL_BIN}/assets
            COMPONENT Engine
    )
endif ()
