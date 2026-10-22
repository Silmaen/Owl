/**
 * @file Oscillator.h
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
 *  Moves an entity back and forth along an offset: a moving platform, a swinging lamp, a bobbing sign.
 *
 * The `owl.oscillators` system moves the entity from its rest position (the `Transform` of the scene) to the rest
 * position plus `offset` and back, once per `period`. An entity with a `PhysicBody` is moved through the physics
 * step by setting its velocity every tick, so a kinematic body carries the bodies standing on it; any other entity
 * has its `Transform` written each frame.
 */
struct OWL_API Oscillator {
	/// Speed profile of the movement.
	enum struct Easing : uint8_t {
		Linear,///< Constant speed, a sharp turn at each end.
		Smooth,///< Sinusoidal: slowing down towards each end.
	};

	/// Whether the entity moves; off leaves it at its rest position.
	bool enabled = true;
	/// Farthest displacement from the rest position, in world units (the range drawn in the viewport).
	math::vec3 offset{4.f, 0.f, 0.f};
	/// Duration of a round trip, in seconds.
	float period = 4.f;
	/// Share of the cycle already done when Play starts (0 to 1).
	float phase = 0.f;
	/// Speed profile.
	Easing easing = Easing::Linear;

	/// Runtime: the rest position was read.
	bool m_captured = false;
	/// Runtime: rest position.
	math::vec3 m_basePosition{0.f, 0.f, 0.f};
	/// Runtime: seconds of movement done.
	float m_time = 0.f;

	/**
	 * @brief
	 *  Position along the offset at a time, as a share of it.
	 * @param[in] iSeconds Seconds of movement.
	 * @return 0 at the rest position, 1 at the far end.
	 */
	[[nodiscard]] auto share(float iSeconds) const -> float;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Oscillator"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Oscillator"; }

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
