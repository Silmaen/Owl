/**
 * @file SimulationLod.h
 * @author Silmaen
 * @date 06/11/2026
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
 *  Simulation level of detail: far from the focus, the entity leaves the full simulation for a coarse model run on a
 *  slower tick, and comes back when the focus nears.
 *
 * The focus is every enabled `StreamingSource`, else the primary camera. Beyond `radius + hysteresis` of all of them
 * the entity is demoted: its script gets `on_demoted()`, then `on_coarse_tick(dt)` every `coarseInterval` ticks instead
 * of `on_update` and `on_tick`; its physics body sleeps, held still (`Hold`) or carried along its arc around the
 * strongest gravity field (`Rails`). Within `radius` it is promoted back: the body wakes with its velocity and the script
 * gets `on_promoted()`. Promotions and demotions run on the fixed tick in a fixed order, so a replay meets them at the
 * same ticks.
 */
struct OWL_API SimulationLod {
	/// What a demoted physics body does.
	enum struct Model : uint8_t {
		Hold,///< It stays where it was demoted, its velocity kept for its promotion.
		Rails///< It follows its conic arc around the strongest gravity field (a straight line without one).
	};
	/// Whether the entity may be demoted; off promotes it at the next tick.
	bool enabled = true;
	/// Distance to the nearest focus within which the entity is fully simulated, in world units.
	float radius = 200.f;
	/// Extra distance beyond `radius` before a demotion, so an entity on the edge does not switch every tick.
	float hysteresis = 20.f;
	/// Ticks between two coarse ticks of a demoted entity.
	uint32_t coarseInterval = 10;
	/// What a demoted physics body does.
	Model model = Model::Rails;

	/// Highest accepted `coarseInterval`.
	static constexpr uint32_t maxCoarseInterval = 3600;

	/// Runtime: the entity is demoted.
	bool m_demoted = false;
	/// Runtime: ticks left before the next coarse tick.
	uint32_t m_coarseCountdown = 0;
	/// Runtime: velocity of the body when it was demoted, carried by the coarse ticks.
	math::vec3f m_velocity{0.f, 0.f, 0.f};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Simulation LOD"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SimulationLod"; }

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
