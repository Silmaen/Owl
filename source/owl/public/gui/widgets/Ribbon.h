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
 * @brief
 *  Microsoft Office-style ribbon widget.
 *
 * The ribbon is organized as **tabs → groups → buttons**. Two button sizes are
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
	/**
	 * @brief
	 *  Visual size of a ribbon button.
	 */
	enum struct ButtonSize : uint8_t {
		Large,///< 32 px icon with the label below it.
		Small,///< 16 px icon with the label to the right.
	};

	/**
	 * @brief
	 *  A single ribbon button.
	 */
	struct Button {
		/// Name of the icon to look up in the `IconBank`.
		std::string iconName;
		/// Short text displayed below (large) or to the right (small) of the icon.
		std::string label;
		/// Tooltip shown on hover. Typically, includes the keyboard shortcut.
		std::string tooltip;
		/// Returns true when the button should accept clicks.
		std::function<bool()> isEnabled{[]() -> bool { return true; }};
		/// Returns true when the button is in a toggled-on state (highlighted).
		std::function<bool()> isChecked{[]() -> bool { return false; }};
		/// Invoked on click (only when enabled). Ignored when `popupContents` is set — clicking opens the popup instead.
		std::function<void()> onClick{[]() -> void {}};
		/// Button size (large icon-over-label or small icon-then-label).
		ButtonSize size = ButtonSize::Small;
		/**
		 * @brief
		 *  Optional popup contents — when set, clicking the button opens a popup whose body is filled by this callback.
		 *
		 * The button automatically renders a small caret indicator on the right (small) or bottom (large) to hint at
		 * the dropdown affordance. Mutually exclusive with `onClick`: when `popupContents` is set, `onClick` is not
		 * invoked on press.
		 */
		// The explicit `{}` keeps clang `-Wmissing-designated-field-initializers` happy on the
		// many existing `Button{...}` designated-init call sites that legitimately omit this
		// trailing field — the redundant-member-init tidy hit is intentional here.
		std::function<void()> popupContents{};// NOLINT(readability-redundant-member-init)
	};

	/**
	 * @brief
	 *  A named group of buttons inside a ribbon tab.
	 */
	struct Group {
		/// Group label rendered below the buttons.
		std::string label;
		/// Buttons belonging to this group.
		std::vector<Button> buttons;
	};

	/**
	 * @brief
	 *  A ribbon tab — contains several groups.
	 */
	struct Tab {
		/// Tab label rendered in the tab bar.
		std::string label;
		/// Groups inside this tab.
		std::vector<Group> groups;
		/**
		 * @brief
		 *  Whether the tab title should be highlighted in the tab bar.
		 *
		 * When true, the tab title is drawn with the theme's accent (secondary) colour to draw
		 * attention — used to highlight the File tab.
		 */
		bool highlighted = false;
	};

	/**
	 * @brief
	 *  Ribbon.
	 */
	Ribbon() = default;

	~Ribbon() = default;

	/**
	 * @brief
	 *  Ribbon.
	 */
	Ribbon(const Ribbon&) = delete;

	/**
	 * @brief
	 *  Ribbon.
	 */
	Ribbon(Ribbon&&) = delete;

	auto operator=(const Ribbon&) -> Ribbon& = delete;

	auto operator=(Ribbon&&) -> Ribbon& = delete;

	/**
	 * @brief
	 *  Add an empty tab to the ribbon.
	 * @param[in] iLabel Tab label rendered in the tab bar.
	 * @return The index of the newly created tab.
	 */
	auto addTab(std::string iLabel) -> size_t;

	/**
	 * @brief
	 *  Mark a tab as highlighted (drawn with the theme accent colour in the tab bar).
	 * @param[in] iTabIndex Index of the target tab.
	 * @param[in] iHighlighted True to highlight, false to render normally.
	 */
	void setTabHighlighted(size_t iTabIndex, bool iHighlighted);

	/**
	 * @brief
	 *  Add an empty group to a tab.
	 * @param[in] iTabIndex Index of the target tab.
	 * @param[in] iLabel Label of the new group.
	 * @return The index of the newly created group within the tab.
	 */
	auto addGroup(size_t iTabIndex, std::string iLabel) -> size_t;

	/**
	 * @brief
	 *  Add a button to a group.
	 * @param[in] iTabIndex Index of the target tab.
	 * @param[in] iGroupIndex Index of the target group inside the tab.
	 * @param[in] iButton Button to append.
	 */
	void addButton(size_t iTabIndex, size_t iGroupIndex, Button iButton);

	/**
	 * @brief
	 *  Clear every tab, group, and button.
	 */
	void clear();

	/**
	 * @brief
	 *  Render the ribbon at the current ImGui cursor. Consumes a fixed height.
	 */
	void onRender();

	/**
	 * @brief
	 *  Total pixel height reserved for the ribbon (tabs + group area + labels).
	 * @return The reserved height in pixels.
	 */
	[[nodiscard]] static auto height() -> float;

private:
	/// Top-level tabs displayed by the ribbon, in render order.
	std::vector<Tab> m_tabs;
};

}// namespace owl::gui::widgets
