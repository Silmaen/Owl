/**
 * @file UiWindow.h
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
 *  HUD window: a panel with a title bar that the player drags, raises by clicking and closes.
 *
 * The window rectangle (`UiRect`) includes the title bar. Its children are laid out in the content area under the bar
 * and clipped to it. A window stays inside the screen, blocks the mouse for the world and the widgets under it, and is
 * drawn after the windows below it (its rank in the `Renderer2D` sort key), so it covers their text. Closing hides the entity (`Visibility`); a script
 * shows it again with `ui.set_visible`.
 */
struct OWL_API UiWindow {
	/// Text of the title bar.
	std::string title = "Window";
	/// Height of the title bar, in reference pixels.
	float titleBarHeight = 24.f;
	/// Font size of the title, in reference pixels.
	float titleFontSize = 14.f;
	/// Colour of the window body.
	math::vec4 bodyColor{0.12f, 0.13f, 0.16f, 0.95f};
	/// Colour of the title bar.
	math::vec4 titleColor{0.20f, 0.30f, 0.50f, 1.f};
	/// Colour of the title text and of the close mark.
	math::vec4 titleTextColor{1.f, 1.f, 1.f, 1.f};
	/// Whether the title bar drags the window.
	bool draggable = true;
	/// Whether the title bar shows a close button.
	bool closable = true;
	/// Lua function called when the close button is pressed (before the window is hidden).
	std::string onCloseCallback;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI Window"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiWindow"; }

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
