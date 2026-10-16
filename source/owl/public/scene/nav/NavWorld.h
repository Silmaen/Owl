/**
 * @file NavWorld.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/nav/NavMap.h"
#include "scene/nav/NavProfile.h"
#include "scene/nav/PathQueue.h"

#include <cstdint>
#include <unordered_map>

namespace owl::scene {

class Scene;
class TilemapAsset;
}// namespace owl::scene

namespace owl::scene::nav {

/**
 * @brief
 *  The navigation state of a scene: one graph and one request queue per tilemap entity that has a profile.
 *
 * Lives in the scene (`Scene::getNavigation`), is created on first use and dropped with it. The scene's
 * `owl.navigation` system calls `update` every frame.
 */
class OWL_API NavWorld final {
public:
	/// Graph and requests of one tilemap entity.
	struct Entry {
		/// The graph.
		NavMap map;
		/// The requests.
		PathQueue queue;
	};

	/// Node expansions spent on the requests of the scene each frame, by default.
	static constexpr uint32_t defaultBudget = 4000;

	NavWorld();

	~NavWorld();

	/**
	 * @brief
	 *  Copy constructor.
	 */
	NavWorld(const NavWorld&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	NavWorld(NavWorld&&) = delete;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const NavWorld&) -> NavWorld& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(NavWorld&&) -> NavWorld& = delete;

	/**
	 * @brief
	 *  The entry of a tilemap entity, brought up to date with its tilemap.
	 *
	 * Created on first use; new rules rebuild the graph, an edited tilemap rebuilds the chunks that changed.
	 * @param[in] iKey Key of the entity (its UUID).
	 * @param[in] iAsset The tilemap.
	 * @param[in] iProfile The rules (not empty).
	 * @return The entry.
	 */
	auto acquire(uint64_t iKey, const TilemapAsset& iAsset, const NavProfile& iProfile) -> Entry&;

	/**
	 * @brief
	 *  The entry of a tilemap entity, without updating it.
	 * @param[in] iKey Key of the entity.
	 * @return The entry, nullptr when none.
	 */
	[[nodiscard]] auto find(uint64_t iKey) -> Entry*;

	/**
	 * @brief
	 *  Serve the pending requests of the scene within the budget of the frame.
	 * @param[in,out] ioScene The scene whose tilemaps are read.
	 */
	void update(Scene& ioScene);

	/**
	 * @brief
	 *  Set the node expansions spent on requests each frame.
	 * @param[in] iBudget The budget (at least 1).
	 */
	void setBudget(uint32_t iBudget);

	/**
	 * @brief
	 *  Node expansions spent on requests each frame.
	 * @return The budget.
	 */
	[[nodiscard]] auto getBudget() const -> uint32_t { return m_budget; }

	/**
	 * @brief
	 *  Drop every graph and request.
	 */
	void clear();

private:
	/// Entries by entity.
	std::unordered_map<uint64_t, Entry> m_entries;
	/// Node expansions per frame.
	uint32_t m_budget = defaultBudget;
};

}// namespace owl::scene::nav
