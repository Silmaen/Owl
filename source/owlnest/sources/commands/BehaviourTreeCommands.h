/**
 * @file BehaviourTreeCommands.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <scene/ai/BehaviourTree.h>

#include <string>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  A behaviour tree open in an editor tab, as the `bt.*` commands edit it.
 */
class BehaviourTreeEditor {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	BehaviourTreeEditor() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	BehaviourTreeEditor(const BehaviourTreeEditor&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	BehaviourTreeEditor(BehaviourTreeEditor&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This editor.
	 */
	auto operator=(const BehaviourTreeEditor&) -> BehaviourTreeEditor& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This editor.
	 */
	auto operator=(BehaviourTreeEditor&&) -> BehaviourTreeEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~BehaviourTreeEditor();

	/**
	 * @brief
	 *  The tree as it is.
	 * @return The tree.
	 */
	[[nodiscard]] virtual auto tree() const -> const scene::ai::BehaviourTree& = 0;

	/**
	 * @brief
	 *  Replace the tree, as one undo entry.
	 * @param[in] iNext The tree after the edit.
	 * @param[in] iDescription Description of the undo entry.
	 * @param[in] iMerge Whether it merges with the previous entry of the same description (a drag, typing).
	 * @return False when nothing changes.
	 */
	virtual auto apply(scene::ai::BehaviourTree iNext, const std::string& iDescription, bool iMerge) -> bool = 0;
};

/**
 * @brief
 *  Register the `bt.*` commands: create a tree, add, link, move, set and remove its nodes and its blackboard keys.
 * @param[in,out] ioRegistry The registry.
 */
void addBehaviourTreeCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
