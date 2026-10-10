/**
 * @file SectionHeader.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

namespace owl::gui::widgets {

/**
 * @brief
 *  What the user did with a section header this frame.
 */
struct SectionHeaderResult {
	/// The section is expanded: draw its body, then call `endSection`.
	bool open = false;
	/// The menu button (vertical ellipsis) was clicked.
	bool menuClicked = false;
	/// The header row itself is hovered.
	bool hovered = false;
	/// Top-left corner of the header row, in screen coordinates.
	math::vec2 min{0.f, 0.f};
	/// Bottom-right corner of the header row, in screen coordinates.
	math::vec2 max{0.f, 0.f};
};

/**
 * @brief
 *  Collapsible header of an inspector section or component: chevron, icon, title in the section style, and a menu
 *  button at the right end. Neutral surface colour, so headers never read as a selection.
 * @param[in] iTitle Title, also used as the ImGui identifier of the section.
 * @param[in] iIconName IconBank icon drawn before the title, or null.
 * @param[in] iWithMenu Whether to draw the menu button.
 * @param[in] iDefaultOpen Expanded the first time it is shown.
 * @return The open state and the clicks of this frame.
 */
OWL_API auto sectionHeader(const char* iTitle, const char* iIconName = nullptr, bool iWithMenu = true,
						   bool iDefaultOpen = true) -> SectionHeaderResult;

/**
 * @brief
 *  Close a section whose header returned `open`.
 */
OWL_API void endSection();

}// namespace owl::gui::widgets
