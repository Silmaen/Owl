/**
 * @file imgui_text_edit.h
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Thin wrapper around the DepManager-provided `imgui_color_text_edit` package
 * (see `depmanager.yml`). Only suppresses the strict warnings raised when
 * including the third-party header; contains no vendored code.
 */

#pragma once

#include <core/Macros.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_CLANG("-Wshadow-field-in-constructor")
OWL_DIAG_DISABLE_CLANG("-Wdocumentation-unknown-command")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
OWL_DIAG_DISABLE_CLANG("-Wimplicit-int-conversion")
OWL_DIAG_DISABLE_CLANG("-Wshorten-64-to-32")
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
OWL_DIAG_DISABLE_CLANG("-Wdocumentation")
OWL_DIAG_DISABLE_CLANG("-Wdouble-promotion")
#include <TextEditor.h>
OWL_DIAG_POP
