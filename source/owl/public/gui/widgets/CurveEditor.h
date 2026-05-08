/**
 * @file CurveEditor.h
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/Curve.h"

#include <imgui.h>

namespace owl::gui::widgets {
/**
 * @brief
 *  Inspector widget that displays and edits a `math::Curve` via ImCurveEdit.
 *
 * Renders a 2D editing canvas (drag points, double-click to add, right-click to remove)
 * followed by a one-line interpolation-mode combo (Constant / Linear / Smooth).
 *
 * @param[in] iLabel Stable id used to scope ImGui state.
 * @param[in,out] ioCurve Curve to edit; values flow back into the curve in place.
 * @param[in] iSize Pixel size of the editing canvas. Default `{0, 160}` makes the canvas span
 *                  the available content width with a 160-pixel-tall canvas.
 * @return True when the curve was modified during this call.
 */
OWL_API auto curveEditor(const char* iLabel, math::Curve& ioCurve, ImVec2 iSize = {0.f, 160.f}) -> bool;

}// namespace owl::gui::widgets
