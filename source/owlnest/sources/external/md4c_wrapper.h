/**
 * @file md4c_wrapper.h
 * @author Silmaen
 * @date 29/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Thin wrapper around the DepManager-provided `md4c` package (see
 * `depmanager.yml`). Only suppresses the strict warnings raised when including
 * the third-party header; contains no vendored code.
 */

#pragma once

#include <core/Macros.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wdocumentation-unknown-command")
#include <md4c.h>
OWL_DIAG_POP
