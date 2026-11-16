/**
 * @file InterestManager.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "network/Match.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace owl::network {

/**
 * @brief
 *  A replicated entity the server may send to a client.
 */
struct OWL_API InterestCandidate {
	/// UUID of the entity.
	uint64_t entity = 0;
	/// World position.
	math::vec3 position{0.f, 0.f, 0.f};
	/// Distance within which a client receives it from one of its viewpoints; 0 for everywhere.
	float relevance = 0.f;
	/// Priority accumulated per snapshot it is not sent.
	float priority = 1.f;
};

/**
 * @brief
 *  Which entities each client receives: the relevant ones (near one of its viewpoints), the most overdue first within
 *  the snapshot budget, so that no relevant entity starves.
 */
class OWL_API InterestManager final {
public:
	/**
	 * @brief
	 *  Whether an entity is relevant to viewpoints.
	 * @param[in] iCandidate The entity.
	 * @param[in] iViewpoints The positions of the client; none for a client that sees everything.
	 * @return True when the entity is relevant everywhere or near a viewpoint.
	 */
	[[nodiscard]] static auto isRelevant(const InterestCandidate& iCandidate, std::span<const math::vec3> iViewpoints)
			-> bool;

	/**
	 * @brief
	 *  Choose the entities of a client's next snapshot and age the ones left out.
	 * @param[in] iPlayer The client.
	 * @param[in] iCandidates Every replicated entity.
	 * @param[in] iViewpoints The positions of the client; none for a client that sees everything.
	 * @param[in] iBudget Most entities of the snapshot.
	 * @return The UUIDs of the entities to send, the most overdue first.
	 */
	[[nodiscard]] auto select(PlayerId iPlayer, std::span<const InterestCandidate> iCandidates,
							  std::span<const math::vec3> iViewpoints, uint32_t iBudget) -> std::vector<uint64_t>;

	/**
	 * @brief
	 *  Entities relevant to a client at its last selection, sent or not.
	 * @param[in] iPlayer The client.
	 * @return The UUIDs.
	 */
	[[nodiscard]] auto getRelevant(PlayerId iPlayer) const -> std::vector<uint64_t>;

	/**
	 * @brief
	 *  Forget a client that left.
	 * @param[in] iPlayer The client.
	 */
	void forget(PlayerId iPlayer) { m_clients.erase(iPlayer); }

	/**
	 * @brief
	 *  Forget every client.
	 */
	void clear() { m_clients.clear(); }

private:
	/// The priority each relevant entity accumulated for a client, by UUID.
	using Accumulators = std::unordered_map<uint64_t, float>;
	/// Accumulators per client.
	std::unordered_map<PlayerId, Accumulators> m_clients;
};

}// namespace owl::network
