/**
 * @file shaderc.h
 * @author Silmaen
 * @date 06/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Macros.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wswitch-enum")
OWL_DIAG_DISABLE_CLANG("-Wdouble-promotion")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
#include <shaderc/shaderc.hpp>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
OWL_DIAG_POP
