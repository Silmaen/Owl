/**
 * @file NavMesh.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "core/expected.h"
#include "math/matrices.h"
#include "math/vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace owl::scene::nav {

/**
 * @brief
 *  How a navigation mesh is cut from the geometry of a scene, in world units (Y up).
 */
struct OWL_API NavMeshSettings {
	/// Width of a voxel on X and Z: the precision of the borders.
	float cellSize = 0.25f;
	/// Height of a voxel: the precision of the floor heights.
	float cellHeight = 0.1f;
	/// Free height an agent needs above the floor.
	float agentHeight = 1.8f;
	/// Radius of the agents: the floor is shrunk by it along the walls.
	float agentRadius = 0.4f;
	/// Highest step an agent climbs (a stair, a kerb).
	float maxClimb = 0.4f;
	/// Steepest walkable slope, in degrees.
	float maxSlope = 45.f;
	/// Smallest island of floor kept, in square units (a table top, a crate lid are dropped below it).
	float minRegionArea = 2.f;
	/// Largest distance between the border of the mesh and the voxel border it simplifies.
	float maxEdgeError = 0.3f;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two settings value by value.
	 * @param[in] iOther The other settings.
	 * @return True when every value is equal.
	 */
	auto operator==(const NavMeshSettings& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  The triangles a navigation mesh is cut from, in world space.
 */
struct OWL_API NavGeometry {
	/// Corners of the triangles.
	std::vector<math::vec3> vertices;
	/// Three indices in `vertices` per triangle, counter-clockwise seen from the side the agents walk on.
	std::vector<uint32_t> indices;

	/**
	 * @brief
	 *  Add triangles moved by a matrix.
	 * @param[in] iVertices Corners, in the space of the matrix.
	 * @param[in] iIndices Three indices per triangle (an index out of range drops its triangle).
	 * @param[in] iTransform Matrix to world space.
	 */
	void addTriangles(std::span<const math::vec3> iVertices, std::span<const uint32_t> iIndices,
					  const math::mat4& iTransform);

	/**
	 * @brief
	 *  Add the six faces of a box.
	 * @param[in] iSize Full size of the box, centred on the origin of the matrix.
	 * @param[in] iTransform Matrix to world space.
	 */
	void addBox(const math::vec3& iSize, const math::mat4& iTransform);

	/**
	 * @brief
	 *  Add a cylinder along Y, its sides cut in twelve faces.
	 * @param[in] iDiameter Diameter of the cylinder.
	 * @param[in] iHeight Full height, centred on the origin of the matrix.
	 * @param[in] iTransform Matrix to world space.
	 */
	void addCylinder(float iDiameter, float iHeight, const math::mat4& iTransform);

	/**
	 * @brief
	 *  Check if there is no triangle.
	 * @return True when empty.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return indices.size() < 3; }
};

/// Why a navigation mesh could not be built.
enum struct NavBuildError : uint8_t {
	NoGeometry,///< No triangle in the bounds.
	InvalidSettings,///< A size or a height is not positive, or the bounds are empty.
	TooLarge,///< The bounds hold more voxel columns than the builder accepts (see `NavMesh::MaxColumns`).
	NoWalkableArea,///< No floor is wide, flat and high enough for an agent.
};

/**
 * @brief
 *  A jump, a ladder or a door: a link between two points of a navigation mesh, crossed in a straight line.
 */
struct OWL_API NavMeshLink {
	/// Start point, in world space.
	math::vec3 start;
	/// End point, in world space.
	math::vec3 end;
	/// Whether the link is crossed both ways.
	bool bidirectional = true;
	/// Cost of the crossing per unit of length, at least 1 (walking costs 1).
	float costScale = 1.f;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two links value by value.
	 * @param[in] iOther The other link.
	 * @return True when every value is equal.
	 */
	auto operator==(const NavMeshLink& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  A point of a navigation mesh: its position and the triangle under it.
 */
struct OWL_API NavPoint {
	/// Position on the surface.
	math::vec3 position;
	/// Index of the triangle holding it.
	uint32_t triangle = 0;
};

/**
 * @brief
 *  A corner of a path on a navigation mesh.
 */
struct OWL_API NavPathPoint {
	/// Position, in world space.
	math::vec3 position;
	/// Link crossed from this corner to the next one, -1 when walking.
	int32_t link = -1;
};

/**
 * @brief
 *  The walkable surface of a 3D scene as triangles, and the shortest paths on it.
 *
 * `build` cuts it from triangles: they are voxelised, the voxels too steep, too low under a ceiling or too close to a
 * wall or a ledge are dropped, the rest is split into regions without overlap, their borders are traced, simplified
 * and cut into triangles. Floors stacked one above the other stay apart; stairs and ramps join them. `findPath` runs
 * A* over the triangles and the links, then pulls each walked stretch tight (funnel algorithm): the path turns only
 * at corners of the border. Everything is computed in a fixed order, so the same input gives the same mesh and the
 * same paths on every target.
 */
class OWL_API NavMesh final {
public:
	/// Most voxel columns `build` accepts (4096 x 4096).
	static constexpr uint32_t MaxColumns = 4096u * 4096u;

	/// A triangle of the mesh, counter-clockwise in the plane (x, z).
	struct Triangle {
		/// Indices of its corners in `getVertices`.
		std::array<uint32_t, 3> vertices{};
		/// Triangle across the edge from corner `i` to corner `i + 1`, -1 on the border.
		std::array<int32_t, 3> neighbours{-1, -1, -1};
	};

	/// A link attached to the mesh.
	struct AttachedLink {
		/// The link, its ends moved onto the mesh.
		NavMeshLink link;
		/// Triangle under the start.
		uint32_t startTriangle = 0;
		/// Triangle under the end.
		uint32_t endTriangle = 0;
	};

	/**
	 * @brief
	 *  Build the mesh from triangles; the previous mesh is dropped.
	 * @param[in] iGeometry The triangles.
	 * @param[in] iSettings How the mesh is cut.
	 * @param[in] iBoundsMin Lowest corner of the box the mesh is cut in.
	 * @param[in] iBoundsMax Highest corner of the box.
	 * @return Nothing on success, the cause otherwise (the mesh is then empty).
	 */
	auto build(const NavGeometry& iGeometry, const NavMeshSettings& iSettings, const math::vec3& iBoundsMin,
			   const math::vec3& iBoundsMax) -> owl::expected<void, NavBuildError>;

	/**
	 * @brief
	 *  Set the mesh from saved triangles; the neighbours are linked again.
	 * @param[in] iVertices Corners.
	 * @param[in] iIndices Three indices per triangle, counter-clockwise in the plane (x, z).
	 */
	void setTriangles(std::span<const math::vec3> iVertices, std::span<const uint32_t> iIndices);

	/**
	 * @brief
	 *  Attach links to the mesh; the previous links are dropped.
	 *
	 * A link whose start or end is farther than `iReach` from the mesh is skipped.
	 * @param[in] iLinks The links.
	 * @param[in] iReach Largest distance between an end and the mesh.
	 */
	void setLinks(std::span<const NavMeshLink> iLinks, float iReach = 1.f);

	/**
	 * @brief
	 *  Drop every triangle and link.
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
	 *  Get the corners of the triangles.
	 * @return The vertices.
	 */
	[[nodiscard]] auto getVertices() const -> const std::vector<math::vec3>& { return m_vertices; }

	/**
	 * @brief
	 *  Get the triangles.
	 * @return The triangles.
	 */
	[[nodiscard]] auto getTriangles() const -> const std::vector<Triangle>& { return m_triangles; }

	/**
	 * @brief
	 *  Get the attached links.
	 * @return The links.
	 */
	[[nodiscard]] auto getLinks() const -> const std::vector<AttachedLink>& { return m_links; }

	/**
	 * @brief
	 *  Get the indices of the triangles, three per triangle, as `setTriangles` reads them.
	 * @return The indices.
	 */
	[[nodiscard]] auto getIndices() const -> std::vector<uint32_t>;

	/**
	 * @brief
	 *  Get the total area of the triangles, seen from above.
	 * @return The area.
	 */
	[[nodiscard]] auto getArea() const -> float;

	/**
	 * @brief
	 *  Find the point of the mesh nearest to a point.
	 * @param[in] iPoint The point.
	 * @param[in] iReach Largest distance to the mesh.
	 * @return The nearest point and its triangle, nothing when the mesh is farther than `iReach`.
	 */
	[[nodiscard]] auto findNearest(const math::vec3& iPoint, float iReach = 2.f) const -> std::optional<NavPoint>;

	/**
	 * @brief
	 *  Get the height of the surface of a triangle above a point of the plane (x, z).
	 * @param[in] iTriangle Index of the triangle.
	 * @param[in] iX X of the point.
	 * @param[in] iZ Z of the point.
	 * @return The height of the plane of the triangle there.
	 */
	[[nodiscard]] auto getHeight(uint32_t iTriangle, float iX, float iZ) const -> float;

	/**
	 * @brief
	 *  Find the shortest path between two points of the mesh.
	 * @param[in] iFrom Start (moved onto the mesh).
	 * @param[in] iTo Goal (moved onto the mesh).
	 * @param[out] oPath The corners, start and goal included.
	 * @param[in] iReach Largest distance between a point and the mesh.
	 * @return True when a path joins them, false when a point is off the mesh or nothing joins them.
	 */
	auto findPath(const math::vec3& iFrom, const math::vec3& iTo, std::vector<NavPathPoint>& oPath,
				  float iReach = 2.f) const -> bool;

	/**
	 * @brief
	 *  Move a point towards a target, sliding along the border of the mesh.
	 * @param[in] iFrom The point and its triangle.
	 * @param[in] iTarget Where it moves to (only its X and Z are used).
	 * @return The point reached, on the surface, and its triangle.
	 */
	[[nodiscard]] auto moveAlongSurface(const NavPoint& iFrom, const math::vec3& iTarget) const -> NavPoint;

	/**
	 * @brief
	 *  Get the edges on the border of the mesh (an edge with no triangle across it).
	 * @return Pairs of vertex indices.
	 */
	[[nodiscard]] auto getBorderEdges() const -> std::vector<std::array<uint32_t, 2>>;

private:
	/// Corners of the triangles.
	std::vector<math::vec3> m_vertices;
	/// Triangles of the mesh.
	std::vector<Triangle> m_triangles;
	/// Links attached to the mesh.
	std::vector<AttachedLink> m_links;
	/// Links leaving each triangle: index in `m_links`, negative (`-1 - index`) when crossed from its end.
	std::vector<std::vector<int32_t>> m_linksFrom;

	/**
	 * @brief
	 *  Link the triangles that share an edge.
	 */
	void linkNeighbours();

	/**
	 * @brief
	 *  Find the triangles joining two triangles, the links crossed included.
	 * @param[in] iFrom Start point.
	 * @param[in] iTo Goal point.
	 * @param[out] oCorridor Triangles in walking order, with the link taken to reach each one (-1 when walking).
	 * @return True when a corridor was found.
	 */
	auto findCorridor(const NavPoint& iFrom, const NavPoint& iTo,
					  std::vector<std::pair<uint32_t, int32_t>>& oCorridor) const -> bool;

	/**
	 * @brief
	 *  Pull a walked stretch of a corridor tight and append its corners.
	 * @param[in] iCorridor Triangles of the stretch, in walking order.
	 * @param[in] iFrom Start of the stretch.
	 * @param[in] iTo End of the stretch.
	 * @param[in,out] ioPath The path, its last corner being `iFrom`.
	 */
	void pullTight(std::span<const uint32_t> iCorridor, const math::vec3& iFrom, const math::vec3& iTo,
				   std::vector<NavPathPoint>& ioPath) const;
};

}// namespace owl::scene::nav
