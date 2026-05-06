/**
 * @file Curve.cpp
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/Curve.h"

#include <algorithm>

namespace owl::math {

namespace {
auto smoothstep(const float iT) -> float {
	const float t = std::clamp(iT, 0.f, 1.f);
	return t * t * (3.f - 2.f * t);
}

}// namespace

void Curve::addKey(const Keyframe iKey) {
	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	const auto it =
			std::ranges::find_if(m_keys, [&](const Keyframe& iEx) -> bool { return iEx.time == iKey.time; });
	OWL_DIAG_POP

	if (it != m_keys.end()) {
		it->value = iKey.value;
		return;
	}
	const auto pos = std::ranges::upper_bound(m_keys, iKey.time, {}, &Keyframe::time);
	m_keys.insert(pos, iKey);
}

void Curve::removeKey(const size_t iIndex) {
	if (iIndex >= m_keys.size())
		return;
	m_keys.erase(m_keys.begin() + static_cast<std::ptrdiff_t>(iIndex));
}

void Curve::setKey(const size_t iIndex, const Keyframe iKey) {
	if (iIndex >= m_keys.size())
		return;
	m_keys[iIndex] = iKey;
	std::ranges::sort(m_keys, {}, &Keyframe::time);
}

auto Curve::evaluate(const float iTime) const -> float {
	if (m_keys.empty())
		return 0.f;
	if (iTime <= m_keys.front().time)
		return m_keys.front().value;
	if (iTime >= m_keys.back().time)
		return m_keys.back().value;
	// Find the right neighbour (first key with time > iTime).
	const auto right = std::ranges::upper_bound(m_keys, iTime, {}, &Keyframe::time);
	const auto left = right - 1;
	const float span = right->time - left->time;
	if (span <= 0.f)
		return left->value;
	const float t = (iTime - left->time) / span;
	switch (m_interpolation) {
		case CurveInterpolation::Constant:
			return left->value;
		case CurveInterpolation::Linear:
			return left->value + (right->value - left->value) * t;
		case CurveInterpolation::Smooth:
			return left->value + (right->value - left->value) * smoothstep(t);
	}
	return left->value;
}

}// namespace owl::math
