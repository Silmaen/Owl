/**
 * @file ProceduralSky.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene::component {

/**
 * @brief
 *  Computes the sky of the `Environment` of the entity from the position of the sun instead of reading an image: a
 *  clear day sky, a glow at dusk and dawn, a starry night with the moon.
 *
 * The environment keeps its `intensity` (the luminance of a pixel of 1, the zenith with the sun 60 degrees high), its
 * `rotation` and `showSky`; its `hdri` is not read. The sky is drawn again when the sun moves, a `DayNightCycle` of the
 * entity moving it.
 */
struct OWL_API ProceduralSky {
	/// Whether the sky is computed; off goes back to the image or the constant ambient of the environment.
	bool enabled = true;
	/// Direction towards the sun; a `DayNightCycle` writes it.
	math::vec3 sunDirection{0.3f, 0.8f, -0.5f};
	/// Haze of the air, from 2 (very clear) to 10 (hazy).
	float turbidity = 3.f;
	/// Colour of the ground below the horizon (sRGB).
	math::vec3 groundColor{0.38f, 0.36f, 0.33f};
	/// Colour of the night sky (sRGB).
	math::vec3 nightColor{0.42f, 0.48f, 0.66f};
	/// Brightness of the night sky against the zenith at noon (1).
	float nightBrightness = 0.015f;
	/// Brightness of the stars, 0 for none.
	float stars = 1.f;
	/// Angular diameter of the sun and moon discs, in degrees; 0 for none.
	float discSize = 1.5f;
	/// Width of the sky image, in pixels (its height is half).
	uint32_t resolution = 512;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Procedural Sky"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "ProceduralSky"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
