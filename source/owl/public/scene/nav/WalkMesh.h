/**
 * @file WalkMesh.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "math/vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace owl::scene::nav {

/**
 * @brief
 *  A floor where actors walk freely: an outline and the holes cut in it (a well, a table, a pond).
 * The points are on a plane (x, y); the outline and the holes may be given in either winding.
 */
struct OWL_API WalkPolygon {
	/// Outline of the floor, at least three points.
	std::vector<math::vec2> outline;
	/// Holes inside the outline, each one at least three points.
	std::vector<std::vector<math::vec2>> holes;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two polygons point by point.
	 * @param[in] iOther The other polygon.
	 * @return True when every point is equal.
	 */
	auto operator==(const WalkPolygon& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  The triangles of a set of `WalkPolygon`s and the shortest paths across them.
 *
 * `build` bridges the holes into their outline and cuts the result into triangles (ear clipping); polygons whose edges
 * meet vertex to vertex join into one floor. `findPath` runs A* over the triangles, from edge middle to edge middle,
 * then pulls the corridor tight (funnel algorithm): the path only turns at corners of the outlines and the holes. A
 * point outside the floor is first moved to the nearest point of its edge.
 */
class OWL_API WalkMesh final {
public:
	/// A triangle of the floor, counter-clockwise.
	struct Triangle {
		/// Indices of its corners in `getVertices`.
		std::array<uint32_t, 3> vertices{};
		/// Triangle across the edge from corner `i` to corner `i + 1`, -1 on the border of the floor.
		std::array<int32_t, 3> neighbours{-1, -1, -1};
	};

	/**
	 * @brief
	 *  Cut polygons into triangles; the previous triangles are dropped.
	 * @param[in] iPolygons The polygons (one with fewer than three outline points is skipped).
	 */
	void build(std::span<const WalkPolygon> iPolygons);

	/**
	 * @brief
	 *  Drop every triangle.
	 */
	void clear();

	/**
	 * @brief
	 *  Check if the mesh has no triangle.
	 * @return True when empty.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return m_triangles.empty(); }

	/**
	 * @brief
	 *  Get the corners of the triangles (equal points of the polygons are merged).
	 * @return The vertices.
	 */
	[[nodiscard]] auto getVertices() const -> const std::vector<math::vec2>& { return m_vertices; }

	/**
	 * @brief
	 *  Get the triangles.
	 * @return The triangles.
	 */
	[[nodiscard]] auto getTriangles() const -> const std::vector<Triangle>& { return m_triangles; }

	/**
	 * @brief
	 *  Get the total area of the floor.
	 * @return The area.
	 */
	[[nodiscard]] auto getArea() const -> float;

	/**
	 * @brief
	 *  Find the triangle holding a point.
	 * @param[in] iPoint The point.
	 * @return The index of the triangle, nothing when the point is off the floor.
	 */
	[[nodiscard]] auto findTriangle(const math::vec2& iPoint) const -> std::optional<uint32_t>;

	/**
	 * @brief
	 *  Check if a point is on the floor (borders included).
	 * @param[in] iPoint The point.
	 * @return True when on the floor.
	 */
	[[nodiscard]] auto contains(const math::vec2& iPoint) const -> bool { return findTriangle(iPoint).has_value(); }

	/**
	 * @brief
	 *  Find the point of the floor nearest to a point.
	 * @param[in] iPoint The point.
	 * @return The point itself when on the floor, else the nearest point of a border; nothing when the mesh is empty.
	 */
	[[nodiscard]] auto getClosestPoint(const math::vec2& iPoint) const -> std::optional<math::vec2>;

	/**
	 * @brief
	 *  Find the shortest path between two points of the floor.
	 * @param[in] iFrom Start (moved onto the floor when outside).
	 * @param[in] iTo Goal (moved onto the floor when outside).
	 * @param[out] oPath The corners of the path, start and goal included.
	 * @return True when a path joins them, false when they lie on two floors that do not touch or the mesh is empty.
	 */
	auto findPath(const math::vec2& iFrom, const math::vec2& iTo, std::vector<math::vec2>& oPath) const -> bool;

	/**
	 * @brief
	 *  Get the edges of the border of the floor (an edge with no triangle across it).
	 * @return Pairs of vertex indices.
	 */
	[[nodiscard]] auto getBorderEdges() const -> std::vector<std::array<uint32_t, 2>>;

private:
	/// Merged corners of the polygons.
	std::vector<math::vec2> m_vertices;
	/// Triangles of the floor.
	std::vector<Triangle> m_triangles;

	/**
	 * @brief
	 *  Index of a point in `m_vertices`, added when new.
	 * @param[in] iPoint The point.
	 * @return The index.
	 */
	auto addVertex(const math::vec2& iPoint) -> uint32_t;

	/**
	 * @brief
	 *  Find the triangle of a point on the floor, or of its nearest border point.
	 * @param[in,out] ioPoint The point, moved onto the floor when outside.
	 * @return The triangle, nothing when the mesh is empty.
	 */
	auto locate(math::vec2& ioPoint) const -> std::optional<uint32_t>;

	/**
	 * @brief
	 *  Link the triangles that share an edge.
	 */
	void linkNeighbours();
};

}// namespace owl::scene::nav
