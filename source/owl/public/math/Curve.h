/**
 * @file Curve.h
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <vector>

namespace owl::math {
/**
 * @brief
 *  Interpolation mode applied between two adjacent keyframes of a `Curve`.
 */
enum struct CurveInterpolation : uint8_t {
	Constant,///< Hold the left key value until the next key.
	Linear,///< Linear interpolation between the two key values.
	Smooth,///< Smoothstep interpolation (Hermite with zero tangents at endpoints).
};

/**
 * @brief
 *  Single sample on a `Curve`.
 */
struct OWL_API Keyframe {
	/// Time coordinate (curve domain). Curves are sorted ascending by `time`.
	float time{0.f};
	/// Sampled value at `time`.
	float value{0.f};
};

/**
 * @brief
 *  Sorted list of `Keyframe`s with an interpolation mode, sampled by `evaluate`.
 *
 * Curves are inclusive: out-of-range queries clamp to the first/last value (no
 * extrapolation). An empty curve evaluates to 0. Keyframes are kept sorted by `time`;
 * inserts that share an existing time replace the value at that time.
 */
class OWL_API Curve final {
public:
	Curve() = default;

	~Curve() = default;

	Curve(const Curve&) = default;

	Curve(Curve&&) noexcept = default;

	auto operator=(const Curve&) -> Curve& = default;

	auto operator=(Curve&&) noexcept -> Curve& = default;

	/**
	 * @brief
	 *  Insert a key, sorted by time. If a key already exists at the same time, its value is overwritten.
	 * @param[in] iKey Keyframe to add.
	 */
	void addKey(Keyframe iKey);

	/**
	 * @brief
	 *  Remove the key at the given index.
	 * @param[in] iIndex 0-based index of the key to remove. Out-of-range values are silently ignored.
	 */
	void removeKey(size_t iIndex);

	/**
	 * @brief
	 *  Replace the key at `iIndex`. The list is re-sorted if `iKey.time` shifts past a neighbour.
	 * @param[in] iIndex 0-based index of the key to set.
	 * @param[in] iKey New key value.
	 */
	void setKey(size_t iIndex, Keyframe iKey);

	/**
	 * @brief
	 *  Number of keyframes currently stored.
	 * @return The keyframe count.
	 */
	[[nodiscard]] auto keyCount() const noexcept -> size_t { return m_keys.size(); }

	/**
	 * @brief
	 *  Read-only access to a key.
	 * @param[in] iIndex 0-based index. Caller must ensure `iIndex < keyCount()`.
	 * @return The keyframe at `iIndex`.
	 */
	[[nodiscard]] auto key(size_t iIndex) const -> const Keyframe& { return m_keys[iIndex]; }

	/**
	 * @brief
	 *  Read-only span over the underlying key list (already sorted by time).
	 * @return The keyframe list.
	 */
	[[nodiscard]] auto keys() const noexcept -> const std::vector<Keyframe>& { return m_keys; }

	/**
	 * @brief
	 *  Set the interpolation mode applied between adjacent keys.
	 * @param[in] iMode The interpolation mode to apply on subsequent evaluations.
	 */
	void setInterpolation(CurveInterpolation iMode) noexcept { m_interpolation = iMode; }

	/**
	 * @brief
	 *  Current interpolation mode.
	 * @return The interpolation mode.
	 */
	[[nodiscard]] auto getInterpolation() const noexcept -> CurveInterpolation { return m_interpolation; }

	/**
	 * @brief
	 *  Whether the curve has no keyframes.
	 * @return True when no keyframes are stored.
	 */
	[[nodiscard]] auto empty() const noexcept -> bool { return m_keys.empty(); }

	/**
	 * @brief
	 *  Remove every keyframe.
	 */
	void clear() noexcept { m_keys.clear(); }

	/**
	 * @brief
	 *  Sample the curve at `iTime`.
	 * @param[in] iTime Query time.
	 * @return Empty curve → 0; before first key → first.value; after last → last.value;
	 *         otherwise interpolated between the bracketing keys per `getInterpolation()`.
	 */
	[[nodiscard]] auto evaluate(float iTime) const -> float;

private:
	/// Keyframes ordered by ascending time; binary-searched in `evaluate`.
	std::vector<Keyframe> m_keys;
	/// Interpolation mode used between adjacent keys.
	CurveInterpolation m_interpolation{CurveInterpolation::Linear};
};

}// namespace owl::math
