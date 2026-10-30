/**
 * @file SplineCurve.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::math {

/**
 * @brief
 *  How a `SplineCurve` joins its control points.
 */
enum struct SplineInterpolation : uint8_t {
	Linear,///< Straight segments between the points.
	CatmullRom,///< A smooth curve through every point, its tangents computed from the neighbours.
	Bezier,///< A cubic Bezier curve through every point, shaped by the tangent handles of each point.
};

/**
 * @brief
 *  A control point of a `SplineCurve`.
 */
struct OWL_API SplinePoint {
	/// Position of the point.
	vec3 position{0.f, 0.f, 0.f};
	/// Bezier handle before the point, as an offset from it (unused by the other interpolations).
	vec3 tangentIn{0.f, 0.f, 0.f};
	/// Bezier handle after the point, as an offset from it (unused by the other interpolations).
	vec3 tangentOut{0.f, 0.f, 0.f};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two points field by field.
	 * @param[in] iOther The other point.
	 * @return True when every field is equal.
	 */
	auto operator==(const SplinePoint& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  A point of a `SplineCurve` found by distance.
 */
struct OWL_API SplineSample {
	/// Position on the curve.
	vec3 position{0.f, 0.f, 0.f};
	/// Unit direction of the curve there (null on a curve of one point).
	vec3 tangent{0.f, 0.f, 0.f};
	/// Distance from the first point along the curve.
	float distance{0.f};
	/// Curve parameter: the segment index plus the position in it, from 0 to the segment count.
	float parameter{0.f};
};

/**
 * @brief
 *  A curve through control points, open or closed, sampled by distance along it.
 *
 * Every interpolation is evaluated as one cubic Bezier segment per pair of points. An arc-length table (`resolution`
 * samples per segment) is rebuilt by each setter, so the queries are constant: `sampleAtDistance` is a binary search
 * in it, and two threads may read one curve.
 */
class OWL_API SplineCurve final {
public:
	/**
	 * @brief
	 *  Build an empty open Catmull-Rom curve.
	 */
	SplineCurve() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~SplineCurve() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	SplineCurve(const SplineCurve&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	SplineCurve(SplineCurve&&) noexcept = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This curve.
	 */
	auto operator=(const SplineCurve&) -> SplineCurve& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This curve.
	 */
	auto operator=(SplineCurve&&) noexcept -> SplineCurve& = default;

	/**
	 * @brief
	 *  Compare the definitions of two curves (points, interpolation, closure, resolution).
	 * @param[in] iOther The other curve.
	 * @return True when they describe the same curve.
	 */
	[[nodiscard]] auto operator==(const SplineCurve& iOther) const -> bool;

	/**
	 * @brief
	 *  Replace the control points.
	 * @param[in] iPoints The points, in order.
	 */
	void setPoints(std::vector<SplinePoint> iPoints);

	/**
	 * @brief
	 *  The control points.
	 * @return The points, in order.
	 */
	[[nodiscard]] auto getPoints() const -> const std::vector<SplinePoint>& { return m_points; }

	/**
	 * @brief
	 *  Choose how the points are joined.
	 * @param[in] iInterpolation The interpolation.
	 */
	void setInterpolation(SplineInterpolation iInterpolation);

	/**
	 * @brief
	 *  How the points are joined.
	 * @return The interpolation.
	 */
	[[nodiscard]] auto getInterpolation() const -> SplineInterpolation { return m_interpolation; }

	/**
	 * @brief
	 *  Open or close the curve (a closed curve joins its last point to the first).
	 * @param[in] iClosed Whether the curve is closed.
	 */
	void setClosed(bool iClosed);

	/**
	 * @brief
	 *  Whether the curve joins its last point to the first.
	 * @return True when closed.
	 */
	[[nodiscard]] auto isClosed() const -> bool { return m_closed; }

	/**
	 * @brief
	 *  Set the number of arc-length samples per segment.
	 * @param[in] iResolution Samples per segment, clamped to [1, 256].
	 */
	void setResolution(uint32_t iResolution);

	/**
	 * @brief
	 *  The number of arc-length samples per segment.
	 * @return Samples per segment.
	 */
	[[nodiscard]] auto getResolution() const -> uint32_t { return m_resolution; }

	/**
	 * @brief
	 *  Number of segments: one per pair of consecutive points, plus the closing one.
	 * @return The segment count, 0 below two points.
	 */
	[[nodiscard]] auto getSegmentCount() const -> size_t { return m_controls.size() / 4; }

	/**
	 * @brief
	 *  Length of the curve, measured on the arc-length table.
	 * @return The length.
	 */
	[[nodiscard]] auto getLength() const -> float { return m_lengths.empty() ? 0.f : m_lengths.back(); }

	/**
	 * @brief
	 *  Position at a curve parameter.
	 * @param[in] iParameter Segment index plus the position in it, from 0 to the segment count (wrapped when closed).
	 * @return The position; the only point of a one-point curve, the origin of an empty one.
	 */
	[[nodiscard]] auto evaluate(float iParameter) const -> vec3;

	/**
	 * @brief
	 *  Derivative of the position at a curve parameter.
	 * @param[in] iParameter Segment index plus the position in it.
	 * @return The derivative, null below two points.
	 */
	[[nodiscard]] auto getDerivative(float iParameter) const -> vec3;

	/**
	 * @brief
	 *  Curve parameter at a distance along the curve.
	 * @param[in] iDistance Distance from the first point: clamped to the length on an open curve, wrapped on a closed
	 *  one.
	 * @return The parameter.
	 */
	[[nodiscard]] auto getParameterAtDistance(float iDistance) const -> float;

	/**
	 * @brief
	 *  Distance along the curve at a curve parameter.
	 * @param[in] iParameter Segment index plus the position in it.
	 * @return The distance from the first point.
	 */
	[[nodiscard]] auto getDistanceAtParameter(float iParameter) const -> float;

	/**
	 * @brief
	 *  Position and direction at a distance along the curve.
	 * @param[in] iDistance Distance from the first point: clamped on an open curve, wrapped on a closed one.
	 * @return The sample.
	 */
	[[nodiscard]] auto sampleAtDistance(float iDistance) const -> SplineSample;

	/**
	 * @brief
	 *  Distance along the curve of the point of the curve nearest to a position.
	 * @param[in] iPosition The position.
	 * @return The distance from the first point (0 below two points).
	 */
	[[nodiscard]] auto getNearestDistance(const vec3& iPosition) const -> float;

	/**
	 * @brief
	 *  The positions of the arc-length table, to draw the curve as a polyline.
	 * @return The samples, from the first point to the last (the first again when closed).
	 */
	[[nodiscard]] auto getPolyline() const -> const std::vector<vec3>& { return m_samples; }

	/**
	 * @brief
	 *  Bezier handle that makes a point smooth: a sixth of the vector between its neighbours (the Catmull-Rom
	 *  tangent), towards the next point.
	 * @param[in] iIndex Index of the point.
	 * @return The `tangentOut` handle (`tangentIn` is its opposite), null for an unknown point.
	 */
	[[nodiscard]] auto getSmoothTangent(size_t iIndex) const -> vec3;

	/**
	 * @brief
	 *  Insert a control point on the curve without moving it: the Bezier handles around it are split exactly, the
	 *  Catmull-Rom shape changes a little near the new point.
	 * @param[in] iParameter Curve parameter of the new point.
	 * @return The index of the new point, or the point count when the curve has fewer than two points.
	 */
	auto insertAtParameter(float iParameter) -> size_t;

private:
	/// Rebuild the Bezier controls and the arc-length table from the definition.
	void rebuild();

	/**
	 * @brief
	 *  Split a parameter into a segment index and a position in it.
	 * @param[in] iParameter The curve parameter.
	 * @param[out] oLocal The position in the segment, in [0, 1].
	 * @return The segment index.
	 */
	[[nodiscard]] auto locate(float iParameter, float& oLocal) const -> size_t;

	/// Control points, in order.
	std::vector<SplinePoint> m_points;
	/// How the points are joined.
	SplineInterpolation m_interpolation{SplineInterpolation::CatmullRom};
	/// Whether the last point joins the first.
	bool m_closed{false};
	/// Arc-length samples per segment.
	uint32_t m_resolution{16};
	/// Four Bezier controls per segment.
	std::vector<vec3> m_controls;
	/// Cumulative length at each sample of the table.
	std::vector<float> m_lengths;
	/// Position at each sample of the table.
	std::vector<vec3> m_samples;
};

}// namespace owl::math
