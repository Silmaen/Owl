/**
 * @file LagCompensation.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/LagCompensation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>

namespace owl::network {

void HitHistory::record(const double iTime, const uint64_t iEntity, const math::vec3& iCentre, const float iRadius) {
	auto& track = m_entities[iEntity];
	track.radius = iRadius;
	if (!track.samples.empty() && track.samples.back().time >= iTime) {
		track.samples.back().centre = iCentre;
		return;
	}
	track.samples.push_back({.time = iTime, .centre = iCentre});
}

void HitHistory::prune(const double iOldest) {
	for (auto it = m_entities.begin(); it != m_entities.end();) {
		auto& samples = it->second.samples;
		// The last sample before the window stays: the start of the window interpolates from it.
		while (samples.size() > 1 && samples[1].time <= iOldest) samples.pop_front();
		if (samples.empty() || samples.back().time < iOldest)
			it = m_entities.erase(it);
		else
			++it;
	}
}

auto HitHistory::getPositionAt(const uint64_t iEntity, const double iTime) const -> std::optional<math::vec3> {
	const auto found = m_entities.find(iEntity);
	if (found == m_entities.end() || found->second.samples.empty())
		return std::nullopt;
	const auto& samples = found->second.samples;
	if (iTime <= samples.front().time)
		return samples.front().centre;
	if (iTime >= samples.back().time)
		return samples.back().centre;
	const auto after = std::ranges::upper_bound(samples, iTime, std::less{},
												[](const Sample& iSample) -> double { return iSample.time; });
	const auto& next = *after;
	const auto& previous = *std::prev(after);
	const double span = next.time - previous.time;
	const auto share = static_cast<float>(span > 0.0 ? (iTime - previous.time) / span : 1.0);
	return previous.centre + (next.centre - previous.centre) * share;
}

auto HitHistory::raycast(const double iTime, const math::vec3& iOrigin, const math::vec3& iDirection,
						 const float iMaxDistance, const uint64_t iIgnored) const -> std::optional<RewindHit> {
	const float length = iDirection.norm();
	if (length <= 0.f || !std::isfinite(length))
		return std::nullopt;
	const math::vec3 direction = iDirection * (1.f / length);
	std::optional<RewindHit> nearest;
	for (const auto& [entity, track]: m_entities) {
		if (entity == iIgnored || track.radius <= 0.f)
			continue;
		const auto centre = getPositionAt(entity, iTime);
		if (!centre.has_value())
			continue;
		const math::vec3 toCentre = *centre - iOrigin;
		const float along = toCentre * direction;
		const float distanceSq = toCentre.normSq() - along * along;
		const float radiusSq = track.radius * track.radius;
		if (distanceSq > radiusSq)
			continue;
		const float entry = along - std::sqrt(std::max(0.f, radiusSq - distanceSq));
		const float distance = std::max(0.f, entry);
		if (along + track.radius < 0.f || distance > iMaxDistance)
			continue;
		if (!nearest.has_value() || distance < nearest->distance ||
			(distance <= nearest->distance && entity < nearest->entity))
			nearest = RewindHit{.entity = entity, .distance = distance};
	}
	return nearest;
}

}// namespace owl::network
