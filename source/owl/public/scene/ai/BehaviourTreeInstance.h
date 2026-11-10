/**
 * @file BehaviourTreeInstance.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "scene/Entity.h"
#include "scene/ai/BehaviourTree.h"

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace owl::scene::ai {

/**
 * @brief
 *  What a node returned the last time it ran.
 */
enum struct NodeStatus : uint8_t {
	Idle,///< Not run since the agent started, or stopped.
	Running,///< Not done yet: it runs again at the next tick.
	Success,///< Done, it succeeded.
	Failure///< Done, it failed.
};

/**
 * @brief
 *  One agent running a behaviour tree: the state of each node and the blackboard of the agent.
 *
 * A tick runs the tree from the root. A node that returns `Running` is resumed at the next tick; the sequences and
 * selectors remember the child they were at, a reactive selector tries its children above again and stops the
 * running one when one of them succeeds or runs. The random draws come from the stream `owl.behaviour_trees` of the
 * scene, so the same game replays the same choices.
 */
class OWL_API BehaviourTreeInstance final {
public:
	/**
	 * @brief
	 *  Start an agent on a tree, its blackboard at the values the tree declares, then at the overrides.
	 * @param[in] iTree The tree.
	 * @param[in] iOverrides Values that replace those of the tree, by key (a key the tree lacks or of another kind
	 *  is skipped).
	 */
	BehaviourTreeInstance(shared<const BehaviourTree> iTree, std::span<const BlackboardKey> iOverrides);

	/**
	 * @brief
	 *  Destructor.
	 */
	~BehaviourTreeInstance();

	BehaviourTreeInstance(const BehaviourTreeInstance&) = delete;

	BehaviourTreeInstance(BehaviourTreeInstance&&) = delete;

	auto operator=(const BehaviourTreeInstance&) -> BehaviourTreeInstance& = delete;

	auto operator=(BehaviourTreeInstance&&) -> BehaviourTreeInstance& = delete;

	/**
	 * @brief
	 *  Run the tree once.
	 * @param[in] iEntity The agent.
	 * @param[in] iSeconds Duration of the tick.
	 * @return What the root returned.
	 */
	auto tick(const Entity& iEntity, float iSeconds) -> NodeStatus;

	/**
	 * @brief
	 *  Stop every running node (a walk is stopped) and put the nodes back to `Idle`.
	 * @param[in] iEntity The agent.
	 */
	void halt(const Entity& iEntity);

	/**
	 * @brief
	 *  What the root returned at the last tick.
	 * @return The status.
	 */
	[[nodiscard]] auto getStatus() const -> NodeStatus;

	/**
	 * @brief
	 *  What a node returned the last time it ran.
	 * @param[in] iNode The node.
	 * @return The status, `Idle` for a node that never ran or is not in the tree.
	 */
	[[nodiscard]] auto getNodeStatus(core::UUID iNode) const -> NodeStatus;

	/**
	 * @brief
	 *  The blackboard of the agent.
	 * @return The values by key.
	 */
	[[nodiscard]] auto getBlackboard() const -> const std::map<std::string, BlackboardValue, std::less<>>&;

	/**
	 * @brief
	 *  A value of the blackboard.
	 * @param[in] iKey The key.
	 * @return The value, null when the key holds nothing.
	 */
	[[nodiscard]] auto getValue(std::string_view iKey) const -> const BlackboardValue*;

	/**
	 * @brief
	 *  Write a value on the blackboard.
	 * @param[in] iKey The key.
	 * @param[in] iValue The value.
	 */
	void setValue(std::string_view iKey, BlackboardValue iValue);

	/**
	 * @brief
	 *  The tree the agent runs.
	 * @return The tree.
	 */
	[[nodiscard]] auto getTree() const -> const shared<const BehaviourTree>&;

	/**
	 * @brief
	 *  Number of ticks run so far.
	 * @return The count.
	 */
	[[nodiscard]] auto getTickCount() const -> uint64_t;

private:
	/// Forward-declared implementation.
	struct Impl;
	/// The implementation.
	uniq<Impl> mp_impl;
};

}// namespace owl::scene::ai
