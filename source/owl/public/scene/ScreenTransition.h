/**
 * @file ScreenTransition.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::scene {

/**
 * @brief Screen transition effects (fade, slide).
 *
 * Renders a full-screen overlay with animated alpha. Integrated into the scene's
 * render loop after UI rendering.
 */
class OWL_API ScreenTransition final {
public:
	ScreenTransition() = delete;

	/// Transition type.
	enum struct Type : uint8_t {
		None,///< No transition.
		FadeIn,///< Fade from opaque to transparent.
		FadeOut,///< Fade from transparent to opaque.
	};

	/**
	 * @brief Start a transition.
	 * @param[in] iType The transition type.
	 * @param[in] iDuration Duration in seconds.
	 */
	static void start(Type iType, float iDuration);

	/**
	 * @brief Update the transition (call once per frame).
	 * @param[in] iDeltaTime Frame delta time in seconds.
	 */
	static void update(float iDeltaTime);

	/**
	 * @brief Render the transition overlay.
	 * @param[in] iViewportWidth Viewport width in pixels.
	 * @param[in] iViewportHeight Viewport height in pixels.
	 */
	static void render(float iViewportWidth, float iViewportHeight);

	/// @brief Check if a transition is currently active.
	[[nodiscard]] static auto isActive() -> bool;

	/// @brief Get the current transition progress (0..1).
	[[nodiscard]] static auto getProgress() -> float;

	/// @brief Reset the transition state.
	static void reset();

private:
	/// Current transition type.
	static Type s_type;
	/// Duration in seconds.
	static float s_duration;
	/// Elapsed time.
	static float s_elapsed;
};

}// namespace owl::scene
