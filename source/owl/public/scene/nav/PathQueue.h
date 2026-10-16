/**
 * @file PathQueue.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "scene/nav/NavMap.h"
#include "scene/nav/Pathfinder.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::scene::nav {

/// Handle of a path request (0 is never given).
using PathRequestId = uint32_t;

/// Where a path request stands.
enum struct PathStatus : uint8_t {
	Unknown = 0,///< No such request (never made, or released).
	Pending = 1,///< Waiting for its turn or being searched.
	Found = 2,///< The path is ready.
	NotFound = 3,///< No path joins the cells.
};

/**
 * @brief
 *  Path requests of one `NavMap`, served in order with a budget of node expansions per call.
 *
 * `request` queues a query and returns a handle; `update` (called once per frame by the scene's navigation system)
 * spends its budget on the oldest pending requests, so a long query is spread over several frames; `getStatus` and
 * `getResult` poll. A search that was running when the map changed starts over on the new graph. The slots (and
 * their path buffers) of released requests are reused: a queue that reached its steady load allocates nothing.
 */
class OWL_API PathQueue final {
public:
	PathQueue();

	~PathQueue();

	/**
	 * @brief
	 *  Copy constructor.
	 */
	PathQueue(const PathQueue&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	PathQueue(PathQueue&&) noexcept;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const PathQueue&) -> PathQueue& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(PathQueue&&) noexcept -> PathQueue&;

	/**
	 * @brief
	 *  Queue a query.
	 * @param[in] iFrom The start cell.
	 * @param[in] iTo The goal cell.
	 * @param[in] iMode The algorithm.
	 * @return The handle of the request.
	 */
	auto request(const math::vec2ui& iFrom, const math::vec2ui& iTo, PathMode iMode = PathMode::Auto) -> PathRequestId;

	/**
	 * @brief
	 *  State of a request.
	 * @param[in] iId The handle.
	 * @return The status.
	 */
	[[nodiscard]] auto getStatus(PathRequestId iId) const -> PathStatus;

	/**
	 * @brief
	 *  Path of a finished request.
	 * @param[in] iId The handle.
	 * @return The path, nullptr unless the request is `Found`. Valid until the request is released or the queue
	 *  asked for another request.
	 */
	[[nodiscard]] auto getResult(PathRequestId iId) const -> const PathResult*;

	/**
	 * @brief
	 *  Drop a request (pending or finished); its slot is reused.
	 * @param[in] iId The handle.
	 */
	void release(PathRequestId iId);

	/**
	 * @brief
	 *  Serve the pending requests, oldest first.
	 * @param[in] iMap The graph, already synchronised with its tilemap.
	 * @param[in] iBudget Node expansions to spend in this call (a request that does not fit stays current).
	 * @return The node expansions spent.
	 */
	auto update(const NavMap& iMap, uint32_t iBudget) -> uint64_t;

	/**
	 * @brief
	 *  Number of requests waiting or running.
	 * @return The count.
	 */
	[[nodiscard]] auto getPendingCount() const -> size_t;

	/**
	 * @brief
	 *  Drop every request.
	 */
	void clear();

private:
	/// One request.
	struct Slot {
		PathRequestId id = 0;
		PathStatus status = PathStatus::Unknown;
		PathMode mode = PathMode::Auto;
		math::vec2ui from{0, 0};
		math::vec2ui to{0, 0};
		PathResult result;
	};

	/**
	 * @brief
	 *  Slot of a handle.
	 * @param[in] iId The handle.
	 * @return The slot, nullptr when unknown.
	 */
	[[nodiscard]] auto findSlot(PathRequestId iId) const -> const Slot*;

	/// The requests, free slots included (id 0).
	std::vector<Slot> m_slots;
	/// The search.
	uniq<Pathfinder> mp_finder;
	/// Handle of the request being searched (0 when none).
	PathRequestId m_current = 0;
	/// Last handle given.
	PathRequestId m_lastId = 0;
};

}// namespace owl::scene::nav
