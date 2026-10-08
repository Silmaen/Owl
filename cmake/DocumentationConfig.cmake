#
# ---=== documentation ===---
#
# The `documentation` target exists whenever Doxygen (with dot) is found; OWL_ENABLE_DOCUMENTATION makes it
# required. A configure never needs Doxygen otherwise (the OwlEngine Conan package, a contributor's machine).
if (${PROJECT_PREFIX}_ENABLE_DOCUMENTATION)
    find_package(Doxygen 1.9.1 REQUIRED COMPONENTS dot)
else ()
    find_package(Doxygen 1.9.1 QUIET COMPONENTS dot)
endif ()
if (NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found: no documentation target.")
    return()
endif ()
message(STATUS "found doxygen version: ${DOXYGEN_VERSION}")
# Generated in the build tree: two presets configured in parallel no longer write the same file.
configure_file(${CMAKE_SOURCE_DIR}/DoxyfileTemplate ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
add_custom_target(documentation
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation"
        VERBATIM)
# Copy doc images so that Markdown relative paths (../images/file.svg) resolve
# from the generated html/ directory.
add_custom_command(TARGET documentation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/doc/images
        ${CMAKE_BINARY_DIR}/Documentation/images
        COMMENT "Copying documentation images")
add_custom_command(TARGET documentation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "look at the result: file:///${CMAKE_BINARY_DIR}/Documentation/html/index.html")
