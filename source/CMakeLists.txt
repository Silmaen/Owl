#
#
#
add_subdirectory(owl)
# The shader compiler and the build-time shader compilation need Slang, part of the render module. Android has no
# Slang: the host bakes its shaders.
if (${PROJECT_PREFIX}_MODULE_RENDER AND NOT ${PROJECT_PREFIX}_PLATFORM_ANDROID)
    add_subdirectory(shader_compiler)
    add_subdirectory(tools)
endif ()
# The benchmarks drive the MCP server of the editor libraries (built without the editor when OWL_BUILD_NEST is off).
# Android builds the game runner only, from the same folder.
if (${PROJECT_PREFIX}_BUILD_NEST OR ${PROJECT_PREFIX}_BENCHMARK OR ${PROJECT_PREFIX}_PLATFORM_ANDROID)
    add_subdirectory(owlnest)
endif ()
