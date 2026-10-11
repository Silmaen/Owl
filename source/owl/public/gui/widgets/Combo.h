/**
 * @file Combo.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <span>

namespace owl::gui::widgets {

/**
 * @brief
 *  `ImGui::BeginCombo` with a thin chevron at its right end instead of the filled arrow button.
 * @param[in] iLabel Label, also the ImGui identifier; the part after `##` is hidden.
 * @param[in] iPreview Text shown in the closed combo.
 * @param[in] iFlags `ImGuiComboFlags` (the arrow button is always removed).
 * @return True while the list is open: fill it, then call `ImGui::EndCombo`.
 */
OWL_API auto beginCombo(const char* iLabel, const char* iPreview, int iFlags = 0) -> bool;

/**
 * @brief
 *  `ImGui::Combo` over an array of names, with the chevron of `beginCombo`.
 * @param[in] iLabel Label, also the ImGui identifier.
 * @param[in,out] ioCurrent Index of the selected name.
 * @param[in] iItems The names.
 * @return True when the selection changed.
 */
OWL_API auto combo(const char* iLabel, int* ioCurrent, std::span<const char* const> iItems) -> bool;

}// namespace owl::gui::widgets
