/**
 * @file Canvas.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene::component {
/**
 * @brief
 *  Canvas component — marks an entity as a HUD root.
 *
 * The `UiRect` entities under a Canvas are drawn by the HUD pass, in pixels on top of every render layer. The
 * Canvas sets the draw order and how its reference resolution maps to the screen: the widgets are authored in
 * reference pixels, multiplied by `getScale` on screen.
 */
struct OWL_API Canvas {
	/// Coordinate space for the Canvas.
	enum struct Space : uint8_t {
		ScreenOverlay,///< Screen-space overlay (always on top of scene).
	};
	/// How the reference resolution maps to the screen.
	enum struct ScaleMode : uint8_t {
		ConstantPixels,///< One reference pixel is one screen pixel, whatever the resolution.
		ScaleWithHeight,///< The reference height fills the screen height.
		MatchSmallerSide,///< The whole reference resolution fits in the screen (the smaller ratio of the two sides).
	};
	/// The coordinate space.
	Space space = Space::ScreenOverlay;
	/// Sort order — higher values render on top of lower values.
	int32_t sortOrder = 0;
	/// Resolution the widgets are authored at, in pixels.
	math::vec2 referenceResolution = {1280.f, 720.f};
	/// How `referenceResolution` maps to the screen.
	ScaleMode scaleMode = ScaleMode::ScaleWithHeight;

	/**
	 * @brief
	 *  Get the number of screen pixels per reference pixel.
	 * @param[in] iScreenSize The screen (viewport) size in pixels.
	 * @return The scale factor (1 for a degenerate reference resolution or screen).
	 */
	[[nodiscard]] auto getScale(const math::vec2& iScreenSize) const -> float;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Canvas"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Canvas"; }

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
