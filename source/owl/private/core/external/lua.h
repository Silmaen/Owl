/**
 * @file lua.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Macros.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
OWL_DIAG_POP
