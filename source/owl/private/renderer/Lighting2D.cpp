/**
 * @file Lighting2D.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Lighting2D.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>

namespace owl::renderer {

namespace {
constexpr float g_twoPi = 2.f * std::numbers::pi_v<float>;

auto cross(const math::vec2& iA, const math::vec2& iB) -> float { return iA.x() * iB.y() - iA.y() * iB.x(); }

auto distanceToSegment(const math::vec2& iPoint, const Occluder2D& iSegment) -> float {
	const math::vec2 edge = iSegment.end - iSegment.start;
	const float lengthSq = edge.normSq();
	const float t = lengthSq > 0.f ? std::clamp(((iPoint - iSegment.start) * edge) / lengthSq, 0.f, 1.f) : 0.f;
	return (iSegment.start + edge * t - iPoint).norm();
}

auto binOf(const float iAngle, const size_t iBins) -> int64_t {
	const auto bins = static_cast<int64_t>(iBins);
	const auto bin =
			static_cast<int64_t>(std::floor((iAngle + std::numbers::pi_v<float>) / g_twoPi * static_cast<float>(bins)));
	return ((bin % bins) + bins) % bins;
}
}// namespace

void buildShadowRow2D(const math::vec2& iLight, const float iRadius, const std::span<const Occluder2D> iOccluders,
					  const std::span<float> oRow) {
	std::ranges::fill(oRow, iRadius);
	if (oRow.empty())
		return;
	const auto bins = oRow.size();
	const float binWidth = g_twoPi / static_cast<float>(bins);
	for (const auto& segment: iOccluders) {
		if (distanceToSegment(iLight, segment) >= iRadius)
			continue;
		const math::vec2 toStart = segment.start - iLight;
		const math::vec2 edge = segment.end - segment.start;
		const float startAngle = std::atan2(toStart.y(), toStart.x());
		const math::vec2 toEnd = segment.end - iLight;
		float sweep = std::atan2(toEnd.y(), toEnd.x()) - startAngle;
		if (sweep > std::numbers::pi_v<float>)
			sweep -= g_twoPi;
		else if (sweep < -std::numbers::pi_v<float>)
			sweep += g_twoPi;
		const int64_t step = sweep >= 0.f ? 1 : -1;
		const auto count = static_cast<int64_t>(std::ceil(std::abs(sweep) / binWidth)) + 1;
		const int64_t first = binOf(startAngle, bins);
		for (int64_t index = 0; index <= count; ++index) {
			const auto bin = static_cast<size_t>(
					((first + index * step) % static_cast<int64_t>(bins) + static_cast<int64_t>(bins)) %
					static_cast<int64_t>(bins));
			const float angle = -std::numbers::pi_v<float> + (static_cast<float>(bin) + 0.5f) * binWidth;
			const math::vec2 ray{std::cos(angle), std::sin(angle)};
			const float denominator = cross(ray, edge);
			if (std::abs(denominator) < 1e-9f)
				continue;
			const float along = cross(toStart, edge) / denominator;
			const float onSegment = cross(toStart, ray) / denominator;
			if (along <= 0.f || onSegment < 0.f || onSegment > 1.f)
				continue;
			oRow[bin] = std::min(oRow[bin], along);
		}
	}
}

auto lightAt2D(const Light2DData& iLight, const math::vec2& iPoint) -> math::vec3 {
	const math::vec2 offset = iPoint - iLight.position;
	const float distance = offset.norm();
	if (distance >= iLight.radius)
		return {0.f, 0.f, 0.f};
	float attenuation = std::pow(1.f - distance / std::max(iLight.radius, 1e-4f), iLight.falloff);
	if (iLight.cosOuter >= -1.f && distance > 1e-6f) {
		const float cosine = (offset * (1.f / distance)) * iLight.direction;
		const float span = std::max(iLight.cosInner - iLight.cosOuter, 1e-4f);
		const float t = std::clamp((cosine - iLight.cosOuter) / span, 0.f, 1.f);
		attenuation *= t * t * (3.f - 2.f * t);
	}
	return iLight.color * attenuation;
}

}// namespace owl::renderer
