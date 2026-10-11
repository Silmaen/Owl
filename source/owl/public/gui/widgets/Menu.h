/**
 * @file Menu.h
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
 *  What a context-menu entry shows.
 */
struct MenuItemSpec {
	/// `gui::icon` glyph in the icon column, or null for an empty column.
	const char* glyph = nullptr;
	/// Label, also the ImGui identifier of the entry.
	const char* label = "";
	/// Shortcut shown muted on the right, or null.
	const char* shortcut = nullptr;
	/// Destructive action (delete, remove): drawn in the error colour.
	bool danger = false;
	/// Whether the entry can be clicked.
	bool enabled = true;
};

/**
 * @brief
 *  Entry of a popup or context menu: a muted glyph in a fixed icon column, so labels align whether they have an
 *  icon or not, the label, and its shortcut on the right.
 * @param[in] iSpec What to show.
 * @return True when the entry was clicked.
 */
OWL_API auto menuItem(const MenuItemSpec& iSpec) -> bool;

/**
 * @brief
 *  Title of a context menu: the name of what was clicked in the strong style, an optional muted kind after it, and
 *  a separator below.
 * @param[in] iTitle Name of the item.
 * @param[in] iKind Kind of the item (`Entity`, `Texture`...), or null.
 */
OWL_API void menuHeader(const char* iTitle, const char* iKind = nullptr);

}// namespace owl::gui::widgets
