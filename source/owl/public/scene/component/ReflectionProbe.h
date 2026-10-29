/**
 * @file ReflectionProbe.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene::component {

/// Smallest face size of a reflection probe cube, in texels.
constexpr uint32_t g_minProbeResolution = 16;
/// Largest face size of a reflection probe cube, in texels.
constexpr uint32_t g_maxProbeResolution = 512;

/**
 * @brief
 *  A local reflection probe of the 3D layers: the scene seen from the entity's world position, captured in a cube and
 *  prefiltered by roughness, replaces the specular of the environment inside its volume. The volume is centred on
 *  the probe and axis-aligned (the rotation of the entity is ignored); the two probes of largest influence blend at a
 *  point, the environment fills what they leave.
 */
struct OWL_API ReflectionProbe {
	/**
	 * @brief
	 *  Shape of the volume of influence.
	 */
	enum struct Shape : uint8_t {
		Box,///< An axis-aligned box of half size `extents`.
		Sphere,///< A sphere of `radius`.
	};

	/// Shape of the volume of influence.
	Shape shape = Shape::Box;
	/// Half size of a box volume along each world axis, in world units.
	math::vec3 extents{2.f, 2.f, 2.f};
	/// Radius of a sphere volume, in world units.
	float radius = 3.f;
	/// Distance inside the volume over which the probe fades in from its border, in world units.
	float blendDistance = 0.5f;
	/// Whether the reflections are projected on the volume (parallax of a room) instead of seen from infinitely far.
	bool parallax = true;
	/// Scale of the captured radiance.
	float intensity = 1.f;
	/// Size of each face of the captured cube, in texels.
	uint32_t resolution = 128;
	/// Whether the probe is captured again every few frames (one probe per frame) instead of once when baked.
	bool realtime = false;
	/// Raised to capture the probe again at its next frame (the Bake button); not saved.
	uint32_t captureRequest = 0;

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Reflection Probe"; }

	/**
	 * @brief
	 *  Key of the component in the scene files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "ReflectionProbe"; }

	/**
	 * @brief
	 *  Write the component.
	 * @param[in] iOut Serializer to write into.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read the component; missing keys keep their defaults, sizes are clamped to valid values.
	 * @param[in] iNode Serializer to read from.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
