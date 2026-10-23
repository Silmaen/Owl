/**
 * @file Frustum.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/box.h"
#include "math/matrices.h"
#include "math/vectors.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace owl::math {

/**
 * @brief
 *  Where a box lies relative to a frustum.
 */
enum struct Containment : uint8_t {
	Outside,///< Entirely outside one of the planes.
	Intersects,///< Crosses at least one plane.
	Inside///< Entirely inside every plane.
};

/**
 * @brief
 *  The six planes of a view-projection, each `(normal, distance)` with the normal pointing inside.
 *
 * The depth is the reversed-Z one of the engine projections (`perspectiveReversedZ`, `orthoReversedZ`): near plane
 * clip `z <= w`, far plane `z >= 0`. With an infinite far plane that last plane has a null normal and keeps every box.
 */
class Frustum final {
public:
	/**
	 * @brief
	 *  Build the planes of a view-projection matrix (Gribb-Hartmann).
	 * @param[in] iViewProjection The view-projection (world to clip).
	 * @return The frustum, planes normalised.
	 */
	[[nodiscard]] static constexpr auto fromViewProjection(const mat4& iViewProjection) -> Frustum {
		Frustum frustum;
		const auto row = [&iViewProjection](const size_t iRow) -> vec4 {
			return vec4{iViewProjection(iRow, 0), iViewProjection(iRow, 1), iViewProjection(iRow, 2),
						iViewProjection(iRow, 3)};
		};
		const vec4 last = row(3);
		frustum.m_planes = {last + row(0), last - row(0), last + row(1), last - row(1), last - row(2), row(2)};
		for (auto& plane: frustum.m_planes) {
			if (const float lenSq = plane.x() * plane.x() + plane.y() * plane.y() + plane.z() * plane.z(); lenSq > 0.f)
				plane *= 1.f / std::sqrt(lenSq);
		}
		auto& nearPlane = frustum.m_planes[4];
		nearPlane.w() += nearSlack(nearPlane);
		return frustum;
	}

	/**
	 * @brief
	 *  Get the planes: left, right, bottom, top, near, far.
	 * @return The planes, the near one moved outward by its tolerance.
	 */
	[[nodiscard]] constexpr auto getPlanes() const -> const std::array<vec4, 6>& { return m_planes; }

	/**
	 * @brief
	 *  Classify a box against the planes.
	 * @param[in] iBox The box, in the space of the view-projection.
	 * @return Where the box lies.
	 */
	[[nodiscard]] constexpr auto classify(const box3f& iBox) const -> Containment {
		const auto& mn = iBox.min();
		const auto& mx = iBox.max();
		bool inside = true;
		for (const auto& plane: m_planes) {
			const vec3 far{plane.x() >= 0.f ? mx.x() : mn.x(), plane.y() >= 0.f ? mx.y() : mn.y(),
						   plane.z() >= 0.f ? mx.z() : mn.z()};
			if (distance(plane, far) < 0.f)
				return Containment::Outside;
			const vec3 near{plane.x() >= 0.f ? mn.x() : mx.x(), plane.y() >= 0.f ? mn.y() : mx.y(),
							plane.z() >= 0.f ? mn.z() : mx.z()};
			if (distance(plane, near) < 0.f)
				inside = false;
		}
		return inside ? Containment::Inside : Containment::Intersects;
	}

	/**
	 * @brief
	 *  Check whether a box touches the frustum.
	 * @param[in] iBox The box, in the space of the view-projection.
	 * @return True unless the box is entirely outside a plane.
	 */
	[[nodiscard]] constexpr auto intersects(const box3f& iBox) const -> bool {
		return classify(iBox) != Containment::Outside;
	}

private:
	/**
	 * @brief
	 *  Signed distance of a point to a plane.
	 * @param[in] iPlane The plane, normalised.
	 * @param[in] iPoint The point.
	 * @return The distance, positive inside.
	 */
	[[nodiscard]] static constexpr auto distance(const vec4& iPlane, const vec3& iPoint) -> float {
		return iPlane.x() * iPoint.x() + iPlane.y() * iPoint.y() + iPlane.z() * iPoint.z() + iPlane.w();
	}

	/**
	 * @brief
	 *  Distance a box may lie outside the near plane and still be kept: a box lying on it (a sprite on the near plane
	 *  of an orthographic camera) stays visible whatever the rounding of the plane, as the GPU clipping keeps it.
	 * @param[in] iPlane The near plane, normalised.
	 * @return The tolerance, relative to the distance of the plane to the origin.
	 */
	[[nodiscard]] static constexpr auto nearSlack(const vec4& iPlane) -> float {
		return 1e-5f * (1.f + (iPlane.w() < 0.f ? -iPlane.w() : iPlane.w()));
	}

	/// Left, right, bottom, top, near and far planes.
	std::array<vec4, 6> m_planes{};
};

/**
 * @brief
 *  The world box of a local box under an affine transform (Arvo: each axis of the matrix spread over the extent).
 * @param[in] iTransform The transform (column vectors, translation in the last column).
 * @param[in] iBox The local box.
 * @return The box enclosing the transformed one.
 */
[[nodiscard]] constexpr auto transformBox(const mat4& iTransform, const box3f& iBox) -> box3f {
	vec3 mn{iTransform(0, 3), iTransform(1, 3), iTransform(2, 3)};
	vec3 mx = mn;
	for (size_t row = 0; row < 3; ++row) {
		for (size_t col = 0; col < 3; ++col) {
			const float a = iTransform(row, col) * iBox.min()[col];
			const float b = iTransform(row, col) * iBox.max()[col];
			mn[row] += a < b ? a : b;
			mx[row] += a < b ? b : a;
		}
	}
	return box3f{mn, mx};
}

}// namespace owl::math
