/**
 * @file BehaviourTreeLibrary.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/ai/BehaviourTree.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace owl::scene::ai {

/**
 * @brief
 *  The `.owlbt` trees read so far, by asset path, shared by every agent running them.
 */
class OWL_API BehaviourTreeLibrary final {
public:
	BehaviourTreeLibrary() = default;

	~BehaviourTreeLibrary() = default;

	BehaviourTreeLibrary(const BehaviourTreeLibrary&) = delete;

	BehaviourTreeLibrary(BehaviourTreeLibrary&&) = delete;

	auto operator=(const BehaviourTreeLibrary&) -> BehaviourTreeLibrary& = delete;

	auto operator=(BehaviourTreeLibrary&&) -> BehaviourTreeLibrary& = delete;

	/**
	 * @brief
	 *  The tree of an asset, read from the pack or the asset folders the first time.
	 * @param[in] iAsset The asset path.
	 * @return The tree, null when it cannot be read.
	 */
	[[nodiscard]] auto get(std::string_view iAsset) -> shared<const BehaviourTree>;

	/**
	 * @brief
	 *  Forget a tree: the next `get` reads its file again (after an edit).
	 * @param[in] iAsset The asset path.
	 */
	void invalidate(std::string_view iAsset);

	/**
	 * @brief
	 *  Forget every tree.
	 */
	void clear();

	/**
	 * @brief
	 *  Number of files read so far.
	 * @return The count.
	 */
	[[nodiscard]] auto getLoadCount() const -> uint64_t;

private:
	/// Guards the trees.
	mutable std::mutex m_mutex;
	/// Trees by asset path (null for a file that failed to load).
	std::unordered_map<std::string, shared<const BehaviourTree>> m_trees;
	/// Number of files read.
	uint64_t m_loadCount = 0;
};

}// namespace owl::scene::ai
