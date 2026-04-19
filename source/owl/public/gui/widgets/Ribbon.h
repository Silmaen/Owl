/**
 * @file Ribbon.h
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <functional>
#include <string>
#include <vector>

namespace owl::gui::widgets {

/**
 * @brief Microsoft Office-style ribbon widget.
 *
 * The ribbon is organised as **tabs → groups → buttons**. Two button sizes are
 * supported:
 *
 * - **Large**: a 32 px icon with the label below it (primary actions).
 * - **Small**: a 16 px icon with the label to its right (secondary actions).
 *   Small buttons stack vertically inside a column, up to three per column, so
 *   three small buttons occupy roughly the same height as one large button plus
 *   its caption.
 *
 * Buttons are entirely driven by callbacks: `isEnabled`, `isChecked`
 * (for toggle-style buttons such as gizmo mode), and `onClick`.
 */
class OWL_API Ribbon final {
public:
	enum struct ButtonSize : uint8_t { Large, Small };

	/// @brief A single ribbon button.
	struct Button {
		/// Name of the icon to look up in the `IconBank`.
		std::string iconName;
		/// Short text displayed below (large) or to the right (small) of the icon.
		std::string label;
		/// Tooltip shown on hover. Typically includes the keyboard shortcut.
		std::string tooltip;
		/// Returns true when the button should accept clicks.
		std::function<bool()> isEnabled{[]() -> bool { return true; }};
		/// Returns true when the button is in a toggled-on state (highlighted).
		std::function<bool()> isChecked{[]() -> bool { return false; }};
		/// Invoked on click (only when enabled).
		std::function<void()> onClick{[]() -> void {}};
		/// Button size (large icon-over-label or small icon-then-label).
		ButtonSize size = ButtonSize::Small;
	};

	/// @brief A named group of buttons inside a ribbon tab.
	struct Group {
		std::string label;
		std::vector<Button> buttons;
	};

	/// @brief A ribbon tab — contains several groups.
	struct Tab {
		std::string label;
		std::vector<Group> groups;
		/// When true, the tab title is drawn with the theme's accent (secondary) color to draw
		/// attention — used to highlight the File tab.
		bool highlighted = false;
	};

	Ribbon() = default;
	~Ribbon() = default;
	Ribbon(const Ribbon&) = delete;
	Ribbon(Ribbon&&) = delete;
	auto operator=(const Ribbon&) -> Ribbon& = delete;
	auto operator=(Ribbon&&) -> Ribbon& = delete;

	/// @brief Add an empty tab to the ribbon. Returns its index.
	auto addTab(std::string iLabel) -> size_t;
	/// @brief Mark `iTabIndex` as highlighted (drawn with the theme accent color in the tab bar).
	void setTabHighlighted(size_t iTabIndex, bool iHighlighted);
	/// @brief Add an empty group to `iTabIndex`. Returns the group index.
	auto addGroup(size_t iTabIndex, std::string iLabel) -> size_t;
	/// @brief Add a button to a group.
	void addButton(size_t iTabIndex, size_t iGroupIndex, Button iButton);

	/// @brief Clear every tab, group, and button.
	void clear();

	/// @brief Render the ribbon at the current ImGui cursor. Consumes a fixed height.
	void onRender();

	/// @brief Total pixel height reserved for the ribbon (tabs + group area + labels).
	[[nodiscard]] static auto height() -> float;

private:
	std::vector<Tab> m_tabs;
};

}// namespace owl::gui::widgets
