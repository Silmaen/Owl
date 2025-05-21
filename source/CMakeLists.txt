#
#
#
add_subdirectory(owl)
if (${PROJECT_PREFIX}_BUILD_NEST)
    add_subdirectory(owlnest)
endif ()
if (${PROJECT_PREFIX}_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif ()
if (${PROJECT_PREFIX}_BUILD_DRONE)
    add_subdirectory(owldrone)
endif ()
if (${PROJECT_PREFIX}_BUILD_CAST)
    add_subdirectory(owlcast)
endif ()
