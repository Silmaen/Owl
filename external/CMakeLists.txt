#
# the third parties
#

#
# fmt library
add_subdirectory(fmt)

#
# include targets for googletest
add_subdirectory(GoogleTest)

#
# include targets for glad
add_subdirectory(glad)
#
# include targets for glfw
add_subdirectory(glfw)
#
# include targets for glm
add_subdirectory(glm)

#
# include targets for imgui
option(IMGUI_USE_DOCKING "Use experimental version of ImGui that has Docking system" ON)
if (IMGUI_USE_DOCKING)
    add_subdirectory(imgui/docking)
else ()
    add_subdirectory(imgui/1.89.1)
endif ()

#
# include targets for spdlog
add_subdirectory(spdlog)
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(spdlog PRIVATE -Wno-ignored-attributes)
endif ()
#
# include targets for yaml-cpp
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL " ")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL " ")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL " ")
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL " ")
add_subdirectory(yaml-cpp)
#
# include targets for nativefiledialog
add_subdirectory(nativefiledialog)


# ---- SPIR-V cross/tools/headers
set(SPIRV_SKIP_TESTS ON CACHE BOOL " ")
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL " ")
set(SPIRV_CROSS_CLI OFF CACHE BOOL " ")
set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL " ")
set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL " ")
set(SPIRV_WERROR OFF CACHE BOOL " ")
set(SHADERC_ENABLE_SHARED_CRT ON)
add_subdirectory(SPIRV-Cross)
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)

# ---- gls lang ---------------
add_subdirectory(glslang)

# ---- Shaderc -----------
set(SHADERC_SKIP_TESTS ON CACHE BOOL " ")
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL " ")
set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL " ")
add_subdirectory(shaderc)
