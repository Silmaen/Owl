/**
 * @file InterestManager.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/InterestManager.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace owl::network {

auto InterestManager::isRelevant(const InterestCandidate& iCandidate, const std::span<const math::vec3> iViewpoints)
		-> bool {
	if (iCandidate.relevance <= 0.f || iViewpoints.empty())
		return true;
	const float range = iCandidate.relevance * iCandidate.relevance;
	return std::ranges::any_of(iViewpoints, [&iCandidate, range](const math::vec3& iViewpoint) -> bool {
		const math::vec3 delta = iCandidate.position - iViewpoint;
		return delta.normSq() <= range;
	});
}

auto InterestManager::select(const PlayerId iPlayer, const std::span<const InterestCandidate> iCandidates,
							 const std::span<const math::vec3> iViewpoints, const uint32_t iBudget)
		-> std::vector<uint64_t> {
	auto& accumulators = m_clients[iPlayer];
	Accumulators next;
	std::vector<std::pair<float, uint64_t>> ranked;
	for (const auto& candidate: iCandidates) {
		if (!isRelevant(candidate, iViewpoints))
			continue;
		const auto found = accumulators.find(candidate.entity);
		const float urgency = (found == accumulators.end() ? 0.f : found->second) + candidate.priority;
		next[candidate.entity] = urgency;
		ranked.emplace_back(urgency, candidate.entity);
	}
	// The entity order breaks the ties, so that the same state picks the same entities on every run.
	std::ranges::sort(ranked, [](const auto& iLeft, const auto& iRight) -> bool {
		return iLeft.first > iRight.first || (iLeft.first >= iRight.first && iLeft.second < iRight.second);
	});
	if (ranked.size() > iBudget)
		ranked.resize(iBudget);
	std::vector<uint64_t> chosen;
	chosen.reserve(ranked.size());
	for (const auto& [urgency, entity]: ranked) {
		next[entity] = 0.f;
		chosen.push_back(entity);
	}
	accumulators = std::move(next);
	return chosen;
}

auto InterestManager::getRelevant(const PlayerId iPlayer) const -> std::vector<uint64_t> {
	std::vector<uint64_t> relevant;
	if (const auto found = m_clients.find(iPlayer); found != m_clients.end()) {
		for (const auto& entity: found->second | std::views::keys) relevant.push_back(entity);
	}
	std::ranges::sort(relevant);
	return relevant;
}

}// namespace owl::network
