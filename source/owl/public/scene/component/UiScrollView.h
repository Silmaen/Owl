/**
 * @file UiScrollView.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {
/**
 * @brief
 *  HUD scroll view: shows a window of a content larger than itself, the children of the widget.
 *
 * The children are laid out in the viewport (the rectangle minus the bars that the overflow needs) shifted by the
 * scroll offset, and clipped to it; a click, a wheel notch or the focus only reaches what is in view. The size of the
 * content is `contentSize`, or, on an axis left at 0, the extent of the children placed by a `UiLayout` of the same
 * widget, or else the viewport (nothing to scroll). The mouse wheel over the view, the bars (drag the thumb, click the
 * track), the `ui_scroll` action and the focus moving to a widget inside scroll it; scripts use `ui.scroll_set`.
 */
struct OWL_API UiScrollView {
	/// Whether the content scrolls vertically.
	bool vertical = true;
	/// Whether the content scrolls horizontally.
	bool horizontal = false;
	/// Size of the content in reference pixels; 0 on an axis takes the extent of the `UiLayout` children, else the view.
	math::vec2 contentSize{0.f, 0.f};
	/// Distance the content is scrolled by, in reference pixels (kept inside the content by the layout).
	math::vec2 scrollOffset{0.f, 0.f};
	/// Scroll distance of one wheel notch, in reference pixels.
	float wheelStep = 48.f;
	/// Whether the bars are drawn (and take room) when the content overflows.
	bool showScrollBar = true;
	/// Thickness of a bar, in reference pixels.
	float scrollBarWidth = 8.f;
	/// Colour of the bar track.
	math::vec4 trackColor{1.f, 1.f, 1.f, 0.08f};
	/// Colour of the bar thumb.
	math::vec4 thumbColor{1.f, 1.f, 1.f, 0.45f};

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI Scroll View"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiScrollView"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
