/**
 * @file lunasvg_wrapper.h
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Owlnest-side wrapper around the DepManager-provided `lunasvg` package.
 * The engine has its own `core/external/lunasvg.h` for private use; owlnest
 * needs a separate wrapper because it links lunasvg directly (the engine
 * does not propagate it).
 */

#pragma once

#include <core/Macros.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
OWL_DIAG_DISABLE_CLANG("-Wdouble-promotion")
#include <lunasvg.h>
OWL_DIAG_POP
