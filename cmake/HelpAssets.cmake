# =============================================================================
# HelpAssets.cmake
# Author: Silmaen
# Date: 28/04/2026
# Copyright (c) 2026 All rights reserved.
#
# Bundles the pages of the Owl Nest help into `<build>/help_bundle/` (installed as `assets/help/`).
#
# `doc/help.yml` lists the pages, grouped by theme, in sidebar order; every `doc/pages/*.md` must be listed there
# (the design pages of `doc/pages/design/` never are: they are plans, read in the Doxygen documentation). The
# configure copies them flat, writes `index.yml` (id, title, group, path of each page, in that order) and makes them
# plain Markdown for the md4c-based `HelpPanel`:
#  * Doxygen syntax goes (`[TOC]`, the `<!-- subpage: x.md -->` markers), but the heading anchors (`{#id}`): the
#    renderer reads them as the link targets of their headings;
#  * image references point at `images/`: `../images/foo.svg` (doc/pages) and `doc/images/foo.png` (README) keep
#    their sub-folder of `doc/images/`, `engine_assets/<dir>/foo.png` (the README logo) becomes `images/foo.png`;
#  * mermaid fences become `images/mermaid/<sha>.svg` drawn by `tools/doc/mermaid_svg.py`;
#  * `tools/doc/help_bundle.py` turns the links to pages outside the bundle into text and the status badges into
#    `badge:` images the renderer draws as coloured tags, so the help shows no dead link and fetches nothing.
# External links (`[text](https://...)`) are kept: the renderer opens them in the default browser.
# =============================================================================

function(_owl_help_scrub_markdown IN_FILE OUT_FILE)
    file(READ "${IN_FILE}" CONTENT)
    # Drop bare "[TOC]" lines.
    string(REGEX REPLACE "(^|\r?\n)[ \t]*\\[TOC\\][ \t]*(\r?\n|$)" "\\1" CONTENT "${CONTENT}")
    # Rewrite "../images/foo.svg" (doc/pages convention) -> "images/foo.svg".
    string(REGEX REPLACE "\\]\\(\\.\\./images/" "](images/" CONTENT "${CONTENT}")
    # Rewrite "doc/images/foo.png" (README convention) -> "images/foo.png".
    string(REGEX REPLACE "\\]\\(doc/images/" "](images/" CONTENT "${CONTENT}")
    # Project-relative "engine_assets/<x>/foo.svg" (the README logo) -> "images/foo.svg".
    string(REGEX REPLACE "\\]\\(engine_assets/[^/)]+/([^)]+)\\)" "](images/\\1)" CONTENT "${CONTENT}")
    # Same rewrites for raw `<img src="...">` tags (a fixed width, as the README logo).
    string(REGEX REPLACE "src=\"\\.\\./images/" "src=\"images/" CONTENT "${CONTENT}")
    string(REGEX REPLACE "src=\"doc/images/" "src=\"images/" CONTENT "${CONTENT}")
    string(REGEX REPLACE "src=\"engine_assets/[^/\"]+/([^\"]+)\"" "src=\"images/\\1\"" CONTENT "${CONTENT}")
    # Drop the `<!-- subpage: x.md -->` markers: they only give Doxygen its page tree.
    string(REGEX REPLACE "<!-- subpage: [A-Za-z0-9_./-]+ -->[ \t]*\r?\n?" "" CONTENT "${CONTENT}")
    file(WRITE "${OUT_FILE}" "${CONTENT}")
endfunction()

function(_owl_help_extract_title IN_FILE OUT_VAR)
    file(STRINGS "${IN_FILE}" FIRST_LINE LIMIT_COUNT 1)
    string(REGEX REPLACE "^# " "" TITLE "${FIRST_LINE}")
    string(REGEX REPLACE " *\\{#[^}]*\\} *$" "" TITLE "${TITLE}")
    set(${OUT_VAR} "${TITLE}" PARENT_SCOPE)
endfunction()

