/**
 * @file Animator.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "data/animation/Skeleton.h"
#include "data/animation/StateMachine.h"
#include "math/matrices.h"

#include <cstddef>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  Plays the clips of the skinned mesh of the entity (`MeshRenderer`) through a state machine.
 *
 * The `owl.animation` system advances the machine, cross-fades the states and computes the skin matrices the mesh
 * layer draws with; the bone matrices it keeps are read by the sockets bound to a bone. The machine is authored in
 * the inspector and read and driven from Lua (`animator.*`).
 *
 * On-disk form:
 * ```yaml
 * Animator:
 *   playing: true
 *   speed: 1
 *   entry: idle
 *   parameters: [{name: speed, type: Float, value: 0}]
 *   states: [{name: idle, clip: idle, speed: 1, loop: true}, {name: walk, clip: walk, speed: 1, loop: true}]
 *   transitions:
 *     - {from: idle, to: walk, duration: 0.2, exitTime: -1, conditions: [{parameter: speed, op: Greater, threshold: 0.1}]}
 * ```.
 */
struct OWL_API Animator {
	/// The authored state machine.
	data::animation::StateMachine machine;
	/// Whether the machine advances (off holds the current pose).
	bool playing = true;
	/// Time scale of the machine.
	float speed = 1.f;
	/// Plays in Edit mode too (the Owl Nest preview; written only when set, no effect in a game).
	bool preview = false;
	/// The machine being played (runtime only).
	data::animation::StateMachinePlayer player;
	/// The current pose (runtime only).
	data::animation::Pose pose;
	/// Model-space matrix of every bone of the current pose (runtime only).
	std::vector<math::mat4> boneMatrices;
	/// Skin matrices of the current pose (runtime only).
	std::vector<math::mat4> skinMatrices;
	/// Mesh resource the runtime state was built for (runtime only, never dereferenced).
	const void* source = nullptr;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Animator"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Animator"; }

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

	/**
	 * @brief
	 *  Drop the runtime state, so that the next update restarts the machine in its entry state.
	 */
	void invalidate();

	/**
	 * @brief
	 *  Check if the runtime holds a pose for a skeleton of a given size.
	 * @param[in] iBoneCount The bone count.
	 * @return True when the bone and skin matrices are ready.
	 */
	[[nodiscard]] auto hasPose(size_t iBoneCount) const -> bool {
		return iBoneCount > 0 && skinMatrices.size() == iBoneCount && boneMatrices.size() == iBoneCount;
	}
};

}// namespace owl::scene::component
