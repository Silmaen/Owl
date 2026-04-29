# =============================================================================
# HelpAssets.cmake
# Author: Silmaen
# Date: 28/04/2026
# Copyright (c) 2026 All rights reserved.
#
# Bundles the Markdown documentation pages from `doc/pages/` plus the canonical
# repository-root files (README, CHANGELOG, CONTRIBUTING) into
# `engine_assets/help/`, generates an `index.yml` describing each page, and
# scrubs Doxygen-specific syntax (`{#page-anchor}`, `[TOC]`) so the bundled
# pages render cleanly in the in-editor `HelpPanel` (md4c-based renderer).
#
# Image references are normalised to point at `engine_assets/help/images/`:
#  * `../images/foo.svg` (Doxygen INPUT convention) -> `images/foo.svg`
#  * `engine_assets/<dir>/foo.png` (project-relative paths used in the README,
#    such as `engine_assets/logo/logo_owl.png`) -> `images/foo.png`
#  * `https://...` images are downloaded once at configure time, cached under
#    `images/badges/<sha1>.svg`, and the reference is rewritten to the local
#    path. shields.io URLs all return SVG so the renderer picks them up via
#    `lunasvg` like any other local SVG image.
#
# External text links (`[text](https://...)`) are PRESERVED — the renderer
# routes them to the user's default browser at click time (see
# `core::utils::openExternalUrl`).
# =============================================================================

# Download an HTTPS image into the local cache and return its bundled path
# (relative to the help root). Reuses the on-disk cache when available.
# Empty OUT_LOCAL when the download fails (caller leaves the URL untouched).
function(_owl_help_fetch_image IMG_URL CACHE_DIR OUT_LOCAL)
    string(SHA1 HASH "${IMG_URL}")
    # All shields.io and other badge providers return SVG (lunasvg handles them).
    set(LOCAL_NAME "${HASH}.svg")
    set(LOCAL_PATH "${CACHE_DIR}/${LOCAL_NAME}")
    if (NOT EXISTS "${LOCAL_PATH}")
        file(DOWNLOAD "${IMG_URL}" "${LOCAL_PATH}"
                STATUS DL_STATUS
                TIMEOUT 15
                INACTIVITY_TIMEOUT 10
                TLS_VERIFY ON)
        list(GET DL_STATUS 0 DL_CODE)
        if (NOT DL_CODE EQUAL 0)
            file(REMOVE "${LOCAL_PATH}")
            message(STATUS "Owl: skipped help badge fetch (${DL_CODE}): ${IMG_URL}")
            set(${OUT_LOCAL} "" PARENT_SCOPE)
            return()
        endif ()
    endif ()
    set(${OUT_LOCAL} "images/badges/${LOCAL_NAME}" PARENT_SCOPE)
endfunction()

function(_owl_help_scrub_markdown IN_FILE OUT_FILE BADGE_CACHE_DIR)
    file(READ "${IN_FILE}" CONTENT)
    # Strip ATX-heading Doxygen anchors:  "# Title {#page-foo}" -> "# Title"
    string(REGEX REPLACE "([ \t]*\\{#[A-Za-z0-9_-]+\\})+[ \t]*(\r?\n|$)" "\\2" CONTENT "${CONTENT}")
    # Drop bare "[TOC]" lines.
    string(REGEX REPLACE "(^|\r?\n)[ \t]*\\[TOC\\][ \t]*(\r?\n|$)" "\\1" CONTENT "${CONTENT}")
    # Rewrite "../images/foo.svg" (doc/pages convention) -> "images/foo.svg".
    string(REGEX REPLACE "\\]\\(\\.\\./images/" "](images/" CONTENT "${CONTENT}")
    # Rewrite project-relative "engine_assets/<x>/foo.svg" (used by README for the logo) ->
    # "images/foo.svg" so the file lives next to the rest of the bundled images.
    string(REGEX REPLACE "\\]\\(engine_assets/[^/)]+/([^)]+)\\)" "](images/\\1)" CONTENT "${CONTENT}")
    # Resolve HTTPS image URLs by downloading them into the local cache and rewriting refs.
    string(REGEX MATCHALL "!\\[[^]]*\\]\\(https?://[^) ]+\\)" REMOTE_IMAGES "${CONTENT}")
    foreach (RAW IN LISTS REMOTE_IMAGES)
        # Extract the URL between the parentheses.
        string(REGEX MATCH "\\((https?://[^) ]+)\\)" _MATCH "${RAW}")
        set(IMG_URL "${CMAKE_MATCH_1}")
        if (IMG_URL STREQUAL "")
            continue()
        endif ()
        _owl_help_fetch_image("${IMG_URL}" "${BADGE_CACHE_DIR}" LOCAL_REF)
        if (LOCAL_REF STREQUAL "")
            continue()
        endif ()
        # Replace every occurrence of the URL by the local path. We replace the URL
        # rather than the whole pattern so the alt text and surrounding markdown stay intact.
        string(REPLACE "${IMG_URL}" "${LOCAL_REF}" CONTENT "${CONTENT}")
    endforeach ()
    file(WRITE "${OUT_FILE}" "${CONTENT}")
