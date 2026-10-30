/**
 * @file ReverbZone.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/matrices.h"
#include "math/vectors.h"
#include "sound/AudioEffects.h"

#include <cstdint>

namespace owl::scene::component {

/**
 * @brief
 *  A volume giving its reverb (a cave, a hall, outdoors) and its filter (underwater) to the sounds the listener hears
 *  inside it, blended with the zones around over a fade distance.
 *
 * The volume follows the world transform of the entity: a box of half size `extents` or a sphere of `radius`, both
 * scaled, rotated and moved with it, so the gizmo resizes it in the viewport.
 */
struct OWL_API ReverbZone {
	/// Shape of the volume.
	enum struct Shape : uint8_t {
		Box,///< A box of half size `extents`.
		Sphere,///< A sphere of `radius`.
	};

	/// Shape of the volume.
	Shape shape = Shape::Box;
	/// Half size of a box volume, in local units of the entity.
	math::vec3 extents{2.f, 2.f, 2.f};
	/// Radius of a sphere volume, in local units of the entity (scaled by the largest axis).
	float radius = 3.f;
	/// Distance outside the volume over which the zone fades out, in world units.
	float blendDistance = 1.f;
	/// Reverb preset; Custom uses `reverb`.
	sound::ReverbPreset preset = sound::ReverbPreset::Cave;
	/// Reverb parameters of a Custom zone.
	sound::Reverb reverb = sound::getReverbPreset(sound::ReverbPreset::Cave);
	/// Filter put on the sounds heard inside the zone (a low-pass underwater).
	sound::Filter filter{};

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Reverb Zone"; }

	/**
	 * @brief
	 *  Key of the component in the files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "ReverbZone"; }

	/**
	 * @brief
	 *  Reverb of the zone: the preset's, or `reverb` for Custom.
	 * @return The reverb.
	 */
	[[nodiscard]] auto getReverb() const -> sound::Reverb;

	/**
	 * @brief
	 *  Distance from a point to the volume of the zone.
	 * @param[in] iWorld World transform of the entity.
	 * @param[in] iPoint The point, in the same space.
	 * @return The distance in world units, 0 inside the volume.
	 */
	[[nodiscard]] auto getDistanceOutside(const math::mat4& iWorld, const math::vec3& iPoint) const -> float;

	/**
	 * @brief
	 *  What the zone gives at a point: its reverb, its filter and its weight there.
	 * @param[in] iWorld World transform of the entity.
	 * @param[in] iPoint The point (the listener).
	 * @return The sample.
	 */
	[[nodiscard]] auto sample(const math::mat4& iWorld, const math::vec3& iPoint) const -> sound::AudioZoneSample;

	/**
	 * @brief
	 *  Write the component.
	 * @param[in] iOut The serializer.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read the component.
	 * @param[in] iNode The serializer.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
