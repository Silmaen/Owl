/**
 * @file tracy.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Macros.h"

// Only meaningful with the Tracy backend: elsewhere its headers are not even fetched.
#ifdef OWL_PROFILER_TRACY

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wreserved-macro-identifier")
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
OWL_DIAG_DISABLE_CLANG("-Wundef")
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
OWL_DIAG_POP

#endif
