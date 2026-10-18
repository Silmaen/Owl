#
#
#
add_subdirectory(owl)
# Build-time shader compilation needs Slang, part of the render module.
if (${PROJECT_PREFIX}_MODULE_RENDER)
    add_subdirectory(tools)
endif ()
# The benchmarks drive the MCP server of the editor libraries (built without the editor when OWL_BUILD_NEST is off).
if (${PROJECT_PREFIX}_BUILD_NEST OR ${PROJECT_PREFIX}_BENCHMARK)
    add_subdirectory(owlnest)
endif ()
