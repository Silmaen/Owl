/**
 * @file UiFocus.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"

#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  Focus settings of a HUD widget: whether the keyboard and the gamepad can reach it, where the focus goes next, how
 *  the focus is drawn.
 *
 * A `UiButton` (not disabled) or a `UiSlider` is focusable without this component; it only changes the defaults.
 * Without a neighbour, the focus goes to the nearest focusable widget in the direction pressed. A neighbour is an
 * entity UUID: duplicating or instantiating the subtree points it at the copy.
 */
struct OWL_API UiFocus {
	/// Whether the widget can take the focus (false keeps a widget out of the keyboard and gamepad navigation).
	bool focusable = true;
	/// Whether the widget takes the focus, shown, as soon as the scene plays (and where a lost focus restarts).
	bool initial = false;
	/// UUID of the widget the focus goes to on `ui_up` (0: the nearest one above).
	core::UUID up{0};
	/// UUID of the widget the focus goes to on `ui_down` (0: the nearest one below).
	core::UUID down{0};
	/// UUID of the widget the focus goes to on `ui_left` (0: the nearest one on the left).
	core::UUID left{0};
	/// UUID of the widget the focus goes to on `ui_right` (0: the nearest one on the right).
	core::UUID right{0};
	/// Name of the Lua function called when the widget takes the focus (it reads `_focused_entity`).
	std::string onFocusCallback;
	/// Colour of the frame drawn around the focused widget.
	math::vec4 highlightColor{1.f, 0.82f, 0.2f, 1.f};
	/// Thickness of the frame, in reference pixels.
	float highlightThickness = 2.f;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI Focus"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiFocus"; }

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