# Read `doc/help.yml`: OUT_PAGES gets the absolute page paths in order, OUT_GROUPS the group of each page.
function(_owl_help_read_toc TOC_FILE OUT_PAGES OUT_GROUPS)
    file(STRINGS "${TOC_FILE}" TOC_LINES)
    set(PAGES "")
    set(GROUPS "")
    set(GROUP "")
    foreach (LINE IN LISTS TOC_LINES)
        if (LINE MATCHES "^  - title: (.+)$")
            set(GROUP "${CMAKE_MATCH_1}")
        elseif (LINE MATCHES "^      - ([A-Za-z0-9_./-]+\\.md)$")
            set(PAGE "${CMAKE_SOURCE_DIR}/${CMAKE_MATCH_1}")
            if (GROUP STREQUAL "")
                message(FATAL_ERROR "Owl: ${TOC_FILE}: page ${CMAKE_MATCH_1} outside a group.")
            endif ()
            if (NOT EXISTS "${PAGE}")
                message(FATAL_ERROR "Owl: ${TOC_FILE}: no page ${CMAKE_MATCH_1}.")
            endif ()
            list(APPEND PAGES "${PAGE}")
            list(APPEND GROUPS "${GROUP}")
        endif ()
    endforeach ()
    set(${OUT_PAGES} "${PAGES}" PARENT_SCOPE)
    set(${OUT_GROUPS} "${GROUPS}" PARENT_SCOPE)
endfunction()

