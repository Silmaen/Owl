#
#
#
add_subdirectory(owl)
# The shader compiler and the build-time shader compilation need Slang, part of the render module.
if (${PROJECT_PREFIX}_MODULE_RENDER)
    add_subdirectory(shader_compiler)
    add_subdirectory(tools)
endif ()
# The benchmarks drive the MCP server of the editor libraries (built without the editor when OWL_BUILD_NEST is off).
if (${PROJECT_PREFIX}_BUILD_NEST OR ${PROJECT_PREFIX}_BENCHMARK)
    add_subdirectory(owlnest)
endif ()
