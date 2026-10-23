/**
 * @file ProceduralSky.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "EnvironmentMap.h"
#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::renderer {

/**
 * @brief
 *  Inputs of the analytic sky: where the sun is and what the air, the ground and the night look like.
 */
struct OWL_API SkyParameters {
	/// Direction towards the sun, normalised; below the horizon for the night.
	math::vec3 sunDirection{0.f, 1.f, 0.f};
	/// Haze of the air, from 2 (very clear) to 10 (hazy).
	float turbidity = 3.f;
	/// Colour of the ground below the horizon, linear.
	math::vec3 groundColor{0.12f, 0.11f, 0.1f};
	/// Colour of the night sky, linear.
	math::vec3 nightColor{0.15f, 0.2f, 0.4f};
	/// Brightness of the night sky against the zenith at noon (1).
	float nightBrightness = 0.015f;
	/// Brightness of the stars, 0 for none.
	float stars = 1.f;
	/// Angular diameter of the sun and moon discs, in degrees; 0 for none.
	float discSize = 2.5f;
};

/**
 * @brief
 *  The sun, the moon and the stars the sky shader draws over the environment image at the resolution of the screen,
 *  so they move smoothly and stay sharp; they are never in the image (nor in the lighting it gives).
 */
struct OWL_API SkyBodies {
	/// Direction towards the sun, normalised; the moon is opposite.
	math::vec3 sunDirection{0.f, 1.f, 0.f};
	/// Angular radius of the sun and moon discs, in radians; 0 draws no disc.
	float discRadius = 0.f;
	/// Radiance of the sun disc against the zenith at noon (1).
	float sunRadiance = 0.f;
	/// Radiance of the moon disc.
	float moonRadiance = 0.f;
	/// Radiance of the brightest stars; 0 draws none.
	float starRadiance = 0.f;
};

/**
 * @brief
 *  Get the sun, the moon and the stars of a sky: the sun fades out below the horizon, the moon and the stars come out
 *  at night.
 * @param[in] iParameters The sky.
 * @return What the sky shader draws.
 */
[[nodiscard]] OWL_API auto skyBodiesOf(const SkyParameters& iParameters) -> SkyBodies;

/**
 * @brief
 *  Draw the sky of the parameters as an equirectangular image, the convention of `Equirectangular`.
 *
 * The day sky follows the Preetham clear-sky model, a glow lights the horizon around the sun at dusk and dawn, and the
 * night fades in a dark sky; the sun, the moon and the stars are left to the sky shader (`skyBodiesOf`). A pixel of 1 is the luminance of the zenith with
 * the sun 60 degrees high, so the `intensity` of the environment keeps its meaning.
 * @param[in] iParameters The sky to draw.
 * @param[in] iSize Size of the image (2:1).
 * @return The image.
 */
[[nodiscard]] OWL_API auto renderProceduralSky(const SkyParameters& iParameters, const math::vec2ui& iSize)
		-> FloatImage;

}// namespace owl::renderer
