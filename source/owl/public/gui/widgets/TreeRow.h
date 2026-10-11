/**
 * @file TreeRow.h
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
 *  What a tree row shows.
 */
struct TreeRowSpec {
	/// Name of the row.
	const char* label = "";
	/// Lucide glyph drawn before the label (`gui::icon::*`), or null.
	const char* glyph = nullptr;
	/// Colour of the glyph; a zero alpha means the muted text colour.
	math::vec4 glyphColor{0.f, 0.f, 0.f, 0.f};
	/// IconBank icon drawn before the label when no glyph is given, or null.
	const char* iconName = nullptr;
	/// Muted text after the label (a child count), or null.
	const char* trailing = nullptr;
	/// Colour of the label; a zero alpha means the text colour.
	math::vec4 labelColor{0.f, 0.f, 0.f, 0.f};
	/// Highlighted as the selection: faint accent and an accent bar on the left.
	bool selected = false;
	/// The row can be expanded (a chevron is drawn).
	bool hasChildren = false;
	/// Expanded the first time it is shown.
	bool defaultOpen = false;
};

/**
 * @brief
 *  What the user did with a tree row this frame.
 */
struct TreeRowResult {
	/// The row has children and is expanded: draw them, then call `endTreeRow`.
	bool open = false;
	/// The row was clicked outside its chevron.
	bool clicked = false;
	/// The mouse is over the row, or over a button drawn on it.
	bool hovered = false;
	/// Top-left corner of the row, in screen coordinates.
	math::vec2 min{0.f, 0.f};
	/// Bottom-right corner of the row, in screen coordinates.
	math::vec2 max{0.f, 0.f};
};

/**
 * @brief
 *  A 24-unit row of a tree (Hierarchy, folders): a thin chevron, an icon, the label and a muted count; hover one
 *  surface step lighter, selection in the faint accent with a bar on the left. The row is the last ImGui item, so
 *  drag and drop and context menus attach to it as to a tree node.
 * @param[in] iId Unique ImGui identifier of the row.
 * @param[in] iSpec What to show.
 * @return The open state and the clicks of this frame.
 */
OWL_API auto treeRow(const char* iId, const TreeRowSpec& iSpec) -> TreeRowResult;

/**
 * @brief
 *  Close a row whose `treeRow` returned `open`.
 */
OWL_API void endTreeRow();

}// namespace owl::gui::widgets
