/**
 * @file BehaviourTreeAgent.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "scene/ai/BehaviourTree.h"

#include <string>
#include <vector>

namespace owl::scene::ai {
class BehaviourTreeInstance;
}// namespace owl::scene::ai

namespace owl::scene::component {

/**
 * @brief
 *  Runs a behaviour tree (`.owlbt`) on the entity: the AI of an enemy, a guard, a villager.
 *
 * The `owl.behaviour_trees` system (`Scripts`, fixed tick only, after the scripts) ticks the tree of every enabled
 * agent in UUID order. Its blackboard starts at the values the tree declares, then at `overrides`. Its leaves walk with
 * the `NavAgent` of the entity, steer its `SteeringAgent` and call the Lua functions of its script. Lua (`bt.*`)
 * reads and writes the blackboard while the game runs.
 */
struct OWL_API BehaviourTreeAgent {
	/// Whether the tree runs; off stops it, and its walk.
	bool enabled = true;
	/// Asset path of the `.owlbt` tree.
	std::string tree;
	/// Values of the blackboard at the start in place of those of the tree, by key.
	std::vector<ai::BlackboardKey> overrides;

	/// Runtime: the running tree, created at the first tick.
	shared<ai::BehaviourTreeInstance> m_instance;
	/// Runtime: entity the instance runs for (a copied entity starts its own).
	core::UUID m_owner{0};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Behaviour Tree Agent"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "BehaviourTreeAgent"; }

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
