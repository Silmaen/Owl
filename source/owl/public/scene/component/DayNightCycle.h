/**
 * @file DayNightCycle.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief
 *  Runs the hours of a day: the sun rises, crosses the sky and sets, then the moon lights the night.
 *
 * The `owl.day_night` system advances `timeOfDay` and, each frame (in Owl Nest too, without advancing it), turns the
 * directional `Light` of `sun` along the path of the sun by day and of the moon by night, with their colour and
 * illuminance, and gives the direction of the sun to the `ProceduralSky` of the entity (and the size of its disc
 * when the light has an `angularRadius`). In Play, it also raises the
 * exposure of every camera of the scene at night by `nightExposure`. The sun rises at 6 h towards
 * `azimuth`, culminates at `maxElevation` at noon and sets at 18 h on the opposite side.
 */
struct OWL_API DayNightCycle {
	/// Whether the hours advance; off freezes the time of day (the lighting still follows it).
	bool enabled = true;
	/// Time of day, in hours from 0 to 24.
	float timeOfDay = 9.f;
	/// Duration of a whole day, in seconds.
	float dayLength = 600.f;
	/// Direction of the sunrise about the vertical axis, in degrees (0: towards -Z, 90: towards +X).
	float azimuth = 90.f;
	/// Height of the sun at noon, in degrees above the horizon.
	float maxElevation = 60.f;
	/// Directional light turned with the sun and the moon (0: none).
	core::UUID sun{0};
	/// Illuminance of the sun at its highest, in lux.
	float sunIlluminance = 100000.f;
	/// Colour of the sun high in the sky (sRGB).
	math::vec3 sunColor{1.f, 0.96f, 0.9f};
	/// Colour of the sun at the horizon (sRGB).
	math::vec3 sunsetColor{1.f, 0.55f, 0.3f};
	/// Illuminance of the moon at its highest, in lux (brighter than nature, for a playable night).
	float moonIlluminance = 4000.f;
	/// Colour of the moonlight (sRGB).
	math::vec3 moonColor{0.62f, 0.72f, 1.f};
	/// Exposure added to the cameras at night in Play, in EV, as the eye adapts to the dark.
	float nightExposure = 3.f;

	/// Runtime: exposure added to the cameras of the scene at the last frame.
	float m_appliedExposure = 0.f;

	/**
	 * @brief
	 *  Get the direction towards the sun at the time of day.
	 * @return The normalised direction, below the horizon at night.
	 */
	[[nodiscard]] auto getSunDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Day Night Cycle"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "DayNightCycle"; }

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
