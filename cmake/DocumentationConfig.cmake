#
# ---=== documentation ===---
#
find_package(Doxygen
        REQUIRED dot)
if (DOXYGEN_FOUND)
    message(STATUS "found doxygen version: ${DOXYGEN_VERSION}")
    if (${DOXYGEN_VERSION} VERSION_LESS 1.9.1)
        message(FATAL_ERROR "Doxygen version 1.9.1 or higher is required")
    endif ()
    configure_file(${CMAKE_SOURCE_DIR}/DoxyfileTemplate ${CMAKE_SOURCE_DIR}/Doxyfile)
    add_custom_target(documentation)
    add_custom_command(
            TARGET documentation PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Copying Doxyfile to binary dir")
    add_custom_command(TARGET documentation POST_BUILD
            COMMAND ${DOXYGEN_EXECUTABLE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation"
            VERBATIM)
    # Copy doc images so that Markdown relative paths (../images/file.svg) resolve
    # from the generated html/ directory.
    add_custom_command(TARGET documentation POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/doc/images
            ${CMAKE_CURRENT_BINARY_DIR}/Documentation/images
            COMMENT "Copying documentation images")
    add_custom_command(TARGET documentation POST_BUILD
            COMMAND echo "look at the result: file:///${CMAKE_CURRENT_BINARY_DIR}/Documentation/html/index.html")
else (DOXYGEN_FOUND)
    message(ERROR no doxygen found)
endif (DOXYGEN_FOUND)
