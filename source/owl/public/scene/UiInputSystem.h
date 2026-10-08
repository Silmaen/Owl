/**
 * @file UiInputSystem.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"

namespace owl::scene {
/**
 * @brief
 *  Manages UI input: hit-testing, hover/focus tracking, click routing.
 *
 * Called once per frame during runtime. Updates button states, handles clicks,
 * and invokes Lua callbacks. UI elements consume mouse events before the scene. The hover and press state
 * lives in each scene (`Scene::getUiInputState()`).
 */
class OWL_API UiInputSystem final {
public:
	UiInputSystem() = delete;

	/**
	 * @brief
	 *  Process UI input for the current frame.
	 * @param[in] iScene The scene whose UI receives the mouse (nullptr: nothing happens).
	 * @param[in] iViewportSize The viewport dimensions.
	 * @param[in] iMousePos Mouse position in viewport coordinates.
	 * @param[in] iMousePressed Whether the left mouse button is currently pressed.
	 */
	static void update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
					   bool iMousePressed);

	/**
	 * @brief
	 *  Check if the UI of a scene is currently consuming the mouse (hover over interactive element).
	 * @param[in] iScene The scene.
	 * @return True if a UI element is hovered or focused.
	 */
	[[nodiscard]] static auto isUIConsuming(const Scene& iScene) -> bool;

	/**
	 * @brief
	 *  Reset the UI input state of a scene.
	 * @param[in,out] ioScene The scene.
	 */
	static void reset(Scene& ioScene);
};

}// namespace owl::scene
