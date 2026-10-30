/**
 * @file GradientEditor.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/Core.h"
#include "renderer/ParticleSystem.h"

namespace owl::gui::widgets {

/**
 * @brief
 *  Inspector widget that displays and edits a colour gradient.
 *
 * Draws the gradient as a bar with a marker under each stop: click a marker to select it, drag it along the bar to move
 * it, right-click it to remove it, double-click the bar to add a stop with the colour found there. The selected stop's
 * colour and position are edited below the bar; an empty gradient shows the colour it stands for (white).
 *
 * @param[in] iLabel Stable id used to scope ImGui state.
 * @param[in,out] ioGradient Gradient to edit.
 * @return True when the gradient was modified during this call.
 */
OWL_API auto gradientEditor(const char* iLabel, renderer::ColorGradient& ioGradient) -> bool;

}// namespace owl::gui::widgets
