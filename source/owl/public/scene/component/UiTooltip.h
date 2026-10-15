/**
 * @file UiTooltip.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  Tooltip of a HUD widget: a text box near the mouse once the widget has been hovered for a while.
 *
 * It applies to the widget carrying it and to its descendants without a tooltip of their own. The box is sized to the
 * text, kept inside the screen and drawn over every window.
 */
struct OWL_API UiTooltip {
	/// Text shown (line feeds allowed).
	std::string text;
	/// Hover time before the tooltip shows, in seconds.
	float delay = 0.5f;
	/// Font size, in reference pixels.
	float fontSize = 14.f;
	/// Space between the text and the border of the box, in reference pixels.
	float padding = 4.f;
	/// Offset of the box top-left corner from the mouse, in reference pixels.
	math::vec2 offset{14.f, 18.f};
	/// Colour of the box.
	math::vec4 backgroundColor{0.05f, 0.05f, 0.07f, 0.95f};
	/// Colour of the text.
	math::vec4 textColor{1.f, 1.f, 1.f, 1.f};

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI Tooltip"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiTooltip"; }

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
