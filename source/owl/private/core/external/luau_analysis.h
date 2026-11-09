/**
 * @file luau_analysis.h
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Macros.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Weverything")
OWL_DIAG_DISABLE_GCC("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wuseless-cast")
OWL_DIAG_DISABLE_GCC("-Wconversion")
OWL_DIAG_DISABLE_GCC("-Wsign-conversion")
OWL_DIAG_DISABLE_GCC("-Wpedantic")
OWL_DIAG_DISABLE_GCC("-Wold-style-cast")
OWL_DIAG_DISABLE_GCC("-Wnon-virtual-dtor")
OWL_DIAG_DISABLE_GCC("-Wdeprecated-declarations")
#include <Luau/Ast.h>
#include <Luau/BuiltinDefinitions.h>
#include <Luau/Config.h>
#include <Luau/ConfigResolver.h>
#include <Luau/Error.h>
#include <Luau/FileResolver.h>
#include <Luau/Frontend.h>
#include <Luau/Linter.h>
#include <Luau/Parser.h>
#include <Luau/Type.h>
#include <Luau/TypeArena.h>
OWL_DIAG_POP
