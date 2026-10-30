/**
 * @file Trail.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/Core.h"
#include "core/Serializer.h"
#include "math/Curve.h"
#include "math/vectors.h"
#include "renderer/ParticleSystem.h"

#include <cstdint>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  A ribbon trail behind the entity: a sword swing, a rocket, a comet, a car's lights.
 *
 * The `owl.trails` system records the world position of the entity each time it moved `minDistance` away from the last
 * point, and drops the points older than `lifetime`; a `RendererParticles` layer draws the ribbon through them, facing
 * the camera, its colour and width following the curves along its length (0 at the entity, 1 at the tail).
 */
struct OWL_API Trail {
	/// Whether the trail records and draws.
	bool enabled = true;
	/// Seconds a point of the trail lives.
	float lifetime = 0.5f;
	/// Distance the entity moves before a new point is recorded, in world units.
	float minDistance = 0.05f;
	/// Most points of the trail (2 to 128).
	uint32_t maxPoints = 32;
	/// Width of the trail at a width multiplier of 1, in world units.
	float width = 0.2f;
	/// Width multiplier along the trail (empty: 1 to 0).
	math::Curve widthOverLength;
	/// Tint of the trail, sRGB as authored.
	math::vec4 color{1.f, 1.f, 1.f, 1.f};
	/// Colour along the trail, multiplying the tint (empty: opaque to transparent).
	renderer::ColorGradient colorOverLength;
	/// Luminance added to the colour, in nits: the bloom makes it glow.
	float emissive = 0.f;
	/// How the trail covers what is behind it.
	renderer::ParticleBlend blend = renderer::ParticleBlend::Additive;
	/// Runtime: the points, newest first (xyz world position, w its age in seconds).
	std::vector<math::vec4> m_points;
	/// Runtime: the tables baked from the curves.
	renderer::ParticleTables m_tables;
	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Trail"; }
	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Trail"; }
	/**
	 * @brief
	 *  Bake the tables of the trail from its curves (the default ones when they are empty).
	 * @return The tables.
	 */
	[[nodiscard]] auto bakeTables() const -> renderer::ParticleTables;
	/**
	 * @brief
	 *  Record a frame of the trail: the points age, the old ones go, the head follows the entity.
	 * @param[in] iPosition World position of the entity.
	 * @param[in] iSeconds Duration of the frame.
	 */
	void record(const math::vec3& iPosition, float iSeconds);
	/**
	 * @brief
	 *  Follow a shift of the floating origin: the points move back by it.
	 * @param[in] iShift The shift of the origin.
	 */
	void shiftOrigin(const math::vec3& iShift);
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
