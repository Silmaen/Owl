/**
 * @file vma.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Macros.h"

#include <vulkan/vulkan.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
OWL_DIAG_DISABLE_CLANG("-Wimplicit-int-conversion")
OWL_DIAG_DISABLE_CLANG("-Wshorten-64-to-32")
OWL_DIAG_DISABLE_CLANG("-Wcast-align")
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wreserved-macro-identifier")
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
OWL_DIAG_DISABLE_CLANG("-Wundef")
OWL_DIAG_DISABLE_CLANG("-Wmissing-field-initializers")
OWL_DIAG_DISABLE_CLANG("-Wunused-parameter")
OWL_DIAG_DISABLE_CLANG("-Wunused-variable")
OWL_DIAG_DISABLE_CLANG("-Wnullability-completeness")
OWL_DIAG_DISABLE_CLANG("-Wnullability-extension")
OWL_DIAG_DISABLE_CLANG("-Wswitch-default")
OWL_DIAG_DISABLE_CLANG("-Wswitch-enum")
OWL_DIAG_DISABLE_CLANG("-Wextra-semi-stmt")
OWL_DIAG_DISABLE_CLANG("-Wcovered-switch-default")
OWL_DIAG_DISABLE_CLANG("-Wdouble-promotion")
OWL_DIAG_DISABLE_CLANG("-Wcomma")
OWL_DIAG_DISABLE_CLANG("-Wmissing-noreturn")
OWL_DIAG_DISABLE_CLANG("-Wunused-function")
OWL_DIAG_DISABLE_CLANG("-Wunused-member-function")
OWL_DIAG_DISABLE_CLANG("-Wunused-macros")
OWL_DIAG_DISABLE_CLANG("-Wunsafe-buffer-usage")
OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
OWL_DIAG_DISABLE_GCC("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wunused-parameter")
OWL_DIAG_DISABLE_GCC("-Wunused-variable")
OWL_DIAG_DISABLE_GCC("-Wmissing-field-initializers")
OWL_DIAG_DISABLE_GCC("-Wsign-conversion")
OWL_DIAG_DISABLE_GCC("-Wconversion")
OWL_DIAG_DISABLE_GCC("-Wuseless-cast")
OWL_DIAG_DISABLE_GCC("-Wduplicated-branches")
OWL_DIAG_DISABLE_GCC("-Wold-style-cast")
OWL_DIAG_DISABLE_GCC("-Wcast-align")
#include <vk_mem_alloc.h>
OWL_DIAG_POP