function(owl_bundle_help_assets)
    # In the build tree: the configure writes nothing in the sources. Not `help/`: the Ninja generator
    # declares a `help` target at the build root, and `clean` fails on a directory of that name.
    set(HELP_DIR "${CMAKE_BINARY_DIR}/help_bundle")
    if (IS_DIRECTORY "${CMAKE_BINARY_DIR}/help")
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/help")
    endif ()
    # A bundle generated in the sources (`engine_assets/help`, ignored by git) is an asset directory of every
    # development run: drop it so no stale copy of the pages can be read.
    if (EXISTS "${CMAKE_SOURCE_DIR}/engine_assets/help/index.yml")
        file(REMOVE_RECURSE "${CMAKE_SOURCE_DIR}/engine_assets/help")
        message(STATUS "Owl: removed the stale help bundle engine_assets/help.")
    endif ()
    set(${PROJECT_PREFIX}_HELP_DIR "${HELP_DIR}" PARENT_SCOPE)
    set(IMG_OUT_DIR "${HELP_DIR}/images")
    file(REMOVE_RECURSE "${IMG_OUT_DIR}/badges")
    file(MAKE_DIRECTORY "${IMG_OUT_DIR}")

    # ---- Source files ----
    set(OWL_HELP_TOC "${CMAKE_SOURCE_DIR}/doc/help.yml")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OWL_HELP_TOC}")
    _owl_help_read_toc("${OWL_HELP_TOC}" OWL_HELP_PAGES OWL_HELP_GROUPS)
    # Pages are flattened by basename and the help panel matches ids case-insensitively.
    set(_OWL_HELP_SEEN "")
    foreach (HELP_FILE IN LISTS OWL_HELP_PAGES)
        get_filename_component(_NAME "${HELP_FILE}" NAME)
        string(TOLOWER "${_NAME}" _KEY)
        if ("${_KEY}" IN_LIST _OWL_HELP_SEEN)
            message(FATAL_ERROR "Owl: help pages clash on '${_NAME}' (case-insensitive): ${HELP_FILE}")
        endif ()
        list(APPEND _OWL_HELP_SEEN "${_KEY}")
    endforeach ()
    # Every user page has its place in the help: a new page goes into `doc/help.yml`.
    file(GLOB _OWL_DOC_PAGES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/doc/pages/*.md")
    foreach (DOC_PAGE IN LISTS _OWL_DOC_PAGES)
        if (NOT "${DOC_PAGE}" IN_LIST OWL_HELP_PAGES)
            file(RELATIVE_PATH _REL "${CMAKE_SOURCE_DIR}" "${DOC_PAGE}")
            message(FATAL_ERROR "Owl: ${_REL} is not listed in doc/help.yml.")
        endif ()
    endforeach ()
    # Sub-folders (`nest/` screenshots, `showcase/`) keep their place under `images/`.
    file(GLOB_RECURSE OWL_HELP_IMAGES CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}/doc/images"
            "${CMAKE_SOURCE_DIR}/doc/images/*")
    # Project-relative images referenced from the README (the logo).
    file(GLOB OWL_HELP_LOGO_IMAGES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/engine_assets/logo/*")

    # ---- Copy + scrub markdown ----
    # Start from a clean page set so a renamed or dropped page does not linger in the bundle.
    file(GLOB _OWL_HELP_STALE "${HELP_DIR}/*.md")
    if (_OWL_HELP_STALE)
        file(REMOVE ${_OWL_HELP_STALE})
    endif ()
    foreach (HELP_FILE IN LISTS OWL_HELP_PAGES)
        get_filename_component(FULLNAME "${HELP_FILE}" NAME)
        _owl_help_scrub_markdown("${HELP_FILE}" "${HELP_DIR}/${FULLNAME}")
    endforeach ()

    # ---- Python passes: mermaid fences -> SVG files, then links and badges ----
    # The help panel has no JavaScript: every ```mermaid fence becomes an image reference to
    # `images/mermaid/<sha>.svg`, drawn by the repository's own renderer (plain SVG for lunasvg). A diagram the
    # renderer cannot draw fails the configure.
    foreach (_SCRIPT mermaid_svg help_bundle)
        set(_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/tools/doc/${_SCRIPT}.py")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_SCRIPT_PATH}")
        if (_SCRIPT STREQUAL "mermaid_svg")
            set(_ARGS bundle "${HELP_DIR}")
        else ()
            set(_ARGS "${HELP_DIR}")
        endif ()
        execute_process(
                COMMAND ${Python3_EXECUTABLE} "${_SCRIPT_PATH}" ${_ARGS}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE _RESULT
                OUTPUT_VARIABLE _OUTPUT
                ERROR_VARIABLE _ERROR
        )
        if (NOT _RESULT EQUAL 0)
            message(FATAL_ERROR "Owl: ${_SCRIPT}.py failed on the help pages:\n${_ERROR}")
        endif ()
    endforeach ()

    # ---- Copy images verbatim (SVG / PNG / JPG …) ----
    foreach (IMG IN LISTS OWL_HELP_IMAGES)
        get_filename_component(IMG_DIR "${IMG}" DIRECTORY)
        file(COPY "${CMAKE_SOURCE_DIR}/doc/images/${IMG}" DESTINATION "${IMG_OUT_DIR}/${IMG_DIR}")
    endforeach ()
    foreach (IMG IN LISTS OWL_HELP_LOGO_IMAGES)
        if (NOT IS_DIRECTORY "${IMG}")
            file(COPY "${IMG}" DESTINATION "${IMG_OUT_DIR}")
        endif ()
    endforeach ()

    # ---- Index ----
    # Each entry, in sidebar order: id (basename without extension), title (first H1 line, scrubbed), group (from
    # `doc/help.yml`), path (file basename relative to the help root).
    set(INDEX_CONTENT "Help:\n  Version: 2\n  Pages:\n")
    list(LENGTH OWL_HELP_PAGES _COUNT)
    math(EXPR _LAST "${_COUNT} - 1")
    foreach (INDEX RANGE ${_LAST})
        list(GET OWL_HELP_PAGES ${INDEX} HELP_FILE)
        list(GET OWL_HELP_GROUPS ${INDEX} GROUP)
        get_filename_component(BASENAME "${HELP_FILE}" NAME_WE)
        get_filename_component(FULLNAME "${HELP_FILE}" NAME)
        _owl_help_extract_title("${HELP_DIR}/${FULLNAME}" TITLE)
        if (TITLE STREQUAL "")
            set(TITLE "${BASENAME}")
        endif ()
        # Escape backslash and double-quote for safe YAML embedding.
        string(REPLACE "\\" "\\\\" TITLE_ESC "${TITLE}")
        string(REPLACE "\"" "\\\"" TITLE_ESC "${TITLE_ESC}")
        string(APPEND INDEX_CONTENT "    - id: \"${BASENAME}\"\n")
        string(APPEND INDEX_CONTENT "      title: \"${TITLE_ESC}\"\n")
        string(APPEND INDEX_CONTENT "      group: \"${GROUP}\"\n")
        string(APPEND INDEX_CONTENT "      path: \"${FULLNAME}\"\n")
    endforeach ()
    file(WRITE "${HELP_DIR}/index.yml" "${INDEX_CONTENT}")

    message(STATUS "Owl: bundled ${_COUNT} help pages → ${HELP_DIR}")
endfunction()
