/**
 * @file SplineCurve.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/SplineCurve.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace owl::math {

namespace {

auto bezier(const vec3& iB0, const vec3& iB1, const vec3& iB2, const vec3& iB3, const float iT) -> vec3 {
	const float u = 1.f - iT;
	return iB0 * (u * u * u) + iB1 * (3.f * u * u * iT) + iB2 * (3.f * u * iT * iT) + iB3 * (iT * iT * iT);
}

auto bezierDerivative(const vec3& iB0, const vec3& iB1, const vec3& iB2, const vec3& iB3, const float iT) -> vec3 {
	const float u = 1.f - iT;
	return (iB1 - iB0) * (3.f * u * u) + (iB2 - iB1) * (6.f * u * iT) + (iB3 - iB2) * (3.f * iT * iT);
}

auto lerp(const vec3& iA, const vec3& iB, const float iT) -> vec3 { return iA + (iB - iA) * iT; }

auto unit(const vec3& iVector) -> vec3 {
	const float length = iVector.norm();
	return length > std::numeric_limits<float>::epsilon() ? iVector * (1.f / length) : vec3{0.f, 0.f, 0.f};
}

}// namespace

auto SplineCurve::operator==(const SplineCurve& iOther) const -> bool {
	return m_points == iOther.m_points && m_interpolation == iOther.m_interpolation && m_closed == iOther.m_closed &&
		   m_resolution == iOther.m_resolution;
}

void SplineCurve::setPoints(std::vector<SplinePoint> iPoints) {
	m_points = std::move(iPoints);
	rebuild();
}

void SplineCurve::setInterpolation(const SplineInterpolation iInterpolation) {
	m_interpolation = iInterpolation;
	rebuild();
}

void SplineCurve::setClosed(const bool iClosed) {
	m_closed = iClosed;
	rebuild();
}

void SplineCurve::setResolution(const uint32_t iResolution) {
	m_resolution = std::clamp(iResolution, 1u, 256u);
	rebuild();
}

auto SplineCurve::getSmoothTangent(const size_t iIndex) const -> vec3 {
	const size_t count = m_points.size();
	if (iIndex >= count || count < 2)
		return {0.f, 0.f, 0.f};
	const bool wraps = m_closed && count > 2;
	const size_t previous = iIndex > 0 ? iIndex - 1 : (wraps ? count - 1 : 0);
	const size_t next = iIndex + 1 < count ? iIndex + 1 : (wraps ? 0 : count - 1);
	return (m_points[next].position - m_points[previous].position) * (1.f / 6.f);
}

void SplineCurve::rebuild() {
	m_controls.clear();
	m_lengths.clear();
	m_samples.clear();
	const size_t count = m_points.size();
	if (count == 1) {
		m_samples.push_back(m_points.front().position);
		m_lengths.push_back(0.f);
	}
	if (count < 2)
		return;
	const size_t segments = m_closed ? count : count - 1;
	m_controls.reserve(segments * 4);
	for (size_t segment = 0; segment < segments; ++segment) {
		const auto& start = m_points[segment];
		const auto& end = m_points[(segment + 1) % count];
		switch (m_interpolation) {
			case SplineInterpolation::Linear:
				m_controls.insert(m_controls.end(), {start.position, lerp(start.position, end.position, 1.f / 3.f),
													 lerp(start.position, end.position, 2.f / 3.f), end.position});
				break;
			case SplineInterpolation::CatmullRom:
				m_controls.insert(m_controls.end(),
								  {start.position, start.position + getSmoothTangent(segment),
								   end.position - getSmoothTangent((segment + 1) % count), end.position});
				break;
			case SplineInterpolation::Bezier:
				m_controls.insert(m_controls.end(), {start.position, start.position + start.tangentOut,
													 end.position + end.tangentIn, end.position});
				break;
		}
	}
	const size_t samples = segments * m_resolution;
	m_samples.reserve(samples + 1);
	m_lengths.reserve(samples + 1);
	m_samples.push_back(m_controls.front());
	m_lengths.push_back(0.f);
	for (size_t index = 1; index <= samples; ++index) {
		const size_t segment = (index - 1) / m_resolution;
		const float local = static_cast<float>(index - segment * m_resolution) / static_cast<float>(m_resolution);
		const vec3 position = bezier(m_controls[segment * 4], m_controls[segment * 4 + 1], m_controls[segment * 4 + 2],
									 m_controls[segment * 4 + 3], local);
		m_lengths.push_back(m_lengths.back() + (position - m_samples.back()).norm());
		m_samples.push_back(position);
	}
}

auto SplineCurve::locate(const float iParameter, float& oLocal) const -> size_t {
	const size_t segments = getSegmentCount();
	const auto total = static_cast<float>(segments);
	float parameter = iParameter;
	if (m_closed)
		parameter = std::fmod(std::fmod(parameter, total) + total, total);
	parameter = std::clamp(parameter, 0.f, total);
	const auto segment = std::min(static_cast<size_t>(parameter), segments - 1);
	oLocal = parameter - static_cast<float>(segment);
	return segment;
}

auto SplineCurve::evaluate(const float iParameter) const -> vec3 {
	if (m_points.empty())
		return {0.f, 0.f, 0.f};
	if (getSegmentCount() == 0)
		return m_points.front().position;
	float local = 0.f;
	const size_t segment = locate(iParameter, local);
	return bezier(m_controls[segment * 4], m_controls[segment * 4 + 1], m_controls[segment * 4 + 2],
				  m_controls[segment * 4 + 3], local);
}

auto SplineCurve::getDerivative(const float iParameter) const -> vec3 {
	if (getSegmentCount() == 0)
		return {0.f, 0.f, 0.f};
	float local = 0.f;
	const size_t segment = locate(iParameter, local);
	return bezierDerivative(m_controls[segment * 4], m_controls[segment * 4 + 1], m_controls[segment * 4 + 2],
							m_controls[segment * 4 + 3], local);
}

auto SplineCurve::getParameterAtDistance(const float iDistance) const -> float {
	const float length = getLength();
	if (getSegmentCount() == 0 || length <= 0.f)
		return 0.f;
	float distance = iDistance;
	if (m_closed)
		distance = std::fmod(std::fmod(distance, length) + length, length);
	distance = std::clamp(distance, 0.f, length);
	const auto upper = std::ranges::upper_bound(m_lengths, distance);
	if (upper == m_lengths.end())
		return static_cast<float>(getSegmentCount());
	const auto index = static_cast<size_t>(std::distance(m_lengths.begin(), upper));
	const float before = m_lengths[index - 1];
	const float span = m_lengths[index] - before;
	const float fraction = span > 0.f ? (distance - before) / span : 0.f;
	return (static_cast<float>(index - 1) + fraction) / static_cast<float>(m_resolution);
}

auto SplineCurve::getDistanceAtParameter(const float iParameter) const -> float {
	if (getSegmentCount() == 0)
		return 0.f;
	float local = 0.f;
	const size_t segment = locate(iParameter, local);
	const float steps = (static_cast<float>(segment) + local) * static_cast<float>(m_resolution);
	const auto index = std::min(static_cast<size_t>(steps), m_lengths.size() - 1);
	if (index + 1 >= m_lengths.size())
		return m_lengths.back();
	const float fraction = steps - static_cast<float>(index);
	return m_lengths[index] + (m_lengths[index + 1] - m_lengths[index]) * fraction;
}

auto SplineCurve::sampleAtDistance(const float iDistance) const -> SplineSample {
	if (getSegmentCount() == 0)
		return {.position = evaluate(0.f), .tangent = {0.f, 0.f, 0.f}, .distance = 0.f, .parameter = 0.f};
	const float parameter = getParameterAtDistance(iDistance);
	const float length = getLength();
	float distance = iDistance;
	if (m_closed && length > 0.f)
		distance = std::fmod(std::fmod(distance, length) + length, length);
	return {.position = evaluate(parameter),
			.tangent = unit(getDerivative(parameter)),
			.distance = std::clamp(distance, 0.f, length),
			.parameter = parameter};
}

auto SplineCurve::getNearestDistance(const vec3& iPosition) const -> float {
	if (m_samples.size() < 2)
		return 0.f;
	float best = std::numeric_limits<float>::max();
	float bestDistance = 0.f;
	for (size_t index = 0; index + 1 < m_samples.size(); ++index) {
		const vec3 chord = m_samples[index + 1] - m_samples[index];
		const float chordSq = chord.normSq();
		const float along =
				chordSq > 0.f ? std::clamp((iPosition - m_samples[index]) * chord / chordSq, 0.f, 1.f) : 0.f;
		if (const float missSq = (m_samples[index] + chord * along - iPosition).normSq(); missSq < best) {
			best = missSq;
			bestDistance = m_lengths[index] + (m_lengths[index + 1] - m_lengths[index]) * along;
		}
	}
	return bestDistance;
}

auto SplineCurve::insertAtParameter(const float iParameter) -> size_t {
	if (getSegmentCount() == 0)
		return m_points.size();
	float local = 0.f;
	const size_t segment = locate(iParameter, local);
	const size_t count = m_points.size();
	const vec3& b0 = m_controls[segment * 4];
	const vec3& b1 = m_controls[segment * 4 + 1];
	const vec3& b2 = m_controls[segment * 4 + 2];
	const vec3& b3 = m_controls[segment * 4 + 3];
	// De Casteljau split of the segment: the two halves draw the same curve.
	const vec3 a = lerp(b0, b1, local);
	const vec3 b = lerp(b1, b2, local);
	const vec3 c = lerp(b2, b3, local);
	const vec3 ab = lerp(a, b, local);
	const vec3 bc = lerp(b, c, local);
	const vec3 middle = lerp(ab, bc, local);
	SplinePoint point{.position = middle, .tangentIn = ab - middle, .tangentOut = bc - middle};
	if (m_interpolation == SplineInterpolation::Bezier) {
		m_points[segment].tangentOut = a - b0;
		m_points[(segment + 1) % count].tangentIn = c - b3;
	}
	const size_t index = segment + 1;
	m_points.insert(m_points.begin() + static_cast<std::ptrdiff_t>(index), point);
	rebuild();
	return index;
}

}// namespace owl::math
