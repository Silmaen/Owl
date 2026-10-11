/**
 * @file Toggle.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::gui::widgets {

/**
 * @brief
 *  Left / right switch for an on / off property: a rounded track in the accent when on, its knob on the right.
 *  Sized from the font, centred in the height of a field; the label after `##` is hidden.
 * @param[in] iId Unique ImGui identifier.
 * @param[in,out] ioValue The value, flipped on click.
 * @return True when the value changed.
 */
OWL_API auto toggle(const char* iId, bool* ioValue) -> bool;

/**
 * @brief
 *  Check box the size of a capital letter, followed by its label: a drop-in replacement of `ImGui::Checkbox`, whose
 *  square is as tall as a whole field.
 * @param[in] iLabel The label, also the ImGui identifier; the part after `##` is hidden.
 * @param[in,out] ioValue The value, flipped on click.
 * @return True when the value changed.
 */
OWL_API auto checkbox(const char* iLabel, bool* ioValue) -> bool;

}// namespace owl::gui::widgets
