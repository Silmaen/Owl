/**
 * @file glfw3.h
 * @author Silmaen
 * @date 12/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Macros.h"

#if !defined(OWL_PLATFORM_WEB)
// Before GLFW, which declares its Vulkan surface functions when it sees the Vulkan header.
#include <vulkan/vulkan.h>
#endif

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wdocumentation-unknown-command")
OWL_DIAG_DISABLE_CLANG("-Wdocumentation-deprecated-sync")
OWL_DIAG_DISABLE_CLANG("-Wdocumentation")
#include <GLFW/glfw3.h>
OWL_DIAG_POP
