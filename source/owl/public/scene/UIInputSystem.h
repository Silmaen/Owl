/**
 * @file UIInputSystem.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"

namespace owl::scene {

/**
 * @brief Manages UI input: hit-testing, hover/focus tracking, click routing.
 *
 * Called once per frame during runtime. Updates button states, handles clicks,
 * and invokes Lua callbacks. UI elements consume mouse events before the scene.
 */
class OWL_API UIInputSystem final {
public:
	UIInputSystem() = delete;

	/**
	 * @brief Process UI input for the current frame.
	 * @param[in] iScene The active scene.
	 * @param[in] iViewportSize The viewport dimensions.
	 * @param[in] iMousePos Mouse position in viewport coordinates.
	 * @param[in] iMousePressed Whether the left mouse button is currently pressed.
	 */
	static void update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
					   bool iMousePressed);

	/**
	 * @brief Check if the UI is currently consuming the mouse (hover over interactive element).
	 * @return True if a UI element is hovered or focused.
	 */
	[[nodiscard]] static auto isUIConsuming() -> bool;

	/**
	 * @brief Reset the input system state.
	 */
	static void reset();

private:
	/// Whether a UI element is currently hovered.
	static bool s_consuming;
	/// Whether the mouse was pressed last frame (for click detection).
	static bool s_wasPressed;
};

}// namespace owl::scene
