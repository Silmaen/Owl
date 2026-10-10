#
#
#
add_subdirectory(owl)
# Build-time shader compilation needs Slang, part of the render module.
if (${PROJECT_PREFIX}_MODULE_RENDER)
    add_subdirectory(tools)
endif ()
if (${PROJECT_PREFIX}_BUILD_NEST)
    add_subdirectory(owlnest)
endif ()