endfunction()

function(_owl_help_extract_title IN_FILE OUT_VAR)
    file(STRINGS "${IN_FILE}" FIRST_LINE LIMIT_COUNT 1)
    string(REGEX REPLACE "^# " "" TITLE "${FIRST_LINE}")
    string(REGEX REPLACE " *\\{#[^}]*\\} *$" "" TITLE "${TITLE}")
    set(${OUT_VAR} "${TITLE}" PARENT_SCOPE)
endfunction()

function(owl_bundle_help_assets)
    set(HELP_DIR "${CMAKE_SOURCE_DIR}/engine_assets/help")
    set(IMG_OUT_DIR "${HELP_DIR}/images")
    set(BADGE_CACHE_DIR "${IMG_OUT_DIR}/badges")
    file(MAKE_DIRECTORY "${HELP_DIR}")
    file(MAKE_DIRECTORY "${IMG_OUT_DIR}")
    file(MAKE_DIRECTORY "${BADGE_CACHE_DIR}")

    # ---- Source files ----
    file(GLOB OWL_HELP_PAGES "${CMAKE_SOURCE_DIR}/doc/pages/*.md")
    set(OWL_HELP_ROOT_FILES
            "${CMAKE_SOURCE_DIR}/README.md"
            "${CMAKE_SOURCE_DIR}/CHANGELOG.md"
            "${CMAKE_SOURCE_DIR}/CONTRIBUTING.md"
    )
    file(GLOB OWL_HELP_IMAGES "${CMAKE_SOURCE_DIR}/doc/images/*")
    # Project-relative images referenced from the README (logo, etc.) — copy them into
    # `images/` alongside `doc/images/` so the rewritten markdown can find them.
    file(GLOB OWL_HELP_LOGO_IMAGES "${CMAKE_SOURCE_DIR}/engine_assets/logo/*")

    # ---- Copy + scrub markdown ----
    foreach (HELP_FILE IN LISTS OWL_HELP_PAGES OWL_HELP_ROOT_FILES)
        if (NOT EXISTS "${HELP_FILE}")
            continue()
        endif ()
        get_filename_component(FULLNAME "${HELP_FILE}" NAME)
        _owl_help_scrub_markdown("${HELP_FILE}" "${HELP_DIR}/${FULLNAME}" "${BADGE_CACHE_DIR}")
    endforeach ()

    # ---- Copy images verbatim (SVG / PNG / JPG …) ----
    foreach (IMG IN LISTS OWL_HELP_IMAGES OWL_HELP_LOGO_IMAGES)
        if (NOT IS_DIRECTORY "${IMG}")
            file(COPY "${IMG}" DESTINATION "${IMG_OUT_DIR}")
        endif ()
    endforeach ()

    # ---- Index ----
    # Each entry: id (basename without extension), title (first H1 line, scrubbed),
    # category (parent directory: "pages" → "guides", root → "reference"),
    # path (file basename relative to engine_assets/help/).
    set(INDEX_CONTENT "Help:\n  Version: 1\n  Pages:\n")
    foreach (HELP_FILE IN LISTS OWL_HELP_PAGES OWL_HELP_ROOT_FILES)
        if (NOT EXISTS "${HELP_FILE}")
            continue()
        endif ()
        get_filename_component(BASENAME "${HELP_FILE}" NAME_WE)
        get_filename_component(FULLNAME "${HELP_FILE}" NAME)
        get_filename_component(PARENT_DIR "${HELP_FILE}" DIRECTORY)
        get_filename_component(PARENT_NAME "${PARENT_DIR}" NAME)
        if (PARENT_NAME STREQUAL "pages")
            set(CATEGORY "guides")
        else ()
            set(CATEGORY "reference")
        endif ()
        # Title from the (scrubbed) first line of the bundled file so we do not pick up
        # Doxygen anchors that the source still carries. Falls back to the basename when the
        # extractor couldn't find a `# heading` (file with no H1).
        _owl_help_extract_title("${HELP_DIR}/${FULLNAME}" TITLE)
        if (TITLE STREQUAL "")
            set(TITLE "${BASENAME}")
        endif ()
        # Escape backslash and double-quote for safe YAML embedding.
        string(REPLACE "\\" "\\\\" TITLE_ESC "${TITLE}")
        string(REPLACE "\"" "\\\"" TITLE_ESC "${TITLE_ESC}")
        string(APPEND INDEX_CONTENT "    - id: \"${BASENAME}\"\n")
        string(APPEND INDEX_CONTENT "      title: \"${TITLE_ESC}\"\n")
        string(APPEND INDEX_CONTENT "      category: \"${CATEGORY}\"\n")
        string(APPEND INDEX_CONTENT "      path: \"${FULLNAME}\"\n")
    endforeach ()
    file(WRITE "${HELP_DIR}/index.yml" "${INDEX_CONTENT}")

    message(STATUS "Owl: bundled help pages → ${HELP_DIR}")
endfunction()
