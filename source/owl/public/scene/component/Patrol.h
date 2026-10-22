/**
 * @file Patrol.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  Walks an entity from waypoint to waypoint, or wanders at random around its post: a villager, a guard, a pet.
 *
 * The `owl.patrols` system moves the `Transform` at `speed` towards the next waypoint, waits `waitTime` seconds at
 * each, and turns round at the last one (`PingPong`) or goes back to the first (`Loop`). Without waypoints, a
 * `wanderRadius` above 0 makes it pick a random point around its rest position each time it arrives (the choice
 * depends on `seed` only). The waypoints are positions in the space of the `Transform`; their z is the one of the
 * entity. In Owl Nest each is a handle in the viewport.
 */
struct OWL_API Patrol {
	/// What the entity does at the last waypoint.
	enum struct Mode : uint8_t {
		Loop,///< Walks back to the first waypoint.
		PingPong,///< Walks the waypoints in reverse.
	};

	/// Whether the entity moves; off leaves it where it is.
	bool enabled = true;
	/// Positions to walk through, in the space of the `Transform`.
	std::vector<math::vec3> waypoints;
	/// What the entity does at the last waypoint.
	Mode mode = Mode::PingPong;
	/// Walking speed, in world units per second.
	float speed = 1.2f;
	/// Seconds spent at each waypoint.
	float waitTime = 1.5f;
	/// Radius of the random wander around the rest position when there are no waypoints, in world units; 0 for none.
	float wanderRadius = 0.f;
	/// Seed of the random wander.
	float seed = 0.f;
	/// Mirror the sprite (scale x) towards the direction it walks.
	bool faceDirection = false;
	/// Height of the hop while walking, in world units; 0 for none.
	float hopHeight = 0.f;
	/// Hops per second times 2 pi: the rate of the hop.
	float hopRate = 9.f;

	/// Runtime: the starting position was read.
	bool m_captured = false;
	/// Runtime: rest position, the centre of the wander.
	math::vec3 m_basePosition{0.f, 0.f, 0.f};
	/// Runtime: scale x before mirroring.
	float m_baseScaleX = 1.f;
	/// Runtime: current position without the hop.
	math::vec3 m_position{0.f, 0.f, 0.f};
	/// Runtime: wander target.
	math::vec3 m_target{0.f, 0.f, 0.f};
	/// Runtime: index of the waypoint walked to.
	std::size_t m_index = 0;
	/// Runtime: whether the ping-pong goes back to the first waypoint.
	bool m_reverse = false;
	/// Runtime: seconds left to wait.
	float m_wait = 0.f;
	/// Runtime: seconds walked, the clock of the hop.
	float m_walked = 0.f;
	/// Runtime: wander choices made.
	uint32_t m_choices = 0;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Patrol"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Patrol"; }

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
