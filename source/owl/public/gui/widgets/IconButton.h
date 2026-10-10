/**
 * @file IconButton.h
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
 *  Appearance and help of an icon button.
 */
struct IconButtonSpec {
	/// Lucide glyph (`gui::icon::*`).
	const char* glyph = nullptr;
	/// Tooltip title; also the accessible name of the button.
	const char* tooltip = nullptr;
	/// Shortcut shown in the tooltip, or null.
	const char* shortcut = nullptr;
	/// One sentence shown under the tooltip title, or null.
	const char* description = nullptr;
	/// Drawn as toggled on (accent glyph on the faint accent).
	bool selected = false;
	/// Greyed out and not clickable.
	bool disabled = false;
};

/**
 * @brief
 *  Square, frameless button showing a Lucide glyph, one frame high, with a rich tooltip.
 *
 * The frame only appears on hover and press; a selected button keeps the faint accent background.
 * @param[in] iId Unique ImGui identifier.
 * @param[in] iSpec Glyph, help text and state.
 * @return True on click.
 */
OWL_API auto iconButton(const char* iId, const IconButtonSpec& iSpec) -> bool;

}// namespace owl::gui::widgets
