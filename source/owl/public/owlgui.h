/**
 * @file owlgui.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

/**
 * @brief
 *  Everything of `<owl.h>` plus the engine headers that include imgui.
 *
 * Link `Owl::Gui` (`find_package(OwlEngine COMPONENTS Gui)`) to use it: `Owl::OwlEngine` alone does not provide
 * imgui.
 */
#include "owl.h"

#include "gui/utils.h"
#include "gui/widgets/AssetField.h"
#include "gui/widgets/CurveEditor.h"
