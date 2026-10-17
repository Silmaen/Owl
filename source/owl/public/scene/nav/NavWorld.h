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
#include <map>
#include <span>
#include <unordered_map>
#include <vector>

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
	/// A route a script asks the debug overlay to draw.
	struct DebugRoute {
		/// Cells of the route, in order.
		std::vector<math::vec2ui> cells;
		/// Colour of the line (RGB).
		math::vec3 color{1.f, 1.f, 1.f};
	};

	/// Who asks for the debug overlay: a script (a key of the game) or the editor (Show menu of the viewport).
	enum struct DebugSource : uint8_t {
		/// A script, through `path.set_debug`.
		Script = 1,
		/// The editor viewport.
		Editor = 2,
	};

	/// Graph and requests of one tilemap entity.
	struct Entry {
		/// The graph.
		NavMap map;
		/// The requests.
		PathQueue queue;
		/// The routes to draw, by the key a script gave them.
		std::map<uint64_t, DebugRoute> debugRoutes;
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

	/**
	 * @brief
	 *  Ask for, or stop asking for, the debug overlay (graph and routes drawn over the maps).
	 * @param[in] iSource Who asks.
	 * @param[in] iVisible True to show.
	 */
	void setDebugVisible(DebugSource iSource, bool iVisible);

	/**
	 * @brief
	 *  Whether the debug overlay is on, whoever asked.
	 * @return True when drawn.
	 */
	[[nodiscard]] auto isDebugVisible() const -> bool { return m_debugRequests != 0; }

	/**
	 * @brief
	 *  Set the route drawn for a key, replacing the previous one; an empty route removes it.
	 * @param[in] iKey Key of the tilemap entity.
	 * @param[in] iRouteId Key of the route (a vehicle, for instance).
	 * @param[in] iCells Cells of the route.
	 * @param[in] iColor Colour of the line.
	 */
	void setDebugRoute(uint64_t iKey, uint64_t iRouteId, std::span<const math::vec2ui> iCells,
					   const math::vec3& iColor);

	/**
	 * @brief
	 *  Remove every debug route of a tilemap entity.
	 * @param[in] iKey Key of the tilemap entity.
	 */
	void clearDebugRoutes(uint64_t iKey);

	/**
	 * @brief
	 *  Every entry, to draw them.
	 * @return The entries by entity key.
	 */
	[[nodiscard]] auto getEntries() const -> const std::unordered_map<uint64_t, Entry>& { return m_entries; }

private:
	/// Entries by entity.
	std::unordered_map<uint64_t, Entry> m_entries;
	/// Node expansions per frame.
	uint32_t m_budget = defaultBudget;
	/// Bits of `DebugSource` asking for the overlay.
	uint8_t m_debugRequests = 0;
};

}// namespace owl::scene::nav
