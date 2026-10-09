/**
 * @file ryml.h
 * @author Silmaen
 * @date 09/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Macros.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wcast-qual")
OWL_DIAG_DISABLE_CLANG("-Wdeprecated-declarations")
OWL_DIAG_DISABLE_GCC("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wold-style-cast")
OWL_DIAG_DISABLE_GCC("-Wcast-qual")
OWL_DIAG_DISABLE_GCC("-Wdeprecated-declarations")
#include <ryml.hpp>
#include <ryml_std.hpp>
OWL_DIAG_POP
